cmake_minimum_required(VERSION 3.8)
project(Vulkanite)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(libraries/include)

if(EXISTS "libraries/include/reactphysics3d")
    add_definitions(-DREACT_PHYSICS_3D)
endif()

set(SOURCE_FILES src/main.cpp src/window.cpp src/window.h src/VulkanInterface.cpp src/VulkanInterface.h src/logger.cpp src/logger.h src/Camera.cpp src/Camera.h src/Transform.cpp src/Transform.h src/KeyboardInput.cpp src/KeyboardInput.h src/Model.cpp src/Model.h src/Texture.cpp src/Texture.h src/Mesh.cpp src/Mesh.h src/GenericThreadPool.cpp src/GenericThreadPool.h src/SpecificThreadPool.cpp src/SpecificThreadPool.h src/ParticleSystem.cpp src/ParticleSystem.h src/ImageAttachment.h src/Terrain.cpp src/Terrain.h)
add_executable(Vulkanite ${SOURCE_FILES})

find_package(Vulkan REQUIRED)
target_link_libraries(Vulkanite ${Vulkan_LIBRARIES})
include_directories(${Vulkan_INCLUDE_DIR})

target_link_libraries(Vulkanite ${CMAKE_CURRENT_SOURCE_DIR}/libraries/lib/glfw3dll.lib)
target_link_libraries(Vulkanite ${CMAKE_CURRENT_SOURCE_DIR}/libraries/lib/libassimp.dll.a)