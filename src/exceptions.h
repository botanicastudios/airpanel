#include <stdexcept>
#include <string>

struct ImageFileNotFound : public std::runtime_error {
  ImageFileNotFound(std::string const &filename)
      : std::runtime_error("Image file not found: " + filename) {}
};
