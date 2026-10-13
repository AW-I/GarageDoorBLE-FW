# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# The app version comes from main/version.h. It is embedded in the image (esp_app_desc_t),
# where the phone app reads it from the bundled firmware file.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/main/version.h" VERSION_LINE REGEX "^#define VERSION ")
string(REGEX REPLACE ".*\"(.*)\".*" "\\1" PROJECT_VER "${VERSION_LINE}")
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(GarageDoorBLE)
