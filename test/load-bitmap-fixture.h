#include <stdlib.h>
#include <string>
#include <vector>

const int COLOR_MODE_1BPP = 1;
const int COLOR_MODE_8BPP = 8;

std::vector<unsigned char>
read_bmp_into_byte_array(const char *filename, const int output_bits_per_pixel);
