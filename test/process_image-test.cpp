#include "../src/core.h"
#include "load-bitmap-fixture.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

using namespace std;
using testing::ElementsAre;
using testing::ElementsAreArray;

TEST(process_image, decodes_640x384_landscape_1bpp) {
  EXPECT_THAT(
      process_image(parse_message(
          "{\"type\":\"message\",\"data\":{\"action\":\"refresh\","
          "\"image\":\"./fixtures/640x384_landscape_1bpp_in.png\"}}")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/640x384_landscape_1bpp_out.bmp", COLOR_MODE_1BPP)));
}

TEST(process_image, should_throw_if_file_not_found) {
  EXPECT_THROW(process_image(parse_message(
                   "{\"type\":\"message\",\"data\":{\"action\":\"refresh\","
                   "\"image\":\"./fixtures/does_not_exist.png\"}}")),
               ImageFileNotFound);
}
