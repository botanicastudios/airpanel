#include "../src/exceptions.h"
#include "load-bitmap-fixture.h"
#include <stdlib.h>
#include <string>
#include <vector>
using namespace std;

std::vector<unsigned char>
read_bmp_into_byte_array(const char *filename,
                         const int output_bits_per_pixel) {

  FILE *f;
  if ((f = fopen(filename, "rb")) == NULL) {
    throw ImageFileNotFound(filename);
  };

  unsigned char info[54];
  fread(info, sizeof(unsigned char), 54, f);

  int width = *(int *)&info[18];
  int height = *(int *)&info[22];

  int size = 3 * width * height;
  unsigned char *data = new unsigned char[size];
  fread(data, sizeof(unsigned char), size, f);
  fclose(f);

  // If we're outputting 1bpp, we'll represent 8 pixels per byte
  int bytes_per_row =
      output_bits_per_pixel == COLOR_MODE_1BPP ? width / 8 : width;

  std::vector<unsigned char> frame_buffer(height * bytes_per_row);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < bytes_per_row; x++) {

      // We're only working with grayscale 24bpp bitmaps, so we'll just sample
      // the blue value of the pixel We're only supporting images with a width
      // divisible by 4, so no padding needed

      unsigned int current_byte = 0;

      for (int bit = 0; bit < 8; bit++) {

        if (data[((height - 1 - y) /* starting from bottom left for BMP! */ *
                  width * 3 /* bytes per pixel */) +
                 (x * 8 + bit) * 3] > 127) {
          current_byte = current_byte | 1 << (7 - bit);
        }
      }

      frame_buffer[(y * bytes_per_row) + x] =
          static_cast<unsigned char>(current_byte);
    }
  }

  return frame_buffer;
}
