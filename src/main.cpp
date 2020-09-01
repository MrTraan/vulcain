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

#include "collider.h"
#include "entity.h"
#include "game.h"
#include "guizmo.h"
#include "housing.h"
#include "mesh.h"
#include "navigation.h"
#include "ngLib/ngcontainers.h"
#include "ngLib/nglib.h"
#include "packer.h"
#include "packer_resource_list.h"
#include "registery.h"
#include "renderer.h"
#include "shader.h"
#include "window.h"

#if defined( ENABLE_TESTING )
#define CATCH_CONFIG_RUNNER
#include <catch.hpp>
#endif

#if defined( _WIN32 )
#include <filesystem>
// Encourage drivers to use graphics cards over integrated graphics
extern "C" {
_declspec( dllexport ) DWORD NvOptimusEnablement = 0x00000001;
_declspec( dllexport ) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
}

#else
NG_UNSUPPORTED_PLATFORM // GOOD LUCK LOL
#endif

#if defined( BENCHMARK_ENABLED )
#include "../test/benchmarks.h"
#endif

#define STB_IMAGE_IMPLEMENTATION // force following include to generate implementation
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

void GLErrorCallback( GLenum         source,
                      GLenum         type,
                      GLuint         id,
                      GLenum         severity,
                      GLsizei        length,
                      const GLchar * message,
                      const void *   userParam ) {
	if ( severity != GL_DEBUG_SEVERITY_NOTIFICATION ) {
		ng::Errorf( "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
		            ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ), type, severity, message );
	}
}

Game * theGame;

static void DrawDebugWindow();

SystemManager systemManager;
Registery     registery;

Camera mainCamera;
Camera modelInspectorCamera;

Framebuffer   modelInspectorFramebuffer;
constexpr int modelInspectorWidth = 400;
constexpr int modelInspectorHeight = 400;

static void Update( float dt ) { systemManager.Update( registery, dt ); }

static void FixedUpdate() {}

static void Render() {}

struct InstancedModelBatch {
	Model *                       model;
	ng::DynamicArray< glm::vec3 > positions;
	bool                          dirty = false;
	u32                           arrayBuffer;

	void AddInstanceAtPosition( const glm::vec3 & position ) {
		positions.PushBack( position );
		dirty = true;
	}

	bool RemoveInstancesWithPosition( const glm::vec3 & position ) {
		bool modified = positions.DeleteValueFast( position );
		if ( modified ) {
			dirty = true;
			return true;
		}
		return false;
	}

	void Init( Model * model ) {
		this->model = model;
		glGenBuffers( 1, &arrayBuffer );
		UpdateArrayBuffer();

		for ( unsigned int i = 0; i < model->meshes.size(); i++ ) {
			unsigned int VAO = model->meshes[ i ].vao;
			glBindVertexArray( VAO );
			glEnableVertexAttribArray( 3 );
			glVertexAttribPointer( 3, 3, GL_FLOAT, GL_FALSE, sizeof( glm::vec3 ), ( void * )0 );
			glVertexAttribDivisor( 3, 1 );

			glBindVertexArray( 0 );
		}
	}

	void UpdateArrayBuffer() {
		glBindBuffer( GL_ARRAY_BUFFER, arrayBuffer );
		glBufferData( GL_ARRAY_BUFFER, positions.Size() * sizeof( glm::vec3 ), positions.data, GL_STATIC_DRAW );
	}

	void Render() {
		if ( dirty ) {
			UpdateArrayBuffer();
			dirty = false;
		}
		Shader & shader = g_shaderAtlas.instancedShader;
		shader.Use();
		for ( const Mesh & mesh : model->meshes ) {
			glm::mat4 transform( 1.0f );
			glm::mat3 normalMatrix( glm::transpose( glm::inverse( transform ) ) );
			shader.SetMatrix3( "normalTransform", normalMatrix );
			shader.SetVector( "material.ambient", mesh.material->ambiant );
			shader.SetVector( "material.diffuse", mesh.material->diffuse );
			shader.SetVector( "material.specular", mesh.material->specular );
			shader.SetFloat( "material.shininess", mesh.material->shininess );

			glActiveTexture( GL_TEXTURE0 );
			glBindVertexArray( mesh.vao );
			glBindTexture( GL_TEXTURE_2D, mesh.material->diffuseTexture.id );
			if ( mesh.material->mode == Material::MODE_TRANSPARENT ) {
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			}
			glDrawElementsInstanced( GL_TRIANGLES, ( GLsizei )mesh.indices.size(), GL_UNSIGNED_INT, nullptr,
			                         positions.Size() );
			glDisable( GL_BLEND );
			glBindVertexArray( 0 );
		}
	}
};

InstancedModelBatch roadBatchedTiles;

void SpawnRoadBlock( Registery & reg, Map & map, Cell cell ) {
	if ( map.GetTile( cell ) != MapTile::ROAD ) {
		map.SetTile( cell, MapTile::ROAD );
		roadBatchedTiles.AddInstanceAtPosition( GetPointInCornerOfCell( cell ) );
	}
}

Entity SpawnBuilding( Registery & reg, Map & map, Cell cell, u32 sizeX, u32 sizeZ, const Model * model ) {
	Entity         newBuilding = reg.CreateEntity();
	CpntBuilding & buildingCpnt = reg.AssignComponent< CpntBuilding >( newBuilding );
	buildingCpnt.cell = cell;
	buildingCpnt.tileSizeX = sizeX;
	buildingCpnt.tileSizeZ = sizeZ;

	reg.AssignComponent< CpntRenderModel >( newBuilding, model );

	// Center render model in the middle of the tile size
	CpntTransform & transform = reg.AssignComponent< CpntTransform >( newBuilding );
	transform.SetTranslation(
	    glm::vec3( cell.x + buildingCpnt.tileSizeX / 2.0f, 0, cell.z + buildingCpnt.tileSizeZ / 2.0f ) );

	reg.AssignComponent< CpntBoxCollider >(
	    newBuilding, CpntBoxCollider( ( model->maxCoords + model->minCoords ) / 2.0f, model->size ) );

	for ( u32 x = cell.x; x < cell.x + sizeX; x++ ) {
		for ( u32 z = cell.z; z < cell.z + sizeZ; z++ ) {
			map.SetTile( x, z, MapTile::BLOCKED );
		}
	}

	return newBuilding;
}

int main( int ac, char ** av ) {
	ng::Init();

#if defined( ENABLE_TESTING )
	int result = Catch::Session().run( ac, av );
	return result;
#endif

#if defined( BENCHMARK_ENABLED )
	if ( ac > 1 && strcmp( "--run-benchmarks", av[ 1 ] ) == 0 ) {
		RunBenchmarks( ac - 1, av + 1 );
		return 0;
	}
#endif

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

	glEnable( GL_DEBUG_OUTPUT );
	glDebugMessageCallback( GLErrorCallback, 0 );

	{
		auto vendor = glGetString( GL_VENDOR );
		auto renderer = glGetString( GL_RENDERER );
		ng::Printf( "Using graphics card: %s %s\n ", vendor, renderer );
	}

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
	modelInspectorFramebuffer.Allocate( modelInspectorWidth, modelInspectorHeight );

	Guizmo::Init();

	TracyGpuContext;
	ng::LinkedList< int > list;
	for ( int i = 0; i < 64; i++ ) {
		list.PushFront( i );
	}

	auto  lastFrameTime = std::chrono::high_resolution_clock::now();
	float fixedTimeStepAccumulator = 0.0f;

	g_modelAtlas.LoadAllModels();

	Texture pinkTexture = CreatePlaceholderPinkTexture();
	Texture whiteTexture = CreateDefaultWhiteTexture();

	// Register system
	systemManager.CreateSystem< SystemHousing >();
	systemManager.CreateSystem< SystemBuildingProducing >();
	systemManager.CreateSystem< SystemNavAgent >();

	Model groundModel;
	CreateTexturedPlane( 200.0f, 200.0f, 64.0f,
	                     *( theGame->package.GrabResource( PackerResources::GRASS_TEXTURE_PNG ) ), groundModel );
	Model roadModel;
	CreateTexturedPlane( 1.0f, 1.0f, 1.0f, *( theGame->package.GrabResource( PackerResources::ROAD_TEXTURE_PNG ) ),
	                     roadModel );

	roadBatchedTiles.Init( &roadModel );

	Entity player = registery.CreateEntity();
	{
		registery.AssignComponent< CpntRenderModel >( player, g_modelAtlas.cubeMesh );
		CpntTransform & transform = registery.AssignComponent< CpntTransform >( player );
		transform.SetTranslation( { 0.5f, 0.0f, 0.5f } );
		registery.AssignComponent< CpntNavAgent >( player );
	}

	Map & map = theGame->map;
	map.AllocateGrid( 200, 200 );
	SpawnRoadBlock( registery, map, Cell( 0, 0 ) );

	for ( u32 x = 30; x <= 100; x++ ) {
		for ( u32 z = 30; z <= 100; z++ ) {
			if ( x % 10 == 0 || z % 10 == 0 )
				SpawnRoadBlock( registery, map, Cell( x, z ) );
		}
	}

	Entity storageHouse = SpawnBuilding( registery, map, Cell( 10, 10 ), 3, 3, g_modelAtlas.storeHouseMesh );
	registery.AssignComponent< CpntBuildingStorage >( storageHouse );

	Entity                  wheatFarm = SpawnBuilding( registery, map, Cell( 10, 20 ), 4, 4, g_modelAtlas.farmMesh );
	CpntBuildingProducing & producer = registery.AssignComponent< CpntBuildingProducing >( wheatFarm );
	producer.batchSize = 4;
	producer.timeToProduceBatch = ng::DurationInMs( 5000.0f );
	producer.resource = GameResource::WHEAT;

	for ( u32 z = 10; z < 24; z++ ) {
		SpawnRoadBlock( registery, map, Cell( 9, z ) );
	}

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
		int numFixedSteps = ( int )floorf( fixedTimeStepAccumulator / FIXED_TIMESTEP );
		if ( numFixedSteps > 0 ) {
			fixedTimeStepAccumulator -= numFixedSteps * FIXED_TIMESTEP;
		}
		for ( int i = 0; i < numFixedSteps; i++ ) {
			FixedUpdate();
		}
		Update( dt );
		{
			static glm::vec3 cameraTarget( 0.0f, 0.0f, 0.0f );
			static glm::vec3 cameraUp( 0.0f, 1.0f, 0.0f );
			static float     cameraDistance = 50.0f;
			static float     cameraRotationAngle = 45.0f;

			// Update camera

			ImGui::SliderFloat( "camera rotation angle", &cameraRotationAngle, 0.0f, 360.0f );

			auto cameraRotationMatrix =
			    glm::rotate( glm::mat4( 1.0f ), glm::radians( cameraRotationAngle ), glm::vec3( 0.0f, 1.0f, 0.0f ) );

			glm::vec3 & cameraPosition = mainCamera.position;
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

			mainCamera.view = glm::lookAt( cameraPosition, cameraTarget, cameraUp );
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

			mainCamera.proj = glm::ortho( aspectRatio * cameraSize / 2, -aspectRatio * cameraSize / 2, -cameraSize / 2,
			                              cameraSize / 2, 0.3f, 1000.0f );

			// draw grid
			// for ( int x = 0; x < 100; x += 1 ) {
			//	Guizmo::Line( glm::vec3( x, 0.0f, 0.0f ), glm::vec3( x, 0.0f, 100.0f ), Guizmo::colWhite );
			//}
			// for ( int z = 0; z < 100; z += 1 ) {
			//	Guizmo::Line( glm::vec3( 0.0f, 0.0f, z ), glm::vec3( 100.0f, 0.0f, z ), Guizmo::colWhite );
			//}

			ImGui::Text( "Mouse position: %d %d", io.mouse.position.x, io.mouse.position.y );
			ImGui::Text( "Mouse offset: %f %f", io.mouse.offset.x, io.mouse.offset.y );

			glm::vec4 viewport = glm::vec4( 0, window.height, window.width, -window.height );
			glm::vec3 mousePositionWorldSpace =
			    glm::unProject( glm::vec3( io.mouse.position.x, io.mouse.position.y, 0 ), glm::mat4( 1.0f ),
			                    mainCamera.proj * mainCamera.view, viewport );
			Ray mouseRaycast;
			mouseRaycast.origin = mousePositionWorldSpace;
			mouseRaycast.direction = cameraFront;
			ng_assert( cameraFront.y != 0.0f );
			float a = -mousePositionWorldSpace.y / cameraFront.y;
			mousePositionWorldSpace = mousePositionWorldSpace + a * cameraFront;
			ImGui::Text( "Mouse position world space : %f %f %f", mousePositionWorldSpace.x, mousePositionWorldSpace.y,
			             mousePositionWorldSpace.z );

			glm::vec3 mousePositionWorldSpaceFloored( ( int )floorf( mousePositionWorldSpace.x ), 0.0f,
			                                          ( int )floorf( mousePositionWorldSpace.z ) );
			Guizmo::Rectangle( mousePositionWorldSpaceFloored, 1.0f, 1.0f, Guizmo::colRed );

			for ( auto const & [ e, box ] : registery.IterateOver< CpntBoxCollider >() ) {
				const CpntTransform & transform = registery.GetComponent< CpntTransform >( e );
				if ( RayCollidesWithBox( mouseRaycast, box, transform ) ) {
					Guizmo::LinesAroundCube( transform.GetMatrix() * glm::vec4( box.center, 1.0f ), box.size,
					                         Guizmo::colRed );
				}
			}

			Cell   mouseCellPosition = GetCellForPoint( mousePositionWorldSpaceFloored );
			Entity hoveredEntity = INVALID_ENTITY_ID;
			for ( auto const & [ e, building ] : registery.IterateOver< CpntBuilding >() ) {
				if ( IsCellInsideBuilding( building, mouseCellPosition ) ) {
					hoveredEntity = e;
					break;
				}
			}

			if ( io.mouse.IsButtonPressed( Mouse::Button::RIGHT ) ) {
				// move player to cursor
				glm::vec3 playerPosition = registery.GetComponent< CpntTransform >( player ).GetTranslation();
				bool      pathFound = map.roadNetwork.FindPath(
                    GetCellForPoint( playerPosition ), GetCellForPoint( mousePositionWorldSpaceFloored ), map,
                    registery.GetComponent< CpntNavAgent >( player ).pathfindingNextSteps );
				ng::Printf( "Path found %d\n", pathFound );
				std::vector< Cell > foo;
				AStar( GetCellForPoint( playerPosition ), GetCellForPoint( mousePositionWorldSpaceFloored ),
				       ASTAR_ALLOW_DIAGONALS, map, foo );
			}
			if ( io.mouse.IsButtonDown( Mouse::Button::LEFT ) ) {
				if ( io.keyboard.IsKeyDown( KEY_LEFT_SHIFT ) ) {
					// delete cell
					if ( hoveredEntity != INVALID_ENTITY_ID ) {
						CpntBuilding * building = registery.TryGetComponent< CpntBuilding >( hoveredEntity );
						if ( building != nullptr ) {
							registery.MarkForDelete( hoveredEntity );
							for ( u32 x = building->cell.x; x < building->cell.x + building->tileSizeX; x++ ) {
								for ( u32 z = building->cell.z; z < building->cell.z + building->tileSizeZ; z++ ) {
									map.SetTile( x, z, MapTile::EMPTY );
								}
							}
						}
					} else if ( map.GetTile( mouseCellPosition ) == MapTile::ROAD ) {
						roadBatchedTiles.RemoveInstancesWithPosition( GetPointInCornerOfCell( mouseCellPosition ) );
						map.SetTile( mouseCellPosition, MapTile::EMPTY );
					}
				} else if ( map.GetTile( mouseCellPosition ) == MapTile::EMPTY ) {
					// Build road cell
					SpawnRoadBlock( registery, map, mouseCellPosition );
				}
			}
		}
		{
			theGame->window.BindDefaultFramebuffer();
			mainCamera.Bind();
			Render();
			static glm::vec3 lightDirection( -1.0f, -1.0f, 0.0f );
			ImGui::DragFloat3( "lightDirection", &lightDirection[ 0 ] );

			static float lightAmbiant = 0.7f;
			static float lightDiffuse = 0.5f;
			static float lightSpecular = 1.0f;
			ImGui::SliderFloat( "light ambiant", &lightAmbiant, 0.0f, 1.0f );
			ImGui::SliderFloat( "light diffuse", &lightDiffuse, 0.0f, 1.0f );
			ImGui::SliderFloat( "light specular", &lightSpecular, 0.0f, 1.0f );

			LightUBOData lightUBOData{};
			lightUBOData.direction = glm::vec4( glm::normalize( lightDirection ), 1.0f );
			lightUBOData.ambient = glm::vec4( glm::vec3( lightAmbiant ), 1.0f );
			lightUBOData.diffuse = glm::vec4( glm::vec3( lightDiffuse ), 1.0f );
			lightUBOData.specular = glm::vec4( glm::vec3( lightSpecular ), 1.0f );
			FillLightUBO( &lightUBOData );

			Shader & defaultShader = g_shaderAtlas.defaultShader;
			defaultShader.Use();

			// Just push the ground a tiny below y to avoid clipping
			CpntTransform groundTransform;
			groundTransform.SetTranslation( { 0.0f, -0.01f, 0.0f } );
			DrawModel( groundModel, groundTransform, defaultShader );

			roadBatchedTiles.Render();

			defaultShader.Use();
			for ( auto const & [ e, renderModel ] : registery.IterateOver< CpntRenderModel >() ) {
				if ( renderModel.model != nullptr ) {
					DrawModel( *renderModel.model, registery.GetComponent< CpntTransform >( e ), defaultShader );
				}
			}

			static bool drawCollisionBoxes = false;
			ImGui::Checkbox( "draw collision boxes", &drawCollisionBoxes );
			if ( drawCollisionBoxes ) {
				for ( auto const & [ e, box ] : registery.IterateOver< CpntBoxCollider >() ) {
					const CpntTransform & transform = registery.GetComponent< CpntTransform >( e );
					Guizmo::LinesAroundCube( transform.GetMatrix() * glm::vec4( box.center, 1.0f ), box.size,
					                         Guizmo::colGreen );
				}
			}

			static bool drawRoadNodes = false;
			ImGui::Checkbox( "draw road nodes", &drawRoadNodes );
			if ( drawRoadNodes ) {
				for ( auto & node : map.roadNetwork.nodes ) {
					Guizmo::Rectangle( GetPointInCornerOfCell( node.position ), 1.0f, 1.0f, Guizmo::colBlue );
				}
			}

			Guizmo::Draw();
		}

		ng::GetConsole().Draw();
		DrawDebugWindow();

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

	modelInspectorFramebuffer.Destroy();
	ShutdownRenderer();
	g_shaderAtlas.FreeShaders();
	g_modelAtlas.FreeAllModels();

	window.Shutdown();
	ng::Shutdown();

	delete theGame;

	SDL_Quit();
	return 0;
}

void DrawDebugWindow() {
	// static bool opened = true;
	// ImGui::ShowDemoWindow( &opened );
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
	if ( ImGui::TreeNode( "Model inspector" ) ) {
		static glm::vec3 cameraPosition = glm::vec3( 5.0f, 5.0f, 5.0f );
		ImGui::DragFloat3( "camera position", &cameraPosition.x );
		modelInspectorCamera.position = cameraPosition;
		modelInspectorCamera.proj =
		    glm::perspective( 90.0f, ( float )modelInspectorWidth / modelInspectorHeight, 0.01f, 100.0f );
		modelInspectorCamera.view =
		    glm::lookAt( modelInspectorCamera.position, glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 0.0f, 1.0f, 0.0f ) );

		modelInspectorCamera.Bind();
		modelInspectorFramebuffer.Bind();
		modelInspectorFramebuffer.Clear();
		CpntTransform localTransform;
		DrawModel( *( g_modelAtlas.farmMesh ), localTransform, g_shaderAtlas.defaultShader );

		ImGui::Image( ( ImTextureID )modelInspectorFramebuffer.textureID,
		              ImVec2( modelInspectorWidth, modelInspectorHeight ), ImVec2( 0, 1 ), ImVec2( 1, 0 ) );
		theGame->window.BindDefaultFramebuffer();
		mainCamera.Bind();
		ImGui::TreePop();
	}
	// if ( ImGui::TreeNode( "Systems" ) ) {
	//	std::vector< std::pair< const char *, ISystem * > > systemWithNames;
	//	for ( auto & [ type, system ] : systemManager.systems ) {
	//		systemWithNames.push_back( { type.name(), system } );
	//	}

	//	static int currentlySelected = 0;
	//	if ( ImGui::BeginCombo( "system selected", systemWithNames[ currentlySelected ].first ) ) {
	//		for ( int i = 0; i < systemWithNames.size(); i++ ) {
	//			bool isSelected = currentlySelected == i;
	//			if ( ImGui::Selectable( systemWithNames[ i ].first, isSelected ) ) {
	//				currentlySelected = i;
	//			}

	//			if ( isSelected ) {
	//				ImGui::SetItemDefaultFocus();
	//			}
	//		}
	//		ImGui::EndCombo();
	//	}
	//	systemWithNames[ currentlySelected ].second->DebugDraw();
	//	ImGui::TreePop();
	//}
	if ( ImGui::Button( "Check road network integrity" ) ) {
		bool ok = theGame->map.roadNetwork.CheckNetworkIntegrity();
		if ( ok ) {
			ng::Printf( "Road network looks fine!\n" );
		}
	}

	ImGui::End();
}
