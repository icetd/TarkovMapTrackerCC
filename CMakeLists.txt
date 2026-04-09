cmake_minimum_required(VERSION 3.10)
project(TarkovMapTrackerCC)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
add_subdirectory(thirdparty/glfw)
add_subdirectory(thirdparty/glm)

add_definitions(-w)
add_definitions(-DPROTOBUF_USE_DLLS)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_BUILD_TYPE Release)
set (CMAKE_INCLUDE_CURRENT_DIR ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set (SOURCE_DIR  "${CMAKE_CURRENT_LIST_DIR}/src") 
set (INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/src")


set (GLFW_INC			"${CMAKE_CURRENT_LIST_DIR}/thirdparty/glfw/include")
set (GLAD_SRC			"${CMAKE_CURRENT_LIST_DIR}/thirdparty/glad/src")
set (GLAD_INC			"${CMAKE_CURRENT_LIST_DIR}/thirdparty/glad/include")
set (STB_IMAGE_INC		"${CMAKE_CURRENT_LIST_DIR}/thirdparty/stb_image")
set (GLM_INC			"${CMAKE_CURRENT_LIST_DIR}/thirdparty/glm")
set (IMPLOT_SRC			"${CMAKE_CURRENT_LIST_DIR}/thirdparty/implot")
set (IMGUI_SRC			"${CMAKE_CURRENT_LIST_DIR}/thirdparty/imgui")
set (JSON_INC			"${CMAKE_CURRENT_LIST_DIR}/thirdparty/json/include")

file(GLOB_RECURSE SRC_FILES
    "${SOURCE_DIR}/*.c*"
    "${SOURCE_DIR}/Graphics/Buffers/*.c*"
    "${SOURCE_DIR}/Graphics/Renderer/*.c*"
    "${SOURCE_DIR}/Ui/*.c*"
    "${SOURCE_DIR}/Core/*.c*"
	"${GLAD_SRC}/*c*"
	"${IMGUI_SRC}/*.c*"
	"${STB_IMAGE_INC}/*.h"
)

file (GLOB_RECURSE INC_FILES
    "${INCLUDE_DIR}/*.h"
	"${IMGUI_SRC}/*.h"
	"${IMPLOT_SRC}/*.h"
	"${GLAD_INC}/*h"
)

file (GLOB_RECURSE RES_FILES
    "${CMAKE_SOURCE_DIR}/res/*.rc"
)

source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${SRC_FILES})
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${INC_FILES})
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${RES_FILES})
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${PROTO_FILES})
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${PROTO_SRCS})
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${PROTO_HDRS})

link_directories(
)

include_directories(
	${INCLUDE_DIR}
	${GLFW_INC}
	${GLAD_INC}
	${STB_IMAGE_INC}
	${GLM_INC}
	${YAML_CPP_INC}
	${IMGUI_SRC}
    ${JSON_INC}
) 

list(APPEND EXTRA_LIBS
	ws2_32 opengl32 glfw User32 Gdi32 Shell32 SetupAPI
)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${INC_FILES} ${RES_FILES})

# Add the target includes for MY_PROJECT 
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${SOURCE_DIR})


#===================== LINKING LIBRARIES =======================#
target_link_libraries(${PROJECT_NAME} ${EXTRA_LIBS})

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/output)

file(COPY ${CMAKE_SOURCE_DIR}/res DESTINATION ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})
file(COPY ${CMAKE_SOURCE_DIR}/configs DESTINATION ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})