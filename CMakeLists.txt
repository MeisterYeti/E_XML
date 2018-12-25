# ----------------------------------------------------------------------------------
#	This file is part of the E_XML Library
#	Copyright (C) 2018 Sascha Brandt <sascha@brandt.graphics>
#	
#	This library is subject to the terms of the Mozilla Public License, v. 2.0.
#	You should have received a copy of the MPL along with this library; see the 
#	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
# ----------------------------------------------------------------------------------

# required by CMake
cmake_minimum_required(VERSION 3.1.0)

project(E_XML)

# ensures that the project is build with C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add C++ source files to the project
add_library(${PROJECT_NAME} SHARED
	E_MicroXMLReader.cpp
	MicroXML.cpp
)

# pass the project name to the Main.cpp file in form of the define 'LIBRARY_NAME'
set_source_files_properties(src/Main.cpp PROPERTIES COMPILE_DEFINITIONS LIBRARY_NAME="${PROJECT_NAME}")

# make sure that the built .dll or .so file is placed into the 'build' or 'bin' directory
set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# --- Set up dependencies to the PADrend libraries ---

# add ./cmake to module search path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# find EScript library
find_package(EScript)
if(ESCRIPT_FOUND)
	target_include_directories(${PROJECT_NAME} PUBLIC ${ESCRIPT_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} LINK_PUBLIC ${ESCRIPT_LIBRARIES})
endif()
