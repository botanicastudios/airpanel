/* -*- mode: c; c-file-style: "openbsd" -*- */
/*
 * Copyright (c) 2014 Toby Marsden <toby@botanicastudios.io>
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

#ifndef _BOOTSTRAP_H
#define _BOOTSTRAP_H

#include <stdlib.h>
#include <string>

#include "config.h"
#include "exceptions.h"
#include "logger.h"

#include "cJSON.h"
#include <png.h>

#include <vector>

using namespace std;

/* TODO:5001 Declare here functions that you will use in several files. Those
 * TODO:5001 functions should not be prefixed with `static` keyword. All other
 * TODO:5001 functions should.
 */

struct Message {
  std::string action;
  std::string image_filename;
  bool offset_x_specified = false;
  int offset_x;
  bool offset_y_specified = false;
  int offset_y;
  bool orientation_specified;
  int orientation;
  bool action_is_refresh() { return action == string("refresh"); };
  bool has_image_filename() { return image_filename != string(""); }
};

struct Pixel {
  int x;
  int x_byte_index;
  int y;
  bool in_bounds;
};

struct DisplayProperties {
  int width;
  int height;
  int color_mode;
  int orientation;
  std::string processor;
};

struct TranslationProperties {
  int offset_x;
  int offset_y;
  int display_width;
  int display_height;
  int image_width;
  int image_height;
  int orientation;
};

Message parse_message(const char *message);

unsigned int convert_to_gray(unsigned int R, unsigned int G, unsigned int B,
                             unsigned int A);

void process_message(Message message);

std::vector<unsigned char> process_image(Message action);

void debug_write_bmp(Message action);

void write_to_display(std::vector<unsigned char> &bitmap_frame_buffer);

#endif
