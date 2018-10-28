#include "exceptions.h"
#include "logger.h"
#include "readpng.h"
#include <png.h>
#include <stdlib.h>
#include <string>

ImageProperties read_png_file(std::string filename) {
  ImageProperties image_properties;

  FILE *fp;

  if ((fp = fopen(filename.c_str(), "rb")) == NULL) {
    throw ImageFileNotFound(filename);
  }

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

  image_properties.width = png_get_image_width(png, info);
  image_properties.height = png_get_image_height(png, info);
  image_properties.color_type = png_get_color_type(png, info);
  image_properties.bit_depth = png_get_bit_depth(png, info);

  // Read any color_type into 8bit depth, RGBA format.
  // See http://www.libpng.org/pub/png/libpng-manual.txt

  if (image_properties.bit_depth == 16)
    png_set_strip_16(png);

  if (image_properties.color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
  if (image_properties.color_type == PNG_COLOR_TYPE_GRAY &&
      image_properties.bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  // These color_type don't have an alpha channel then fill it with 0xff.
  if (image_properties.color_type == PNG_COLOR_TYPE_RGB ||
      image_properties.color_type == PNG_COLOR_TYPE_GRAY ||
      image_properties.color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  if (image_properties.color_type == PNG_COLOR_TYPE_GRAY ||
      image_properties.color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  png_read_update_info(png, info);

  image_properties.bytes_per_pixel = static_cast<unsigned int>(
      png_get_rowbytes(png, info) / image_properties.width);

  image_properties.row_pointers = static_cast<png_bytep *>(
      malloc(sizeof(png_bytep) * image_properties.height));
  for (int y = 0; y < image_properties.height; y++) {
    image_properties.row_pointers[y] =
        static_cast<png_byte *>(malloc(png_get_rowbytes(png, info)));
  }

  png_read_image(png, image_properties.row_pointers);

  fclose(fp);
  png_destroy_read_struct(&png, &info, NULL);
  png = NULL;
  info = NULL;
  return image_properties;
}
