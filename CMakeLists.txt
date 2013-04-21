cmake_minimum_required(VERSION 2.8)

project(WPILib)

set(CMAKE_CXX_FLAGS "-g -Og -std=c++11 -Wall -fpermissive")

set(HEADER_DIR include/WPILib)
set(LIB_NAME wpiLib)
set(LIB_DIR lib)
set(VERSION "2013.4")

configure_file(WPILibConfig.cmake.in ${PROJECT_BINARY_DIR}/WPILibConfig.cmake @ONLY)
configure_file(WPILibConfigVersion.cmake.in ${PROJECT_BINARY_DIR}/WPILibConfigVersion.cmake @ONLY)

file(GLOB_RECURSE WPI_SOURCES *.cpp)
set(WPI_INCLUDES ${CMAKE_SOURCE_DIR})
set(WPI_LIBS "")
add_library(${LIB_NAME} ${WPI_SOURCES})
include_directories(${WPI_INCLUDES})
target_link_libraries(${LIB_NAME} ${WPI_LIBS})
install(TARGETS ${LIB_NAME} 
	LIBRARY DESTINATION ${LIB_DIR} 
	ARCHIVE DESTINATION ${LIB_DIR} 
	RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/ 
	DESTINATION ${HEADER_DIR}
	FILES_MATCHING
		PATTERN *.h
		PATTERN *.cpp
		PATTERN ".git" EXCLUDE
		PATTERN ".settings" EXCLUDE
		PATTERN "${HEADER_DIR}" EXCLUDE #prevent infinite recurse case
)
install(FILES
   "${PROJECT_BINARY_DIR}/WPILibConfig.cmake"
   "${PROJECT_BINARY_DIR}/WPILibConfigVersion.cmake"
   DESTINATION "share/WPILib/cmake/"
)
