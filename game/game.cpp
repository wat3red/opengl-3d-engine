#include "game.h"
#include "scene/script_registry.h"
#include "game/scripts/doom_player.h"
#include "game/scripts/doom_enemy.h"
#include "game/scripts/doom_pickup.h"
#include "game/scripts/doom_hud.h"
#include "game/scripts/shotgun_viewmodel.h"

namespace Game {

void Initialize()
{
	ScriptRegistry::Register<DoomPlayer>("DoomPlayer");
	ScriptRegistry::Register<DoomEnemy>("DoomEnemy");
	ScriptRegistry::Register<DoomPickup>("DoomPickup");
	ScriptRegistry::Register<DoomHUD>("DoomHUD");
	ScriptRegistry::Register<ShotgunViewModel>("ShotgunViewModel");
}

} // namespace Game
