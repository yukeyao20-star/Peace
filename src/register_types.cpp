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
#include "bird_feeder.h"
#include "hud_clock.h"
#include "butterfly_swarm.h"
#include "duck_flock.h"
#include "lake_jump_fx.h"
#include "fish_school.h"
#include "dragonfly_swarm.h"
#include "frog_pond.h"
#include "rowboat.h"
#include "kelp_field.h"
#include "lily_pond.h"
#include "squirrel_field.h"
#include "shells.h"
#include "owl_perch.h"
#include "stone_path.h"
#include "cabin_life.h"
#include "dandelion_field.h"
#include "swing_set.h"
#include "icicle_field.h"
#include "camp_cat.h"
#include "dock_gull.h"
#include "wind_charms.h"
#include "stew_pot.h"
#include "woodpecker.h"
#include "deer_meadow.h"
#include "hammock.h"
#include "floating_lanterns.h"
#include "cabin_yard.h"
#include "rocking_chair.h"
#include "wish_lantern_dock.h"
#include "photo_frame.h"
#include "beehive.h"
#include "camp_tent.h"
#include "hedgehog.h"
#include "kite.h"
#include "snowman.h"
#include "dock_moths.h"
#include "heron_pond.h"
#include "campfire.h"
#include "hearth_fire.h"
#include "ice_hole.h"
#include "mailbox.h"
#include "wall_clock.h"
#include "stew_bowl.h"
#include "music_box.h"
#include "rain_chain.h"
#include "clothesline.h"
#include "scarecrow.h"
#include "chicken_coop.h"
#include "bookshelf.h"
#include "picnic_spot.h"
#include "sunflowers.h"
#include "fairy_ring.h"
#include "dew_webs.h"
#include "hay_bales.h"
#include "swallow_nest.h"
#include "pumpkins.h"
#include "umbrella_stand.h"
#include "flower_bed.h"
#include "birdbath.h"
#include "pinwheels.h"
#include "paper_plane.h"
#include "kitchen_shelf.h"
#include "writing_desk.h"
#include "reading_corner.h"
#include "path_trinkets.h"
#include "window_sill.h"
#include "dock_trinkets.h"
#include "barn_tools.h"
#include "camp_gear.h"
#include "plant_stand.h"
#include "rain_gear.h"
#include "garden_tools.h"
#include "crate_stack.h"
#include "fishing_gear.h"
#include "kitchen_pantry.h"
#include "dog_fetch.h"
#include "birdhouse.h"
#include "rose_arch.h"
#include "camp_guitar.h"
#include "millstone.h"
#include "doorstep_box.h"
#include "pan_rack.h"
#include "telescope.h"
#include "weather_station.h"
#include "garden_windmill.h"
#include "hanging_baskets.h"
#include "hearth_tools.h"
#include "garden_gnome.h"
#include "checkers_table.h"
#include "tabletop_games.h"
#include "picnic_food.h"
#include "winter_gear.h"
#include "butterfly_garden.h"
#include "cricket_chorus.h"
#include "dragonfly_dips.h"
#include "turtle_log.h"
#include "sprinkler.h"
#include "signpost.h"
#include "dock_bell.h"
#include "firefly_jar.h"
#include "paper_boat.h"
#include "wooden_toys.h"
#include "stone_cairn.h"
#include "entryway_shelf.h"
#include "jar_lights.h"
#include "hand_pump.h"
#include "firewood_stack.h"
#include "compost_bin.h"
#include "sundial.h"
#include "bean_trellis.h"
#include "bicycle.h"
#include "porch_lights.h"
#include "fish_drying_rack.h"
#include "honey_stand.h"
#include "cabin_sign.h"
#include "herb_bundles.h"
#include "net_rack.h"
#include "workbench.h"
#include "painter_easel.h"
#include "willow_tree.h"
#include "bread_oven.h"
#include "otter.h"
#include "yarn_basket.h"
#include "menu_board.h"
#include "beach_finds.h"
#include "morning_glory.h"
#include "snail_patch.h"
#include "leaf_drift.h"
#include "bench.h"
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
	GDREGISTER_CLASS(BirdFeeder);
	GDREGISTER_CLASS(HUDClock);
	GDREGISTER_CLASS(ButterflySwarm);
	GDREGISTER_CLASS(DuckFlock);
	GDREGISTER_CLASS(LakeJumpFx);
	GDREGISTER_CLASS(FishSchool);
	GDREGISTER_CLASS(DragonflySwarm);
	GDREGISTER_CLASS(FrogPond);
	GDREGISTER_CLASS(Rowboat);
	GDREGISTER_CLASS(KelpField);
	GDREGISTER_CLASS(LilyPond);
	GDREGISTER_CLASS(SquirrelField);
	GDREGISTER_CLASS(BeachShell);
	GDREGISTER_CLASS(ShellField);
	GDREGISTER_CLASS(OwlPerch);
	GDREGISTER_CLASS(StonePath);
	GDREGISTER_CLASS(CabinLife);
	GDREGISTER_CLASS(DandelionField);
	GDREGISTER_CLASS(SwingSet);
	GDREGISTER_CLASS(IcicleField);
	GDREGISTER_CLASS(CampCat);
	GDREGISTER_CLASS(DockGull);
	GDREGISTER_CLASS(WindCharms);
	GDREGISTER_CLASS(StewPot);
	GDREGISTER_CLASS(Woodpecker);
	GDREGISTER_CLASS(DeerMeadow);
	GDREGISTER_CLASS(Hammock);
	GDREGISTER_CLASS(FloatingLanterns);
	GDREGISTER_CLASS(CabinYard);
	GDREGISTER_CLASS(RockingChair);
	GDREGISTER_CLASS(WishLanternDock);
	GDREGISTER_CLASS(PhotoFrame);
	GDREGISTER_CLASS(Beehive);
	GDREGISTER_CLASS(CampTent);
	GDREGISTER_CLASS(Hedgehog);
	GDREGISTER_CLASS(Kite);
	GDREGISTER_CLASS(Snowman);
	GDREGISTER_CLASS(DockMoths);
	GDREGISTER_CLASS(HeronPond);
	GDREGISTER_CLASS(Campfire);
	GDREGISTER_CLASS(HearthFire);
	GDREGISTER_CLASS(IceHole);
	GDREGISTER_CLASS(Mailbox);
	GDREGISTER_CLASS(WallClock);
	GDREGISTER_CLASS(StewBowl);
	GDREGISTER_CLASS(MusicBox);
	GDREGISTER_CLASS(RainChain);
	GDREGISTER_CLASS(Clothesline);
	GDREGISTER_CLASS(Scarecrow);
	GDREGISTER_CLASS(ChickenCoop);
	GDREGISTER_CLASS(Bookshelf);
	GDREGISTER_CLASS(PicnicSpot);
	GDREGISTER_CLASS(Sunflowers);
	GDREGISTER_CLASS(FairyRing);
	GDREGISTER_CLASS(DewWebs);
	GDREGISTER_CLASS(HayBales);
	GDREGISTER_CLASS(SwallowNest);
	GDREGISTER_CLASS(Pumpkins);
	GDREGISTER_CLASS(UmbrellaStand);
	GDREGISTER_CLASS(FlowerBed);
	GDREGISTER_CLASS(Birdbath);
	GDREGISTER_CLASS(Pinwheels);
	GDREGISTER_CLASS(PaperPlane);
	GDREGISTER_CLASS(KitchenShelf);
	GDREGISTER_CLASS(WritingDesk);
	GDREGISTER_CLASS(ReadingCorner);
	GDREGISTER_CLASS(PathTrinkets);
	GDREGISTER_CLASS(WindowSill);
	GDREGISTER_CLASS(DockTrinkets);
	GDREGISTER_CLASS(BarnTools);
	GDREGISTER_CLASS(CampGear);
	GDREGISTER_CLASS(PlantStand);
	GDREGISTER_CLASS(RainGear);
	GDREGISTER_CLASS(GardenTools);
	GDREGISTER_CLASS(CrateStack);
	GDREGISTER_CLASS(FishingGear);
	GDREGISTER_CLASS(KitchenPantry);
	GDREGISTER_CLASS(DogFetch);
	GDREGISTER_CLASS(Birdhouse);
	GDREGISTER_CLASS(RoseArch);
	GDREGISTER_CLASS(CampGuitar);
	GDREGISTER_CLASS(Millstone);
	GDREGISTER_CLASS(DoorstepBox);
	GDREGISTER_CLASS(PanRack);
	GDREGISTER_CLASS(Telescope);
	GDREGISTER_CLASS(WeatherStation);
	GDREGISTER_CLASS(GardenWindmill);
	GDREGISTER_CLASS(HangingBaskets);
	GDREGISTER_CLASS(HearthTools);
	GDREGISTER_CLASS(GardenGnome);
	GDREGISTER_CLASS(CheckersTable);
	GDREGISTER_CLASS(TabletopGames);
	GDREGISTER_CLASS(PicnicFood);
	GDREGISTER_CLASS(WinterGear);
	GDREGISTER_CLASS(ButterflyGarden);
	GDREGISTER_CLASS(CricketChorus);
	GDREGISTER_CLASS(DragonflyDips);
	GDREGISTER_CLASS(TurtleLog);
	GDREGISTER_CLASS(Sprinkler);
	GDREGISTER_CLASS(Signpost);
	GDREGISTER_CLASS(DockBell);
	GDREGISTER_CLASS(FireflyJar);
	GDREGISTER_CLASS(PaperBoat);
	GDREGISTER_CLASS(WoodenToys);
	GDREGISTER_CLASS(StoneCairn);
	GDREGISTER_CLASS(EntrywayShelf);
	GDREGISTER_CLASS(JarLights);
	GDREGISTER_CLASS(HandPump);
	GDREGISTER_CLASS(FirewoodStack);
	GDREGISTER_CLASS(CompostBin);
	GDREGISTER_CLASS(Sundial);
	GDREGISTER_CLASS(BeanTrellis);
	GDREGISTER_CLASS(Bicycle);
	GDREGISTER_CLASS(PorchLights);
	GDREGISTER_CLASS(FishDryingRack);
	GDREGISTER_CLASS(HoneyStand);
	GDREGISTER_CLASS(CabinSign);
	GDREGISTER_CLASS(HerbBundles);
	GDREGISTER_CLASS(NetRack);
	GDREGISTER_CLASS(Workbench);
	GDREGISTER_CLASS(PainterEasel);
	GDREGISTER_CLASS(WillowTree);
	GDREGISTER_CLASS(BreadOven);
	GDREGISTER_CLASS(Otter);
	GDREGISTER_CLASS(YarnBasket);
	GDREGISTER_CLASS(MenuBoard);
	GDREGISTER_CLASS(BeachFinds);
	GDREGISTER_CLASS(MorningGlory);
	GDREGISTER_CLASS(SnailPatch);
	GDREGISTER_CLASS(LeafDrift);
	GDREGISTER_CLASS(Bench);
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
