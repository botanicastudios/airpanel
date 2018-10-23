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
#include "readpng.h"

#include "cJSON.h"
#include "epd7in5.h"
#include "epdif.h"
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <vector>

using namespace std;

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

unsigned int convert_to_gray(unsigned int R, unsigned int G, unsigned int B,
                             unsigned int A) {
  double R_linear = sRGB_to_linear(R / 255.0);
  double G_linear = sRGB_to_linear(G / 255.0);
  double B_linear = sRGB_to_linear(B / 255.0);
  double gray_linear =
      0.2126 * R_linear + 0.7152 * G_linear + 0.0722 * B_linear;
  return static_cast<unsigned int>(round(linear_to_sRGB(gray_linear) * A));
}

Message parse_message(const char *message_string) {
  Message message;
  cJSON *message_json;
  cJSON *data;
  message_json = cJSON_Parse(message_string);
  data = NULL;
  data = cJSON_GetObjectItemCaseSensitive(message_json, "data");
  if (cJSON_IsObject(data)) {
    cJSON *actionJSON = cJSON_GetObjectItemCaseSensitive(data, "action");
    cJSON *imageJSON = cJSON_GetObjectItemCaseSensitive(data, "image");
    if (actionJSON && actionJSON->valuestring != NULL) {
      message.action = string(actionJSON->valuestring);
    }
    if (imageJSON && imageJSON->valuestring != NULL) {
      message.image_filename = string(imageJSON->valuestring);
    }
  }
  cJSON_Delete(message_json);
  return message;
}

void process_message(Message message) {
  if (message.action_is_refresh()) {
    if (message.has_image_filename()) {
      std::vector<unsigned char> bitmap_frame_buffer = process_image(message);
      write_to_display(bitmap_frame_buffer);
    } else {
      LOG_WARNING << "Message with `refresh` action received, but no "
                     "`image` was provided";
    }
  }
}

/**
 * Receives a JSON string of the format:
 * {"type":"message","data":{"action":"refresh","image":"/path/to/the/image.png"}}
 * It loads the file and returns a byte array ready to be sent to the display
 */
std::vector<unsigned char> process_image(Message action) {
  /**
   * The bitmap frame buffer will consist of bytes (i.e. char)
   * in a vector. For a 1-bit display, each byte represents 8
   * 1-bit pixels. It will therefore be 1/8 of the width of the
   * display, and its full height.
   */

  unsigned int frame_buffer_length =
      static_cast<unsigned int>(ceil(DISPLAY_WIDTH / 8) * DISPLAY_HEIGHT);
  std::vector<unsigned char> bitmap_frame_buffer(frame_buffer_length);

  LOG_INFO << "Displaying image file at: " << action.image_filename;

  // Populate the row pointers with pixel data from the PNG image,
  // in RGBA format, using libpng
  ImageProperties image_properties = read_png_file(action.image_filename);

  /*
   * Our `rows` will contain a 2D, 1-bit representation of the
   * image, one vector element per pixel.
   */
  std::vector<std::vector<int>> rows(image_properties.height,
                                     std::vector<int>(image_properties.width));

  for (unsigned int y = 0; y < image_properties.height; y++) {
    for (unsigned int x = 0; x < image_properties.width; x++) {

      // The row pointers contain RGBA data as one byte per channel
      unsigned int gray_color = convert_to_gray(
          image_properties
              .row_pointers[y][x * image_properties.bytes_per_pixel + 0],
          image_properties
              .row_pointers[y][x * image_properties.bytes_per_pixel + 1],
          image_properties
              .row_pointers[y][x * image_properties.bytes_per_pixel + 2],
          image_properties
              .row_pointers[y][x * image_properties.bytes_per_pixel + 3]);

      // If a pixel is more than 50% bright, make it white. Otherwise,
      // black.
      rows[y][x] = (gray_color > 127);
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
        if ((image_properties.width <= (x * 8 + bit)) ||
            (image_properties.height <= y) || rows[y][x * 8 + bit] == 1) {
          current_byte = current_byte | 1 << (7 - bit);
        }
      }

      // push each completed byte into the frame buffer
      bitmap_frame_buffer[(y * bytes_per_row) + x] =
          static_cast<unsigned char>(current_byte);
    }
  }

  // debug print byte frame buffer
  /*for (unsigned int i = 0; i < bitmap_frame_buffer.size(); i++) {
    if (i % 16 == 0) {
      printf("\n");
    }
    printf("0X%02X,", bitmap_frame_buffer[i]);
  }
  printf("\n");*/

  return bitmap_frame_buffer;
}

/**
 * Receives a frame buffer in the form of a byte array, the bits of which
 * represent the pixels to be displayed. Uses the `epdif` library from Waveshare
 * to write the frame buffer to the device.
 */

void write_to_display(std::vector<unsigned char> &bitmap_frame_buffer) {
  Epd epd;
  if (epd.Init() != 0) {
    printf("e-Paper init failed\n");
  } else {
    // send the frame buffer to the panel
    epd.DisplayFrame(bitmap_frame_buffer.data());
  }
}
