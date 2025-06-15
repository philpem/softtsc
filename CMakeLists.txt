# SoftTSC - Software MPT1327 Trunking System Controller
# Copyright (C) 2013-2014 Paul Banks (http://paulbanks.org)
# 
# This file is part of SoftTSC
#
# SoftTSC is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# SoftTSC is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with SoftTSC.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required (VERSION 2.8)

# Set project name and version
project(softtsc)
set( ${CMAKE_PROJECT_NAME}_VERSION_MAJOR 0 )
set( ${CMAKE_PROJECT_NAME}_VERSION_MINOR 1 )
set( ${CMAKE_PROJECT_NAME}_VERSION_BUGFIX 0 )

# Add cmake finders
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(PkgConfig REQUIRED)
find_package(GLIB2 REQUIRED)
find_package(JACK REQUIRED)

include_directories( include 
                     ${GLIB2_INCLUDE_DIRS}
	             ${JACK_INCLUDE_DIRS}  )

add_subdirectory(mskmodem)
add_subdirectory(module)

# Packaging
set(CPACK_SOURCE_GENERATOR TGZ)
set(CPACK_SOURCE_IGNORE_FILES
  "~$"
  "\\\\.gitignore$"
  "^${PROJECT_SOURCE_DIR}/.git/"
  "^${PROJECT_SOURCE_DIR}/doc/"
  "^${PROJECT_SOURCE_DIR}/build/"
  "^${PROJECT_SOURCE_DIR}/module/tsc-debug.log"
  "\\\\.pyc$"
  "__pycache__/$"
)
include(CPack)

