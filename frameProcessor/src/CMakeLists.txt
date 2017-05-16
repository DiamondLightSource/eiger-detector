set(CMAKE_INCLUDE_CURRENT_DIR on)

include_directories(${FRAMEPROCESSOR_DIR}/include ${ODINDATA_INCLUDE_DIRS} ${HDF5_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${LOG4CXX_INCLUDE_DIRS}/.. ${ZEROMQ_INCLUDE_DIRS})

# Add library for eiger process plugin
add_library(EigerProcessPlugin SHARED EigerProcessPlugin.cpp)
target_link_libraries(EigerProcessPlugin ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES} ${HDF5_LIBRARIES} ${HDF5HL_LIBRARIES} ${COMMON_LIBRARY})

set(CMAKE_BUILD_TYPE Debug)
