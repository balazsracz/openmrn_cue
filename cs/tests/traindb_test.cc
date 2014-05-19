#include "utils/test_main.hxx"
#include "mobilestation/TrainDb.hxx"

namespace NMRAnet {
void PrintTo(const NMRAnet::NodeID& id, std::ostream& o) {
  o << "Node ID 0x" << StringPrintf("%012x", id);
}
}

namespace mobilestation {

TEST(TrainDbTest, getaddress) {
  TrainDb db;
  ASSERT_TRUE(db.is_train_id_known(1));
  EXPECT_EQ(0x060100000033ULL, db.get_traction_node(1));
}

TEST(TrainDbTest, isknown) {
  TrainDb db;
  EXPECT_TRUE(db.is_train_id_known(1));
  EXPECT_TRUE(db.is_train_id_known(21));
  EXPECT_FALSE(db.is_train_id_known(22));
  EXPECT_FALSE(db.is_train_id_known(150));
}

TEST(TrainDbTest, fnaddress) {
  TrainDb db;
  ASSERT_TRUE(db.is_train_id_known(1));
  EXPECT_EQ(0U, db.get_function_address(1, 2));
  EXPECT_EQ(1U, db.get_function_address(1, 3));
  EXPECT_EQ(3U, db.get_function_address(1, 4));
  EXPECT_EQ(4U, db.get_function_address(1, 5));
  EXPECT_EQ(TrainDb::UNKNOWN_FUNCTION, db.get_function_address(1, 6));
  EXPECT_EQ(TrainDb::UNKNOWN_FUNCTION, db.get_function_address(1, 17));
  EXPECT_EQ(TrainDb::UNKNOWN_FUNCTION, db.get_function_address(1, 130));
}

}  // namespace mobilestation
