set(EIGERFAN_DIR ${DATA_DIR}/eigerfan)
set(FRAMEPROCESSOR_DIR ${DATA_DIR}/frameProcessor)
set(FRAMESIMULATOR_DIR ${DATA_DIR}/frameSimulator)
set(TEST_DIR ${DATA_DIR}/test)

set(STREAM2_DIR ${DATA_DIR}/third_party/dectris/stream_v2/examples)
set(STREAM2_LIBRARY stream2) # This is not ideal as it has to match stream_v2 CMakeLists definition

include_directories(${DATA_DIR}/include)
include_directories(${DATA_DIR}/common/include)

add_subdirectory(third_party EXCLUDE_FROM_ALL)
add_subdirectory(${EIGERFAN_DIR})
add_subdirectory(${FRAMEPROCESSOR_DIR})
add_subdirectory(${FRAMESIMULATOR_DIR})
add_subdirectory(${TEST_DIR})
