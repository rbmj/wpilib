cmake_minimum_required(VERSION 2.8)
# Toolchain File for VxWorks

#information needed by cmake:
set(CMAKE_SYSTEM_NAME vxWorks)
set(CMAKE_SYSTEM_VERSION 6) #TODO: get real version number
set(CMAKE_SYSTEM_PROCESSOR PPC)

#system wide settings
include(CMakeForceCompiler)

set(TOOLCHAIN_PREFIX "/usr/local/powerpc-wrs-vxworks")
set(CMAKE_INSTALL_PREFIX ${TOOLCHAIN_PREFIX})
set(TOOLCHAIN_IS_GCCDIST true)
set(WIND_BASE "${TOOLCHAIN_PREFIX}/wind_base")
set(DEPLOYIP 10.6.12.2)

set(VXWORKS_USE_SOFT_FLOAT false)

CMAKE_FORCE_C_COMPILER(powerpc-wrs-vxworks-gcc GNU)
set(NM_EXECUTABLE powerpc-wrs-vxworks-nm)
set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN_PREFIX}")

#set necessary internal cmake flags
set(CMAKE_C_COMPILE_OBJECT "<CMAKE_C_COMPILER> <DEFINES> <SOURCE> -o <OBJECT> <FLAGS>")
set(CMAKE_CXX_COMPILE_OBJECT "<CMAKE_CXX_COMPILER> <DEFINES> <SOURCE> -o <OBJECT> <FLAGS>")
set(CMAKE_C_CREATE_STATIC_LIBRARY "<CMAKE_AR> cr <TARGET> <LINK_FLAGS> <OBJECTS>" "<CMAKE_RANLIB> <TARGET>")
set(CMAKE_CXX_CREATE_STATIC_LIBRARY "<CMAKE_AR> cr <TARGET> <LINK_FLAGS> <OBJECTS>" "<CMAKE_RANLIB> <TARGET>")
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER>  <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_LIBRARY_PATH_FLAG -L)

# (embedded) targets without operating system usually don't support shared libraries
SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
# is this true?  We can add a dependency on another kernel module

# To help the find_xxx() commands, set at least the following so CMAKE_FIND_ROOT_PATH
# works at least for some simple cases:
SET(CMAKE_SYSTEM_INCLUDE_PATH /include )
SET(CMAKE_SYSTEM_LIBRARY_PATH /lib )
SET(CMAKE_SYSTEM_PROGRAM_PATH /bin )

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

#config
if(${TOOLCHAIN_IS_GCCDIST})
	set(SYSTEM_INCLUDES " -I${WIND_BASE}/target/h -I${WIND_BASE}/target/h/wrn/coreip")
else()
	set(SYSTEM_INCLUDES "")
endif()
set(WPILIB_LIBRARY "${WIND_BASE}/vxworks-6.3/target/lib/WPILib.a")
set(WPILIB_INCLUDE_DIR "${WIND_BASE}/vxworks-6.3/target/h/WPILib")
set(CPU PPC603)
set(TOOL_FAMILY gnu)
set(TOOL gnu)
set(ARCH_SPEC " -mcpu=603 -mstrict-align ${SOFT_FLOAT} -mlongcall")

if (${VXWORKS_USE_SOFT_FLOAT})
	set(ARCH_SPEC "${ARCH_SPEC} -msoft_float")
endif()

#Set CFLAGS, LDFLAGS
set(VXWORKS_COMPILE_FLAGS " ${ARCH_SPEC} -ansi -Wall ${SYSTEM_INCLUDES} -DCPU=${CPU} -DTOOL_FAMILY=${TOOL_FAMILY} -DTOOL=${TOOL} -D_WRS_KERNEL")
set(VXWORKS_DKM_LINK_FLAGS " -r -nostdlib -Wl,-X") #should be equivalent to -nostdlib -Wl,-X,-r but I don't want to mess...

macro(add_vxworks_library TGTNAME SOURCES INCLUDES LIBS)
	add_library(${TGTNAME} STATIC ${SOURCES})
	set_target_properties(${TGTNAME} PROPERTIES COMPILE_FLAGS "${VXWORKS_COMPILE_FLAGS} -nostdlib")
	target_link_libraries(${TGTNAME} ${LIBS})
	include_directories(${INCLUDES})
endmacro()

#Add a VxWorks Downloadable Kernel Module Target
macro(add_vxworks_dkm TGTNAME SOURCES INCLUDES LIBS)
	#Compile and link in all static libraries
	add_executable(${TGTNAME}_PartialImage ${SOURCES})
	set_target_properties(${TGTNAME}_PartialImage PROPERTIES SUFFIX .o)
	set_target_properties(${TGTNAME}_PartialImage PROPERTIES COMPILE_FLAGS "${VXWORKS_COMPILE_FLAGS}")
	set_target_properties(${TGTNAME}_PartialImage PROPERTIES LINK_FLAGS "${VXWORKS_DKM_LINK_FLAGS}")
	target_link_libraries(${TGTNAME}_PartialImage ${LIBS})
	include_directories(${INCLUDES})

	#add circular dependency on purpose to tell CMake to shut up that it doesn't know where ${TGTNAME}_PartialImage.o comes from. Bug?
	add_custom_command(
		OUTPUT ${TGTNAME}_PartialImage.o
		COMMAND echo #reasonably cross platform noop
		DEPENDS ${TGTNAME}_PartialImage
	)

	#VxWorks DKMs have no code to load static constructors/destructors for C++, so the next creates symbol tables that the kernel
	#can read and execute the static ctors/dtors at load time (AFAIK).  This needs to be done AFTER static libraries are linked
	#in as these libraries may also contain static ctors/dtors that need to be loaded as well.
	add_custom_command(
		OUTPUT ctdt.c
		COMMAND ${NM_EXECUTABLE} ${TGTNAME}_PartialImage.o | tclsh "${WIND_BASE}/host/resource/hutils/tcl/munch.tcl" -c ppc > ctdt.c
		DEPENDS ${TGTNAME}_PartialImage.o
	)

	#Add an executable with both the original object code and the descriptor tables
	add_executable(${TGTNAME} ctdt.c ${TGTNAME}_PartialImage.o)
	set_target_properties(${TGTNAME} PROPERTIES SUFFIX .out)
	set_target_properties(${TGTNAME} PROPERTIES COMPILE_FLAGS "${VXWORKS_COMPILE_FLAGS}")
	#use the link script provided
	set_target_properties(${TGTNAME} PROPERTIES LINK_FLAGS "${VXWORKS_DKM_LINK_FLAGS} -T ${WIND_BASE}/target/h/tool/gnu/ldscripts/link.OUT")

	#install
	if(${UNIX} OR ${MSYS})
		install(CODE "execute_process(COMMAND \"${TOOLCHAIN_PREFIX}/deploy.sh\" \"${CMAKE_BINARY_DIR}/${TGTNAME}.out\" ${DEPLOYIP})")
	else()
		install(CODE "execute_process(COMMAND \"${TOOLCHAIN_PREFIX}\\deploy.bat\" \"${CMAKE_BINARY_DIR}\\${TGTNAME}.out\" ${DEPLOYIP})")
	endif()
endmacro()

#project specific
project(WPI)
file(GLOB_RECURSE WPI_SOURCES *.cpp)
set(WPI_INCLUDES ${CMAKE_SOURCE_DIR})
set(WPI_LIBS "") #libstdc++.a)
add_vxworks_library(WPILib "${WPI_SOURCES}" "${WPI_INCLUDES}" "${WPI_LIBS}")
install(TARGETS WPILib LIBRARY DESTINATION lib ARCHIVE DESTINATION lib RUNTIME DESTINATION bin)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/ DESTINATION include/WPILib FILES_MATCHING PATTERN *.h)


