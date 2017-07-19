#include <gtest/gtest.h>
#include "httpd.h"

using namespace std;

TEST(httpd_test, echo) {
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
