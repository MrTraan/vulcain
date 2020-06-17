#pragma once
#include "ngLib/logs.h"
#include <GL/gl3w.h>
#include <SDL2/SDL.h>
#include <imgui/imgui.h>
#include <stdexcept>
#include <tracy/Tracy.hpp>

constexpr char WINDOW_TITLE[] = "Vulcain";
constexpr int  WINDOW_WIDTH = 1280;
constexpr int  WINDOW_HEIGHT = 720;

class Window {
  public:
	int width;
	int height;

	void Init( int width = WINDOW_WIDTH, int height = WINDOW_HEIGHT, char * title = ( char * )WINDOW_TITLE ) {
		this->width = width;
		this->height = height;
#if __APPLE__
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG ); // Always required on Mac
#else
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
#endif
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 5 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 4 );
		SDL_WindowFlags window_flags =
		    ( SDL_WindowFlags )( SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI );
		glWindow =
		    SDL_CreateWindow( title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags );
		if ( !this->glWindow ) {
			throw std::runtime_error( "Fatal Error: Could not create GLFW Window" );
		}
		glContext = SDL_GL_CreateContext( glWindow );
		SDL_GL_MakeCurrent( glWindow, glContext );
		SDL_GL_SetSwapInterval( 1 ); // Enable vsync

		// gl3w: load all OpenGL function pointers
		if ( gl3wInit() )
			throw std::runtime_error( "Failed to initialize OpenGL\n" );
		if ( !gl3wIsSupported( 4, 5 ) )
			throw std::runtime_error( "OpenGL 4.5 is not supported\n" );
		ng::Printf( "OpenGL %s, GLSL %s\n", glGetString( GL_VERSION ), glGetString( GL_SHADING_LANGUAGE_VERSION ) );

		// configure global opengl state
		glDepthFunc( GL_LEQUAL );
		glFrontFace( GL_CCW );

		glEnable( GL_DEPTH_TEST );

		glEnable( GL_MULTISAMPLE );
		glEnable( GL_DEBUG_OUTPUT );
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );

		// glEnable( GL_BLEND );
		// glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}

	void DebugDraw() {
		static bool enableVsync = true;
		if ( ImGui::Checkbox( "Vsync", &enableVsync ) ) {
			SDL_GL_SetSwapInterval( enableVsync ? 1 : 0 );
		}
	}

	void Shutdown() {
		SDL_GL_DeleteContext( glContext );
		SDL_DestroyWindow( glWindow );
	}

	void Clear() {
		ZoneScoped;
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	void SwapBuffers() {
		ZoneScoped;
		SDL_GL_SwapWindow( glWindow );
	}

	bool          shouldClose = false;
	SDL_Window *  glWindow;
	SDL_GLContext glContext;
};
