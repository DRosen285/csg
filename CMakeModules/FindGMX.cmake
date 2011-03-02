# - Find libgmx
# Find the native libgmx headers and libraries.
#
#  GMX_INCLUDE_DIRS - where to find gromacs/tpxio.h, etc.
#  GMX_LIBRARIES    - List of libraries when using libgmx.
#  GMX_FOUND        - True if libgmx found.
#  GMX_PKG          - Whether we are using libgmx oder libgmx_d
#  GMX_DEFINITIONS  - Extra definies needed by libgmx
#
# Copyright 2009-2011 The VOTCA Development Team (http://www.votca.org)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

find_package(PkgConfig)

pkg_check_modules(PC_GMX libgmx_d)
set(GMX_PKG "libgmx_d")
find_path(GMX_INCLUDE_DIR gromacs/tpxio.h HINTS ${PC_GMX_INCLUDE_DIRS})
foreach (LIB ${PC_GMX_LIBRARIES})
  find_library(GMX_${LIB} NAMES ${LIB} HINTS ${PC_GMX_LIBRARY_DIRS} )
  list(APPEND GMX_LIBRARY ${GMX_${LIB}})
endforeach(LIB)
if ("${GMX_LIBRARY}" STREQUAL "NOTFOUND" OR "${GMX_INCLUDE_DIR}" STREQUAL "NOTFOUND")
  pkg_check_modules(PC_GMX libgmx)
  find_path(GMX_INCLUDE_DIR gromacs/tpxio.h HINTS ${PC_GMX_INCLUDE_DIRS})
  foreach (LIB ${PC_GMX_LIBRARIES})
    find_library(GMX_${LIB} NAMES ${LIB} HINTS ${PC_GMX_LIBRARY_DIRS} )
    list(APPEND GMX_LIBRARY ${GMX_${LIB}})
  endforeach(LIB)
  set(GMX_PKG "libgmx")
endif ("${GMX_LIBRARY}" STREQUAL "NOTFOUND" OR "${GMX_INCLUDE_DIR}" STREQUAL "NOTFOUND")

foreach(DEF ${PC_GMX_CFLAGS_OTHER})
  if (${DEF} MATCHES "^-D")
    list(APPEND GMX_DEFINITIONS ${DEF})
  endif (${DEF} MATCHES "^-D")
endforeach(DEF)

set(GMX_LIBRARIES ${GMX_LIBRARY} )
set(GMX_INCLUDE_DIRS ${GMX_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set GMX_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(GMX DEFAULT_MSG GMX_LIBRARY GMX_INCLUDE_DIR )

mark_as_advanced(GMX_INCLUDE_DIR GMX_LIBRARY GMX_DEFINITIONS )
