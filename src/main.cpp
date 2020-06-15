#include <GL/gl3w.h>

#include <SDL.h>
#include <chrono>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_sdl.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>

#include "game.h"
#include "guizmo.h"
#include "ngLib/nglib.h"
#include "packer.h"
#include "renderer.h"
#include "shader.h"
#include "window.h"

#if defined( _WIN32 )
#include <filesystem>
#else
NG_UNSUPPORTED_PLATFORM // GOOD LUCK LOL
#endif

#define STB_TRUETYPE_IMPLEMENTATION // force following include to generate implementation
#include <stb_truetype.h>

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

Game * theGame;

static void DrawDebugWindow();

static void Update( float dt ) {}
static void FixedUpdate() {}

static void Render() {}

int main( int ac, char ** av ) {
	ng::Init();

	if ( ac > 1 && strcmp( "--create-archive", av[ 1 ] ) == 0 ) {
		bool success = PackerCreateArchive( FS_BASE_PATH, "resources.lz4" );
		ng_assert( success == true );
		return 0;
	}

	if ( SDL_Init( SDL_INIT_AUDIO | SDL_INIT_VIDEO ) < 0 ) {
		ng::Errorf( "SDL_Init failed: %s\n", SDL_GetError() );
		return 1;
	}

	theGame = new Game();
	theGame->state = Game::State::MENU;

	if ( true ) {
		bool success = PackerCreateRuntimeArchive( FS_BASE_PATH, &theGame->package );
		ng_assert( success == true );
	} else {
		bool success = PackerReadArchive( "resources.lz4", &theGame->package );
		ng_assert( success == true );
	}

	Window & window = theGame->window;
	IO &     io = theGame->io;

	window.Init();

	// Setup imgui
	ImGui::CreateContext();
	ImGuiIO &              imio = ImGui::GetIO();
	constexpr const char * imguiSaveFilePath = "C:\\Users\\natha\\Desktop\\minecrouft_imgui.ini";
	imio.IniFilename = imguiSaveFilePath;
	imio.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
	imio.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGuiStyle & style = ImGui::GetStyle();
	if ( imio.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
		style.WindowRounding = 0.0f;
		style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
	}
	ImGui_ImplSDL2_InitForOpenGL( window.glWindow, window.glContext );
	ImGui_ImplOpenGL3_Init( "#version 150" );

	InitRenderer();
	g_shaderAtlas.CompileAllShaders();

	Guizmo::Init();

	TracyGpuContext;

	auto  lastFrameTime = std::chrono::high_resolution_clock::now();
	float fixedTimeStepAccumulator = 0.0f;

	glm::mat4 view = glm::lookAt( glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 0.0f, 0.0f, -1.0f ), glm::vec3( 0.0f, 1.0f, 0.0f ) );
	glm::mat4 proj = glm::ortho( 0.0f, (float)window.width, (float)window.height, 0.0f, -100.0f, 100.0f );
	glm::mat4 viewProj = view * proj;
	ViewProjUBOData uboData{};
	uboData.view = view;
	uboData.projection = proj;
	uboData.viewProj = viewProj;

	FillViewProjUBO( &uboData );

	glm::mat4 transform(1.0f);
	glm::vec3 scale;
	glm::quat rotation;
	glm::vec3 translation;
	glm::vec3 skew;
	glm::vec4 perspective;

	while ( !window.shouldClose ) {
		ZoneScopedN( "MainLoop" );

		auto  currentFrameTime = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration_cast< std::chrono::microseconds >( currentFrameTime - lastFrameTime ).count() /
		           1000000.0f;
		lastFrameTime = currentFrameTime;

		{
			ZoneScopedN( "NewFrameSetup" );
			window.Clear();
			io.Update( window );
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame( window.glWindow );
			ImGui::NewFrame();
			Guizmo::NewFrame();
		}

		fixedTimeStepAccumulator += dt;
		int numFixedSteps = floorf( fixedTimeStepAccumulator / FIXED_TIMESTEP );
		if ( numFixedSteps > 0 ) {
			fixedTimeStepAccumulator -= numFixedSteps * FIXED_TIMESTEP;
		}
		for ( int i = 0; i < numFixedSteps; i++ ) {
			FixedUpdate();
		}
		Update( dt );
		Render();
	
		glm::decompose( transform, scale, rotation, translation, skew, perspective );
		bool needRebuild = false;
		needRebuild |= ImGui::DragFloat3("Translation", &translation.x);
		needRebuild |= ImGui::DragFloat3("Scale", &scale.x);

		if ( needRebuild ) {
			transform = glm::mat4(1.0f);
			transform = glm::translate(transform, translation);
			transform = glm::scale( transform, scale );
			transform = transform * glm::toMat4(rotation);
		}

		ImGui::DragFloat4("0", &transform[0].x );
		ImGui::DragFloat4("1", &transform[1].x );
		ImGui::DragFloat4("2", &transform[2].x );
		ImGui::DragFloat4("3", &transform[3].x );

		Guizmo::Line(glm::vec3(0, 0, -5), glm::vec3(100, 100, -5), Guizmo::colYellow);
		float redZ = -6.0f;
		ImGui::SliderFloat("z red", &redZ, -10.0f, 10.0f);
		Guizmo::Line(glm::vec3(100, 0, redZ), glm::vec3(0, 100, redZ), Guizmo::colRed);

		ng::GetConsole().Draw();
		DrawDebugWindow();

		Guizmo::Draw();
		{
			ZoneScopedN( "Render_IMGUI" );
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
			if ( imio.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
				SDL_Window *  backup_current_window = SDL_GL_GetCurrentWindow();
				SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				SDL_GL_MakeCurrent( backup_current_window, backup_current_context );
			}
		}

		window.SwapBuffers();
		TracyGpuCollect;
		FrameMark;
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	ShutdownRenderer();
	g_shaderAtlas.FreeShaders();

	window.Shutdown();
	ng::Shutdown();

	delete theGame;

	SDL_Quit();
	return 0;
}

void DrawDebugWindow() {
	// bool demoOpen = true;
	// ImGui::ShowDemoWindow( &demoOpen );

	ImGui::Begin( "Debug" );

	ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	             ImGui::GetIO().Framerate );

	if ( ImGui::TreeNode( "IO" ) ) {
		theGame->io.DebugDraw();
		ImGui::TreePop();
	}

	ImGui::End();
}
