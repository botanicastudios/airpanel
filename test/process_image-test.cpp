#include "../src/core.h"
#include "../src/exceptions.h"
#include "load-bitmap-fixture.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <vector>

using namespace std;
using testing::ElementsAre;
using testing::ElementsAreArray;

std::vector<unsigned char> result_640x384_landscape_1bpp = process_message(
    parse_message("{\"type\":\"message\",\"data\":{\"action\":\"refresh\","
                  "\"image\":\"./fixtures/640x384_landscape_1bpp_in.png\"}}"),
    1, 1);

vector<unsigned char> fixture_640x384_landscape_1bpp = read_bmp_into_byte_array(
    "./fixtures/640x384_landscape_1bpp_out.bmp", COLOR_MODE_1BPP);

TEST(process_message, decodes_640x384_landscape_1bpp) {
  EXPECT_THAT(result_640x384_landscape_1bpp,
              ElementsAreArray(fixture_640x384_landscape_1bpp));
}

TEST(process_message, should_throw_if_file_not_found) {
  EXPECT_THROW(
      std::vector<unsigned char> result_image_file_not_found = process_message(
          parse_message(
              "{\"type\":\"message\",\"data\":{\"action\":\"refresh\","
              "\"image\":\"./fixtures/does_not_exist.png\"}}"),
          1, 1),
      ImageFileNotFound);
}
