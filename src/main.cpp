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

#include "buildings/building.h"
#include "buildings/placement.h"
#include "collider.h"
#include "entity.h"
#include "game.h"
#include "guizmo.h"
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
#include "pathfinding_job.h"
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

Camera mainCamera;

static void Update( float dt ) {}

static void FixedUpdate( Duration ticks ) { theGame->systemManager.Update( theGame->registery, ticks ); }

static void Render() {}

InstancedModelBatch roadBatchedTiles;

void SpawnRoadTile( Registery & reg, Map & map, Cell cell ) {
	if ( map.GetTile( cell ) != MapTile::ROAD ) {
		map.SetTile( cell, MapTile::ROAD );
		roadBatchedTiles.AddInstanceAtPosition( GetPointInCornerOfCell( cell ) );
	}
}

void DeleteRoadTile( Registery & reg, Map & map, Cell cell ) {
	if ( map.GetTile( cell ) == MapTile::ROAD || map.GetTile( cell ) == MapTile::ROAD_BLOCK ) {
		map.SetTile( cell, MapTile::EMPTY );
		roadBatchedTiles.RemoveInstancesWithPosition( GetPointInCornerOfCell( cell ) );
	}
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

	Window &   window = theGame->window;
	IO &       io = theGame->io;
	Renderer & renderer = theGame->renderer;

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

	g_shaderAtlas.CompileAllShaders();
	renderer.InitRenderer( window.width, window.height );

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
	theGame->systemManager.CreateSystem< SystemHousing >();
	theGame->systemManager.CreateSystem< SystemBuildingProducing >();
	theGame->systemManager.CreateSystem< SystemNavAgent >();
	theGame->systemManager.CreateSystem< SystemMarket >();
	theGame->systemManager.CreateSystem< SystemSeller >();
	theGame->systemManager.CreateSystem< SystemServiceBuilding >();
	theGame->systemManager.CreateSystem< SystemServiceWanderer >();
	theGame->systemManager.CreateSystem< SystemResourceInventory >();
	theGame->systemManager.CreateSystem< SystemFetcher >();
	theGame->systemManager.CreateSystem< SystemPathfinding >();
	theGame->systemManager.CreateSystem< SystemMigrant >();

	theGame->systemManager.StartJobs();

	Model groundModel;
	CreateTexturedPlane( 200.0f, 200.0f, 64.0f,
	                     *( theGame->package.GrabResource( PackerResources::GRASS_TEXTURE_PNG ) ), groundModel );
	Model roadModel;
	CreateTexturedPlane( 1.0f, 1.0f, 1.0f, *( theGame->package.GrabResource( PackerResources::ROAD_TEXTURE_PNG ) ),
	                     roadModel );

	roadBatchedTiles.Init( &roadModel );

	Map &       map = theGame->map;
	Registery & registery = theGame->registery;
	map.AllocateGrid( 200, 200 );

	while ( !window.shouldClose ) {
		ZoneScopedN( "MainLoop" );

		auto  currentFrameTime = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration_cast< std::chrono::microseconds >( currentFrameTime - lastFrameTime ).count() /
		           1000000.0f;
		if ( dt > 1.0f ) {
			// delta time is higher than 1 second, this probably means we breaked in the debugger, let's go back to
			// normal
			dt = 1.0f / 60.0f;
		}

		static bool pauseSim = false;
		if ( io.keyboard.IsKeyPressed( KEY_SPACE ) ) {
			pauseSim = !pauseSim;
		}

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
		// for ( int i = 0; i < numFixedSteps; i++ ) {
		theGame->clock += numFixedSteps;
		FixedUpdate( numFixedSteps );
		//}
		Update( pauseSim ? 0.0f : dt );
		{
			ZoneScopedN( "Update that should go away" );

			static CpntTransform cameraTargetTransform;
			static glm::vec3     cameraTargetNewPosition( 0.0f, 0.0f, 0.0f );
			static float         cameraTargetRotation = 45.0f;
			ImGui::SliderFloat( "camera target rotation angle", &cameraTargetRotation, -180.0f, 180.0f );
			cameraTargetTransform.SetRotation( { 0.0f, cameraTargetRotation, 0.0f } );
			ImGui::Text( "camera target front: %f %f %f\n", cameraTargetTransform.Front().x,
			             cameraTargetTransform.Front().y, cameraTargetTransform.Front().z );

			static float movementSpeed = 0.5f;
			static float scrollSpeed = 1.0f;
			static float movementTime = 5.0f;
			ImGui::SliderFloat( "movement speed", &movementSpeed, 0.0f, 90.0f );
			ImGui::SliderFloat( "scroll speed ", &scrollSpeed, 0.0f, 90.0f );
			ImGui::SliderFloat( "movement time", &movementTime, 0.0f, 90.0f );

			static float cameraDistance = 250.0f;
			static float cameraNewDistance = cameraDistance;
			static float cameraRotationAngle = 45.0f;
			ImGui::SliderFloat( "camera distance", &cameraDistance, 0.0f, 500.0f );
			ImGui::SliderFloat( "camera rotation angle", &cameraRotationAngle, -180.0f, 180.0f );

			// the closer the camera is, the slower we want to go
			// Consider base speed is the speed when the camera is 250 unit away up
			float scaledMovementSpeed = cameraDistance / 250.0f * movementSpeed;

			if ( io.keyboard.IsKeyDown( eKey::KEY_D ) )
				cameraTargetNewPosition += ( cameraTargetTransform.Right() * scaledMovementSpeed );
			if ( io.keyboard.IsKeyDown( eKey::KEY_A ) )
				cameraTargetNewPosition -= ( cameraTargetTransform.Right() * scaledMovementSpeed );
			if ( io.keyboard.IsKeyDown( eKey::KEY_W ) )
				cameraTargetNewPosition += ( cameraTargetTransform.Front() * scaledMovementSpeed );
			if ( io.keyboard.IsKeyDown( eKey::KEY_S ) )
				cameraTargetNewPosition -= ( cameraTargetTransform.Front() * scaledMovementSpeed );

			cameraTargetTransform.SetTranslation(
			    glm::mix( cameraTargetTransform.GetTranslation(), cameraTargetNewPosition, dt * movementTime ) );
			ImGui::Text( "Camera target position: %f %f %f\n", cameraTargetTransform.GetTranslation().x,
			             cameraTargetTransform.GetTranslation().y, cameraTargetTransform.GetTranslation().z );

			cameraNewDistance += io.mouse.wheelMotion.y * scrollSpeed;
			cameraDistance = glm::mix( cameraDistance, cameraNewDistance, dt * movementTime );

			CpntTransform cameraTransform;
			cameraTransform.SetTranslation( { 0.0f, cameraDistance, -cameraDistance } );
			cameraTransform.SetRotation( { cameraRotationAngle, 0.0f, 0.0f } );
			ImGui::Text( "Camera position before transform: %f %f %f\n", cameraTransform.GetTranslation().x,
			             cameraTransform.GetTranslation().y, cameraTransform.GetTranslation().z );
			ImGui::Text( "Camera front before transform: %f %f %f\n", cameraTransform.Front().x,
			             cameraTransform.Front().y, cameraTransform.Front().z );
			cameraTransform = cameraTargetTransform * cameraTransform;
			ImGui::Text( "Camera target position after transform: %f %f %f\n", cameraTransform.GetTranslation().x,
			             cameraTransform.GetTranslation().y, cameraTransform.GetTranslation().z );
			ImGui::Text( "Camera front after transform: %f %f %f\n", cameraTransform.Front().x,
			             cameraTransform.Front().y, cameraTransform.Front().z );

			mainCamera.position = cameraTransform.GetTranslation();
			mainCamera.front = cameraTransform.Front();
			mainCamera.view =
			    glm::lookAt( cameraTransform.GetTranslation(),
			                 cameraTransform.GetTranslation() + cameraTransform.Front(), cameraTransform.Up() );

			float           aspectRatio = ( float )window.width / window.height;
			static float    fovY = 10.0f;
			constexpr float nearPlane = 0.1f;
			constexpr float farPlane = 1000.0f;
			ImGui::SliderFloat( "fovy", &fovY, 0.0f, 90.0f );
			mainCamera.proj = glm::perspective( glm::radians( fovY ), aspectRatio, nearPlane, farPlane );

			ImGui::Text( "Mouse position: %d %d", io.mouse.position.x, io.mouse.position.y );
			ImGui::Text( "Mouse offset: %f %f", io.mouse.offset.x, io.mouse.offset.y );

			// Mouse position in normalized device coordinates
			float     x = ( 2.0f * io.mouse.position.x ) / window.width - 1.0f;
			float     y = 1.0f - ( 2.0f * io.mouse.position.y ) / window.height;
			float     z = 1.0f;
			glm::vec3 ray_nds( x, y, z );

			// Homogeneous Clip Coordinates
			glm::vec4 ray_clip( ray_nds.x, ray_nds.y, -1.0f, 1.0f );

			// Eye coordinates
			glm::vec4 ray_eye = glm::inverse( mainCamera.proj ) * ray_clip;
			ray_eye = glm::vec4( ray_eye.x, ray_eye.y, -1.0f, 0.0f );

			// world coordinates
			glm::vec3 ray_world = glm::inverse( mainCamera.view ) * ray_eye;

			Ray mouseRaycast;
			mouseRaycast.origin = cameraTransform.GetTranslation();
			mouseRaycast.direction = glm::normalize( ray_world );
			ng_assert( mouseRaycast.direction.y != 0.0f );
			float     a = -mouseRaycast.origin.y / mouseRaycast.direction.y;
			glm::vec3 mouseProjectionOnGround = mouseRaycast.origin + a * mouseRaycast.direction;
			ImGui::Text( "Mouse projection on ground : %f %f %f", mouseProjectionOnGround.x, mouseProjectionOnGround.y,
			             mouseProjectionOnGround.z );

			glm::vec3 mouseProjectionOnGroundFloored( ( int )floorf( mouseProjectionOnGround.x ), 0.0f,
			                                          ( int )floorf( mouseProjectionOnGround.z ) );

			Cell   mouseCellPosition = GetCellForPoint( mouseProjectionOnGroundFloored );
			Entity hoveredEntity = INVALID_ENTITY;
			for ( auto const & [ e, building ] : registery.IterateOver< CpntBuilding >() ) {
				if ( IsCellInsideBuilding( building, mouseCellPosition ) ) {
					hoveredEntity = e;
					break;
				}
			}

			static MouseAction  currentMouseAction = MouseAction::SELECT;
			static BuildingKind buildingKindSelected;
			static Cell         mouseDragCellStart = INVALID_CELL;
			static bool         mouseStartedDragging = false;
			static Entity       selectedEntity = INVALID_ENTITY;
			{
				if ( io.keyboard.IsKeyPressed( KEY_V ) ) {
					currentMouseAction = MouseAction::BUILD;
					buildingKindSelected = BuildingKind::HOUSE;
				}
				if ( ImGui::Begin( "Buildings" ) ) {
					if ( ImGui::Button( "Road" ) ) {
						currentMouseAction = MouseAction::BUILD_ROAD;
					}
					if ( ImGui::Button( "Road Block" ) ) {
						currentMouseAction = MouseAction::BUILD;
						buildingKindSelected = BuildingKind::ROAD_BLOCK;
					}
					if ( ImGui::Button( "House" ) ) {
						currentMouseAction = MouseAction::BUILD;
						buildingKindSelected = BuildingKind::HOUSE;
					}
					if ( ImGui::Button( "Farm" ) ) {
						currentMouseAction = MouseAction::BUILD;
						buildingKindSelected = BuildingKind::FARM;
					}
					if ( ImGui::Button( "Storehouse" ) ) {
						currentMouseAction = MouseAction::BUILD;
						buildingKindSelected = BuildingKind::STORAGE_HOUSE;
					}
					if ( ImGui::Button( "Market" ) ) {
						currentMouseAction = MouseAction::BUILD;
						buildingKindSelected = BuildingKind::MARKET;
					}
					if ( ImGui::Button( "Fountain" ) ) {
						currentMouseAction = MouseAction::BUILD;
						buildingKindSelected = BuildingKind::FOUNTAIN;
					}
					if ( ImGui::Button( "Delete" ) ) {
						currentMouseAction = MouseAction::DESTROY;
					}
					ImGui::End();
				}
			}

			if ( selectedEntity != INVALID_ENTITY ) {
				if ( ImGui::Begin( "Entity selected" ) ) {
					ImGui::Text( "ID: %llu\n", selectedEntity );
					if ( registery.HasComponent< CpntHousing >( selectedEntity ) ) {
						auto & housing = registery.GetComponent< CpntHousing >( selectedEntity );
						ImGui::Text( "Number of habitants: %d / %d\n", housing.numCurrentlyLiving,
						             housing.maxHabitants );
						ImGui::Text( "Number of incoming migrants : %d\n", housing.numIncomingMigrants );
						for ( int i = 0; i < ( int )GameService::NUM_SERVICES; i++ ) {
							if ( housing.isServiceRequired[ i ] == true ) {
								ImGui::Text(
								    "Service %d: is fulfilled ? %s, last accessed %lld frames ago", i,
								    IsServiceFulfilled( housing.lastServiceAccess[ i ], theGame->clock ) ? "Yes" : "No",
								    theGame->clock - housing.lastServiceAccess[ i ] );
							}
						}
					}
					if ( registery.HasComponent< CpntBuildingProducing >( selectedEntity ) ) {
						auto & producer = registery.GetComponent< CpntBuildingProducing >( selectedEntity );
						float  timeSinceLastProduction = DurationToSeconds( producer.timeSinceLastProduction );
						float  timeToProduce = DurationToSeconds( producer.timeToProduceBatch );
						ImGui::SliderFloat( "production", &timeSinceLastProduction, 0.0f, timeToProduce );
					}

					if ( registery.HasComponent< CpntResourceInventory >( selectedEntity ) ) {
						auto & storage = registery.GetComponent< CpntResourceInventory >( selectedEntity );
						ImGui::Text( "Storage content" );
						for ( int i = 0; i < ( int )GameResource::NUM_RESOURCES; i++ ) {
							const char * name = GameResourceToString( ( GameResource )i );
							ImGui::Text( "%s: %d\n", name, storage.GetResourceAmount( ( GameResource )i ) );
						}
					}
					ImGui::End();
				}
			}

			if ( currentMouseAction == MouseAction::SELECT ) {
				// TODO: how could we highlight object under cursor?
				if ( io.mouse.IsButtonPressed( Mouse::Button::LEFT ) ) {
					selectedEntity = FindBuildingByPosition( registery, mouseCellPosition );
				}
			} else {
				selectedEntity = INVALID_ENTITY;
			}

			if ( currentMouseAction == MouseAction::BUILD ) {
				auto size = GetBuildingSize( buildingKindSelected );
				auto color = CanPlaceBuilding( mouseCellPosition, buildingKindSelected, map ) ? Guizmo::colGreen
				                                                                              : Guizmo::colRed;
				Guizmo::Rectangle( GetPointInCornerOfCell( mouseCellPosition ), ( float )size.x, ( float )size.y,
				                   color );
			}
			if ( currentMouseAction == MouseAction::BUILD_ROAD && mouseStartedDragging == true ) {
				for ( u32 x = mouseDragCellStart.x; x != mouseCellPosition.x;
				      mouseDragCellStart.x < mouseCellPosition.x ? x++ : x-- ) {
					Cell cell( x, mouseDragCellStart.z );
					auto color = map.GetTile( cell ) == MapTile::EMPTY ? Guizmo::colGreen : Guizmo::colRed;
					Guizmo::Rectangle( GetPointInCornerOfCell( cell ), 1, 1, color );
				}
				for ( u32 z = mouseDragCellStart.z; z != mouseCellPosition.z;
				      mouseDragCellStart.z < mouseCellPosition.z ? z++ : z-- ) {
					Cell cell( mouseCellPosition.x, z );
					auto color = map.GetTile( cell ) == MapTile::EMPTY ? Guizmo::colGreen : Guizmo::colRed;
					Guizmo::Rectangle( GetPointInCornerOfCell( cell ), 1, 1, color );
				}
				Cell cell( mouseCellPosition.x, mouseCellPosition.z );
				auto color = map.GetTile( cell ) == MapTile::EMPTY ? Guizmo::colGreen : Guizmo::colRed;
				Guizmo::Rectangle( GetPointInCornerOfCell( cell ), 1, 1, color );
			}
			if ( currentMouseAction == MouseAction::DESTROY && mouseStartedDragging == true ) {
				Cell start( MIN( mouseDragCellStart.x, mouseCellPosition.x ),
				            MIN( mouseDragCellStart.z, mouseCellPosition.z ) );
				int  offsetX = ABS( ( int )mouseCellPosition.x - ( int )mouseDragCellStart.x );
				int  offsetZ = ABS( ( int )mouseCellPosition.z - ( int )mouseDragCellStart.z );
				if ( offsetX == 0 ) {
					offsetX = 1;
				}
				if ( offsetZ == 0 ) {
					offsetZ = 1;
				}
				Guizmo::Rectangle( GetPointInCornerOfCell( start ), offsetX, offsetZ, Guizmo::colRed );
			}

			if ( io.mouse.IsButtonPressed( Mouse::Button::LEFT ) ) {
				if ( currentMouseAction == MouseAction::BUILD ) {
					if ( CanPlaceBuilding( mouseCellPosition, buildingKindSelected, map ) ) {
						PlaceBuilding( registery, mouseCellPosition, buildingKindSelected, map );
					}
				}
				if ( currentMouseAction == MouseAction::BUILD_ROAD || currentMouseAction == MouseAction::DESTROY ) {
					mouseStartedDragging = true;
					mouseDragCellStart = mouseCellPosition;
				}
			}

			if ( currentMouseAction == MouseAction::BUILD_ROAD &&
			     io.mouse.IsButtonDown( Mouse::Button::LEFT ) == false && mouseStartedDragging == true ) {
				for ( u32 x = mouseDragCellStart.x; x != mouseCellPosition.x;
				      mouseDragCellStart.x < mouseCellPosition.x ? x++ : x-- ) {
					Cell cell( x, mouseDragCellStart.z );
					if ( map.GetTile( cell ) == MapTile::EMPTY ) {
						SpawnRoadTile( registery, map, cell );
					}
				}
				for ( u32 z = mouseDragCellStart.z; z != mouseCellPosition.z;
				      mouseDragCellStart.z < mouseCellPosition.z ? z++ : z-- ) {
					Cell cell( mouseCellPosition.x, z );
					if ( map.GetTile( cell ) == MapTile::EMPTY ) {
						SpawnRoadTile( registery, map, cell );
					}
				}
				Cell cell( mouseCellPosition.x, mouseCellPosition.z );
				if ( map.GetTile( cell ) == MapTile::EMPTY ) {
					SpawnRoadTile( registery, map, cell );
				}

				mouseStartedDragging = false;
			}

			if ( currentMouseAction == MouseAction::DESTROY && io.mouse.IsButtonDown( Mouse::Button::LEFT ) == false &&
			     mouseStartedDragging == true ) {
				Cell start( MIN( mouseDragCellStart.x, mouseCellPosition.x ),
				            MIN( mouseDragCellStart.z, mouseCellPosition.z ) );
				int  offsetX = ABS( ( int )mouseCellPosition.x - ( int )mouseDragCellStart.x );
				int  offsetZ = ABS( ( int )mouseCellPosition.z - ( int )mouseDragCellStart.z );
				if ( offsetX == 0 ) {
					offsetX = 1;
				}
				if ( offsetZ == 0 ) {
					offsetZ = 1;
				}

				for ( u32 x = start.x; x < start.x + offsetX; x++ ) {
					for ( u32 z = start.z; z < start.z + offsetZ; z++ ) {
						if ( map.GetTile( x, z ) == MapTile::ROAD || map.GetTile( x, z ) == MapTile::ROAD_BLOCK ) {
							DeleteRoadTile( registery, map, Cell( x, z ) );
						}
					}
				}

				Area areaOfDeletion{};
				areaOfDeletion.center = start;
				areaOfDeletion.sizeX = offsetX;
				areaOfDeletion.sizeZ = offsetZ;
				DeleteBuildingsInsideArea( registery, areaOfDeletion, map );

				mouseStartedDragging = false;
			}

			if ( ( io.mouse.IsButtonDown( Mouse::Button::RIGHT ) || io.keyboard.IsKeyDown( KEY_ESCAPE ) ) &&
			     currentMouseAction != MouseAction::SELECT ) {
				currentMouseAction = MouseAction::SELECT;
				mouseStartedDragging = false;
			}
		}
		{
			ZoneScopedN( "Render" );
			theGame->window.BindDefaultFramebuffer();
			mainCamera.Bind();
			Render();
			static glm::vec3 lightDirection( -1.0f, -1.0f, 0.0f );
			ImGui::DragFloat3( "lightDirection", &lightDirection[ 0 ] );

			static float lightAmbiant = 0.7f;
			static float lightDiffuse = 0.5f;
			static float lightSpecular = 0.0f;
			ImGui::SliderFloat( "light ambiant", &lightAmbiant, 0.0f, 1.0f );
			ImGui::SliderFloat( "light diffuse", &lightDiffuse, 0.0f, 1.0f );
			ImGui::SliderFloat( "light specular", &lightSpecular, 0.0f, 1.0f );

			LightUBOData lightUBOData{};
			lightUBOData.direction = glm::vec4( glm::normalize( lightDirection ), 1.0f );
			lightUBOData.ambient = glm::vec4( glm::vec3( lightAmbiant ), 1.0f );
			lightUBOData.diffuse = glm::vec4( glm::vec3( lightDiffuse ), 1.0f );
			lightUBOData.specular = glm::vec4( glm::vec3( lightSpecular ), 1.0f );
			renderer.FillLightUBO( &lightUBOData );

			// Just push the ground a tiny below y to avoid clipping
			CpntTransform groundTransform;
			groundTransform.SetTranslation( { 0.0f, -0.1f, 0.0f } );

			renderer.GeometryPass( registery, &groundModel, &groundTransform, 1, &roadBatchedTiles, 1 );
			renderer.LigthningPass();
			renderer.PostProcessPass();

			window.BindDefaultFramebuffer();
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
				for ( auto & node : theGame->roadNetwork.nodes ) {
					Guizmo::Rectangle( GetPointInCornerOfCell( node.position ), 1.0f, 1.0f, Guizmo::colBlue );
				}
			}

			Guizmo::Draw();
		}

		ng::GetConsole().Draw();
		DrawDebugWindow();

		{
			constexpr bool renderImgui = true;
			if ( renderImgui ) {
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
			} else {
				ZoneScopedN( "Render_IMGUI" );
				ImGui::EndFrame();
				if ( imio.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
					SDL_Window *  backup_current_window = SDL_GL_GetCurrentWindow();
					SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
					ImGui::UpdatePlatformWindows();
					SDL_GL_MakeCurrent( backup_current_window, backup_current_context );
				}
				// ImGui::Render();
				// ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
				// if ( imio.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
				//	SDL_Window *  backup_current_window = SDL_GL_GetCurrentWindow();
				//	SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
				//	ImGui::UpdatePlatformWindows();
				//	ImGui::RenderPlatformWindowsDefault();
				//	SDL_GL_MakeCurrent( backup_current_window, backup_current_context );
				//}
			}
		}

		window.SwapBuffers();
		TracyGpuCollect;
		FrameMark;
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	renderer.ShutdownRenderer();
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
	if ( ImGui::TreeNode( "Components" ) ) {
		theGame->registery.DebugDraw();
		ImGui::TreePop();
	}
	if ( ImGui::Button( "Check road network integrity" ) ) {
		bool ok = theGame->roadNetwork.CheckNetworkIntegrity();
		if ( ok ) {
			ng::Printf( "Road network looks fine!\n" );
		}
	}

	if ( ImGui::Begin( "Renderer" ) ) {
		theGame->renderer.DebugDraw();
	}
	ImGui::End();

	ImGui::End();
}
