# CMake entry point.

cmake_minimum_required (VERSION 3.8)

project (vulcain)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DFS_BASE_PATH="${CMAKE_SOURCE_DIR}/resources/")
add_compile_options("$<$<CONFIG:DEBUG>:-DDEBUG>")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd26812")

set (CMAKE_CXX_STANDARD 20)
set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(lz4 REQUIRED)
find_package(tinyxml2 CONFIG REQUIRED)

add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GL3W)

if ( ENABLE_PROFILING )
	add_compile_definitions( TRACY_ENABLE=1 )
	set( TRACY_LIBRARY_PATH "${CMAKE_SOURCE_DIR}/lib/tracy_client.lib" )
else()
	set( TRACY_LIBRARY_PATH "${CMAKE_SOURCE_DIR}/lib/tracy_client_phony.lib" )
endif( ENABLE_PROFILING )

set(GLM_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/lib/glm/")

add_executable(vulcain
	"src/main.cpp"
	"src/io.cpp"
	"src/packer.cpp"
	"src/guizmo.cpp"
	
	"src/ngLib/console.cpp"
	"src/ngLib/logs.cpp"
	"src/ngLib/nglib.cpp"
	"src/ngLib/sys.cpp"

	"lib/gl3w.c"
	# imgui
	"./lib/imgui/imgui.cpp"
	"./lib/imgui/imgui_widgets.cpp"
	"./lib/imgui/imgui_demo.cpp"
	"./lib/imgui/imgui_draw.cpp"
	"./lib/imgui/imgui_impl_sdl.cpp"
	"./lib/imgui/imgui_impl_opengl3.cpp"
  "src/shader.h" "src/shader.cpp" "src/renderer.h" "src/renderer.cpp" "src/mesh.h" "src/mesh.cpp" "src/obj_parser.h" "src/obj_parser.cpp" "src/entity.h" "src/housing.h" "src/housing.cpp" "src/ngLib/ngtime.h" "src/collider.h" "src/collider.cpp" "src/navigation.h" "src/navigation.cpp" "src/ngLib/ngcontainers.h" "src/message.h" "src/registery.h" "src/entity.cpp" "src/collada_parser.h" "src/collada_parser.cpp")

target_include_directories(vulcain
PRIVATE
	"./src"
SYSTEM
	${GLM_INCLUDE_DIRS}
	${SDL2_INCLUDE_DIRS}
	"./lib"
)

target_link_libraries(vulcain PRIVATE
	${OPENGL_gl_LIBRARY} # filled by "find_package(OpenGL REQUIRED)"
	SDL2::SDL2main SDL2::SDL2-static
	lz4::lz4
	${TRACY_LIBRARY_PATH}
    tinyxml2::tinyxml2
)

configure_file("${PROJECT_SOURCE_DIR}/resources/resources.lz4" "resources.lz4" COPYONLY)

