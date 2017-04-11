#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Tries to find OdinData headers and libraries.
#
# Usage of this module as follows:
#
#  find_package(OdinData)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  ODINDATA_ROOT_DIR  Set this variable to the root installation of
#                  	  OdinData Library if the module has problems finding
#                     the proper installation path.
#
# Variables defined by this module:
#
#  ODINDATA_FOUND              System has OdinData libs/headers
#  ODINDATA_LIBRARIES          The OdinData libraries
#  ODINDATA_INCLUDE_DIRS       The location of OdinData headers

message ("\nLooking for odinData headers and libraries")

if (ODINDATA_ROOT_DIR) 
    message (STATUS "Root dir: ${ODINDATA_ROOT_DIR}")
endif ()

find_package(PkgConfig)
IF (PkgConfig_FOUND)
    message("using pkgconfig")
    pkg_check_modules(PC_ODINDATA odinData)
ENDIF(PkgConfig_FOUND)

set(ODINDATA_DEFINITIONS ${PC_ODINDATA_CFLAGS_OTHER})

find_path(ODINDATA_INCLUDE_DIR 
	NAMES
		ClassLoader.h
    PATHS 
		${ODINDATA_ROOT_DIR}/include
        ${PC_ODINDATA_INCLUDEDIR} 
        ${PC_ODINDATA_INCLUDE_DIRS}
)

find_library(ODINDATA_LIBRARY
    NAMES 
		odinData
    PATHS 
		${ODINDATA_ROOT_DIR}/lib 
        ${PC_ODINDATA_LIBDIR} 
        ${PC_ODINDATA_LIBRARY_DIRS}         
)
			 
include(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set ODINDATA_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(ODINDATA 
	DEFAULT_MSG
    ODINDATA_LIBRARY 
	ODINDATA_INCLUDE_DIR
)

mark_as_advanced(ODINDATA_INCLUDE_DIR ODINDATA_LIBRARY)

if (ODINDATA_FOUND)
	set(ODINDATA_INCLUDE_DIRS ${ODINDATA_INCLUDE_DIR})
	set(ODINDATA_LIBRARIES ${ODINDATA_LIBRARY})
	
    get_filename_component(ODINDATA_LIBRARY_DIR ${ODINDATA_LIBRARY} PATH)
    get_filename_component(ODINDATA_LIBRARY_NAME ${ODINDATA_LIBRARY} NAME_WE)
    
    mark_as_advanced(ODINDATA_LIBRARY_DIR ODINDATA_LIBRARY_NAME)

	message (STATUS "Include directories: ${ODINDATA_INCLUDE_DIRS}") 
	message (STATUS "Libraries: ${ODINDATA_LIBRARIES}") 
endif ()
