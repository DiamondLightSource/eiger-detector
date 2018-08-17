
set(CMAKE_INCLUDE_CURRENT_DIR on)

include_directories(${EIGERFAN_DIR}/include ${Boost_INCLUDE_DIRS} ${LOG4CXX_INCLUDE_DIRS}/.. ${ZEROMQ_INCLUDE_DIRS})

file(GLOB APP_SOURCES *.cpp)

add_executable(streamSender ${APP_SOURCES})

target_link_libraries(streamSender ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES})

install(TARGETS streamSender
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

