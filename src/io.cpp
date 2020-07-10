#include "io.h"

#include "game.h"
#include "window.h"
#include <GL/gl3w.h>
#include <SDL2/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_sdl.h>
#include <vector>

void IO::Update( Window & window ) {
	// clear keyboard and mouse state
	for ( int & k : keyboard.keyPressed ) {
		k = KEY_NONE;
	}
	mouse.offset.x = 0;
	mouse.offset.y = 0;
	mouse.wheelMotion.x = 0;
	mouse.wheelMotion.y = 0;

	if ( mouse.leftClick == Mouse::ButtonState::PRESSED )
		mouse.leftClick = Mouse::ButtonState::DOWN;
	if ( mouse.leftClick == Mouse::ButtonState::RELEASED )
		mouse.leftClick = Mouse::ButtonState::UP;
	if ( mouse.rightClick == Mouse::ButtonState::PRESSED )
		mouse.rightClick = Mouse::ButtonState::DOWN;
	if ( mouse.rightClick == Mouse::ButtonState::RELEASED )
		mouse.rightClick = Mouse::ButtonState::UP;

	static bool cursorIsInMainWindow = false;
	u32         mainWindowID = SDL_GetWindowID( window.glWindow );

	SDL_Event event;
	while ( SDL_PollEvent( &event ) ) {
		ImGui_ImplSDL2_ProcessEvent( &event );
		if ( event.type == SDL_QUIT ) {
			window.shouldClose = true;
		}
		if ( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
		     event.window.windowID == SDL_GetWindowID( window.glWindow ) ) {
			window.shouldClose = true;
		}
		if ( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED ) {
			window.width = event.window.data1;
			window.height = event.window.data2;
			glViewport( 0, 0, window.width, window.height );
			// TODO: Refresh UI size
		}
		if ( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_ENTER ) {
			if ( event.window.windowID == mainWindowID ) {
				cursorIsInMainWindow = true;
			}
		}
		if ( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_LEAVE ) {
			if ( event.window.windowID == mainWindowID ) {
				cursorIsInMainWindow = false;
			}
		}
		if ( cursorIsInMainWindow ) {
			if ( event.type == SDL_KEYDOWN ) {
				auto key = event.key.keysym.sym;
				keyboard.RegisterKeyDown( key );
			}
			if ( event.type == SDL_KEYUP ) {
				auto key = event.key.keysym.sym;
				keyboard.RegisterKeyRelease( key );
			}
			if ( event.type == SDL_MOUSEMOTION ) {
				mouse.offset = glm::vec2( event.motion.xrel, event.motion.yrel );
				SDL_GetMouseState( &mouse.position.x, &mouse.position.y );
			}
			if ( event.type == SDL_MOUSEWHEEL ) {
				mouse.wheelMotion.x = event.wheel.x;
				mouse.wheelMotion.y = event.wheel.y;
			}

			if ( event.type == SDL_MOUSEBUTTONUP ) {
				if ( event.button.button == SDL_BUTTON_LEFT ) {
					mouse.leftClick = Mouse::ButtonState::RELEASED;
				}
				if ( event.button.button == SDL_BUTTON_RIGHT ) {
					mouse.rightClick = Mouse::ButtonState::RELEASED;
				}
			}
			if ( event.type == SDL_MOUSEBUTTONDOWN ) {
				if ( event.button.button == SDL_BUTTON_LEFT ) {
					mouse.leftClick = Mouse::ButtonState::PRESSED;
				}
				if ( event.button.button == SDL_BUTTON_RIGHT ) {
					mouse.rightClick = Mouse::ButtonState::PRESSED;
				}
			}
		}
	}
}

void IO::DebugDraw() {
	std::string keyDowns;
	for ( int & keyDown : keyboard.keyDowns ) {
		if ( keyDown == KEY_SPACE )
			keyDowns += "P ";
		else if ( keyDown == KEY_W )
			keyDowns += "W ";
		else if ( keyDown == KEY_A )
			keyDowns += "A ";
		else if ( keyDown == KEY_S )
			keyDowns += "S ";
		else if ( keyDown == KEY_D )
			keyDowns += "D ";
		else if ( keyDown == KEY_NONE )
			keyDowns += "X ";
		else
			keyDowns += "O ";
	}

	ImGui::Text( "Key downs:\n%s", keyDowns.c_str() );
}

void Keyboard::Init( Window & window ) {
	( void )window;
	for ( int i = 0; i < MAX_CONCURRENT_KEY_DOWN; i++ ) {
		keyDowns[ i ] = KEY_NONE;
		keyPressed[ i ] = KEY_NONE;
	}
}

void Keyboard::RegisterKeyDown( SDL_Keycode key ) {
	for ( int & keyPress : keyPressed ) {
		if ( keyPress == KEY_NONE ) {
			keyPress = key;
			break;
		}
	}
	for ( int & keyDown : keyDowns ) {
		// Check if key is already down
		if ( keyDown == key ) {
			return;
		}
	}
	for ( int & keyDown : keyDowns ) {
		if ( keyDown == KEY_NONE ) {
			keyDown = key;
			break;
		}
	}
}

void Keyboard::RegisterKeyRelease( SDL_Keycode key ) {
	for ( int & keyDown : keyDowns ) {
		if ( keyDown == key ) {
			keyDown = KEY_NONE;
		}
	}
}

bool Keyboard::IsKeyDown( eKey key ) const {
	for ( int keyDown : Keyboard::keyDowns ) {
		if ( keyDown == key ) {
			return true;
		}
	}
	return false;
}

bool Keyboard::IsKeyPressed( eKey key ) const {
	for ( int i : Keyboard::keyPressed ) {
		if ( i == key ) {
			return true;
		}
	}
	return false;
}

void Mouse::Init( Window & window ) {
	// SDL_SetRelativeMouseMode( SDL_TRUE );
	// SDL_SetWindowGrab( window.glWindow, SDL_TRUE );
}

bool Mouse::IsButtonDown( Button button ) const {
	if ( button == Button::LEFT ) {
		return leftClick == ButtonState::PRESSED || leftClick == ButtonState::DOWN;
	}
	if ( button == Button::RIGHT ) {
		return rightClick == ButtonState::PRESSED || rightClick == ButtonState::DOWN;
	}
	return false;
}

bool Mouse::IsButtonPressed( Button button ) const {
	if ( button == Button::LEFT ) {
		return leftClick == ButtonState::PRESSED;
	}
	if ( button == Button::RIGHT ) {
		return rightClick == ButtonState::PRESSED;
	}
	return false;
}