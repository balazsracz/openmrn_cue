#ifndef _SERVER_RPC_TEST_HELPER_HXX_
#define _SERVER_RPC_TEST_HELPER_HXX_

#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"

using ::testing::StrictMock;

namespace server {
namespace {

class MockResponsePacket : public PacketFlowInterface {
 public:
  MOCK_METHOD1(received_packet, void(const string&));

  void send(Buffer<string>* b, unsigned priority) override {
    TinyRpcResponse resp;
    resp.ParseFromString(*b->data());
    string text;
    HASSERT(::google::protobuf::TextFormat::PrintToString(resp, &text));
    received_packet(text);
    b->unref();
  }
};

string CanonicalizeProto(const string& text_pb) {
  TinyRpcResponse resp;
  HASSERT(::google::protobuf::TextFormat::ParseFromString(text_pb, &resp));
  string canon;
  HASSERT(::google::protobuf::TextFormat::PrintToString(resp, &canon));
  return canon;
}

class RpcServiceTestHelper {
 protected:
  RpcServiceTestHelper(RpcService* s) : s_(s) {
    ::pipe(down_pipe_);
    ::pipe(up_pipe_);

    send_.reset(new PacketStreamSender(s_, down_pipe_[1]));
    recv_.reset(new PacketStreamReceiver(s_, &response_handler_, up_pipe_[0]));
    s_->set_channel(down_pipe_[0], up_pipe_[1]);
  }

  ~RpcServiceTestHelper() {
    wait();
    ::close(down_pipe_[0]);
    ::close(down_pipe_[1]);
    ::close(up_pipe_[0]);
    ::close(up_pipe_[1]);
  }

  void wait() {
    do {
      usleep(1000);
      wait_for_main_executor();
    } while (!send_->is_waiting() || s_->is_busy());
  }

  void send_request(const TinyRpcRequest& r) {
    auto* b = send_->alloc();
    r.SerializeToString(b->data());
    send_->send(b);
  }

  void send_request(const string& ascii_request) {
    TinyRpcRequest req;
    ASSERT_TRUE(
        ::google::protobuf::TextFormat::ParseFromString(ascii_request, &req));
    send_request(req);
  }

  void expect_response(const string& response) {
    EXPECT_CALL(response_handler_,
                received_packet(CanonicalizeProto(response)));
  }

  void send_request_and_expect_response(const string& request,
                                        const string& response) {
    expect_response(response);
    send_request(request);
  }

 protected:
  StrictMock<MockResponsePacket> response_handler_;

 private:
  RpcService* s_;
  int down_pipe_[2];
  int up_pipe_[2];
  std::unique_ptr<PacketStreamSender> send_;
  std::unique_ptr<PacketStreamReceiver> recv_;
};

}  // namespace
}  // namespace server

#endif  // _SERVER_RPC_TEST_HELPER_HXX_
