project(file_parsing)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")
cmake_minimum_required(VERSION 2.8)

# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${opengl_learn_SOURCE_DIR}/cmake/")

# Add _DEBUG_ define
if(CMAKE_BUILD_TYPE MATCHES "Debug")
   add_definitions(-D_DEBUG_)
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

aux_source_directory(src SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME})
