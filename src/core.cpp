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

DisplayProperties DISPLAY_PROPERTIES;

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

Pixel translate_display_pixel_to_image(int x, int y, int offset_x, int offset_y,
                                       ImageProperties image_properties) {
  Pixel image_pixel = {};

  image_pixel.in_bounds =
      (x >= offset_x && x < (offset_x + image_properties.width) &&
       y >= offset_y && y < (offset_y + image_properties.height));

  if (image_pixel.in_bounds) {
    image_pixel.x = x - offset_x;
    image_pixel.x_byte_index = image_pixel.x * image_properties.bytes_per_pixel;
    image_pixel.y = y - offset_y;
  }

  return image_pixel;
}

int get_current_pixel(int x, int y, int offset_x, int offset_y,
                      ImageProperties image_properties, int background_color) {

  Pixel image_pixel = translate_display_pixel_to_image(x, y, offset_x, offset_y,
                                                       image_properties);

  // If the current pixel being drawn is inside the calculated image bounds,
  // draw the pixel, otherwise draw the background color.
  if (image_pixel.in_bounds) {

    // The row pointers contain RGBA data as one byte per channel: R, B, G
    // and A.
    unsigned int gray_color = convert_to_gray(
        image_properties
            .row_pointers[image_pixel.y][image_pixel.x_byte_index + 0],
        image_properties
            .row_pointers[image_pixel.y][image_pixel.x_byte_index + 1],
        image_properties
            .row_pointers[image_pixel.y][image_pixel.x_byte_index + 2],
        image_properties
            .row_pointers[image_pixel.y][image_pixel.x_byte_index + 3]);

    /***
     *  If we're in 1 bit per pixel mode, then if a pixel is more than 50%
     *  bright, make it white (1). Otherwise, black (0). If we're in 8 bit
     *  per pixel mode, return the full 8-bit grayscale color.
     ***/
    return COLOR_MODE == COLOR_MODE_1BPP ? (gray_color > 127) : gray_color;
  } else {
    return background_color;
  }
}

/**
 * Receives a Message object with the key `image_filename`
 * It loads the file and returns a byte array ready to be sent to the display
 */
std::vector<unsigned char> process_image(Message action) {
  /**
   * The bitmap frame buffer will consist of bytes (i.e. char)
   * in a vector. For a 1-bit display, each byte represents 8
   * 1-bit pixels. It will therefore be 1/8 of the width of the
   * display, and its full height.
   */

  unsigned int bytes_per_row = COLOR_MODE == COLOR_MODE_1BPP
                                   ? DISPLAY_PROPERTIES.width / 8
                                   : DISPLAY_PROPERTIES.height;

  unsigned int frame_buffer_length =
      static_cast<unsigned int>(bytes_per_row * DISPLAY_PROPERTIES.height);
  std::vector<unsigned char> bitmap_frame_buffer(frame_buffer_length);

  LOG_INFO << "Loading image file at: " << action.image_filename;

  /***
   *  Populate the row pointers with pixel data from the PNG image,
   *  in RGBA format, using libpng -- and return the image width, height,
   *  and bytes_per_pixel
   ***/
  ImageProperties image_properties = read_png_file(action.image_filename);

  // Center the image in the display
  int offset_x = floor((DISPLAY_PROPERTIES.width - image_properties.width) / 2);
  int offset_y =
      floor((DISPLAY_PROPERTIES.height - image_properties.height) / 2);

  int background_color_for_color_mode = COLOR_MODE == COLOR_MODE_1BPP
                                            ? (BACKGROUND_COLOR > 127)
                                            : BACKGROUND_COLOR;

  for (int y = 0; y < DISPLAY_PROPERTIES.height; y++) {
    int current_byte = 0;

    for (int x = 0; x < DISPLAY_PROPERTIES.width; x++) {

      int current_pixel =
          get_current_pixel(x, y, offset_x, offset_y, image_properties,
                            background_color_for_color_mode);

      /***
       *  we now have x (between 0 and display_width - 1) and y (between 0 and
       *  display_height - 1).
       *
       *  COLOR_MODE will be equal to either COLOR_MODE_1BPP or COLOR_MODE_8BPP.
       *
       *  If COLOR_MODE == COLOR_MODE_1BPP then current_pixel will be an int
       *  equal to either 1 (white) or 0 (black) and we want to push one byte
       *  into the frame buffer per 8 pixels.
       *
       *  If COLOR_MODE == COLOR_MODE_8BPP then current_pixel will be an int
       *  between 0 and 255 representing the grayscale value of the current
       *  pixel, and we want to push one byte into the frame buffer per pixel.
       ***/

      if (COLOR_MODE == COLOR_MODE_1BPP) {
        /***
         *  Perform a bitwise OR to set the bit in the current_byte representing
         *  the current pixel (of a set of 8), e.g.:
         *  00110011 (current_byte) | 00001000 (i.e. current pixel) = 00111011
         *  If we're on the 8th and final pixel of the current_byte, push the
         *  current_byte into the frame buffer and reset it to zero so we can
         *  process the next 8 pixels.
         ***/
        if (current_pixel == 1) {
          current_byte = current_byte | 1 << (7 - (x % 8));
        }
        if (x % 8 == 7) {
          bitmap_frame_buffer[(y * bytes_per_row) + (x / 8)] =
              static_cast<unsigned char>(current_byte);
          current_byte = 0;
        }
      } else {
        // In 8bpp mode, we can just push individual pixels into the frame
        // buffer as they take up an entire byte.
        bitmap_frame_buffer[(y * bytes_per_row) + x] =
            static_cast<unsigned char>(current_pixel);
      }
    }
  }

  // Debug print byte frame buffer
  IF_LOG(plog::verbose) {
    std::stringstream debug_frame_buffer_line;
    LOG_VERBOSE << "Frame buffer:";
    for (unsigned int i = 0; i < bitmap_frame_buffer.size(); i++) {
      if (i % 16 == 0 && i > 0) {
        LOG_VERBOSE << debug_frame_buffer_line.str();
        debug_frame_buffer_line.str("");
      }
      debug_frame_buffer_line << "0X" << setfill('0') << setw(2)
                              << std::uppercase << std::hex
                              << int(bitmap_frame_buffer[i]) << ",";
    }
  }

  return bitmap_frame_buffer;
}

/**
 * Receives a frame buffer in the form of a byte array, the bits of which
 * represent the pixels to be displayed. Uses the `epdif` library from
 * Waveshare to write the frame buffer to the device.
 */

void write_to_display(std::vector<unsigned char> &bitmap_frame_buffer) {
  Epd epd;
  if (epd.Init() != 0) {
    LOG_ERROR << "e-Paper init failed";
  } else {
    // send the frame buffer to the panel
    epd.DisplayFrame(bitmap_frame_buffer.data());
  }
}
