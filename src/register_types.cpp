#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "player.h"
#include "terrain_builder.h"
#include "door.h"
#include "interactable.h"
#include "window.h"
#include "chest.h"
#include "well.h"
#include "sky_cycle.h"
#include "ice_surface.h"
#include "bird.h"
#include "rabbit.h"
#include "hud_clock.h"
#include "butterfly_swarm.h"
#include "duck_flock.h"
#include "lake_jump_fx.h"
#include "fish_school.h"
#include "dragonfly_swarm.h"
#include "frog_pond.h"
#include "berry_bush.h"
#include "farm_plot.h"
#include "game_director.h"
#include "pause_menu.h"
#include "ambience.h"
#include "save_system.h"
#include "farm_animal.h"
#include "fishing_system.h"
#include "footprint_manager.h"
#include "bed.h"
#include "wardrobe.h"
#include "soccer_ball.h"
#include "soccer_field.h"
#include "parkour_poles.h"
#include "update_manager.h"

using namespace godot;

// 引擎加载本扩展库时会走到这里，在这里登记你所有的 C++ 类。
void initialize_game3d_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_ABSTRACT_CLASS(Interactable);
	GDREGISTER_CLASS(Player);
	GDREGISTER_CLASS(TerrainBuilder);
	GDREGISTER_CLASS(CabinDoor);
	GDREGISTER_CLASS(ShutterWindow);
	GDREGISTER_CLASS(Chest);
	GDREGISTER_CLASS(Well);
	GDREGISTER_CLASS(SkyCycle);
	GDREGISTER_CLASS(IceSurface);
	GDREGISTER_CLASS(Bird);
	GDREGISTER_CLASS(Rabbit);
	GDREGISTER_CLASS(HUDClock);
	GDREGISTER_CLASS(ButterflySwarm);
	GDREGISTER_CLASS(DuckFlock);
	GDREGISTER_CLASS(LakeJumpFx);
	GDREGISTER_CLASS(FishSchool);
	GDREGISTER_CLASS(DragonflySwarm);
	GDREGISTER_CLASS(FrogPond);
	GDREGISTER_CLASS(BerryBush);
	GDREGISTER_CLASS(FarmPlot);
	GDREGISTER_CLASS(GameDirector);
	GDREGISTER_CLASS(PauseMenu);
	GDREGISTER_CLASS(Ambience);
	GDREGISTER_CLASS(SaveSystem);
	GDREGISTER_CLASS(FarmAnimal);
	GDREGISTER_CLASS(FishingSystem);
	GDREGISTER_CLASS(FootprintManager);
	GDREGISTER_CLASS(Bed);
	GDREGISTER_CLASS(Wardrobe);
	GDREGISTER_CLASS(SoccerBall);
	GDREGISTER_CLASS(SoccerField);
	GDREGISTER_CLASS(ParkourPoles);
	GDREGISTER_CLASS(UpdateManager);
}

void uninitialize_game3d_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {
// GDExtension 的入口符号，名字必须和 game3d.gdextension 里的 entry_symbol 一致。
GDExtensionBool GDE_EXPORT game3d_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_game3d_module);
	init_obj.register_terminator(uninitialize_game3d_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
