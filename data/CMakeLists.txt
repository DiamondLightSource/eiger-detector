# Project name
project(eigerdetector)

# Require CMake version >=2.8
cmake_minimum_required(VERSION 2.8)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set the Eiger Fan directory
set(EIGERFAN_DIR ${eigerdetector_SOURCE_DIR}/eigerfan)
# Set the frameReceiver directory
set(FRAMERECEIVER_DIR ${eigerdetector_SOURCE_DIR}/frameReceiver)
# Set the frameProcessor directory
set(FRAMEPROCESSOR_DIR ${eigerdetector_SOURCE_DIR}/frameProcessor)
# Set the frameSimulator directory
set(FRAMESIMULATOR_DIR ${eigerdetector_SOURCE_DIR}/frameSimulator)
# Set the tools directory
set(TOOLS_DIR ${eigerdetector_SOURCE_DIR}/tools)
# Set the common directory
set(COMMON_DIR ${eigerdetector_SOURCE_DIR}/common)

# Appends the cmake/modules path inside the MAKE_MODULE_PATH variable which stores the
# directories of additional CMake modules (ie. MacroOutOfSourceBuild.cmake):
set(CMAKE_MODULE_PATH ${eigerdetector_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# Find and add external packages required for application and test
find_package( Boost 1.41.0
	      REQUIRED
	      COMPONENTS program_options system unit_test_framework date_time thread)
find_package(Log4CXX 0.10.0 REQUIRED)
find_package(ZeroMQ 3.2.4 REQUIRED)
find_package(OdinData REQUIRED)

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
configure_file(${COMMON_DIR}/include/version.h.in "${CMAKE_BINARY_DIR}/include/version.h")
include_directories(${CMAKE_BINARY_DIR}/include)

# Add include directory to include path
include_directories(${eigerdetector_SOURCE_DIR}/include)

# Add include directory to common include path
include_directories(${eigerdetector_SOURCE_DIR}/common/include)


# Add the eigerfan subdirectory
add_subdirectory(${EIGERFAN_DIR})
# Add the frameReceiver subdirectory
add_subdirectory(${FRAMERECEIVER_DIR})
# Add the frameProcessor subdirectory
add_subdirectory(${FRAMEPROCESSOR_DIR})
# Add the frameSimulator subdirectory
add_subdirectory(${FRAMESIMULATOR_DIR})
# Add the tools subdirectory
add_subdirectory(${TOOLS_DIR})

# Add the test subdirectory
add_subdirectory(test)

