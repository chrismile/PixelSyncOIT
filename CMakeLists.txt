cmake_minimum_required (VERSION 3.1)
project (PixelSyncOIT)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)
#MESSAGE(STATUS "CMAKE_MODULE_PATH: " ${CMAKE_MODULE_PATH})

set (CMAKE_CXX_STANDARD 11)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.c)
include_directories(src)

#make VERBOSE=1

option(USE_RAYTRACING "Build Ray Tracing Renderer with OSPRay" OFF)

if(USE_RAYTRACING)
	find_package(ospray REQUIRED)
	include_directories(${OSPRAY_INCLUDE_DIRS})
	add_definitions(-DUSE_RAYTRACING)
else()
	list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/Raytracing/OIT_RayTracing.cpp)
	list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/Raytracing/RTRenderBackend.cpp)
endif()

IF(WIN32)
	add_executable(PixelSyncOIT WIN32 ${SOURCES})
ELSE()
	add_executable(PixelSyncOIT ${SOURCES})
ENDIF()

if(USE_RAYTRACING)
	target_link_libraries(PixelSyncOIT ${OSPRAY_LIBRARIES})
endif()

IF(WIN32)
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
	target_link_libraries(PixelSyncOIT mingw32 SDL2main SDL2)
ELSE()
	target_link_libraries(PixelSyncOIT SDL2)
ENDIF()


cmake_policy(SET CMP0012 NEW)
find_package(OpenMP REQUIRED)
find_package(SGL REQUIRED)
find_package(Boost COMPONENTS system filesystem REQUIRED)
find_package (NetCDF REQUIRED)
IF(WIN32)
	target_link_libraries(PixelSyncOIT boost_system-mt)
	target_link_libraries(PixelSyncOIT boost_filesystem-mt)
	target_link_libraries(PixelSyncOIT opengl32)
	target_link_libraries(PixelSyncOIT glew32)
ELSE()
	target_link_libraries(PixelSyncOIT boost_system)
	target_link_libraries(PixelSyncOIT boost_filesystem)
	target_link_libraries(PixelSyncOIT GL)
	target_link_libraries(PixelSyncOIT GLEW)
ENDIF()
target_link_libraries(PixelSyncOIT tinyxml2)
target_link_libraries(PixelSyncOIT ${SGL_LIBRARIES} ${NETCDF_LIBRARIES})

include_directories(${NETCDF_INCLUDES})

if(OPENMP_FOUND)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()
