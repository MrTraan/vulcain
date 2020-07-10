#include <GL/gl3w.h>

#include <SDL.h>
#include <chrono>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_sdl.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>
#include <unordered_map>

#include "entity.h"
#include "game.h"
#include "guizmo.h"
#include "housing.h"
#include "mesh.h"
#include "ngLib/nglib.h"
#include "obj_parser.h"
#include "packer.h"
#include "packer_resource_list.h"
#include "renderer.h"
#include "shader.h"
#include "window.h"

#if defined( _WIN32 )
#include <filesystem>
#else
NG_UNSUPPORTED_PLATFORM // GOOD LUCK LOL
#endif

#define STB_IMAGE_IMPLEMENTATION // force following include to generate implementation
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

Game * theGame;

static void DrawDebugWindow();

SystemManager systemManager;
Registery     registery;

static void Update( float dt ) { systemManager.Update( registery, dt ); }

static void FixedUpdate() {}

static void Render() {}

struct CpntTransform {
	CpntTransform() {}
	CpntTransform( const glm::mat4 & src ) : matrix( src ) {}
	glm::mat4 matrix = glm::mat4( 1.0f );
};

Entity CreateEntity() {
	static Entity currentId = 0;
	return currentId++;
}

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
	constexpr const char * imguiSaveFilePath = FS_BASE_PATH "imgui.ini";
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

	CpntRenderModel houseMesh;
	ImportObjFile( PackerResources::HOUSE_OBJ, houseMesh );
	for ( Mesh & mesh : houseMesh.meshes ) {
		AllocateMeshGLBuffers( mesh );
	}
	CpntRenderModel farmMesh;
	ImportObjFile( PackerResources::FARM_OBJ, farmMesh );
	for ( Mesh & mesh : farmMesh.meshes ) {
		AllocateMeshGLBuffers( mesh );
	}

	Shader defaultShader =
	    CompileShaderFromResource( PackerResources::SHADERS_DEFAULT_VERT, PackerResources::SHADERS_DEFAULT_FRAG );
	Texture pinkTexture = CreatePlaceholderPinkTexture();
	Texture whiteTexture = CreateDefaultWhiteTexture();

	// Register system
	systemManager.CreateSystem< SystemHousing >();
	systemManager.CreateSystem< SystemBuildingProducing >();

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

		if ( io.keyboard.IsKeyDown( KEY_ESCAPE ) ) {
			window.shouldClose = true;
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
		{
			static glm::vec3 cameraTarget( 20.0f, 0.0f, 20.0f );
			static glm::vec3 cameraUp( 0.0f, 1.0f, 0.0f );
			static float     cameraDistance = 50.0f;
			static float     cameraRotationAngle = 45.0f;

			// Update camera

			ImGui::SliderFloat( "camera rotation angle", &cameraRotationAngle, 0.0f, 360.0f );

			auto cameraRotationMatrix =
			    glm::rotate( glm::mat4( 1.0f ), glm::radians( cameraRotationAngle ), glm::vec3( 0.0f, 1.0f, 0.0f ) );

			glm::vec3 cameraPosition( 0.0f, 0.0f, 0.0f );
			cameraPosition = glm::vec3( 0.0f, cameraDistance, -cameraDistance );
			ImGui::DragFloat3( "camera position before rotation", &cameraPosition.x );
			cameraPosition = glm::vec3( cameraRotationMatrix * glm::vec4( cameraPosition, 1.0f ) );
			cameraPosition += cameraTarget;
			ImGui::DragFloat3( "camera position", &cameraPosition.x );
			ImGui::DragFloat3( "camera target", &cameraTarget.x );

			glm::vec3 cameraFront = glm::normalize( cameraTarget - cameraPosition );
			glm::vec3 cameraRight = glm::normalize( glm::cross( cameraFront, glm::vec3( 0.0f, 1.0f, 0.0f ) ) );
			cameraUp = glm::normalize( glm::cross( cameraRight, cameraFront ) );
			ImGui::DragFloat3( "Camera front", &cameraFront.x );
			ImGui::DragFloat3( "Camera Right", &cameraRight.x );
			ImGui::DragFloat3( "Camera up", &cameraUp.x );

			glm::mat4       view = view = glm::lookAt( cameraPosition, cameraTarget, cameraUp );
			float           aspectRatio = ( float )window.width / window.height;
			static float    cameraSize = 30.0f;
			constexpr float scrollSpeed = 100.0f;
			ImGui::SliderFloat( "camera size", &cameraSize, 1.0f, 100.0f );

			// Move camera
			cameraSize += io.mouse.wheelMotion.y * scrollSpeed * dt;
			cameraSize = std::min( cameraSize, 100.0f );
			cameraSize = std::max( cameraSize, 1.0f );

			glm::vec4       cameraTargetMovement( 0.0f, 0.0f, 0.0f, 1.0f );
			constexpr float cameraMovementSpeed = 10.0f;
			if ( io.keyboard.IsKeyDown( eKey::KEY_D ) )
				cameraTargetMovement.x += cameraMovementSpeed * dt;
			if ( io.keyboard.IsKeyDown( eKey::KEY_A ) )
				cameraTargetMovement.x -= cameraMovementSpeed * dt;
			if ( io.keyboard.IsKeyDown( eKey::KEY_W ) )
				cameraTargetMovement.z += cameraMovementSpeed * dt;
			if ( io.keyboard.IsKeyDown( eKey::KEY_S ) )
				cameraTargetMovement.z -= cameraMovementSpeed * dt;

			auto rotatedMovement = cameraRotationMatrix * cameraTargetMovement;
			// auto rotatedMovement = cameraTargetMovement;
			cameraTarget += glm::vec3( rotatedMovement );

			glm::mat4 proj = glm::ortho( aspectRatio * cameraSize / 2, -aspectRatio * cameraSize / 2, -cameraSize / 2,
			                             cameraSize / 2, 0.3f, 1000.0f );
			Guizmo::Line( glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 10.0f, 0.0f, 0.0f ), Guizmo::colRed );
			// glm::mat4 proj = glm::perspective( glm::radians( 60.0f), (float)window.width / window.height, 0.3f,
			// 1000.0f);
			glm::mat4       viewProj = proj * view;
			ViewProjUBOData uboData{};
			uboData.view = view;
			uboData.projection = proj;
			uboData.viewProj = viewProj;
			uboData.viewPosition = glm::vec4( cameraPosition, 1.0f );

			FillViewProjUBO( &uboData );

			// draw grid
			for ( int x = -100; x < 100; x += 1 ) {
				Guizmo::Line( glm::vec3( x, 0.0f, -100.0f ), glm::vec3( x, 0.0f, 100.0f ), Guizmo::colWhite );
			}
			for ( int z = -100; z < 100; z += 1 ) {
				Guizmo::Line( glm::vec3( -100.0f, 0.0f, z ), glm::vec3( 100.0f, 0.0f, z ), Guizmo::colWhite );
			}

			ImGui::Text( "Mouse position: %d %d", io.mouse.position.x, io.mouse.position.y );
			ImGui::Text( "Mouse offset: %f %f", io.mouse.offset.x, io.mouse.offset.y );

			glm::vec4 viewport = glm::vec4( 0, window.height, window.width, -window.height );
			glm::vec3 mousePositionWorldSpace = glm::unProject(
			    glm::vec3( io.mouse.position.x, io.mouse.position.y, 0 ), glm::mat4( 1.0f ), viewProj, viewport );
			float a = -mousePositionWorldSpace.y / cameraFront.y;
			mousePositionWorldSpace = mousePositionWorldSpace + a * cameraFront;
			ImGui::Text( "Mouse position world space : %f %f %f", mousePositionWorldSpace.x, mousePositionWorldSpace.y,
			             mousePositionWorldSpace.z );

			glm::vec3 mousePositionWorldSpaceFloored( ( int )floorf( mousePositionWorldSpace.x ), 0.0f,
			                                          ( int )floorf( mousePositionWorldSpace.z ) );
			Guizmo::Rectangle( mousePositionWorldSpaceFloored, 1.0f, 1.0f, Guizmo::colRed );

			// Building debug
			constexpr int numBuilding = 2;
			const char *  buildingList[ numBuilding ] = {
                "house",
                "wheat farm",
            };

			static int buildingCurrentlySelected = 0;
			if ( ImGui::BeginCombo( "system selected", buildingList[ buildingCurrentlySelected ] ) ) {
				for ( int i = 0; i < numBuilding; i++ ) {
					bool isSelected = buildingCurrentlySelected == i;
					if ( ImGui::Selectable( buildingList[ i ], isSelected ) ) {
						buildingCurrentlySelected = i;
					}

					if ( isSelected ) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			if ( io.mouse.IsButtonPressed( Mouse::Button::LEFT ) ) {
				Entity newBuilding = CreateEntity();
				if ( buildingCurrentlySelected == 0 ) {
					// Build house
					registery.AssignComponent< CpntRenderModel >( newBuilding, houseMesh );
					registery.AssignComponent< CpntTransform >(
					    newBuilding,
					    glm::translate( glm::mat4( 1.0f ), mousePositionWorldSpaceFloored +
					                                           glm::vec3( houseMesh.roundedSize.x / 2.0f, 0.0f,
					                                                      houseMesh.roundedSize.z / 2.0f ) ) );
					CpntHousing & housing = registery.AssignComponent< CpntHousing >( newBuilding, 0 );
					housing.maxHabitants = 4;
				} else if ( buildingCurrentlySelected == 1 ) {
					// Build farm
					registery.AssignComponent< CpntRenderModel >( newBuilding, farmMesh );
					registery.AssignComponent< CpntTransform >(
					    newBuilding,
					    glm::translate( glm::mat4( 1.0f ), mousePositionWorldSpaceFloored +
					                                           glm::vec3( farmMesh.roundedSize.x / 2.0f, 0.0f,
					                                                      farmMesh.roundedSize.z / 2.0f ) ) );
					CpntBuildingProducing & producer =
					    registery.AssignComponent< CpntBuildingProducing >( newBuilding );
					producer.batchSize = 4;
					producer.maxStorageSize = 24;
					producer.timeToProduceBatch = ng::DurationInMs( 5000.0f );
					producer.resource = GameResource::WHEAT;
				}
			}
		}
		{
			static glm::vec3 lightDirection( -1.0f, -1.0f, 0.0f );
			ImGui::DragFloat3( "lightDirection", &lightDirection[ 0 ] );

			static float lightAmbiant = 0.7f;
			static float lightDiffuse = 0.5f;
			static float lightSpecular = 1.0f;
			ImGui::SliderFloat( "light ambiant", &lightAmbiant, 0.0f, 1.0f );
			ImGui::SliderFloat( "light diffuse", &lightDiffuse, 0.0f, 1.0f );
			ImGui::SliderFloat( "light specular", &lightSpecular, 0.0f, 1.0f );

			defaultShader.Use();
			defaultShader.SetVector( "light.direction", glm::normalize( lightDirection ) );
			defaultShader.SetVector( "light.ambient", glm::vec3( lightAmbiant ) );
			defaultShader.SetVector( "light.diffuse", glm::vec3( lightDiffuse ) );
			defaultShader.SetVector( "light.specular", glm::vec3( lightSpecular ) );

			for ( auto const & [ e, renderModel ] : registery.IterateOver< CpntRenderModel >() ) {
				for ( const Mesh & mesh : renderModel.meshes ) {
					const CpntTransform & transform = registery.GetComponent< CpntTransform >( e );
					defaultShader.SetMatrix( "modelTransform", transform.matrix );
					glm::mat3 normalMatrix( glm::transpose( glm::inverse( transform.matrix ) ) );
					defaultShader.SetMatrix3( "normalTransform", normalMatrix );
					defaultShader.SetVector( "material.ambient", mesh.material->ambiant );
					defaultShader.SetVector( "material.diffuse", mesh.material->diffuse );
					defaultShader.SetVector( "material.specular", mesh.material->specular );
					defaultShader.SetFloat( "material.shininess", mesh.material->shininess );

					glActiveTexture( GL_TEXTURE0 );
					glBindVertexArray( mesh.vao );
					glBindTexture( GL_TEXTURE_2D, mesh.material->diffuseTexture.id );
					if ( mesh.material->mode == Material::MODE_TRANSPARENT ) {
						glEnable( GL_BLEND );
						glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
					}
					glDrawElements( GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, nullptr );
					glDisable( GL_BLEND );
					glBindVertexArray( 0 );
				}
			}
		}
		Render();

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
	static bool opened = true;
	ImGui::ShowDemoWindow( &opened );
	ImGui::Begin( "Debug" );

	ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	             ImGui::GetIO().Framerate );
	if ( ImGui::TreeNode( "Window" ) ) {
		theGame->window.DebugDraw();
		ImGui::TreePop();
	}
	if ( ImGui::TreeNode( "IO" ) ) {
		theGame->io.DebugDraw();
		ImGui::TreePop();
	}
	if ( ImGui::TreeNode( "Systems" ) ) {
		std::vector< std::pair< const char *, ISystem * > > systemWithNames;
		for ( auto & [ type, system ] : systemManager.systems ) {
			systemWithNames.push_back( { type.name(), system } );
		}

		static int currentlySelected = 0;
		if ( ImGui::BeginCombo( "system selected", systemWithNames[ currentlySelected ].first ) ) {
			for ( int i = 0; i < systemWithNames.size(); i++ ) {
				bool isSelected = currentlySelected == i;
				if ( ImGui::Selectable( systemWithNames[ i ].first, isSelected ) ) {
					currentlySelected = i;
				}

				if ( isSelected ) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		systemWithNames[ currentlySelected ].second->DebugDraw();
		ImGui::TreePop();
	}

	ImGui::End();
}
