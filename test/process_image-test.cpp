#include "../src/core.h"
#include "load-bitmap-fixture.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

/***
 *  Note: Fixture BMP files must be saved in 24 bit (RGB) color mode, but the
 *  image itself should be grayscale (or black and white), as we only sample the
 *  blue channel to generate a grayscale image for comparison with the program
 *  output. They must be match the pixel width and height of the display, as
 *  well as the native orientation. Input PNG files can be in any color space,
 *  size and orientation (and should be!).
 ***/

using namespace std;
using testing::ElementsAre;
using testing::ElementsAreArray;

TEST(process_image, should_throw_if_file_not_found) {
  EXPECT_THROW(process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/does_not_exist.png"
                  }
                }
              )")),
               ImageFileNotFound);
}

TEST(process_image, decodes_640x384_1bpp_to_1bpp) {
  EXPECT_THAT(
      process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/640x384a_1bpp_in.png"
                  }
                }
              )")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/640x384a_1bpp_orientation_0_out.bmp", COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_640x384_1bpp_to_1bpp_orientation_180) {
  EXPECT_THAT(process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/640x384a_1bpp_in.png",
                    "orientation": 180
                  }
                }
              )")),

              ElementsAreArray(read_bmp_into_byte_array(
                  "./fixtures/640x384a_1bpp_orientation_180_out.bmp",
                  COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_200x100_8bpp_to_1bpp_orientation_0) {
  EXPECT_THAT(
      process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/200x100_8bpp_in.png",
                    "orientation": 0
                  }
                }
              )")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/200x100_1bpp_orientation_0_out.bmp", COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_200x100_8bpp_to_1bpp_orientation_90) {
  EXPECT_THAT(
      process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/200x100_8bpp_in.png",
                    "orientation": 90
                  }
                }
              )")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/200x100_1bpp_orientation_90_out.bmp", COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_200x100_8bpp_to_1bpp_orientation_180) {
  EXPECT_THAT(
      process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/200x100_8bpp_in.png",
                    "orientation": 180
                  }
                }
              )")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/200x100_1bpp_orientation_180_out.bmp", COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_200x100_8bpp_to_1bpp_orientation_270) {
  EXPECT_THAT(
      process_image(parse_message(R"(
                 {
                   "type": "message",
                   "data": {
                     "action": "refresh",
                     "image": "./fixtures/200x100_8bpp_in.png",
                     "orientation": 270
                   }
                 }
               )")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/200x100_1bpp_orientation_270_out.bmp", COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_200x100_8bpp_to_1bpp_orientation_0_custom_offset) {
  EXPECT_THAT(process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/200x100_8bpp_in.png",
                    "offset_x": 20,
                    "offset_y": 20,
                    "orientation": 0
                  }
                }
              )")),

              ElementsAreArray(read_bmp_into_byte_array(
                  "./fixtures/200x100_1bpp_orientation_0_offset_20_20_out.bmp",
                  COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_200x100_8bpp_to_1bpp_orientation_90_custom_offset) {
  EXPECT_THAT(process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/200x100_8bpp_in.png",
                    "orientation": 90,
                    "offset_x": 20,
                    "offset_y": 20
                  }
                }
              )")),

              ElementsAreArray(read_bmp_into_byte_array(
                  "./fixtures/200x100_1bpp_orientation_90_offset_20_20_out.bmp",
                  COLOR_MODE_1BPP)));
}

TEST(process_image,
     decodes_200x100_8bpp_to_1bpp_orientation_180_custom_offset) {
  EXPECT_THAT(
      process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/200x100_8bpp_in.png",
                    "orientation": 180,
                    "offset_x": 20,
                    "offset_y": 20
                  }
                }
              )")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/200x100_1bpp_orientation_180_offset_20_20_out.bmp",
          COLOR_MODE_1BPP)));
}

TEST(process_image,
     decodes_200x100_8bpp_to_1bpp_orientation_270_custom_offset) {

  EXPECT_THAT(
      process_image(parse_message(R"(
                 {
                   "type": "message",
                   "data": {
                     "action": "refresh",
                     "image": "./fixtures/200x100_8bpp_in.png",
                     "orientation": 270,
                     "offset_x": 20,
                     "offset_y": 20
                   }
                 }
               )")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/200x100_1bpp_orientation_270_offset_20_20_out.bmp",
          COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_200x100_8bpp_to_1bpp_orientation_0_zero_offset) {
  EXPECT_THAT(process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/200x100_8bpp_in.png",
                    "orientation": 0,
                    "offset_x": 0,
                    "offset_y": 0
                  }
                }
              )")),

              ElementsAreArray(read_bmp_into_byte_array(
                  "./fixtures/200x100_1bpp_orientation_0_offset_0_0_out.bmp",
                  COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_840x584_24bpp_to_1bpp_orientation_0) {
  EXPECT_THAT(
      process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/840x584_24bpp_in.png",
                    "orientation": 0
                  }
                }
              )")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/840x584_24bpp_orientation_0_out.bmp", COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_640x384_8bpp_to_1bpp_auto_orientation_landscape) {
  EXPECT_THAT(
      process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/640x384b_8bpp_in.png"
                  }
                }
              )")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/640x384b_1bpp_orientation_0_out.bmp", COLOR_MODE_1BPP)));
}

TEST(process_image, decodes_384x640_24bpp_to_1bpp_auto_orientation_portrait) {
  /*debug_write_bmp(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/384x640_24bpp_in.png"
                  }
                }
              )"));*/
  EXPECT_THAT(
      process_image(parse_message(R"(
                {
                  "type": "message",
                  "data": {
                    "action": "refresh",
                    "image": "./fixtures/384x640_24bpp_in.png"
                  }
                }
              )")),

      ElementsAreArray(read_bmp_into_byte_array(
          "./fixtures/384x640_1bpp_orientation_90_out.bmp", COLOR_MODE_1BPP)));
}
