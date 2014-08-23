#include "utils/async_if_test_helper.hxx"

#include "custom/MemorizingEventHandler.hxx"

namespace nmranet {
namespace {

static const uint64_t EVENT = 0x0501010114FE0000ULL;
static const uint64_t BYTES = 0x0501010114FF0000ULL;

class MemorizingTest : public AsyncNodeTest {
 protected:
  MemorizingTest()
      : mgrbit_(node_, EVENT, 256, 2), mgrbyte_(node_, BYTES, 0x10000, 256) {}
  ~MemorizingTest() { wait(); }

  MemorizingHandlerManager mgrbit_;
  MemorizingHandlerManager mgrbyte_;
};

TEST_F(MemorizingTest, CreateDestroy) {}

TEST_F(MemorizingTest, MemorizeAndQuery) {
  send_packet(":X195B4FFAN0501010114FE0033;");
  wait();
  send_packet_and_expect_response(":X194A4FFAN0501010114FEFFFF;",
                                  ":X195B422AN0501010114FE0033;");
}

TEST_F(MemorizingTest, MultiWrite) {
  send_packet(":X195B4FFAN0501010114FE0033;");
  send_packet(":X195B4FFAN0501010114FE0032;");
  send_packet(":X195B4FFAN0501010114FE0033;");
  send_packet(":X195B4FFAN0501010114FE0032;");
  send_packet(":X195B4FFAN0501010114FE0033;");
  send_packet(":X195B4FFAN0501010114FE0032;");
  wait();
  send_packet_and_expect_response(":X194A4FFAN0501010114FEFFFF;",
                                  ":X195B422AN0501010114FE0032;");
}

TEST_F(MemorizingTest, MultiQuery) {
  send_packet(":X195B4FFAN0501010114FE0033;");
  send_packet(":X195B4FFAN0501010114FE0035;");
  send_packet(":X195B4FFAN0501010114FE0034;");
  send_packet(":X195B4FFAN0501010114FE0024;");
  wait();
  expect_packet(":X195B422AN0501010114FE0033;");
  expect_packet(":X195B422AN0501010114FE0034;");
  expect_packet(":X195B422AN0501010114FE0024;");
  send_packet(":X194A4FFAN0501010114FEFFFF;");
}

TEST_F(MemorizingTest, ByteValueSet) {
  send_packet(":X195B4FFAN0501010114FF1321;");
  send_packet(":X195B4FFAN0501010114FF1328;");
  wait();
  send_packet_and_expect_response(":X194A4FFAN0501010114FF0000;",
                                  ":X195B422AN0501010114FF1328;");
}

}  // namespace
}  // namespace nmranet
