#include "gtest/gtest.h"

int main(int argc, char **argv) {
  printf("argv[0] = %s\n", getenv(argv[0]));
  printf("_ = %s\n", getenv("_"));
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
