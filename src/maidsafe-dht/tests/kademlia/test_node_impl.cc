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
#include <bitset>

#include "boost/lexical_cast.hpp"
#include "boost/thread.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "maidsafe-dht/transport/utils.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/crypto.h"
#include "maidsafe-dht/kademlia/alternative_store.h"
#include "maidsafe-dht/kademlia/securifier.h"
#include "maidsafe-dht/kademlia/rpcs.h"
#include "maidsafe-dht/kademlia/contact.h"
#include "maidsafe-dht/kademlia/node_id.h"
#include "maidsafe-dht/kademlia/node_impl.h"
#include "maidsafe-dht/kademlia/routing_table.h"
#include "maidsafe-dht/kademlia/datastore.h"

namespace maidsafe {

namespace kademlia {

namespace test {

static const boost::uint16_t k = 8;
static const boost::uint16_t alpha = 3;
static const boost::uint16_t beta = 2;

void FindNodeCallback(RankInfoPtr rank_info,
                      int result_size,
                      const std::vector<Contact> &cs,
                      bool *done,
                      std::vector<Contact> *contacts) {
  contacts->clear();
  *contacts = cs;
  *done = true;
}

class CreateContactAndNodeId {
 public:
  CreateContactAndNodeId() : contact_(), node_id_(NodeId::kRandomId),
                   routing_table_(new RoutingTable(node_id_, test::k)) {}
   
  NodeId GenerateUniqueRandomId(const NodeId& holder, const int& pos) {
    std::string holder_id = holder.ToStringEncoded(NodeId::kBinary);
    std::bitset<kKeySizeBits> holder_id_binary_bitset(holder_id);
    NodeId new_node;
    std::string new_node_string;
    bool repeat(true);
    boost::uint16_t times_of_try(0);
    // generate a random ID and make sure it has not been generated previously
    do {
      new_node = NodeId(NodeId::kRandomId);
      std::string new_id = new_node.ToStringEncoded(NodeId::kBinary);
      std::bitset<kKeySizeBits> binary_bitset(new_id);
      for (int i = kKeySizeBits - 1; i >= pos; --i)
        binary_bitset[i] = holder_id_binary_bitset[i];
      binary_bitset[pos].flip();
      new_node_string = binary_bitset.to_string();
      new_node = NodeId(new_node_string, NodeId::kBinary);
      // make sure the new contact not already existed in the routing table
      Contact result;
      routing_table_->GetContact(new_node, &result);
      if (result == Contact())
        repeat = false;
      ++times_of_try;
    } while (repeat && (times_of_try < 1000));
    // prevent deadlock, throw out an error message in case of deadlock
    if (times_of_try == 1000)
      EXPECT_LT(1000, times_of_try);
    return new_node;
  }

  NodeId GenerateRandomId(const NodeId& holder, const int& pos) {
    std::string holder_id = holder.ToStringEncoded(NodeId::kBinary);
    std::bitset<kKeySizeBits> holder_id_binary_bitset(holder_id);
    NodeId new_node;
    std::string new_node_string;

    new_node = NodeId(NodeId::kRandomId);
    std::string new_id = new_node.ToStringEncoded(NodeId::kBinary);
    std::bitset<kKeySizeBits> binary_bitset(new_id);
    for (int i = kKeySizeBits - 1; i >= pos; --i)
      binary_bitset[i] = holder_id_binary_bitset[i];
    binary_bitset[pos].flip();
    new_node_string = binary_bitset.to_string();
    new_node = NodeId(new_node_string, NodeId::kBinary);

    return new_node;
  }

  Contact ComposeContact(const NodeId& node_id, boost::uint16_t port) {
    std::string ip("127.0.0.1");
    std::vector<transport::Endpoint> local_endpoints;
    transport::Endpoint end_point(ip, port);
    local_endpoints.push_back(end_point);
    Contact contact(node_id, end_point, local_endpoints, end_point, false,
                    false, "", "", "");
    return contact;
  }

  void PopulateContactsVector(int count,
                              const int& pos,
                              std::vector<Contact> *contacts) {
    for (int i = 0; i < count; ++i) {
      NodeId contact_id = GenerateRandomId(node_id_, pos);
      Contact contact = ComposeContact(contact_id, 5000);
      contacts->push_back(contact);
    }
  }

  Contact contact_;
  kademlia::NodeId node_id_;
  std::shared_ptr<RoutingTable> routing_table_;
};
// void CreateParameters(NodeConstructionParameters *kcp) {
//   crypto::RsaKeyPair rkp;
//   rkp.GenerateKeys(4096);
//   kcp->alpha = kAlpha;
//   kcp->beta = kBeta;
//   kcp->type = VAULT;
//   kcp->public_key = rkp.public_key();
//   kcp->private_key = rkp.private_key();
//   kcp->k = K;
//   kcp->refresh_time = kRefreshTime;
// }

class TestAlternativeStore : public AlternativeStore {
 public:
  ~TestAlternativeStore() {}
  bool Has(const std::string&) { return false; }
};

// class TestValidator : public SignatureValidator {
//  public:
//   ~TestValidator() {}
//   bool ValidateSignerId(const std::string&, const std::string&,
//                         const std::string&) { return true; }
//   bool ValidateRequest(const std::string&, const std::string&,
//                        const std::string&, const std::string&) { return true; }
// };

class NodeImplTest : public CreateContactAndNodeId, public testing::Test {
 protected:
  NodeImplTest() : CreateContactAndNodeId(),
                   data_store_(),
                   alternative_store_(),
                   securifier_(new Securifier("", "", "")),
                   info_(), rank_info_(), asio_service_(),
                   node_(new Node::Impl(asio_service_, info_,
                         securifier_, alternative_store_, true, test::k,
                         test::alpha, test::beta, bptime::seconds(3600))) {
    data_store_ = node_->data_store_;
    node_->routing_table_ = routing_table_;
  }
   
  static void SetUpTestCase() {
//     test_dir_ = std::string("temp/NodeImplTest") +
//                 boost::lexical_cast<std::string>(RandomUint32());
//    asio_service_.reset(new boost::asio::io_service);
//    udt_.reset(new transport::UdtTransport(asio_service_));
//     std::vector<IP> ips = transport::GetLocalAddresses();
//     transport::Endpoint ep(ips.at(0), 50000);
//    EXPECT_EQ(transport::kSuccess, udt_->StartListening(ep));
// 
//     crypto::RsaKeyPair rkp;
//     rkp.GenerateKeys(4096);
//     NodeConstructionParameters kcp;
//     kcp.alpha = kAlpha;
//     kcp.beta = kBeta;
//     kcp.type = VAULT;
//     kcp.public_key = rkp.public_key();
//     kcp.private_key = rkp.private_key();
//     kcp.k = K;
//     kcp.refresh_time = kRefreshTime;
//     kcp.port = ep.port;
//     node_.reset(new NodeImpl(udt_, kcp));
// 
//     node_->JoinFirstNode(test_dir_ + std::string(".kadconfig"),
//                          ep.ip, ep.port,
//                          boost::bind(&GeneralKadCallback::CallbackFunc,
//                                      &cb_, _1));
//     wait_result(&cb_);
//     ASSERT_TRUE(cb_.result());
//     ASSERT_TRUE(node_->is_joined());
  }
  static void TearDownTestCase() {
//    udt_->StopListening();
//    printf("udt_->StopListening();\n");
//     node_->Leave();
//    transport::UdtTransport::CleanUp();
  }

  void PopulateRoutingTable(boost::uint16_t count, boost::uint16_t pos) {
    for (int num_contact = 0; num_contact < count; ++num_contact) {
      NodeId contact_id = GenerateUniqueRandomId(node_id_, pos);
      Contact contact = ComposeContact(contact_id, 5000);
      AddContact(contact, rank_info_);
    }
  }

  void AddContact(const Contact& contact, const RankInfoPtr rank_info) {
    routing_table_->AddContact(contact, rank_info);
    routing_table_->SetValidated(contact.node_id(), true);
  }

  void GenericCallback(const std::string&, bool *done) { *done = true; }

  std::shared_ptr<Rpcs> GetRpc() {
    return node_->rpcs_;
  }

  void SetRpc(std::shared_ptr<Rpcs> rpc) {
    node_->rpcs_ = rpc;
  }

  std::shared_ptr<DataStore> data_store_;
  AlternativeStorePtr alternative_store_;
  SecurifierPtr securifier_;
  TransportPtr info_;
  RankInfoPtr rank_info_;
  std::shared_ptr<boost::asio::io_service> asio_service_;
  std::shared_ptr<Node::Impl> node_;
//   static std::string test_dir_;
//   static boost::int16_t transport_id_;
//   static boost::shared_ptr<transport::UdtTransport> udt_;
//   static GeneralKadCallback cb_;  
};

// std::string NodeImplTest::test_dir_;
// boost::int16_t NodeImplTest::transport_id_ = 0;
// boost::shared_ptr<transport::UdtTransport> NodeImplTest::udt_;
// boost::shared_ptr<Node::Impl> NodeImplTest::node_;
// GeneralKadCallback NodeImplTest::cb_;
// boost::shared_ptr<boost::asio::io_service> NodeImplTest::asio_service_;

class MockRpcs : public Rpcs, public CreateContactAndNodeId {
 public:
  explicit MockRpcs(std::shared_ptr<boost::asio::io_service> asio_service,
                    SecurifierPtr securifier)
      : Rpcs(asio_service, securifier),
        CreateContactAndNodeId(),
        node_list_mutex_(),
        node_list_(),
        rank_info_(),
        num_of_acquired_(0),
        respond_contacts_(new RoutingTable(node_id_, test::k)) {}
  MOCK_METHOD5(FindNodes, void(const NodeId &key,
                               const SecurifierPtr securifier,
                               const Contact &contact,
                               FindNodesFunctor callback,
                               TransportType type));

  void FindNodeResponseClose(const Contact &c,
                     FindNodesFunctor callback) {
    std::vector<Contact> response_list;
    boost::mutex::scoped_lock loch_queldomage(node_list_mutex_);
    int elements = RandomUint32() % test::k;
    for (int n = 0; n < elements; ++n) {
      int element = RandomUint32() % node_list_.size();
      response_list.push_back(node_list_[element]);
      respond_contacts_->AddContact(node_list_[element], rank_info_);
      respond_contacts_->SetValidated(node_list_[element].node_id(), true);
    }
    boost::thread th(boost::bind(&MockRpcs::ResponseThread, this, callback,
                                 response_list));
  }
  
  void FindNodeResponseNoClose(const Contact &c,
                     FindNodesFunctor callback) {
    boost::mutex::scoped_lock loch_queldomage(node_list_mutex_);
    std::vector<Contact> response_list;
    boost::thread th(boost::bind(&MockRpcs::ResponseThread, this, callback,
                                 response_list));
  }

  void FindNodeFirstNoResponse(const Contact &c,
                     FindNodesFunctor callback) {
    boost::mutex::scoped_lock loch_queldomage(node_list_mutex_);    
    std::vector<Contact> response_list;
    if (num_of_acquired_ == 0) {
      boost::thread th(boost::bind(&MockRpcs::NoResponseThread, this, callback,
                                   response_list));
    } else {
      boost::thread th(boost::bind(&MockRpcs::ResponseThread, this, callback,
                                   response_list));
    }
    ++num_of_acquired_;
  }

  void FindNodeFirstAndLastNoResponse(const Contact &c,
                     FindNodesFunctor callback) {
    boost::mutex::scoped_lock loch_queldomage(node_list_mutex_);
    std::vector<Contact> response_list;
    if ((num_of_acquired_ == (test::k - 1)) || (num_of_acquired_ == 0)) {
      boost::thread th(boost::bind(&MockRpcs::NoResponseThread, this, callback,
                                   response_list));
    } else {
      boost::thread th(boost::bind(&MockRpcs::ResponseThread, this, callback,
                                   response_list));
    }
    ++num_of_acquired_;
  }

  void FindNodeNoResponse(const Contact &c,
                     FindNodesFunctor callback) {
    boost::mutex::scoped_lock loch_queldomage(node_list_mutex_);    
    std::vector<Contact> response_list;
    boost::thread th(boost::bind(&MockRpcs::NoResponseThread, this, callback,
                                 response_list));
  }
  
  void ResponseThread(FindNodesFunctor callback,
                         std::vector<Contact> response_list) {
    boost::uint16_t interval(10 * (RandomUint32() % 5) + 1);
    boost::this_thread::sleep(boost::posix_time::milliseconds(interval));
    callback(rank_info_, response_list.size(), response_list);
  }

  void NoResponseThread(FindNodesFunctor callback,
                         std::vector<Contact> response_list) {
    boost::uint16_t interval(100 * (RandomUint32() % 5) + 1);
    boost::this_thread::sleep(boost::posix_time::milliseconds(interval));
    callback(rank_info_, -1, response_list);
  }

  void PopulateResponseCandidates(int count, const int& pos) {
    PopulateContactsVector(count, pos, &node_list_);
  }

  std::vector<Contact> node_list() {
    boost::mutex::scoped_lock loch_queldomage(node_list_mutex_);
    return node_list_;
  }
//   std::list<Contact> backup_node_list() { return backup_node_list_; }

  boost::uint16_t num_of_acquired_;

// private:
  boost::mutex node_list_mutex_;
  std::vector<Contact> node_list_;
  RankInfoPtr rank_info_;
  std::shared_ptr<RoutingTable> respond_contacts_;
};

TEST_F(NodeImplTest, BEH_KAD_FindNodes) {
  PopulateRoutingTable(test::k, 500);

  std::shared_ptr<Rpcs> old_rpcs = GetRpc();
  std::shared_ptr<MockRpcs> new_rpcs(new MockRpcs(asio_service_, securifier_ ));
  new_rpcs->node_id_ = node_id_;
  SetRpc(new_rpcs);

  NodeId key = NodeId(NodeId::kRandomId);
  {
    // All k populated contacts giving no response
    EXPECT_CALL(*new_rpcs, FindNodes(testing::_, testing::_, testing::_,
                                     testing::_, testing::_))
        .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
            boost::bind(&MockRpcs::FindNodeNoResponse,
                        new_rpcs.get(), _1, _2))));
    std::vector<Contact> lcontacts;
    bool done(false);
    node_->FindNodes(key,
                     boost::bind(&FindNodeCallback, rank_info_, _1, _2, &done,
                                 &lcontacts));
    while (!done)
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    EXPECT_EQ(0, lcontacts.size());
  }
  new_rpcs->num_of_acquired_ = 0;
  {
    // The first of the k populated contacts giving no response
    // all the others give response with an empty closest list
    EXPECT_CALL(*new_rpcs, FindNodes(testing::_, testing::_, testing::_,
                                     testing::_, testing::_))
        .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
            boost::bind(&MockRpcs::FindNodeFirstNoResponse,
                        new_rpcs.get(), _1, _2))));
    std::vector<Contact> lcontacts;
    bool done(false);
    node_->FindNodes(key,
                     boost::bind(&FindNodeCallback, rank_info_, _1, _2, &done,
                                 &lcontacts));
    while (!done)
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    EXPECT_EQ(test::k - 1, lcontacts.size());
  }
  new_rpcs->num_of_acquired_ = 0;
  {
    // The first and the last of the k populated contacts giving no response
    // all the others give response with an empty closest list
    EXPECT_CALL(*new_rpcs, FindNodes(testing::_, testing::_, testing::_,
                                     testing::_, testing::_))
        .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
            boost::bind(&MockRpcs::FindNodeFirstAndLastNoResponse,
                        new_rpcs.get(), _1, _2))));
    std::vector<Contact> lcontacts;
    bool done(false);
    node_->FindNodes(key,
                     boost::bind(&FindNodeCallback, rank_info_, _1, _2, &done,
                                 &lcontacts));
    while (!done)
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    EXPECT_EQ(test::k - 2, lcontacts.size());
  }
  {
    // All k populated contacts response with an empty closest list
    EXPECT_CALL(*new_rpcs, FindNodes(testing::_, testing::_, testing::_,
                                     testing::_, testing::_))
        .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
            boost::bind(&MockRpcs::FindNodeResponseNoClose,
                        new_rpcs.get(), _1, _2))));
    std::vector<Contact> lcontacts;
    bool done(false);
    node_->FindNodes(key,
                     boost::bind(&FindNodeCallback, rank_info_, _1, _2, &done,
                                 &lcontacts));
    while (!done)
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    EXPECT_EQ(test::k, lcontacts.size());
  }
  int count = 10 * test::k;
  new_rpcs->PopulateResponseCandidates(count, 499);
  NodeId target = GenerateRandomId(node_id_, 498);
  std::shared_ptr<RoutingTable> temp(new RoutingTable(target, count));
  new_rpcs->respond_contacts_ = temp;
  {
    // All k populated contacts response with random closest list (not greater
    // than k)
    EXPECT_CALL(*new_rpcs, FindNodes(testing::_, testing::_, testing::_,
                                     testing::_, testing::_))
        .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
            boost::bind(&MockRpcs::FindNodeResponseClose,
                        new_rpcs.get(), _1, _2))));
    std::vector<Contact> lcontacts;
    bool done(false);
    node_->FindNodes(target,
                     boost::bind(&FindNodeCallback, rank_info_, _1, _2, &done,
                                 &lcontacts));
    while (!done)
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    EXPECT_EQ(test::k, lcontacts.size());
    EXPECT_NE(lcontacts[0], lcontacts[test::k / 2]);
    EXPECT_NE(lcontacts[0], lcontacts[test::k - 1]);
    
    std::vector<Contact> close_contacts;
    std::vector<Contact> exclude_contacts;
    new_rpcs->respond_contacts_->GetCloseContacts(target,
                                   test::k, exclude_contacts, &close_contacts);
    EXPECT_EQ(test::k, close_contacts.size());

    auto it = lcontacts.begin();
    while (it != lcontacts.end()) {
      EXPECT_NE(close_contacts.end(),
                std::find(close_contacts.begin(), close_contacts.end(), (*it)));
      ++it;
    }
  }

  SetRpc(old_rpcs);
}

/*
TEST_F(NodeImplTest, BEH_NodeImpl_ContactFunctions) {
  boost::asio::ip::address local_ip;
  ASSERT_TRUE(GetLocalAddress(&local_ip));
  NodeId key1a, key2a, key1b(NodeId::kRandomId), key2b(NodeId::kRandomId),
        target_key(NodeId::kRandomId);
  ContactAndTargetKey catk1, catk2;
  catk1.contact = Contact(key1a, local_ip.to_string(), 5001,
                          local_ip.to_string(), 5001);
  catk2.contact = Contact(key2a, local_ip.to_string(), 5002,
                          local_ip.to_string(), 5002);
  catk1.target_key = catk2.target_key = target_key;
  ASSERT_TRUE(CompareContact(catk1, catk2));
  catk1.contact = Contact(key1b, local_ip.to_string(), 5001,
                          local_ip.to_string(), 5001);
  ASSERT_FALSE(CompareContact(catk1, catk2));

  std::list<LookupContact> contact_list;
  SortLookupContact(target_key, &contact_list);
}

TEST_F(NodeImplTest, BEH_NodeImpl_Uninitialised_Values) {
  DeleteValueCallback dvc;
  SignedValue signed_value, new_value;
  SignedRequest request_signature;
  node_->DeleteValue(NodeId(NodeId::kRandomId), signed_value, request_signature,
                     boost::bind(&DeleteValueCallback::CallbackFunc, &dvc, _1));
  while (!dvc.result())
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
  ASSERT_FALSE(dvc.result());

  UpdateValueCallback uvc;
  node_->UpdateValue(NodeId(NodeId::kRandomId), signed_value, new_value,
                     request_signature, 60 * 60 * 24,
                     boost::bind(&UpdateValueCallback::CallbackFunc, &uvc, _1));
  while (!uvc.result())
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
  ASSERT_FALSE(uvc.result());
}

TEST_F(NodeImplTest, BEH_NodeImpl_ExecuteRPCs) {
  node_->is_joined_ = false;
  SignedValue old_value, new_value;
  SignedRequest sig_req;
  UpdateValueCallback uvc;
  node_->ExecuteUpdateRPCs("summat that doesn't parse", NodeId(NodeId::kRandomId),
                           old_value, new_value, sig_req, 3600 * 24,
                           boost::bind(&UpdateValueCallback::CallbackFunc,
                                       &uvc, _1));
  ASSERT_FALSE(uvc.result());

  DeleteValueCallback dvc;
  node_->DelValue_ExecuteDeleteRPCs("summat that doesn't parse",
                                    NodeId(NodeId::kRandomId),
                                    old_value,
                                    sig_req,
                                    boost::bind(
                                        &DeleteValueCallback::CallbackFunc,
                                        &dvc, _1));
  ASSERT_FALSE(dvc.result());

  dvc.Reset();
  std::vector<Contact> close_nodes;
  NodeId key(NodeId::kRandomId);
  SignedValue svalue;
  SignedRequest sreq;
  boost::shared_ptr<IterativeDelValueData> data(
      new struct IterativeDelValueData(close_nodes, key, svalue, sreq,
                                       boost::bind(
                                          &DeleteValueCallback::CallbackFunc,
                                          &dvc, _1)));
  data->is_callbacked = true;
  DeleteCallbackArgs callback_data(data);
  node_->DelValue_IterativeDeleteValue(NULL, callback_data);
  ASSERT_FALSE(dvc.result());

  node_->is_joined_ = true;
  uvc.Reset();
  FindResponse fr;
  fr.set_result(true);
  std::string ser_fr, ser_c;
  Contact c(NodeId(NodeId::kRandomId), "127.0.0.1", 1234, "127.0.0.2", 1235,
            "127.0.0.3", 1236);
  c.SerialiseToString(&ser_c);
  int count = kMinSuccessfulPecentageStore * K - 1;
  for (int n = 0; n < count; ++n)
    fr.add_closest_nodes(ser_c);

  node_->ExecuteUpdateRPCs(fr.SerializeAsString(), NodeId(NodeId::kRandomId),
                           old_value, new_value, sig_req, 3600 * 24,
                           boost::bind(&UpdateValueCallback::CallbackFunc,
                                       &uvc, _1));
  while (!uvc.result())
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
  ASSERT_FALSE(uvc.result());

  fr.set_result(false);
  uvc.Reset();
  node_->ExecuteUpdateRPCs(fr.SerializeAsString(), NodeId(NodeId::kRandomId),
                           old_value, new_value, sig_req, 3600 * 24,
                           boost::bind(&UpdateValueCallback::CallbackFunc,
                                       &uvc, _1));
  while (!uvc.result())
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
  ASSERT_FALSE(uvc.result());

  dvc.Reset();
  node_->DelValue_IterativeDeleteValue(NULL, callback_data);
  ASSERT_FALSE(dvc.result());

  dvc.Reset();
  node_->DelValue_ExecuteDeleteRPCs("summat that doesn't parse",
                                    NodeId(NodeId::kRandomId),
                                    old_value,
                                    sig_req,
                                    boost::bind(
                                        &DeleteValueCallback::CallbackFunc,
                                        &dvc, _1));
  while (!dvc.result())
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
  ASSERT_FALSE(dvc.result());

  dvc.Reset();
  fr.Clear();
  fr.set_result(true);
  node_->DelValue_ExecuteDeleteRPCs(fr.SerializeAsString(),
                                    NodeId(NodeId::kRandomId),
                                    old_value,
                                    sig_req,
                                    boost::bind(
                                        &DeleteValueCallback::CallbackFunc,
                                        &dvc, _1));
  while (!dvc.result())
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
  ASSERT_FALSE(dvc.result());
}

TEST_F(NodeImplTest, BEH_NodeImpl_NotJoined) {
  node_->is_joined_ = false;
  node_->RefreshRoutine();

  StoreValueCallback svc;
  boost::shared_ptr<IterativeStoreValueData> isvd(
      new IterativeStoreValueData(std::vector<Contact>(), NodeId(), "",
                                  boost::bind(&StoreValueCallback::CallbackFunc,
                                              &svc, _1),
                                  true, 3600 * 24, SignedValue(),
                                  SignedRequest()));

  ASSERT_FALSE(svc.result());
  node_->is_joined_ = true;
}
*/

// TEST_F(NodeImplTest, BEH_NodeImpl_AddContactsToContainer) {
//   bool done(false);
//   std::vector<Contact> contacts;
//   std::list<Contact> lcontacts;
//   boost::shared_ptr<FindNodesArgs> fna(
//       new FindNodesArgs(NodeId(NodeId::kRandomId),
//                         boost::bind(&IterativeSearchCallback, _1, &done,
//                                     &lcontacts)));
//   ASSERT_TRUE(fna->nc.empty());
//   node_->AddContactsToContainer(contacts, fna);
//   ASSERT_TRUE(fna->nc.empty());
// 
//   boost::uint16_t k(K);
//   std::string ip("123.234.231.134");
//   for (boost::uint16_t n = 0; n < k; ++n) {
//     transport::Endpoint ep(ip, n);
//     Contact c(NodeId(NodeId::kRandomId).String(), ep);
//     contacts.push_back(c);
//   }
//   node_->AddContactsToContainer(contacts, fna);
//   ASSERT_EQ(K, fna->nc.size());
//   node_->AddContactsToContainer(contacts, fna);
//   ASSERT_EQ(K, fna->nc.size());
// }

/*
TEST_F(NodeImplTest, BEH_NodeImpl_GetAlphas) {
  bool done(false), calledback(false);
  std::list<Contact> lcontacts;
  NodeId key(NodeId::kRandomId);
  boost::shared_ptr<FindNodesArgs> fna(
      new FindNodesArgs(key, boost::bind(&GenericCallback, _1, &done)));
  ASSERT_TRUE(fna->nc.empty());
  boost::uint16_t a(3);
  ASSERT_TRUE(node_->GetAlphas(a, fna, &lcontacts, &calledback));
  ASSERT_TRUE(fna->nc.empty());
  ASSERT_EQ(boost::uint16_t(0), fna->round);
  ASSERT_TRUE(lcontacts.empty());

  int k(K);
  std::string ip("123.234.231.134");
  std::vector<Contact> vcontacts;
  for (int n = 0; n < k; ++n) {
    Contact c(NodeId(NodeId::kRandomId), ip, n);
    vcontacts.push_back(c);
  }
  node_->AddContactsToContainer(vcontacts, fna);
  ASSERT_EQ(K, fna->nc.size());

  std::list<Contact> inputs(vcontacts.begin(), vcontacts.end());
  SortContactList(key, &inputs);
  std::list<Contact>::iterator it(inputs.begin()), it2;
  int quotient(K/a), remainder(K%a), b(a);
  for (int n = 0; n <= quotient; ++n) {
    if (n == quotient)
      b = remainder;
    ASSERT_FALSE(node_->GetAlphas(a, fna, &lcontacts, &calledback));
    ASSERT_EQ(boost::uint16_t(n + 1), fna->round);
    ASSERT_EQ(size_t(b), lcontacts.size());
    for (it2 = lcontacts.begin(); it2 != lcontacts.end();  ++it2, ++it) {
      ASSERT_TRUE(*it == *it2);
    }
    lcontacts.clear();
  }
  ASSERT_TRUE(node_->GetAlphas(a, fna, &lcontacts, &calledback));
  ASSERT_EQ(boost::uint16_t(quotient + 1), fna->round);
}

TEST_F(NodeImplTest, BEH_NodeImpl_MarkNode) {
  bool done(false), calledback(false);
  std::list<Contact> lcontacts;
  NodeId key(NodeId::kRandomId);
  boost::shared_ptr<FindNodesArgs> fna(
      new FindNodesArgs(key, boost::bind(&GenericCallback, _1, &done)));
  ASSERT_TRUE(fna->nc.empty());
  boost::uint16_t a(3);
  ASSERT_TRUE(node_->GetAlphas(a, fna, &lcontacts, &calledback));
  ASSERT_TRUE(fna->nc.empty());
  ASSERT_EQ(boost::uint16_t(0), fna->round);
  ASSERT_TRUE(lcontacts.empty());

  int k(K);
  std::string ip("123.234.231.134");
  std::vector<Contact> vcontacts;
  for (int n = 0; n < k; ++n) {
    Contact c(NodeId(NodeId::kRandomId), ip, n);
    vcontacts.push_back(c);
  }
  node_->AddContactsToContainer(vcontacts, fna);
  ASSERT_EQ(K, fna->nc.size());

  std::list<Contact> inputs(vcontacts.begin(), vcontacts.end());
  SortContactList(key, &inputs);
  std::list<Contact>::iterator it(inputs.begin()), it2;
  ASSERT_TRUE(node_->MarkNode(*it, fna, kSearchContacted));
  ASSERT_FALSE(node_->GetAlphas(a, fna, &lcontacts, &calledback));
  ASSERT_EQ(boost::uint16_t(1), fna->round);
  ASSERT_EQ(size_t(a), lcontacts.size());
  for (it2 = lcontacts.begin(); it2 != lcontacts.end();  ++it2, ++it) {
    ASSERT_FALSE(*it == *it2);
  }

  ++it;
  ASSERT_TRUE(node_->MarkNode(*it, fna, kSearchDown));
  ASSERT_FALSE(node_->GetAlphas(a, fna, &lcontacts, &calledback));
  ASSERT_EQ(boost::uint16_t(2), fna->round);
  ASSERT_EQ(size_t(a), lcontacts.size());
  for (it2 = lcontacts.begin(); it2 != lcontacts.end();  ++it2, ++it) {
    ASSERT_FALSE(*it == *it2);
  }

  Contact not_in_list(NodeId(NodeId::kRandomId), ip, 8000);
  ASSERT_FALSE(node_->MarkNode(not_in_list, fna, kSearchContacted));
}

TEST_F(NodeImplTest, BEH_NodeImpl_BetaDone) {
  bool done(false), calledback(false);
  std::list<Contact> lcontacts;
  NodeId key(NodeId::kRandomId);
  boost::shared_ptr<FindNodesArgs> fna(
      new FindNodesArgs(key, boost::bind(&GenericCallback, _1, &done)));
  ASSERT_TRUE(fna->nc.empty());
  boost::uint16_t a(3);
  ASSERT_TRUE(node_->GetAlphas(a, fna, &lcontacts, &calledback));
  ASSERT_TRUE(fna->nc.empty());
  ASSERT_EQ(boost::uint16_t(0), fna->round);
  ASSERT_TRUE(lcontacts.empty());

  int k(K);
  std::string ip("123.234.231.134");
  std::vector<Contact> vcontacts;
  for (int n = 0; n < k; ++n) {
    Contact c(NodeId(NodeId::kRandomId), ip, n);
    vcontacts.push_back(c);
  }
  node_->AddContactsToContainer(vcontacts, fna);
  ASSERT_EQ(K, fna->nc.size());

  std::list<Contact> inputs(vcontacts.begin(), vcontacts.end());
  std::vector<std::list<Contact> > valphas;
  SortContactList(key, &inputs);
  std::list<Contact>::iterator it(inputs.begin()), it2;
  int quotient(K/a), remainder(K%a), b(a);
  for (int n = 0; n <= quotient; ++n) {
    if (n == quotient)
      b = remainder;
    ASSERT_FALSE(node_->GetAlphas(a, fna, &lcontacts, &calledback));
    ASSERT_EQ(boost::uint16_t(n + 1), fna->round);
    ASSERT_EQ(size_t(b), lcontacts.size());
    for (it2 = lcontacts.begin(); it2 != lcontacts.end();  ++it2, ++it) {
      ASSERT_TRUE(*it == *it2);
    }
    valphas.push_back(lcontacts);
    lcontacts.clear();
  }
  ASSERT_TRUE(node_->GetAlphas(a, fna, &lcontacts, &calledback));
  ASSERT_EQ(boost::uint16_t(quotient + 1), fna->round);

  for (size_t a = 0; a < valphas.size(); ++a) {
    ASSERT_TRUE(node_->MarkNode(valphas[a].front(), fna, kSearchContacted));
    if (a == valphas.size() - 1)
      ASSERT_TRUE(node_->BetaDone(fna, a + 1));
    else
      ASSERT_FALSE(node_->BetaDone(fna, a + 1)) << a;
    valphas[a].pop_front();
  }

  for (size_t a = 0; a < valphas.size() - 1; ++a) {
    ASSERT_TRUE(node_->MarkNode(valphas[a].front(), fna, kSearchContacted));
    ASSERT_TRUE(node_->BetaDone(fna, a));
  }
}

class MockIterativeSearchResponse : public NodeImpl {
 public:
  MockIterativeSearchResponse(
      boost::shared_ptr<rpcprotocol::ChannelManager> channel_manager,
      boost::shared_ptr<transport::UdtTransport> udt_transport,
      const NodeConstructionParameters &node_parameters)
          : NodeImpl(channel_manager, udt_transport, node_parameters) {}
  MOCK_METHOD2(IterativeSearch, void(const boost::uint16_t &count,
                                     boost::shared_ptr<FindNodesArgs> fna));
  void IterativeSearchDummy(boost::shared_ptr<FindNodesArgs> fna) {
    FindResponse fr;
    fr.set_result(kRpcResultSuccess);
    boost::thread th(fna->callback, fr.SerializeAsString());
  }
};

TEST_F(NodeImplTest, BEH_NodeImpl_IterativeSearchResponse) {
  bool done(false), calledback(false);
  NodeId key(NodeId::kRandomId);
  boost::shared_ptr<FindNodesArgs> fna(
      new FindNodesArgs(key, boost::bind(&GenericCallback, _1, &done)));
  boost::shared_ptr<transport::UdtTransport> udt(new transport::UdtTransport);
  boost::shared_ptr<rpcprotocol::ChannelManager> cm(
      new rpcprotocol::ChannelManager(udt));
  NodeConstructionParameters kcp;
  CreateParameters(&kcp);
  MockIterativeSearchResponse misr(cm, udt, kcp);

  EXPECT_CALL(misr, IterativeSearch(kAlpha, fna))
      .WillOnce(testing::WithArgs<1>(testing::Invoke(
          boost::bind(&MockIterativeSearchResponse::IterativeSearchDummy,
                      &misr, _1))));

  int k(K);
  std::string ip("123.234.231.134");
  std::vector<Contact> vcontacts, popped;
  for (int n = 0; n < k; ++n) {
    Contact c(NodeId(NodeId::kRandomId), ip, n);
    vcontacts.push_back(c);
  }
  misr.AddContactsToContainer(vcontacts, fna);
  ASSERT_EQ(K, fna->nc.size());
  std::list<Contact> lcontacts;
  ASSERT_FALSE(misr.GetAlphas(kcp.alpha, fna, &lcontacts, &calledback));

  boost::shared_ptr<FindNodesRpcArgs> rpc(new FindNodesRpcArgs(lcontacts.front(), fna));
  rpc->response->set_result(kRpcResultSuccess);
  misr.IterativeSearchResponse(rpc);

  popped.push_back(lcontacts.front());
  lcontacts.pop_front();
  popped.push_back(lcontacts.front());
  boost::shared_ptr<FindNodesRpcArgs> rpc1(new FindNodesRpcArgs(lcontacts.front(),
                                                        fna));
  rpc1->response->set_result(kRpcResultSuccess);
  misr.IterativeSearchResponse(rpc1);
  while (!done)
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

  // Check fna to see if the two contacts changed state correctly
  for (size_t n = 0; n < popped.size(); ++n) {
    NodeContainerByContact &index_contact = fna->nc.get<nc_contact>();
    NodeContainerByContact::iterator it = index_contact.find(popped[n]);
    ASSERT_FALSE(it == index_contact.end());
    ASSERT_TRUE((*it).alpha && (*it).contacted && !(*it).down);
    ASSERT_EQ(rpc1->round, (*it).round);
  }
}
*/
/*
//template <class T>
//class MockRpcs : public Rpcs<T> {
// public:
//  MockRpcs(boost::shared_ptr<boost::asio::io_service> asio_service)
//      : Rpcs<T>(asio_service),
//        node_list_mutex_(),
//        node_list_(),
//        backup_node_list_() {}
//  MOCK_METHOD3(FindNodes, void(const NodeId &key,
//                               const Contact &contact,
//                               FindNodesFunctor callback));
//  void FindNodeDummy(kademlia::protobuf::FindNodesResponse *resp,
//                     google::protobuf::Closure *callback) {
//    resp->set_result(true);
//    {
//      boost::mutex::scoped_lock loch_queldomage(node_list_mutex_);
//      if (!node_list_.empty()) {
//        int summat(K/4 + 1);
//        int size = node_list_.size();
//        int elements = RandomUint32() % summat % size;
//        for (int n = 0; n < elements; ++n) {
//          protobuf::Contact *c = resp->add_closest_nodes();
//          *c = node_list_.front().ToProtobuf();
//          node_list_.pop_front();
//        }
//      }
//    }
//    boost::thread th(boost::bind(&MockRpcs::FunctionForThread, this,
//                                 callback));
//  }
//  void FunctionForThread(google::protobuf::Closure *callback) {
//    boost::this_thread::sleep(
//        boost::posix_time::milliseconds(100 * (RandomUint32() % 5 + 1)));
//    callback->Run();
//  }
//  bool AllAlphasBack(boost::shared_ptr<FindNodesArgs> fna) {
//    boost::mutex::scoped_lock loch_surlaplage(fna->mutex);
//    NodeContainerByState &index_state = fna->nc.get<nc_state>();
//    std::pair<NCBSit, NCBSit> pa = index_state.equal_range(kSelectedAlpha);
//    return pa.first == pa.second;
//  }
//  bool GenerateContacts(const boost::uint16_t &total) {
//    if (total > 100 || total < 1)
//      return false;
//    boost::mutex::scoped_lock loch_queldomage(node_list_mutex_);
//    std::string ip("123.234.134.1");
//    for (boost::uint16_t n = 0; n < total; ++n) {
//      transport::Endpoint ep = {ip, n};
//      Contact c(NodeId(NodeId::kRandomId).String(), ep);
//      node_list_.push_back(c);
//    }
//    backup_node_list_ = node_list_;
//    return true;
//  }
//  std::list<Contact> node_list() {
//    boost::mutex::scoped_lock loch_queldomage(node_list_mutex_);
//    return node_list_;
//  }
//  std::list<Contact> backup_node_list() { return backup_node_list_; }
//
// private:
//  boost::mutex node_list_mutex_;
//  std::list<Contact> node_list_, backup_node_list_;
//};
//
//TEST_F(NodeImplTest, BEH_NodeImpl_IterativeSearchHappy) {
//  bool done(false);
//  NodeId key(NodeId::kRandomId);
//  std::list<Contact> lcontacts;
//  boost::shared_ptr<FindNodesArgs> fna(
//      new FindNodesArgs(key,
//                        boost::bind(&IterativeSearchCallback, "", &done, _1)));
//  boost::uint16_t k(K);
//  std::string ip("123.234.231.134");
//  std::vector<Contact> vcontacts, popped;
//  for (boost::uint16_t n = 0; n < k; ++n) {
//    transport::Endpoint ep = {ip, n};
//    Contact c(NodeId(NodeId::kRandomId).String(), ep);
//    vcontacts.push_back(c);
//  }
//  node_->AddContactsToContainer(vcontacts, fna);
//  ASSERT_EQ(K, fna->nc.size());
//
//  boost::shared_ptr<Rpcs<transport::UdtTransport> > old_rpcs = node_->rpcs_;
//  boost::shared_ptr<MockRpcs<transport::UdtTransport> >
//      new_rpcs(new MockRpcs<transport::UdtTransport>(node_->asio_service_));
//  node_->rpcs_ = new_rpcs;
//
////  EXPECT_CALL(*new_rpcs, FindNodes(testing::_, testing::_, testing::_))
////      .Times(K)
////      .WillRepeatedly(testing::WithArgs<0, 2>(testing::Invoke(
////          boost::bind(&MockRpcs::FindNodeDummy, new_rpcs.get(), _1, _2))));
//
//  NodeContainer::iterator node_it = fna->nc.begin();
//  std::list<Contact> alphas;
//  boost::uint16_t a(0);
//  for (; node_it != fna->nc.end() && a < kAlpha; ++node_it, ++a) {
//    alphas.push_back((*node_it).contact);
//  }
//  SortContactList(fna->key, &alphas);
//  node_->IterativeSearch(fna, false, false, &alphas, 0);
//  while (!done || !new_rpcs->AllAlphasBack(fna))
//    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
//
//  std::set<Contact> lset(lcontacts.begin(), lcontacts.end()),
//                    vset(vcontacts.begin(), vcontacts.end());
//  ASSERT_TRUE(lset == vset);
//  node_->rpcs_ = old_rpcs;
//}
//
//TEST_F(NodeImplTest, BEH_NodeImpl_FindNodesHappy) {
//  bool done(false);
//  std::list<Contact> lcontacts;
//  node_->routing_table_->Clear();
//  std::string ip("156.148.126.159");
//  std::vector<Contact> vcontacts;
//  for (boost::uint16_t n = 0; n < K; ++n) {
//    transport::Endpoint ep = {ip, n};
//    Contact c(NodeId(NodeId::kRandomId).String(), ep);
//    EXPECT_EQ(0, node_->AddContact(c, 1, false));
//    vcontacts.push_back(c);
//  }
//
//  boost::shared_ptr<Rpcs<transport::UdtTransport> > old_rpcs = node_->rpcs_;
//  boost::shared_ptr<MockRpcs<transport::UdtTransport> >
//      new_rpcs(new MockRpcs<transport::UdtTransport>(node_->asio_service_));
//  node_->rpcs_ = new_rpcs;
//
////  EXPECT_CALL(*new_rpcs, FindNode(testing::_, testing::_, testing::_))
////      .WillRepeatedly(testing::WithArgs<5, 7>(testing::Invoke(
////          boost::bind(&MockRpcs::FindNodeDummy, new_rpcs.get(), _1, _2))));
//
//  FindNodesParams fnp1;
//  fnp1.key = NodeId(NodeId::kRandomId);
//  fnp1.callback = boost::bind(&IterativeSearchCallback, "", &done, _1);
//  node_->FindNodes(fnp1);
//  while (!done)
//    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
//
//  std::set<Contact> lset(lcontacts.begin(), lcontacts.end()),
//                    vset(vcontacts.begin(), vcontacts.end());
//  ASSERT_TRUE(lset == vset);
//
//  lcontacts.clear();
//  done = false;
//  FindNodesParams fnp2;
//  fnp2.key = NodeId(NodeId::kRandomId);
//  fnp2.callback = boost::bind(&IterativeSearchCallback, "", &done, _1);
//  std::list<Contact> winners, backup;
//  ip = std::string("156.148.126.160");
//  for (boost::uint16_t a = 0; a < K; ++a) {
//    transport::Endpoint ep = {ip, a};
//    Contact c(NodeId(NodeId::kRandomId).String(), ep);
//    fnp2.start_nodes.push_back(c);
//    winners.push_back(c);
//    winners.push_back(vcontacts[a]);
//  }
//
//  node_->FindNodes(fnp2);
//  SortContactList(fnp2.key, &winners);
//  backup = winners;
//  winners.resize(K);
//  while (!done)
//    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
//
//  vset = std::set<Contact>(winners.begin(), winners.end());
//  lset = std::set<Contact>(lcontacts.begin(), lcontacts.end());
//  ASSERT_EQ(vset.size(), lset.size());
//  ASSERT_TRUE(lset == vset);
//
//  lcontacts.clear();
//  done = false;
//  FindNodesParams fnp3;
//  fnp3.key = NodeId(NodeId::kRandomId);
//  fnp3.callback = boost::bind(&IterativeSearchCallback, "", &done, _1);
//  fnp3.start_nodes = fnp2.start_nodes;
//  int top(K/4 + 1);
//  for (int y = 0; y < top; ++y)
//    fnp3.exclude_nodes.push_back(vcontacts[y]);
//
//  node_->FindNodes(fnp3);
//  while (!done)
//    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
//
//  lset = std::set<Contact>(lcontacts.begin(), lcontacts.end());
//  std::set<Contact> back(backup.begin(), backup.end());
//  std::set<Contact>::iterator it;
//  for (size_t e = 0; e < fnp3.exclude_nodes.size(); ++e) {
//    it = lset.find(fnp3.exclude_nodes[e]);
//    ASSERT_TRUE(it == lset.end());
//    it = back.find(fnp3.exclude_nodes[e]);
//    ASSERT_TRUE(it != back.end());
//    back.erase(it);
//  }
//
//  backup = std::list<Contact>(back.begin(), back.end());
//  SortContactList(fnp3.key, &backup);
//  backup.resize(K);
//  back = std::set<Contact>(backup.begin(), backup.end());
//  ASSERT_EQ(lset.size(), back.size());
//  ASSERT_TRUE(lset == back);
//
//  node_->rpcs_ = old_rpcs;
//}
//
//TEST_F(NodeImplTest, BEH_NodeImpl_FindNodesContactsInReponse) {
//  bool done(false);
//  std::list<Contact> lcontacts;
//  node_->routing_table_->Clear();
//  std::string ip("156.148.126.159");
//  std::vector<Contact> vcontacts;
//  for (boost::uint16_t n = 0; n < K; ++n) {
//    transport::Endpoint ep = {ip, n};
//    Contact c(NodeId(NodeId::kRandomId).String(), ep);
//    EXPECT_EQ(0, node_->AddContact(c, 1, false));
//    vcontacts.push_back(c);
//  }
//
//  boost::shared_ptr<Rpcs<transport::UdtTransport> > old_rpcs = node_->rpcs_;
//  boost::shared_ptr<MockRpcs<transport::UdtTransport> >
//      new_rpcs(new MockRpcs<transport::UdtTransport>(node_->asio_service_));
//  node_->rpcs_ = new_rpcs;
//  ASSERT_TRUE(new_rpcs->GenerateContacts(100));
//
////  EXPECT_CALL(*new_rpcs, FindNode(testing::_, testing::_, testing::_))
////      .WillRepeatedly(testing::WithArgs<5, 7>(testing::Invoke(
////          boost::bind(&MockRpcs::FindNodeDummy, new_rpcs.get(), _1, _2))));
//
//  FindNodesParams fnp1;
//  fnp1.key = NodeId(NodeId::kRandomId);
//  fnp1.callback = boost::bind(&IterativeSearchCallback, "", &done, _1);
//  node_->FindNodes(fnp1);
//  while (!done)
//    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
//
//  std::list<Contact> bcontacts = new_rpcs->backup_node_list();
//  bcontacts.insert(bcontacts.end(), vcontacts.begin(), vcontacts.end());
//  std::set<Contact> sss(bcontacts.begin(), bcontacts.end());
//  std::list<Contact> ncontacts = new_rpcs->node_list();
//  std::set<Contact>::iterator it;
//  while (!ncontacts.empty()) {
//    it = sss.find(ncontacts.front());
//    if (it != sss.end())
//      sss.erase(it);
//    ncontacts.pop_front();
//  }
//  bcontacts = std::list<Contact>(sss.begin(), sss.end());
//  SortContactList(fnp1.key, &bcontacts);
//  bcontacts.resize(K);
//  sss = std::set<Contact>(bcontacts.begin(), bcontacts.end());
//
//  std::set<Contact> lset(lcontacts.begin(), lcontacts.end());
//  ASSERT_EQ(lset.size(), sss.size());
//  ASSERT_TRUE(lset == sss);
//
//  node_->rpcs_ = old_rpcs;
//}
*/
/*
TEST_F(NodeImplTest, BEH_NodeImpl_IterativeSearchHappy) {
  bool done(false);
  NodeId key(NodeId::kRandomId);
  std::list<Contact> lcontacts;
  boost::shared_ptr<FindNodesArgs> fna(
      new FindNodesArgs(key,
                        boost::bind(&IterativeSearchCallback, _1, &done,
                                    &lcontacts)));
  boost::uint16_t k(K);
  std::string ip("123.234.231.134");
  std::vector<Contact> vcontacts, popped;
  transport::Endpoint ep;
  for (boost::uint16_t n = 0; n < k; ++n) {
    ep.ip = boost::asio::ip::address::from_string(ip);
    ep.port = 5000 + n;
    NodeId ni(NodeId::kRandomId);
    Contact c(ni.String(), ep);
    printf("%s\n", ni.ToStringEncoded(NodeId::kBase64).c_str());
    vcontacts.push_back(c);
  }
  node_->AddContactsToContainer(vcontacts, fna);
  ASSERT_EQ(K, fna->nc.size());

  boost::shared_ptr<Rpcs> old_rpcs = node_->rpcs_;
  boost::shared_ptr<MockRpcs> new_rpcs(new MockRpcs(node_->asio_service_));
  node_->rpcs_ = new_rpcs;

  EXPECT_CALL(*new_rpcs, FindNodes(testing::_, testing::_, testing::_,
                                   testing::_))
      .Times(K)
      .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
          boost::bind(&MockRpcs::FindNodeDummy, new_rpcs.get(), _1, _2))));

  // Need to pick the initial alpha contacts
  std::list<Contact> alphas(vcontacts.begin(), vcontacts.end());
  SortContactList(fna->key, &alphas);
  alphas.resize(kAlpha);
  node_->MarkAsAlpha(alphas, fna);

  node_->IterativeSearch(fna, false, false, &alphas);
  while (!done || !new_rpcs->AllAlphasBack(fna))
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

  std::set<Contact> lset(lcontacts.begin(), lcontacts.end()),
                    vset(vcontacts.begin(), vcontacts.end());
  ASSERT_TRUE(lset == vset);
  node_->rpcs_ = old_rpcs;
}
*/
/*
TEST_F(NodeImplTest, BEH_KAD_FindNodes) {
  bool done(false);
  std::list<Contact> lcontacts;
  node_->routing_table_->Clear();
  std::string ip("156.148.126.159");
  std::vector<Contact> vcontacts;
  transport::Endpoint ep;
  for (boost::uint16_t n = 0; n < K; ++n) {
    ep.ip = boost::asio::ip::address::from_string(ip);
    ep.port = 5000 + n;
    Contact c(NodeId(NodeId::kRandomId).String(), ep);
    EXPECT_EQ(0, node_->AddContact(c, 1, false));
    vcontacts.push_back(c);
  }

  boost::shared_ptr<Rpcs> old_rpcs = node_->rpcs_;
  boost::shared_ptr<MockRpcs> new_rpcs(new MockRpcs(node_->asio_service_));
  node_->rpcs_ = new_rpcs;
  ASSERT_TRUE(new_rpcs->GenerateContacts(100));

  EXPECT_CALL(*new_rpcs, FindNodes(testing::_, testing::_, testing::_,
                                   testing::_))
      .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
          boost::bind(&MockRpcs::FindNodeDummy, new_rpcs.get(), _1, _2))));

  FindNodesParams fnp1;
  fnp1.key = NodeId(NodeId::kRandomId);
  fnp1.callback = boost::bind(&IterativeSearchCallback, _1, &done, &lcontacts);
  node_->FindNodes(fnp1);
  while (!done)
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

  std::list<Contact> bcontacts = new_rpcs->backup_node_list();
  bcontacts.insert(bcontacts.end(), vcontacts.begin(), vcontacts.end());
  std::set<Contact> sss(bcontacts.begin(), bcontacts.end());
  std::list<Contact> ncontacts = new_rpcs->node_list();
  std::set<Contact>::iterator it;
  while (!ncontacts.empty()) {
    it = sss.find(ncontacts.front());
    if (it != sss.end())
      sss.erase(it);
    ncontacts.pop_front();
  }
  bcontacts = std::list<Contact>(sss.begin(), sss.end());
  SortContactList(fnp1.key, &bcontacts);
  bcontacts.resize(K);
  sss = std::set<Contact>(bcontacts.begin(), bcontacts.end());

  std::set<Contact> lset(lcontacts.begin(), lcontacts.end());
  ASSERT_EQ(lset.size(), sss.size());
  ASSERT_TRUE(lset == sss);

  node_->rpcs_ = old_rpcs;
}
*/
}  // namespace test_nodeimpl

}  // namespace kademlia

}  // namespace maidsafe
