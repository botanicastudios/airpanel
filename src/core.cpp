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
#include <algorithm>
#include <cctype>
#include <ctype.h>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

using namespace std;

DisplayProperties DISPLAY_PROPERTIES;

/***
 * Lift the gamma curve from the pixel from the source image so we can convert
 * to grayscale
 */
inline double sRGB_to_linear(double x) {
  if (x < 0.04045)
    return x / 12.92;
  return pow((x + 0.055) / 1.055, 2.4);
}

/***
 *  Apply gamma curve to the pixel of the destination image again
 */
inline double linear_to_sRGB(double y) {
  if (y <= 0.0031308)
    return 12.92 * y;
  return 1.055 * pow(y, 1 / 2.4) - 0.055;
}

/***
 *  Convert the color to grayscale using a luminosity
 *  formula, which better represents human perception
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

/***
 *  Get JSON parsing out of the way and return a struct.
 */
Action parse_message(const char *message_string) {
  Action message;
  char *endptr;
  message.type = "socket";
  cJSON *message_json;
  cJSON *data;
  message_json = cJSON_Parse(message_string);
  data = NULL;
  data = cJSON_GetObjectItemCaseSensitive(message_json, "data");
  if (cJSON_IsObject(data)) {
    cJSON *actionJSON = cJSON_GetObjectItemCaseSensitive(data, "action");
    cJSON *imageJSON = cJSON_GetObjectItemCaseSensitive(data, "image");
    cJSON *orientationJSON =
        cJSON_GetObjectItemCaseSensitive(data, "orientation");
    cJSON *offsetXJSON = cJSON_GetObjectItemCaseSensitive(data, "offset_x");
    cJSON *offsetYJSON = cJSON_GetObjectItemCaseSensitive(data, "offset_y");
    if (actionJSON && actionJSON->valuestring != NULL) {
      message.action = string(actionJSON->valuestring);
    }
    if (imageJSON && imageJSON->valuestring != NULL) {
      message.image_filename = string(imageJSON->valuestring);
    }
    if (orientationJSON) {
      int orientation;
      if (cJSON_IsNumber(orientationJSON)) {
        orientation = int(orientationJSON->valueint);
      } else {
        long int parsed_orientation =
            strtol(orientationJSON->valuestring, &endptr, 0);
        if (!*endptr) {
          orientation = int(parsed_orientation);
        } else {
          // orientation_specified will be false, so this won't be used
          orientation = 0;
          LOG_WARNING << "Orientation '" << orientationJSON->valuestring
                      << "' could not be understood";
        }
      }
      int valid_orientations[] = {0, 90, 180, 270};
      if (std::find(std::begin(valid_orientations),
                    std::end(valid_orientations),
                    orientation) != std::end(valid_orientations)) {
        message.orientation_specified = true;
        message.orientation = orientation;
      } else {
        LOG_WARNING << "Orientation " << orientation
                    << " supplied, but valid values are 0, 90, 180 and 270";
      }
    }
    if (offsetXJSON) {
      int offset_x;
      if (cJSON_IsNumber(offsetXJSON)) {
        offset_x = int(offsetXJSON->valueint);
      } else {
        long int parsed_offset_x = strtol(offsetXJSON->valuestring, &endptr, 0);
        if (!*endptr) {
          offset_x = int(parsed_offset_x);
        } else {
          offset_x = 0;
          LOG_WARNING << "offset_x '" << offsetXJSON->valuestring
                      << "' could not be understood";
        }
      }
      message.offset_x_specified = true;
      message.offset_x = offset_x;
    }
    if (offsetYJSON) {
      int offset_y;
      if (cJSON_IsNumber(offsetYJSON)) {
        offset_y = int(offsetYJSON->valueint);
      } else {
        long int parsed_offset_y = strtol(offsetYJSON->valuestring, &endptr, 0);
        if (!*endptr) {
          offset_y = int(parsed_offset_y);
        } else {
          offset_y = 0;
          LOG_WARNING << "offset_y '" << offsetYJSON->valuestring
                      << "' could not be understood";
        }
      }
      message.offset_y_specified = true;
      message.offset_y = offset_y;
    }
  }
  cJSON_Delete(message_json);
  return message;
}

/* The main deal: take an incoming message and... display an image!
 */
void process_action(Action action) {
  if (action.action_is_refresh()) {
    if (action.has_image_filename()) {
      std::vector<unsigned char> bitmap_frame_buffer = process_image(action);
      write_to_display(bitmap_frame_buffer);
    } else {
      LOG_WARNING << "Message with `refresh` action received, but no "
                     "`image` was provided";
    }
  }
}

/***
 *  Given an x and y representing a pixel on the display, look up the x and y
 *  co-ordinates from the source image, taking into account orientation and
 *  offset (and the conversion to PNG byte row index, for convenience)
 */
Pixel translate_display_pixel_to_image(
    int x, int y, TranslationProperties translation_properties,
    ImageProperties image_properties) {

  Pixel image_pixel = {};

  switch (translation_properties.orientation) {

  // For 180 orientation, swap the pixels around then proceed, then continue to
  // process as per 0 orientation.
  case 180:
    x = translation_properties.display_width - 1 - x;
    y = translation_properties.display_height - 1 - y;

  /* in_bounds figures out whether this pixel corresponds to an image pixel
   * that lands within the display size, or whether this part of the display
   * is blank because the image is offset, or too short/narrow for the display.
   */
  case 0:
    image_pixel.in_bounds = (x >= translation_properties.offset_x &&
                             x < (translation_properties.offset_x +
                                  translation_properties.image_width) &&
                             y >= translation_properties.offset_y &&
                             y < (translation_properties.offset_y +
                                  translation_properties.image_height));

    // assuming we are to render it, remove the offset to return the pixel
    // co-ordinates from the source image
    if (image_pixel.in_bounds) {
      image_pixel.x = x - translation_properties.offset_x;
      image_pixel.y = y - translation_properties.offset_y;
    }

    break;

  // For 270 orientation, swap the pixels around then proceed, then continue to
  // process as per 90 orientation.
  case 270:
    x = translation_properties.display_height - 1 - x;
    y = translation_properties.display_width - 1 - y;

  /* Look up the image pixel after translating the image 90 degrees
   * clockwise -- this mostly involves taking into account the display
   * width (in its current orientation, which correspond's to the
   * display's native height; see #get_translation_properties below),
   * where we don't need to for an image with no change to its
   * orientation.
   */
  case 90:
    image_pixel.in_bounds = (x >= translation_properties.offset_y &&
                             x < (translation_properties.offset_y +
                                  translation_properties.image_height) &&
                             y >= (translation_properties.display_width -
                                   translation_properties.offset_x -
                                   translation_properties.image_width) &&
                             y < (translation_properties.display_width -
                                  translation_properties.offset_x));

    if (image_pixel.in_bounds) {
      image_pixel.x = translation_properties.display_width - 1 -
                      translation_properties.offset_x - y;
      image_pixel.y = x - translation_properties.offset_y;
    }

    break;
  }

  // Input PNG images will have more than one byte per pixel -- we need to know
  // which byte (corresponding to the red channel of RGB) to start from
  image_pixel.x_byte_index = image_pixel.x * image_properties.bytes_per_pixel;

  return image_pixel;
}

/***
 *  Return the color of the current display pixel, by looking up the
 *  corresponding pixel from the source image and converting it to grayscale or
 *  1-bit black and white (depending on the color mode). If there's no source
 *  pixel (because there's an offset, or the image doesn't cover the whole
 *  display), return the background color.
 */
int get_current_pixel(int x, int y,
                      TranslationProperties translation_properties,
                      ImageProperties image_properties, int background_color) {

  Pixel image_pixel = translate_display_pixel_to_image(
      x, y, translation_properties, image_properties);

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

    /* If we're in 1 bit per pixel mode, then if a pixel is more than 50%
     * bright, make it white (1). Otherwise, black (0). If we're in 8 bit
     * per pixel mode, return the full 8-bit grayscale color.
     */
    return DISPLAY_PROPERTIES.color_mode == COLOR_MODE_1BPP ? (gray_color > 127)
                                                            : gray_color;
  } else {
    return background_color;
  }
}

/***
 *  Set up the orientation, offset and if necessary swap display width and
 * height to make it easier to reason about the display pixel -> image pixel
 *  conversion.
 */
TranslationProperties
get_translation_properties(Action action, ImageProperties image_properties) {
  TranslationProperties translation_properties;

  // If the message provided an orientation, use that preferentially
  if (action.orientation_specified) {
    translation_properties.orientation = action.orientation;
  } else {
    if (DISPLAY_PROPERTIES.orientation_specified) {
      // If display orientation was specified at startup, use that
      translation_properties.orientation = DISPLAY_PROPERTIES.orientation;
    } else {
      // If no orientation was specified, auto orient images so landscape
      // displays rotate portrait images and vice versa
      translation_properties.orientation =
          DISPLAY_PROPERTIES.is_portrait() == image_properties.is_portrait()
              ? 0
              : 90;
    }
  }

  switch (translation_properties.orientation) {
  case 270:
  case 90:
    translation_properties.display_width = DISPLAY_PROPERTIES.height;
    translation_properties.display_height = DISPLAY_PROPERTIES.width;
    break;
  case 180:
  case 0:
  default:
    translation_properties.display_width = DISPLAY_PROPERTIES.width;
    translation_properties.display_height = DISPLAY_PROPERTIES.height;
    break;
  }

  translation_properties.image_width = image_properties.width;
  translation_properties.image_height = image_properties.height;

  translation_properties.offset_x = action.offset_x;
  translation_properties.offset_y = action.offset_y;

  if (!action.offset_x_specified) {
    // Center the image in the display
    translation_properties.offset_x =
        int(floor((translation_properties.display_width -
                   translation_properties.image_width) /
                  2));
  }

  if (!action.offset_y_specified) {
    // Center the image in the display
    translation_properties.offset_y =
        int(floor((translation_properties.display_height -
                   translation_properties.image_height) /
                  2));
  }

  return translation_properties;
}

/***
 *  Receives an Action object with the key `image_filename`
 *  It loads the file and returns a byte array ready to be sent to the display
 */
std::vector<unsigned char> process_image(Action action) {

  /* The bitmap frame buffer will consist of bytes (i.e. char)
   * in a vector. For a 1-bit display, each byte represents 8
   * 1-bit pixels. It will therefore be 1/8 of the width of the
   * display, and its full height.
   */
  unsigned int bytes_per_row = DISPLAY_PROPERTIES.color_mode == COLOR_MODE_1BPP
                                   ? DISPLAY_PROPERTIES.width / 8
                                   : DISPLAY_PROPERTIES.width;

  unsigned int frame_buffer_length =
      static_cast<unsigned int>(bytes_per_row * DISPLAY_PROPERTIES.height);
  std::vector<unsigned char> bitmap_frame_buffer(frame_buffer_length);

  LOG_INFO << "Loading image file at: " << action.image_filename;

  /* Populate the row pointers with pixel data from the PNG image,
   * in RGBA format, using libpng -- and return the image width, height,
   * and bytes_per_pixel
   */
  ImageProperties image_properties = read_png_file(action.image_filename);

  TranslationProperties translation_properties =
      get_translation_properties(action, image_properties);

  int background_color_for_color_mode =
      DISPLAY_PROPERTIES.color_mode == COLOR_MODE_1BPP
          ? (BACKGROUND_COLOR > 127)
          : BACKGROUND_COLOR;

  for (int y = 0; y < DISPLAY_PROPERTIES.height; y++) {
    int current_byte = 0;

    for (int x = 0; x < DISPLAY_PROPERTIES.width; x++) {

      int current_pixel =
          get_current_pixel(x, y, translation_properties, image_properties,
                            background_color_for_color_mode);

      /* We now have x (between 0 and display_width - 1) and y (between 0 and
       * display_height - 1).
       *
       * DISPLAY_PROPERTIES.color_mode  will be equal to either COLOR_MODE_1BPP
       * or COLOR_MODE_8BPP.
       *
       * If COLOR_MODE_1BPP then current_pixel will be an int equal to either 1
       * (white) or 0 (black) and we want to push one byte into the frame buffer
       * per 8 pixels.
       *
       * If COLOR_MODE_8BPP then current_pixel will be an int between 0 and 255
       * representing the grayscale value of the current pixel, and we want to
       * push one byte into the frame buffer per pixel.
       */

      if (DISPLAY_PROPERTIES.color_mode == COLOR_MODE_1BPP) {
        /* Perform a bitwise OR to set the bit in the current_byte
         * representing the current pixel (of a set of 8), e.g.: 00110011
         * (current_byte) | 00001000 (i.e. current pixel) = 00111011 If we're
         * on the 8th and final pixel of the current_byte, push the
         * current_byte into the frame buffer and reset it to zero so we can
         * process the next 8 pixels.
         */
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

/***
 *  Receives a frame buffer in the form of a byte array, the bits of which
 *  represent the pixels to be displayed. Uses the `epdif` library from
 *  Waveshare to write the frame buffer to the device.
 */
void write_to_display(std::vector<unsigned char> &bitmap_frame_buffer) {
  Epd epd;
  if (epd.Init() != 0) {
    LOG_ERROR << "Display initialization failed";
  } else {
    // send the frame buffer to the panel
    epd.DisplayFrame(bitmap_frame_buffer.data());
  }
}
