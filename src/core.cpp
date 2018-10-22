/* -*- mode: c; c-file-style: "openbsd" -*- */
/* TODO:5002 You may want to change the copyright of all files. This is the
 * TODO:5002 ISC license. Choose another one if you want.
 */
/*
 * Copyright (c) 2018 Toby Marsden <toby@botanicastudios.io>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "core.h"

#include "cJSON.h"
#include "epd7in5.h"
#include "epdif.h"
#include <math.h>
#include <png.h>
#include <vector>

// temp
int add(int a, int b) { return a + b; }

using namespace std;

static unsigned int width, height;
static png_byte color_type;
static png_byte bit_depth;
static unsigned int bytes_per_pixel;
static png_bytep *row_pointers;

static void read_png_file(char *filename) {
  FILE *fp = fopen(filename, "rb");

  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png)
    abort();

  png_infop info = png_create_info_struct(png);
  if (!info)
    abort();

  if (setjmp(png_jmpbuf(png)))
    abort();

  png_init_io(png, fp);

  png_read_info(png, info);

  width = png_get_image_width(png, info);
  height = png_get_image_height(png, info);
  color_type = png_get_color_type(png, info);
  bit_depth = png_get_bit_depth(png, info);

  // Read any color_type into 8bit depth, RGBA format.
  // See http://www.libpng.org/pub/png/libpng-manual.txt

  if (bit_depth == 16)
    png_set_strip_16(png);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  // These color_type don't have an alpha channel then fill it with 0xff.
  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  png_read_update_info(png, info);

  bytes_per_pixel =
      static_cast<unsigned int>(png_get_rowbytes(png, info) / width);

  row_pointers = static_cast<png_bytep *>(malloc(sizeof(png_bytep) * height));
  for (unsigned int y = 0; y < height; y++) {
    row_pointers[y] =
        static_cast<png_byte *>(malloc(png_get_rowbytes(png, info)));
  }

  png_read_image(png, row_pointers);

  fclose(fp);
  png_destroy_read_struct(&png, &info, NULL);
  png = NULL;
  info = NULL;
}

/**
 * Lift the gamma curve from the pixel from the source image so we can convert
 * to grayscale
 */
inline double sRGB_to_linear(double x) {
  if (x < 0.04045)
    return x / 12.92;
  return pow((x + 0.055) / 1.055, 2.4);
}

/**
 * Apply gamma curve to the pixel of the destination image again
 */
inline double linear_to_sRGB(double y) {
  if (y <= 0.0031308)
    return 12.92 * y;
  return 1.055 * pow(y, 1 / 2.4) - 0.055;
}

/**
 * Convert the color to grayscale using a luminosity
 * formula, which better represents human perception
 */

inline unsigned int convert_to_gray(unsigned int R, unsigned int G,
                                    unsigned int B, unsigned int A) {
  double R_linear = sRGB_to_linear(R / 255.0);
  double G_linear = sRGB_to_linear(G / 255.0);
  double B_linear = sRGB_to_linear(B / 255.0);
  double gray_linear =
      0.2126 * R_linear + 0.7152 * G_linear + 0.0722 * B_linear;
  return static_cast<unsigned int>(round(linear_to_sRGB(gray_linear) * A));
}

/**
 * Receives a JSON string of the format:
 * {"type":"message","data":{"action":"refresh","image":"/path/to/the/image.png"}}
 * It loads the file and returns a byte array ready to be sent to the display
 */
std::vector<unsigned char> process_message(char *message, int debug,
                                           int verbose) {
  /**
   * The bitmap frame buffer will consist of bytes (i.e. char)
   * in a vector. For a 1-bit display, each byte represents 8
   * 1-bit pixels. It will therefore be 1/8 of the width of the
   * display, and its full height.
   */
  unsigned int frame_buffer_length =
      static_cast<unsigned int>(ceil(DISPLAY_WIDTH / 8) * DISPLAY_HEIGHT);
  std::vector<unsigned char> bitmap_frame_buffer(frame_buffer_length);

  printf("%d\n", debug);
  printf("%d\n", verbose);
  cJSON *message_json = cJSON_Parse(message);
  cJSON *data = NULL;
  cJSON *action = NULL;
  cJSON *image_filename = NULL;
  data = cJSON_GetObjectItemCaseSensitive(message_json, "data");
  if (cJSON_IsObject(data)) {
    action = cJSON_GetObjectItemCaseSensitive(data, "action");
    image_filename = cJSON_GetObjectItemCaseSensitive(data, "image");
    if (cJSON_IsString(action) &&
        (strcmp(action->valuestring, "refresh") == 0) &&
        cJSON_IsString(image_filename) &&
        (image_filename->valuestring != NULL)) {
      printf("Displaying image file at \"%s\"\n", image_filename->valuestring);

      // Populate the row pointers with pixel data from the PNG image,
      // in RGBA format, using libpng
      read_png_file(image_filename->valuestring);

      /*
       * Our `rows` will contain a 2D, 1-bit representation of the
       * image, one vector element per pixel.
       */
      std::vector<std::vector<int>> rows(height, std::vector<int>(width));

      for (unsigned int y = 0; y < height; y++) {
        for (unsigned int x = 0; x < width; x++) {

          // The row pointers contain RGBA data as one byte per channel
          unsigned int gray_color =
              convert_to_gray(row_pointers[y][x * bytes_per_pixel + 0],
                              row_pointers[y][x * bytes_per_pixel + 1],
                              row_pointers[y][x * bytes_per_pixel + 2],
                              row_pointers[y][x * bytes_per_pixel + 3]);

          // If a pixel is more than 50% bright, make it white. Otherwise,
          // black.
          rows[y][x] = (gray_color >= 127);
          // debug print bitmap
          // printf("%s", rows[y][x] ? "█" : " ");
        }
        // printf("\n");
      }

      unsigned int bytes_per_row = DISPLAY_WIDTH / 8;

      /*
       * Convert the 2D matrix of 1-bit values into a flat array of
       * 8-bit `char`s. To glob 8 bits together into a char, we use
       * bit-shifting operators (<<)
       */

      // for each row of the display (not the image!)...
      for (unsigned int y = 0; y < DISPLAY_HEIGHT; y++) {
        // ... and each byte across (i.e. 1/8 of the columns in the display)
        // ...
        for (unsigned int x = 0; x < bytes_per_row; x++) {
          unsigned int current_byte = 0;

          // ... iterate over the byte's 8 bits representing the pixels in
          // the image
          for (unsigned int bit = 0; bit < 8; bit++) {
            // if the image exists to fill the current bit, and the current
            // bit/pixel is 1/black, assign the current bit to its position
            // in a new byte based on its index and assign the current byte
            // to a bitwise OR of this new byte.
            // e.g. rows[1] =
            // 1 1 0 1 0 0 1 1
            // ^              current_byte = 00000000; bit = 0;
            //                current_byte = current_byte | 1 << (7-bit)
            //                current_byte = current_byte | 10000000
            //                current_byte = 10000000;

            //   ^            bit = 1;
            //                current_byte = current_byte | 1 << (7-bit)
            //                current_byte = current_byte | 01000000
            //                current_byte = 11000000;

            // If the image is narrower or shorter than the current pixel
            // location, render white (i.e. 1). Otherwise, render the value
            // at the current pixel location from our rows[][]
            if ((width <= (x * 8 + bit)) || (height <= y) ||
                rows[y][x * 8 + bit] == 1) {
              current_byte = current_byte | 1 << (7 - bit);
            }
          }

          // push each completed byte into the frame buffer
          bitmap_frame_buffer[(y * bytes_per_row) + x] =
              static_cast<unsigned char>(current_byte);
        }
      }
    }
  }
  return bitmap_frame_buffer;
  cJSON_Delete(message_json);
}

/**
 * Receives a frame buffer in the form of a byte array, the bits of which
 * represent the pixels to be displayed. Uses the `epdif` library from Waveshare
 * to write the frame buffer to the device.
 */

void write_to_device(std::vector<unsigned char> &bitmap_frame_buffer) {
  // debug print byte frame buffer
  /*for (unsigned int i = 0; i < bitmap_frame_buffer.size(); i++) {
    if (i % 16 == 0) {
      printf("\n");
    }
    printf("0X%02X,", bitmap_frame_buffer[i]);
  }
  printf("\n");*/

  Epd epd;
  if (epd.Init() != 0) {
    printf("e-Paper init failed\n");
  } else {
    // send the frame buffer to the panel
    epd.DisplayFrame(bitmap_frame_buffer.data());
  }
}
