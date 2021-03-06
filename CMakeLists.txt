# Copyright (c) 2012 CNRS
# Author: Florent Lamiraux
#
# This file is part of hpp-manipulation-corba.
# hpp-manipulation-corba is free software: you can redistribute it
# and/or modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation, either version
# 3 of the License, or (at your option) any later version.
#
# hpp-manipulation-corba is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Lesser Public License for more details.  You should have
# received a copy of the GNU Lesser General Public License along with
# hpp-manipulation-corba.  If not, see
# <http://www.gnu.org/licenses/>.

# Requires at least CMake 2.6 to configure the package.
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(CXX_DISABLE_WERROR true)

INCLUDE(cmake/base.cmake)
INCLUDE(cmake/idl.cmake)
INCLUDE(cmake/python.cmake)

SET(PROJECT_NAME hpp-manipulation-corba)
SET(PROJECT_DESCRIPTION "Corba server for manipulation planning")
SET(PROJECT_URL "")

SET(CLIENT_ONLY FALSE CACHE BOOL "Set to true to install the client only")

SET(CUSTOM_HEADER_DIR hpp/corbaserver/manipulation)

SETUP_PROJECT ()

CONFIG_FILES (include/hpp/corbaserver/manipulation/doc.hh)

SET(${PROJECT_NAME}_HEADERS
  include/hpp/corbaserver/manipulation/client.hh
  )
IF (NOT CLIENT_ONLY)
  SET(${PROJECT_NAME}_HEADERS
    include/hpp/corbaserver/manipulation/server.hh
    include/hpp/corbaserver/manipulation/fwd.hh
    ${CMAKE_BINARY_DIR}/include/hpp/corbaserver/manipulation/doc.hh
    ${${PROJECT_NAME}_HEADERS}
    )
ENDIF (NOT CLIENT_ONLY)

# Activate hpp-util logging if requested
SET (HPP_DEBUG FALSE CACHE BOOL "trigger hpp-util debug output")
IF (HPP_DEBUG)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHPP_DEBUG")
ENDIF()

# Ask Doxygen to create a tree view in html documentation
SET(DOXYGEN_TREEVIEW "NO" CACHE STRING "Set to YES to generate a tree view in the html documentation")

ADD_REQUIRED_DEPENDENCY("hpp-corbaserver >= 3")
ADD_REQUIRED_DEPENDENCY("hpp-wholebody-step-corba >= 3 ")
IF (NOT CLIENT_ONLY)
  ADD_DOC_DEPENDENCY("hpp-core >= 3")
  ADD_REQUIRED_DEPENDENCY("hpp-manipulation")
  ADD_REQUIRED_DEPENDENCY("hpp-manipulation-urdf")
ENDIF (NOT CLIENT_ONLY)
ADD_REQUIRED_DEPENDENCY("omniORB4 >= 4.1.4")

ADD_SUBDIRECTORY(src)

IF (NOT CLIENT_ONLY)
  PKG_CONFIG_APPEND_LIBS(${PROJECT_NAME})
ENDIF (NOT CLIENT_ONLY)

SETUP_PROJECT_FINALIZE()
