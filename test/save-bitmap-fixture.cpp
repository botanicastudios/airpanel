#include "../src/core.h"

extern DisplayProperties DISPLAY_PROPERTIES;

/***
 *  Saves a .ppm image from the generated output frame buffer -- this is
 *  super-useful for debugging. To convert it into a .bmp file, install
 *  ImageMagick and run `convert image.ppm image.bmp` inside ./build
 *
 *  To use it from a test, include something like this alongside your EXPECT_*
 *  statement(s) inside your TEST() {} blocks:
 *
 *  debug_write_ppm(parse_message(R"(
 *    {
 *      "type": "message",
 *      "data": {
 *        "action": "refresh",
 *        "image": "./fixtures/384x640_24bpp_in.png"
 *      }
 *    }
 *  )"));
 **/
void debug_write_ppm(Action action) {
  std::vector<unsigned char> debug_frame_buffer = process_image(action);
  std::vector<unsigned char> debug_24bpp_frame_buffer(
      DISPLAY_PROPERTIES.width * DISPLAY_PROPERTIES.height * 3);

  for (unsigned int i = 0; i < debug_frame_buffer.size(); i++) {
    if (DISPLAY_PROPERTIES.color_mode == COLOR_MODE_1BPP) {
      char byte = debug_frame_buffer[i];
      for (int bit = 0; bit < 8; bit++) {
        int bit_value = ((byte >> (7 - bit)) & 1) * 255;
        debug_24bpp_frame_buffer[i * 8 * 3 + (3 * bit)] = bit_value;
        debug_24bpp_frame_buffer[i * 8 * 3 + (3 * bit) + 1] = bit_value;
        debug_24bpp_frame_buffer[i * 8 * 3 + (3 * bit) + 2] = bit_value;
      }
    } else {
      debug_24bpp_frame_buffer[i * 3] = debug_frame_buffer[i];
      debug_24bpp_frame_buffer[i * 3 + 1] = debug_frame_buffer[i];
      debug_24bpp_frame_buffer[i * 3 + 2] = debug_frame_buffer[i];
    }
  }

  FILE *imageFile;

  imageFile = fopen("./image.ppm", "wb");
  if (imageFile == NULL) {
    perror("ERROR: Cannot open output file");
    exit(EXIT_FAILURE);
  }

  fprintf(imageFile, "P6\n"); // P6 filetype
  fprintf(imageFile, "%d %d\n", DISPLAY_PROPERTIES.width,
          DISPLAY_PROPERTIES.height); // dimensions
  fprintf(imageFile, "255\n");        // Max pixel

  fwrite(reinterpret_cast<char *>(&debug_24bpp_frame_buffer[0]), 1,
         debug_24bpp_frame_buffer.size(), imageFile);

  fclose(imageFile);
}
