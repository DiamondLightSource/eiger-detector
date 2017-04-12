set(CMAKE_INCLUDE_CURRENT_DIR on)

include_directories(${FRAMERECEIVER_DIR}/include ${ODINDATA_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${LOG4CXX_INCLUDE_DIRS}/.. ${ZEROMQ_INCLUDE_DIRS})

# Add library for Eiger decoder
add_library(EigerFrameDecoder SHARED EigerFrameDecoder.cpp EigerFrameDecoderLib.cpp)
target_link_libraries(EigerFrameDecoder ${ODINDATA_LIBRARIES} ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES})

set(CMAKE_BUILD_TYPE Debug)
