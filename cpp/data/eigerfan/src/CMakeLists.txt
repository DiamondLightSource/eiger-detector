
set(CMAKE_CXX_FLAGS "-pthread")

set(CMAKE_INCLUDE_CURRENT_DIR on)

include_directories(
    ${EIGERFAN_DIR}/include
    ${EIGERFAN_DIR}/eigerfan/src
    ${EIGERFAN_DIR}/common/include
    ${Boost_INCLUDE_DIRS}
    ${LOG4CXX_INCLUDE_DIRS}/..
    ${ZEROMQ_INCLUDE_DIRS}
    ${STREAM2_DIR}
)

file(GLOB APP_SOURCES *.cpp)

add_executable(eigerfan ${APP_SOURCES})

target_link_libraries(
    eigerfan
    ${Boost_LIBRARIES}
    ${LOG4CXX_LIBRARIES}
    ${ZEROMQ_LIBRARIES}
    ${STREAM2_LIBRARY}
)

install(
    TARGETS eigerfan
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
