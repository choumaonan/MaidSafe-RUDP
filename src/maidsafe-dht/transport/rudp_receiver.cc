/* Copyright (c) 2010 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "maidsafe-dht/transport/rudp_receiver.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>

#include "maidsafe-dht/transport/rudp_congestion_control.h"
#include "maidsafe-dht/transport/rudp_negative_ack_packet.h"
#include "maidsafe-dht/transport/rudp_peer.h"
#include "maidsafe-dht/transport/rudp_tick_timer.h"
#include "maidsafe/common/utils.h"

namespace asio = boost::asio;
namespace ip = asio::ip;
namespace bptime = boost::posix_time;

namespace maidsafe {

namespace transport {

RudpReceiver::RudpReceiver(RudpPeer &peer, RudpTickTimer &tick_timer,
                           RudpCongestionControl &congestion_control)
  : peer_(peer),
    tick_timer_(tick_timer),
    congestion_control_(congestion_control),
    unread_packets_(),
    acks_(),
    last_ack_packet_sequence_number_(0) {
}

void RudpReceiver::Reset(boost::uint32_t initial_sequence_number) {
  unread_packets_.Reset(initial_sequence_number);
  last_ack_packet_sequence_number_ = initial_sequence_number;
}

bool RudpReceiver::Flushed() const {
  boost::uint32_t ack_packet_seqnum = AckPacketSequenceNumber();
  return acks_.IsEmpty() &&
         (ack_packet_seqnum == last_ack_packet_sequence_number_);
}

size_t RudpReceiver::ReadData(const boost::asio::mutable_buffer &data) {
  for (boost::uint32_t n = unread_packets_.Begin();
       n != unread_packets_.End();
       n = unread_packets_.Next(n)) {
    UnreadPacket &p = unread_packets_[n];
    if (p.lost) {
      return 0;
    } else if (p.packet.Data().size() > p.bytes_read) {
      size_t length = std::min(asio::buffer_size(data),
                               p.packet.Data().size() - p.bytes_read);
      std::memcpy(asio::buffer_cast<void *>(data),
                  p.packet.Data().data() + p.bytes_read, length);
      p.bytes_read += length;
      if (p.packet.Data().size() == p.bytes_read)
        unread_packets_.Remove();
      return length;
    } else {
      unread_packets_.Remove();
    }
  }
  return 0;
}

void RudpReceiver::HandleData(const RudpDataPacket &packet) {
  unread_packets_.SetMaximumSize(congestion_control_.WindowSize());

  boost::uint32_t seqnum = packet.PacketSequenceNumber();

  // Make sure there is space in the window for packets that are expected soon.
  while (unread_packets_.IsComingSoon(seqnum) && !unread_packets_.IsFull())
    unread_packets_.Append(); // New entries are marked "lost" by default.

  // Ignore any packet which isn't in the window.
  if (unread_packets_.Contains(seqnum)) {
    UnreadPacket &p = unread_packets_[seqnum];
    if (p.lost) {
      congestion_control_.OnDataPacketReceived(seqnum);
      p.packet = packet;
      p.lost = false;
      p.bytes_read = 0;
    }
  }

  if (seqnum % congestion_control_.AckInterval() == 0) {
    // Send acknowledgement packets immediately.
    HandleTick();
  } else {
    // Schedule generation of acknowledgement packets for later.
    tick_timer_.TickAfter(congestion_control_.AckDelay());
  }
}

void RudpReceiver::HandleAckOfAck(const RudpAckOfAckPacket &packet) {
  boost::uint32_t ack_seqnum = packet.AckSequenceNumber();

  if (acks_.Contains(ack_seqnum)) {
    Ack &a = acks_[ack_seqnum];
    boost::posix_time::time_duration rtt = tick_timer_.Now() - a.send_time;
    boost::uint64_t rtt_us = rtt.total_microseconds();
    if (rtt_us < UINT32_MAX) {
      congestion_control_.OnAckOfAck(static_cast<boost::uint32_t>(rtt_us));
    }
  }

  while (acks_.Contains(ack_seqnum)) {
    acks_.Remove();
  }
}

void RudpReceiver::HandleTick() {
  bptime::ptime now = tick_timer_.Now();

  // Generate an acknowledgement only if the latest sequence number has
  // changed, or if it has been too long since the last unacknowledged
  // acknowledgement.
  boost::uint32_t ack_packet_seqnum = AckPacketSequenceNumber();
  if ((ack_packet_seqnum != last_ack_packet_sequence_number_) ||
      (!acks_.IsEmpty() &&
       (acks_.Back().send_time + congestion_control_.AckTimeout() <= now))) {
    if (acks_.IsFull())
      acks_.Remove();
    boost::uint32_t n = acks_.Append();
    Ack& a = acks_[n];
    a.packet.SetDestinationSocketId(peer_.Id());
    a.packet.SetAckSequenceNumber(n);
    a.packet.SetPacketSequenceNumber(ack_packet_seqnum);
    a.packet.SetHasOptionalFields(false);
    a.send_time = now;
    peer_.Send(a.packet);
    last_ack_packet_sequence_number_ = ack_packet_seqnum;
    tick_timer_.TickAt(now + congestion_control_.AckTimeout());
  }

  // Generate a negative acknowledgement packet to request missing packets.
  RudpNegativeAckPacket negative_ack;
  negative_ack.SetDestinationSocketId(peer_.Id());
  boost::uint32_t n = unread_packets_.Begin();
  while (n != unread_packets_.End()) {
    if (unread_packets_[n].lost) {
      boost::uint32_t begin = n;
      boost::uint32_t end;
      do {
        end = n;
        n = unread_packets_.Next(n);
      } while (n != unread_packets_.End() && unread_packets_[n].lost);
      if (begin == end)
        negative_ack.AddSequenceNumber(begin);
      else
        negative_ack.AddSequenceNumbers(begin, end);
    } else {
      n = unread_packets_.Next(n);
    }
  }
  if (negative_ack.HasSequenceNumbers()) {
    peer_.Send(negative_ack);
    tick_timer_.TickAt(now + congestion_control_.AckTimeout());
  }
}

boost::uint32_t RudpReceiver::AckPacketSequenceNumber() const {
  // Work out what sequence number we need to acknowledge up to.
  boost::uint32_t ack_packet_seqnum = unread_packets_.Begin();
  while (ack_packet_seqnum != unread_packets_.End() &&
         !unread_packets_[ack_packet_seqnum].lost)
    ack_packet_seqnum = unread_packets_.Next(ack_packet_seqnum);
  return ack_packet_seqnum;
}

}  // namespace transport

}  // namespace maidsafe
