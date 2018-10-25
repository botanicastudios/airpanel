#include <png.h>
#include <string>

struct ImageProperties {
  int width;
  int height;
  png_byte color_type;
  png_byte bit_depth;
  int bytes_per_pixel;
  png_bytep *row_pointers;
  bool is_portrait() { return height > width; }
};

ImageProperties read_png_file(std::string filename);
