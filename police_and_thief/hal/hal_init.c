#include "hal_init.h"
#include "../wifi.h"
#include "../touch.h"
#include "../voice.h"

void hal_init_all(void)
{
    wifi_init();
    touch_init();
    voice_init();
}
