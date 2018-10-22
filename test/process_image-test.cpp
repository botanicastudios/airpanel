#include "../src/core.h"
#include "image_data.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <vector>

using namespace std;
using testing::ElementsAreArray;

std::vector<unsigned char> result_640x384_landscape_1bpp =
    process_message("{\"type\":\"message\",\"data\":{\"action\":\"refresh\","
                    "\"image\":\"./fixtures/640x384_landscape_1bpp.png\"}}",
                    1, 1);

TEST(process_message, decodes_640x384_landscape_1bpp) {
  EXPECT_THAT(result_640x384_landscape_1bpp,
              ElementsAreArray(fixture_640x384_landscape_1bpp));
}
