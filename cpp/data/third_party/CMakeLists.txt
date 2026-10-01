add_subdirectory(dectris/stream_v2/examples EXCLUDE_FROM_ALL)

set_property(TARGET stream2 PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET tinycbor PROPERTY POSITION_INDEPENDENT_CODE ON)
