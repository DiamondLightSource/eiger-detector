# Project name
project(EigerDetector)
set(SOURCE_DIR ${EigerDetector_SOURCE_DIR})

set(CMAKE_CXX_STANDARD 11 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Require CMake version >=2.8
cmake_minimum_required(VERSION 2.8)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_CONFIG_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test_config)

# Appends the cmake/modules path inside the MAKE_MODULE_PATH variable which stores the
# directories of additional CMake modules (ie. MacroOutOfSourceBuild.cmake):
set(CMAKE_MODULE_PATH ${SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# Add the automatically determined parts of the RPATH which point to directories outside the
# build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Find and add external packages required for application and test
find_package(
    Boost 1.41.0 REQUIRED
    COMPONENTS program_options system unit_test_framework date_time thread
)
find_package(LOG4CXX 0.10.0 REQUIRED)
find_package(ZEROMQ 3.2.4 REQUIRED)
find_package(ODINDATA REQUIRED)

# Git versioning
message("Determining eiger-detector version")
include(GetGitRevisionDescription)
git_describe(GIT_DESC_STR)
string(REGEX REPLACE "^([0-9]+)[\\.-].*" "\\1" VERSION_MAJOR "${GIT_DESC_STR}")
string(REGEX REPLACE "^[0-9]+[\\.-]([0-9]+).*" "\\1" VERSION_MINOR "${GIT_DESC_STR}")
string(REGEX REPLACE "^[0-9]+[\\.-][0-9]+[\\.-]([0-9]+).*" "\\1" VERSION_PATCH "${GIT_DESC_STR}")
string(REGEX REPLACE "^[0-9]+[\\.-][0-9]+[\\.-][0-9]+-[0-9]+-(.*)" "\\1" VERSION_SHA1 "${GIT_DESC_STR}")
set(VERSION_SHORT "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
set(VERSION_STR ${GIT_DESC_STR})
message("-- Git describe version: ${GIT_DESC_STR}")
message("-- major:${VERSION_MAJOR} minor:${VERSION_MINOR} patch:${VERSION_PATCH} sha1:${VERSION_SHA1}")
message("-- short version: ${VERSION_SHORT}")

set(COMMON_DIR ${SOURCE_DIR}/common)
set(DATA_DIR ${SOURCE_DIR}/data)

# Add configure output include directory to include path
configure_file(${COMMON_DIR}/include/version.h.in "${CMAKE_BINARY_DIR}/include/version.h")
include_directories(${CMAKE_BINARY_DIR}/include)

# Add subdirectories
add_subdirectory(${DATA_DIR})
