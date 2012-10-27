cmake_minimum_required(VERSION 2.8)
# Toolchain File for VxWorks

project(WPILib)

set(CPACK_GENERATOR DEB;RPM)
set(CPACK_PACKAGE_NAME wpilib)
set(CPACK_PACKAGE_VERSION 0.1)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE all)
set(CPACK_RPM_PACKAGE_ARCHITECTURE noarch)
set(CPACK_PACKAGE_CONTACT rbmj@verizon.net)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C++ Libraries for FRC")

include(CPack)

file(GLOB_RECURSE WPI_SOURCES *.cpp)
set(WPI_INCLUDES ${CMAKE_SOURCE_DIR})
set(WPI_LIBS "")
add_library(wpilib ${WPI_SOURCES})
include_directories(${WPI_INCLUDES})
target_link_libraries(wpilib ${WPI_LIBS})
install(TARGETS wpilib LIBRARY DESTINATION lib ARCHIVE DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/ DESTINATION include/wpilib
	FILES_MATCHING
		PATTERN *.h
		PATTERN ".git" EXCLUDE
		PATTERN ".settings" EXCLUDE
)
