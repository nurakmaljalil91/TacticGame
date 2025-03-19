cmake_minimum_required(VERSION 3.31)
project(TacticGame)

set(CMAKE_CXX_STANDARD 20)

# set glfw path
set(GLFW_PATH vendors/glfw)
include_directories(${GLFW_PATH}/include)
link_directories(${GLFW_PATH}/lib)

# set GLM path
set(GLM_PATH vendors/glm)
include_directories(${GLM_PATH}/include)

# set spdlog path
set(spdlog_PATH vendors/spdlog)
include_directories(${spdlog_PATH}/include)
link_directories(${spdlog_PATH}/lib)

# Set CMAKE_PREFIX_PATH to include spdlog_DIR
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${spdlog_PATH})

find_package(spdlog REQUIRED)

# set glad path
set(GLAD_PATH vendors/glad)
include_directories(${GLAD_PATH}/include)

# set rapidjson path
set(RAPIDJSON_PATH vendors/rapidjson)
include_directories(${RAPIDJSON_PATH}/include)

# set stb_image path
set(STB_IMAGE_PATH vendors/stb)
include_directories(${STB_IMAGE_PATH})

# add resources folder
file(COPY src/resources DESTINATION ${CMAKE_BINARY_DIR})

add_executable(TacticGame
        src/main.cpp
        ${GLAD_PATH}/src/glad.c
        src/Camera.cpp
        src/Camera.h
        src/Shader.cpp
        src/Shader.h
)

# add link libraries
#target_link_libraries(TacticGame PRIVATE glfw3 opengl32)
target_link_libraries(TacticGame PRIVATE glfw3 opengl32 spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
