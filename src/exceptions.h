#include <exception>
#include <string>

struct ImageFileNotFound : public std::exception {
  std::string filename;
  const char *what() const throw() {
    return (std::string("Image file not found: ") + filename).c_str();
  }
  ImageFileNotFound(std::string filename) : filename(filename) {}
};
