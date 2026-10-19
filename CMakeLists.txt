cmake_minimum_required(VERSION 3.20)
project(KerrBlackHole CXX C)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenGL REQUIRED)

find_package(glm CONFIG QUIET)
if(NOT TARGET glm::glm)
	FetchContent_Declare(
		glm
		GIT_REPOSITORY https://github.com/g-truc/glm.git
		GIT_TAG 1.0.1
	)
	FetchContent_MakeAvailable(glm)
endif()

find_package(glfw3 CONFIG QUIET)
if(NOT TARGET glfw)
	set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
	FetchContent_Declare(
		glfw
		GIT_REPOSITORY https://github.com/glfw/glfw.git
		GIT_TAG 3.4
	)
	FetchContent_MakeAvailable(glfw)
endif()

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui" CACHE PATH "Path to ImGui sources")

set(IMGUI_SOURCES "")
if(EXISTS "${IMGUI_DIR}/imgui.cpp")
	message(STATUS "Using local ImGui at ${IMGUI_DIR}")
	set(IMGUI_SOURCES
		${IMGUI_DIR}/imgui.cpp
		${IMGUI_DIR}/imgui_draw.cpp
		${IMGUI_DIR}/imgui_tables.cpp
		${IMGUI_DIR}/imgui_widgets.cpp
		${IMGUI_DIR}/imgui_demo.cpp
		${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
		${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	)
	add_library(imgui STATIC ${IMGUI_SOURCES})
	target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
	target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
	set(IMGUI_TARGET imgui)
else()
	find_package(imgui CONFIG QUIET)
	if(TARGET imgui::imgui)
		set(IMGUI_TARGET imgui::imgui)
	else()
		FetchContent_Declare(
			imgui
			GIT_REPOSITORY https://github.com/ocornut/imgui.git
			GIT_TAG v1.92.2
		)
		FetchContent_Populate(imgui)
		set(IMGUI_DIR "${imgui_SOURCE_DIR}" CACHE PATH "Path to ImGui sources" FORCE)
		set(IMGUI_SOURCES
			${IMGUI_DIR}/imgui.cpp
			${IMGUI_DIR}/imgui_draw.cpp
			${IMGUI_DIR}/imgui_tables.cpp
			${IMGUI_DIR}/imgui_widgets.cpp
			${IMGUI_DIR}/imgui_demo.cpp
			${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
			${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
		)
		add_library(imgui STATIC ${IMGUI_SOURCES})
		target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
		target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
		set(IMGUI_TARGET imgui)
	endif()
endif()

add_executable(kerr_bh
	src/main.cpp
	src/glad.c
	src/shader.cpp
	src/camera.cpp
	src/metric.cpp
	src/renderer.cpp
	src/audio.cpp
)

set_target_properties(kerr_bh PROPERTIES OUTPUT_NAME "kerrblackhole")

target_include_directories(kerr_bh PRIVATE
	src
	third_party
)

target_link_libraries(kerr_bh PRIVATE
	OpenGL::GL
	glfw
	glm::glm
	${IMGUI_TARGET}
)

if(MSVC)
	target_sources(kerr_bh PRIVATE src/version.rc)
elseif(WIN32)
	target_link_libraries(kerr_bh PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/version.res")
endif()

target_compile_definitions(kerr_bh PRIVATE GLFW_INCLUDE_NONE)

if(MSVC)
	target_compile_options(kerr_bh PRIVATE /W3 /permissive- /Zc:__cplusplus)
	target_compile_options(kerr_bh PRIVATE $<$<CONFIG:Release>:/O2 /Oi /Ot /GL /arch:AVX2 /fp:fast /DNDEBUG>)
	target_link_options(kerr_bh PRIVATE $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
else()
	target_compile_options(kerr_bh PRIVATE -Wall -Wextra -Wno-unused-parameter)
	target_compile_options(kerr_bh PRIVATE $<$<CONFIG:Release>:-O3 -march=native -ffast-math -fno-finite-math-only -funroll-loops -DNDEBUG>)
	if(NOT APPLE)
		target_link_options(kerr_bh PRIVATE $<$<CONFIG:Release>:-Wl,-O1>)
	endif()
endif()

if(UNIX AND NOT APPLE)
	target_link_libraries(kerr_bh PRIVATE dl pthread m)
endif()

target_compile_definitions(kerr_bh PRIVATE
	GLM_ENABLE_EXPERIMENTAL
	GLM_FORCE_INLINE
	SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders"
	ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)