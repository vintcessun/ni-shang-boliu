#include "game_engine.h"
#include "game_state.h"
#include "ui_manager.h"

void game_engine_run(void)
{
    game_state_init();
    ui_init();

    while (1) {
        game_state_update();
        ui_update(game_state_get_current());
    }
}
