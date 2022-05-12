set(EIGERFAN_DIR ${DATA_DIR}/eigerfan)
set(FRAMERECEIVER_DIR ${DATA_DIR}/frameReceiver)
set(FRAMEPROCESSOR_DIR ${DATA_DIR}/frameProcessor)
set(FRAMESIMULATOR_DIR ${DATA_DIR}/frameSimulator)
set(TEST_DIR ${DATA_DIR}/test)

include_directories(${DATA_DIR}/include)
include_directories(${DATA_DIR}/common/include)

add_subdirectory(${EIGERFAN_DIR})
add_subdirectory(${FRAMERECEIVER_DIR})
add_subdirectory(${FRAMEPROCESSOR_DIR})
add_subdirectory(${FRAMESIMULATOR_DIR})
add_subdirectory(${TEST_DIR})
