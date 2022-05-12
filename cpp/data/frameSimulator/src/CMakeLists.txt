
set(CMAKE_INCLUDE_CURRENT_DIR on)
ADD_DEFINITIONS(-DBOOST_TEST_DYN_LINK)

include_directories(${FRAMESIMULATOR_DIR}/include ${ODINDATA_INCLUDE_DIRS}
	${Boost_INCLUDE_DIRS} ${LOG4CXX_INCLUDE_DIRS}/.. ${ZEROMQ_INCLUDE_DIRS})

file(GLOB LIB_SOURCES EigerFrameSimulatorPlugin.cpp)

# Add library for eiger plugin
add_library(EigerFrameSimulatorPlugin SHARED ${LIB_SOURCES})

install(TARGETS EigerFrameSimulatorPlugin LIBRARY DESTINATION lib)
