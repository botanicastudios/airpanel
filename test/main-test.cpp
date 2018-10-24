#include "../src/core.h"
#include "gtest/gtest.h"

extern struct DisplayProperties DISPLAY_PROPERTIES;

int main(int argc, char **argv) {
  printf("argv[0] = %s\n", getenv(argv[0]));
  printf("_ = %s\n", getenv("_"));
  testing::InitGoogleTest(&argc, argv);
  DISPLAY_PROPERTIES.width = 640;
  DISPLAY_PROPERTIES.height = 384;
  DISPLAY_PROPERTIES.color_mode = COLOR_MODE_1BPP;
  DISPLAY_PROPERTIES.processor = BCM2835;
  return RUN_ALL_TESTS();
}
