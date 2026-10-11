#include <ignis/ignis.h>

#include "../scripts/doom_game_state.h"
#include "../scripts/doom_player.h"
#include "../scripts/doom_enemy.h"
#include "../scripts/doom_pickup.h"
#include "../scripts/doom_hud.h"
#include "../scripts/shotgun_viewmodel.h"

IGNIS_GAME_MODULE("DoomGame")
{
    IGNIS_REGISTER_SCRIPT(DoomPlayer);
    IGNIS_REGISTER_SCRIPT(DoomEnemy);
    IGNIS_REGISTER_SCRIPT(DoomPickup);
    IGNIS_REGISTER_SCRIPT(DoomHUD);
    IGNIS_REGISTER_SCRIPT(ShotgunViewModel);
}
