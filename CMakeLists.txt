# Project name
project(eigerdaq)

# Require CMake version >=2.8
cmake_minimum_required(VERSION 2.8)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set the Eiger Fan directory
set(EIGERFAN_DIR ${eigerdaq_SOURCE_DIR}/eigerfan)
# Set the frameReceiver directory
set(FRAMERECEIVER_DIR ${eigerdaq_SOURCE_DIR}/frameReceiver)
# Set the frameProcessor directory
set(FRAMEPROCESSOR_DIR ${eigerdaq_SOURCE_DIR}/frameProcessor)

# Appends the cmake/modules path inside the MAKE_MODULE_PATH variable which stores the
# directories of additional CMake modules (ie. MacroOutOfSourceBuild.cmake):
set(CMAKE_MODULE_PATH ${eigerdaq_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# Find and add external packages required for application and test
find_package( Boost 1.41.0
	      REQUIRED
	      COMPONENTS program_options system unit_test_framework thread)
find_package(Log4CXX 0.10.0 REQUIRED)
find_package(ZeroMQ 3.2.4 REQUIRED)
find_package(OdinData REQUIRED)

# Add include directory to include path
include_directories(${eigerdaq_SOURCE_DIR}/include)

# Add include directory to common include path
include_directories(${eigerdaq_SOURCE_DIR}/common/include)


# Add the eigerfan subdirectory
add_subdirectory(${EIGERFAN_DIR})
# Add the frameReceiver subdirectory
add_subdirectory(${FRAMERECEIVER_DIR})
# Add the frameProcessor subdirectory
add_subdirectory(${FRAMEPROCESSOR_DIR})

# Add the test subdirectory
add_subdirectory(test)

