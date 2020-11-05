# CMake entry point.

cmake_minimum_required (VERSION 3.8)

project (vulcain)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DFS_BASE_PATH="${CMAKE_SOURCE_DIR}/resources/")
add_compile_options("")

set (CMAKE_CXX_STANDARD 20)
set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(lz4 REQUIRED)
find_package(tinyxml2 CONFIG REQUIRED)

if ( ENABLE_PROFILING )
	add_compile_definitions( TRACY_ENABLE=1 )
	add_definitions(-DBENCHMARK_ENABLED)
	remove_definitions(-DNG_ASSERT_ENABLED)
	set( TRACY_SOURCES "${CMAKE_SOURCE_DIR}/lib/tracy/TracyClient.cpp" )
	find_package(benchmark CONFIG REQUIRED)
	set(BENCHMARK_LIBRARY "benchmark::benchmark" )
	set( BENCHMARK_SOURCES "test/benchmarks.cpp" )
	if (MSVC)
		string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
		string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else ()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
	endif ()
	add_definitions(-D_HAS_EXCEPTIONS=0)
else()
	set( TRACY_SOURCES "" )
	set( BENCHMARK_SOURCES "" )
	set( BENCHMARK_LIBRARY "" )
endif( ENABLE_PROFILING )

if ( ENABLE_TESTING )
	add_compile_definitions( ENABLE_TESTING=1 )
	set( TEST_SOURCES
	"test/test_road_network.cpp"
	"test/test_containers.cpp"
	"test/test_registry.cpp"
	)
else()
	set( TEST_SOURCES "" )
endif( ENABLE_TESTING)

set(GLM_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/lib/glm/")

add_executable(vulcain
	"src/main.cpp"
	${TEST_SOURCES}
	${TRACY_SOURCES}
	${BENCHMARK_SOURCES} 
	
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

	"src/io.cpp" "src/packer.cpp" "src/guizmo.cpp" "src/shader.h" "src/shader.cpp" "src/renderer.h" "src/renderer.cpp" "src/mesh.h" "src/mesh.cpp" "src/obj_parser.h" "src/obj_parser.cpp" "src/entity.h" "src/collider.h" "src/collider.cpp" "src/navigation.h" "src/navigation.cpp" "src/ngLib/ngcontainers.h" "src/message.h" "src/registery.h"  "src/collada_parser.h" "src/collada_parser.cpp"
 "src/buildings/building.h" "src/buildings/building.cpp"  "src/buildings/placement.h" "src/buildings/placement.cpp" "src/map.h" "src/map.cpp" "src/ui/ui.h" "src/ui/ui.cpp" "src/service.h" "src/service.cpp" "src/game_time.h" "src/message.cpp" "src/system.h" "src/system.cpp" "src/pathfinding_job.h" "src/pathfinding_job.cpp" "src/registery.cpp" "src/buildings/woodworking.h" "src/buildings/woodworking.cpp" "src/buildings/delivery.h" "src/buildings/delivery.cpp" "src/buildings/storage_house.h" "src/buildings/storage_house.cpp" "src/buildings/debug_dump.h" "src/buildings/debug_dump.cpp" "src/buildings/resource_fetcher.h" "src/buildings/resource_fetcher.cpp" "src/environment/trees.h" "src/environment/trees.cpp")


target_compile_options( vulcain PRIVATE
	-DIMGUI_IMPL_OPENGL_LOADER_GL3W
	-DNOMINMAX
	$<$<CONFIG:DEBUG>:-DDEBUG -DNG_ASSERT_ENABLED>
	$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
		-Wall>
	$<$<CXX_COMPILER_ID:MSVC>:
		/wd26812>
)

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
	${BENCHMARK_LIBRARY}
    tinyxml2::tinyxml2
)

configure_file("${PROJECT_SOURCE_DIR}/resources/resources.lz4" "resources.lz4" COPYONLY)

