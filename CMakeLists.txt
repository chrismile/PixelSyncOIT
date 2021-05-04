cmake_minimum_required (VERSION 3.1)
project (PixelSyncOIT)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)
#MESSAGE(STATUS "CMAKE_MODULE_PATH: " ${CMAKE_MODULE_PATH})

set (CMAKE_CXX_STANDARD 11)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.c)
include_directories(src)

#if(MSVC)
#	set(CMAKE_CXX_FLAGS "-W3 /EHsc")
#elseif(CMAKE_COMPILER_IS_GNUCXX)
#	set(CMAKE_CXX_FLAGS "-Wall")
#elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
#	set(CMAKE_CXX_FLAGS "-Wall")
#endif()

# Prefer GLVND by default when available.
if (${CMAKE_VERSION} VERSION_GREATER "3.11")
	cmake_policy(SET CMP0072 NEW)
endif()

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

set(FPHSA_NAME_MISMATCHED TRUE)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(PNG REQUIRED)
if((${CMAKE_GENERATOR} STREQUAL "MinGW Makefiles") OR (${CMAKE_GENERATOR} STREQUAL "MSYS Makefiles"))
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
	target_link_libraries(PixelSyncOIT mingw32)
endif()
target_link_libraries(PixelSyncOIT SDL2::Main)


cmake_policy(SET CMP0012 NEW)
find_package(OpenMP REQUIRED)
find_package(sgl REQUIRED)
find_package(Boost COMPONENTS system filesystem REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Boost COMPONENTS system filesystem REQUIRED)
find_package(NetCDF REQUIRED)
target_link_libraries(PixelSyncOIT tinyxml2)
target_link_libraries(PixelSyncOIT sgl ${Boost_LIBRARIES} ${OPENGL_LIBRARIES} GLEW::GLEW ${NETCDF_LIBRARIES})

include_directories(${sgl_INCLUDES} ${Boost_INCLUDES} ${OPENGL_INCLUDE_DIRS} ${GLEW_INCLUDES} ${NETCDF_INCLUDES})

if(OPENMP_FOUND)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()
