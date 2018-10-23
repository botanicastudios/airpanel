#include <png.h>
#include <string>

struct ImageProperties {
  unsigned int width;
  unsigned int height;
  png_byte color_type;
  png_byte bit_depth;
  unsigned int bytes_per_pixel;
  png_bytep *row_pointers;
};

ImageProperties read_png_file(std::string filename);
