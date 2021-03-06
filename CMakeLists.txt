cmake_minimum_required(VERSION 3.7)

project(node_omx)
set(CMAKE_BUILD_TYPE Debug)
IF(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
ENDIF(CMAKE_COMPILER_IS_GNUCC)
set(CMAKE_CXX_STANDARD 14)

include(NodeJS.cmake)
nodejs_init()

if(EXISTS "/opt/vc/include/bcm_host.h" OR EXISTS "/usr/include/bcm_host.h")
    add_definitions(-DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM)
    link_directories(/opt/vc/lib)
    add_subdirectory( libomx )
    add_nodejs_module(${PROJECT_NAME} node-omx.cpp Player.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE libomx)
    add_executable(testApp test.cpp)
    target_link_libraries(testApp PRIVATE libomx)
else()
    add_nodejs_module(${PROJECT_NAME} node-omx-dummy.cpp Player-dummy.cpp)
endif()
