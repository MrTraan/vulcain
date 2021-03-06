#include <GL/gl3w.h>

#define STB_IMAGE_IMPLEMENTATION // force following include to generate implementation
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION

#include <SDL.h>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_sdl.h>
#include <random>
#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>
#include <unordered_map>

#include "buildings/building.h"
#include "buildings/debug_dump.h"
#include "buildings/delivery.h"
#include "buildings/placement.h"
#include "buildings/resource_fetcher.h"
#include "buildings/storage_house.h"
#include "buildings/woodworking.h"
#include "collider.h"
#include "entity.h"
#include "environment/trees.h"
#include "game.h"
#include "guizmo.h"
#include "mesh.h"
#include "navigation.h"
#include "ngLib/ngcontainers.h"
#include "ngLib/nglib.h"
#include "packer.h"
#include "packer_resource_list.h"
#include "pathfinding_job.h"
#include "registery.h"
#include "renderer.h"
#include "shader.h"
#include "shadows.h"
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

#elif defined( __APPLE__ )

#else
NG_UNSUPPORTED_PLATFORM // GOOD LUCK LOL
#endif

#if defined( BENCHMARK_ENABLED )
#include "../test/benchmarks.h"
#endif

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
Camera shadowCamera;

static void Update( float dt ) {}

static void FixedUpdate( Duration ticks ) { theGame->systemManager.Update( *theGame->registery, ticks ); }

InstancedModelBatch roadBatchedTiles;

struct SystemTransform : public System< CpntTransform > {};

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
	theGame->registery = new Registery( &theGame->systemManager );

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

	if ( OPENGL_VERSION_MAJOR >= 4 && OPENGL_VERSION_MINOR >= 3 ) {
		glEnable( GL_DEBUG_OUTPUT );
		glDebugMessageCallback( GLErrorCallback, 0 );
	}

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

	g_modelAtlas.LoadAllModels();

	// Register system
	theGame->systemManager.CreateSystem< SystemRenderModel >();
	theGame->systemManager.CreateSystem< SystemTransform >();
	theGame->systemManager.CreateSystem< SystemBuilding >();
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
	theGame->systemManager.CreateSystem< SystemWoodshop >();
	theGame->systemManager.CreateSystem< SystemWoodworker >();
	theGame->systemManager.CreateSystem< SystemDeliveryGuy >();
	theGame->systemManager.CreateSystem< SystemStorageHouse >();
	theGame->systemManager.CreateSystem< SystemDebugDump >();
	theGame->systemManager.CreateSystem< SystemTree >();

	theGame->systemManager.StartJobs();

	Model groundModel;
	CreateTexturedPlane( 200.0f, 200.0f, 64.0f,
	                     *( theGame->package.GrabResource( PackerResources::GROUND_TEXTURE_MONOCOLOR_PNG ) ),
	                     groundModel );
	Model roadModel;
	CreateTexturedPlane( 1.0f, 1.0f, 1.0f, *( theGame->package.GrabResource( PackerResources::ROAD_TEXTURE_PNG ) ),
	                     roadModel );

	roadBatchedTiles.Init( &roadModel );

	Map &       map = theGame->map;
	Registery & registery = *theGame->registery;
	map.AllocateGrid( 200, 200 );

#if 1
	// Run functionnal tests
	for ( u32 x = 0; x < 20; x++ ) {
		SpawnRoadTile( registery, map, Cell( x, 0 ) );
	}
	PlaceBuilding( registery, Cell( 1, 1 ), BuildingKind::FOUNTAIN, map );
	PlaceBuilding( registery, Cell( 2, 1 ), BuildingKind::MARKET, map );
	PlaceBuilding( registery, Cell( 5, 1 ), BuildingKind::FARM, map );
	PlaceBuilding( registery, Cell( 8, 1 ), BuildingKind::STORAGE_HOUSE, map );
	// This is our reference house, it is the closest to the market
	Entity house = PlaceBuilding( registery, Cell( 11, 1 ), BuildingKind::HOUSE, map );
	PlaceBuilding( registery, Cell( 13, 1 ), BuildingKind::HOUSE, map );
	PlaceBuilding( registery, Cell( 15, 1 ), BuildingKind::HOUSE, map );
	PlaceBuilding( registery, Cell( 17, 1 ), BuildingKind::HOUSE, map );
	PlaceBuilding( registery, Cell( 19, 1 ), BuildingKind::HOUSE, map );
	// run for 2m
	for ( int64 i = 0; i < 120 * numTicksPerSeconds; i++ ) {
		theGame->clock++;
		FixedUpdate( 1 );
	}

	auto & cpntHousing = registery.GetComponent< CpntHousing >( house );
	ng_assert( cpntHousing.numCurrentlyLiving > 0 );
	ng_assert( cpntHousing.tier == 1 );
#endif

	{
		std::uniform_real_distribution< float > randomFloats( 0.0, 1.0 ); // random floats between [0.0, 1.0]
		std::default_random_engine              generator;
		auto &                                  reg = registery;
		for ( u32 x = 0; x < map.sizeX; x++ ) {
			for ( u32 z = 0; z < map.sizeZ; z++ ) {
				constexpr float treeGenerationThreshold = 0.75f;
				float           simplex = ( glm::simplex( glm::vec2( x / 64.0f, z / 64.0f ) ) + 1.0f ) / 2.0f;
				if ( simplex > treeGenerationThreshold ) {
					auto pine = reg.CreateEntity();
					Cell cell( x, z );
					map.SetTile( cell, MapTile::TREE );
					auto & transform = reg.AssignComponent< CpntTransform >( pine );
					transform.SetTranslation( GetPointInMiddleOfCell( cell ) );
					float modifier = 0.75f + randomFloats( generator ) / 2.0f;
					transform.SetScale( modifier );
					float modifierY = 0.75f + randomFloats( generator ) / 2.0f;
					transform.SetScaleY( modifierY );
					transform.SetRotation( { 0.0f, 360.0f * randomFloats( generator ), 0.0f } );
					reg.AssignComponent< CpntTree >( pine );
				}
				z += roundf( randomFloats( generator ) * 3.0f );
			}
			x += roundf( randomFloats( generator ) * 3.0f );
		}
	}

	auto  lastFrameTime = std::chrono::high_resolution_clock::now();
	float fixedTimeStepAccumulator = 0.0f;
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

		fixedTimeStepAccumulator += dt * theGame->speed;
		int numFixedSteps = ( int )floorf( fixedTimeStepAccumulator / FIXED_TIMESTEP );
		if ( numFixedSteps > 0 ) {
			fixedTimeStepAccumulator -= numFixedSteps * FIXED_TIMESTEP;
		}

		if ( numFixedSteps > 0 ) {
			theGame->clock += numFixedSteps;
			FixedUpdate( numFixedSteps );
		}

		Update( pauseSim ? 0.0f : dt );
		{
			ZoneScopedN( "Update that should go away" );

			static CpntTransform cameraTargetTransform;
			static glm::vec3     cameraTargetNewPosition( 20.0f, 0.0f, 20.0f );
			static float         cameraTargetRotation = 45.0f;
			static float         cameraTargetNewRotation = cameraTargetRotation;

			ImGui::SliderFloat( "camera target rotation angle", &cameraTargetRotation, -180.0f, 180.0f );
			cameraTargetTransform.SetRotation( { 0.0f, cameraTargetRotation, 0.0f } );
			ImGui::Text( "camera target front: %f %f %f\n", cameraTargetTransform.Front().x,
			             cameraTargetTransform.Front().y, cameraTargetTransform.Front().z );

			static float movementSpeed = 80.0f;
			static float scrollSpeed = 5.0f;
			static float rotationSpeed = 80.0f;
			static float movementTime = 5.0f;
			ImGui::SliderFloat( "movement speed", &movementSpeed, 0.0f, 180.0f );
			ImGui::SliderFloat( "rotation speed", &rotationSpeed, 0.0f, 180.0f );
			ImGui::SliderFloat( "scroll speed ", &scrollSpeed, 0.0f, 90.0f );
			ImGui::SliderFloat( "movement time", &movementTime, 0.0f, 90.0f );

			static float cameraDistance = 100.0f;
			static float cameraNewDistance = cameraDistance;
			static float cameraRotationAngle = 45.0f;
			ImGui::SliderFloat( "camera distance", &cameraDistance, 0.0f, 500.0f );
			ImGui::SliderFloat( "camera rotation angle", &cameraRotationAngle, -180.0f, 180.0f );

			// the closer the camera is, the slower we want to go
			// Consider base speed is the speed when the camera is 250 unit away up
			float scaledMovementSpeed = cameraDistance / 250.0f * movementSpeed * dt;

			if ( io.keyboard.IsKeyDown( eKey::KEY_D ) )
				cameraTargetNewPosition += ( cameraTargetTransform.Right() * scaledMovementSpeed );
			if ( io.keyboard.IsKeyDown( eKey::KEY_A ) )
				cameraTargetNewPosition -= ( cameraTargetTransform.Right() * scaledMovementSpeed );
			if ( io.keyboard.IsKeyDown( eKey::KEY_W ) )
				cameraTargetNewPosition += ( cameraTargetTransform.Front() * scaledMovementSpeed );
			if ( io.keyboard.IsKeyDown( eKey::KEY_S ) )
				cameraTargetNewPosition -= ( cameraTargetTransform.Front() * scaledMovementSpeed );

			if ( io.keyboard.IsKeyDown( eKey::KEY_Q ) )
				cameraTargetNewRotation -= rotationSpeed * dt;
			if ( io.keyboard.IsKeyDown( eKey::KEY_E ) )
				cameraTargetNewRotation += rotationSpeed * dt;
			cameraTargetRotation = glm::mix( cameraTargetRotation, cameraTargetNewRotation, dt * movementTime );

			cameraTargetTransform.SetTranslation(
			    glm::mix( cameraTargetTransform.GetTranslation(), cameraTargetNewPosition, dt * movementTime ) );
			ImGui::Text( "Camera target position: %f %f %f\n", cameraTargetTransform.GetTranslation().x,
			             cameraTargetTransform.GetTranslation().y, cameraTargetTransform.GetTranslation().z );

			cameraNewDistance -= io.mouse.wheelMotion.y * scrollSpeed;
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
			static float    farPlane = 1000.0f;
			ImGui::SliderFloat( "fovy", &fovY, 0.0f, 90.0f );
			ImGui::SliderFloat( "far plane", &farPlane, 0.0f, 1000.0f );
			mainCamera.proj = glm::perspective( glm::radians( fovY ), aspectRatio, nearPlane, farPlane );

			shadowCamera.position = mainCamera.position;
			shadowCamera.front = mainCamera.front;
			shadowCamera.view = mainCamera.view;
			shadowCamera.proj = glm::ortho( -20, 20, -20, 20 );

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
					if ( ImGui::Button( "Woodshop" ) ) {
						currentMouseAction = MouseAction::BUILD;
						buildingKindSelected = BuildingKind::WOODSHOP;
					}
					if ( ImGui::Button( "Fountain" ) ) {
						currentMouseAction = MouseAction::BUILD;
						buildingKindSelected = BuildingKind::FOUNTAIN;
					}
					if ( ImGui::Button( "[DEBUG] Dump" ) ) {
						currentMouseAction = MouseAction::BUILD;
						buildingKindSelected = BuildingKind::DEBUG_DUMP;
					}
					if ( ImGui::Button( "Delete" ) ) {
						currentMouseAction = MouseAction::DESTROY;
					}
					ImGui::End();
				}
			}

			if ( selectedEntity != INVALID_ENTITY ) {
				if ( ImGui::Begin( "Entity selected" ) ) {
					ImGui::Text( "ID: %u, version: %u\n", selectedEntity.id, selectedEntity.version );
					if ( registery.HasComponent< CpntBuilding >( selectedEntity ) ) {
						auto & building = registery.GetComponent< CpntBuilding >( selectedEntity );
						ImGui::Text( "Employees: %d/%d\n", building.workersEmployed, building.workersNeeded );
						ImGui::Text( "Connected to a road: %s\n", building.hasRoadConnection ? "Yes" : "No" );
					}
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
						const CpntBuilding & cpntBuilding = registery.GetComponent< CpntBuilding >( selectedEntity );
						float                efficiency = cpntBuilding.GetEfficiency();
						if ( efficiency > 0 ) {
							float timeToProduce = DurationToSeconds(
							    Duration( ( double )producer.timeToProduceBatch / ( double )efficiency ) );
							ImGui::SliderFloat( "production", &timeSinceLastProduction, 0.0f, timeToProduce );
						} else {
							ImGui::Text( "Stalling" );
						}
					}

					if ( registery.HasComponent< CpntResourceInventory >( selectedEntity ) ) {
						auto & storage = registery.GetComponent< CpntResourceInventory >( selectedEntity );
						ImGui::Text( "Storage content" );
						for ( int i = 0; i < ( int )GameResource::NUM_RESOURCES; i++ ) {
							const char * name = GameResourceToString( ( GameResource )i );
							ImGui::Text( "%s: %d\n", name, storage.GetResourceAmount( ( GameResource )i ) );
						}
					}

					if ( registery.HasComponent< CpntDebugDump >( selectedEntity ) ) {
						auto &               dump = registery.GetComponent< CpntDebugDump >( selectedEntity );
						static const char ** resourceList = nullptr;
						if ( resourceList == nullptr ) {
							resourceList = new const char *[ ( int )GameResource::NUM_RESOURCES ];
							ForEveryGameResource( resource ) {
								resourceList[ ( int )resource ] = GameResourceToString( resource );
							}
						}

						int currentIndex = ( int )dump.resourceToDump;
						if ( ImGui::Combo( "resource to dump", &currentIndex, resourceList,
						                   ( int )GameResource::NUM_RESOURCES ) ) {
							dump.resourceToDump = ( GameResource )currentIndex;
						}
					}
				}
				ImGui::End();
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
			static glm::vec3 lightDirection( 1.0f, -2.0f, 1.0f );
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

			static Frustrum cameraFrustum;
			cameraFrustum.Update( mainCamera.view, mainCamera.proj );
			auto shadowCameraViewProj = ComputeShadowCameraViewProj( registery, cameraFrustum, lightDirection );

			theGame->window.BindDefaultFramebuffer();
			ViewProjUBOData uboData{};
			uboData.view = mainCamera.view;
			uboData.projection = mainCamera.proj;
			uboData.viewProj = mainCamera.proj * mainCamera.view;
			uboData.shadowViewProj = shadowCameraViewProj;
			uboData.cameraPosition = glm::vec4( mainCamera.position, 1.0f );
			uboData.cameraFront = glm::vec4( mainCamera.front, 1.0f );
			theGame->renderer.FillViewProjUBO( &uboData );

			// Just push the ground a tiny below y to avoid clipping
			CpntTransform groundTransform;
			groundTransform.SetTranslation( { 0.0f, 0.0f, 0.0f } );

			renderer.GeometryPass( registery, &groundModel, &groundTransform, 1, &roadBatchedTiles, 1 );
			renderer.LigthningPass();
			renderer.PostProcessPass();
			renderer.DebugPass();

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

			{
				// Game stats UI
				if ( ImGui::Begin( "Game stats" ) ) {
					ImGui::Text( "Total population: %u\n",
					             theGame->systemManager.GetSystem< SystemHousing >().totalPopulation );
					ImGui::Text( "Total employed: %u\n",
					             theGame->systemManager.GetSystem< SystemBuilding >().totalEmployed );
					ImGui::Text( "Employees needed: %u\n",
					             theGame->systemManager.GetSystem< SystemBuilding >().totalEmployeesNeeded );
					ImGui::Text( "Chomeurs de la rue: %u\n",
					             theGame->systemManager.GetSystem< SystemBuilding >().totalUnemployed );
				}
				ImGui::End();
			}
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
	if ( ImGui::Begin( "Application" ) ) {
		ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
		             ImGui::GetIO().Framerate );
		ImGui::SliderFloat( "Game speed", &( theGame->speed ), 0.0f, 10.0f );
		if ( ImGui::TreeNode( "Window" ) ) {
			theGame->window.DebugDraw();
			ImGui::TreePop();
		}
		if ( ImGui::TreeNode( "IO" ) ) {
			theGame->io.DebugDraw();
			ImGui::TreePop();
		}
		if ( ImGui::TreeNode( "Systems" ) ) {
			theGame->systemManager.DebugDraw();
			ImGui::TreePop();
		}
		if ( ImGui::TreeNode( "Components" ) ) {
			theGame->registery->DebugDraw();
			ImGui::TreePop();
		}
		if ( ImGui::Button( "Check road network integrity" ) ) {
			bool ok = theGame->roadNetwork.CheckNetworkIntegrity();
			if ( ok ) {
				ng::Printf( "Road network looks fine!\n" );
			}
		}
	}
	ImGui::End();
	if ( ImGui::Begin( "Renderer" ) ) {
		theGame->renderer.DebugDraw();
	}
	ImGui::End();
}
