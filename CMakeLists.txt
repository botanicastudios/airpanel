cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

# Call cmake with -D TESTS=ON to set this flag to true.
option(TESTS "build tests" OFF)

project(sample_project CXX C)

# Core and main are split. This allows us to link core to main and tests.

# Core library. *.cpp should be added here.
add_library(core
  ./src/exceptions.h
  ./src/config.h
  ./src/core.h
  ./src/core.cpp
  ./src/readpng.h
  ./src/readpng.cpp
  ./include/bcm2835.h
  ./include/bcm2835.c
  ./include/cJSON.h
  ./include/cJSON.cpp
  ./include/epd7in5.h
  ./include/epd7in5.cpp
  ./include/epdif.h
  ./include/epdif.cpp
  ./include/log.h
  ./include/log.c
  )

include_directories(./include)

# Search for pkg-config
include (FindPkgConfig)
if (NOT PKG_CONFIG_FOUND)
    message (FATAL_ERROR "pkg-config not found")
endif ()

# Check for libpng
pkg_check_modules (LIBPNG libpng16 REQUIRED)
if (NOT LIBPNG_FOUND)
    message(FATAL_ERROR "You don't seem to have libpng16 development libraries installed")
else ()
    include_directories (${LIBPNG_INCLUDE_DIRS})
    link_directories (${LIBPNG_LIBRARY_DIRS})
    link_libraries (${LIBPNG_LIBRARIES})
endif ()

# Main entry point.
add_executable(wollemi
  ./src/main.cpp ${LIBPNG_LINK_FLAGS})

# Link core to main.
target_link_libraries(wollemi
  core)

# Add flags.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")

if(TESTS)

  enable_testing()

  # Process the CMakeLists.txt in third-party/googletest, sets project(gtest) and
  # project(gmock), gives access to ${gtest_SOURCE_DIR} and ${gmock_SOURCE_DIR}.
  add_subdirectory(third-party/googletest)

  include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
  include_directories(${gmock_SOURCE_DIR}/include ${gmock_SOURCE_DIR})

  # Tests. *-test.cpp should be added here.
  add_executable(tests
    ./test/main-test.cpp
    ./test/load-bitmap-fixture.cpp
    ./test/load-bitmap-fixture.h
    ./test/process_image-test.cpp)

  file(COPY test/fixtures DESTINATION .)

  # Link core, pthread and gtest to tests.
  target_link_libraries(tests
    pthread
    gtest
    gtest_main
    core)

endif()
