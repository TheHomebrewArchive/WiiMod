#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <wiilight.h>

#include "dopios.h"
#include "fat.h"
#include "gecko.h"
#include "network.h"
#include "sys.h"
#include "tools.h"
#include "video.h"

/* Variables */
bool Shutdown = false;
extern void udelay(int us);

void __Sys_ResetCallback() {
    ExitMainThread = true;
}

void __Sys_PowerCallback() {
    /* Poweroff console */
    Shutdown = true;
    ExitMainThread = true;
}

void System_Init() {
    /* Initialize video subsytem */
#ifndef NO_DEBUG
    InitGecko();
#endif
    gprintf("\nWii Mod\n");
    gprintf("Initializing Wii\n");
    gprintf("VideoInit\n");
    Video_Init();

    gprintf("WPAD_Init\n");
    WPAD_Init();
    gprintf("PAD_Init\n");
    PAD_Init();
    WIILIGHT_Init();
    
    /* Set RESET/POWER button callback */
    WPAD_SetPowerButtonCallback(__Sys_PowerCallback);
    SYS_SetResetCallback(__Sys_ResetCallback);
    SYS_SetPowerCallback(__Sys_PowerCallback);
}

void System_Deinit() {
    WPAD_Shutdown();
    NetworkShutdown();
}

void Sys_Reboot(void) {
    gprintf("Restart console\n");
    System_Deinit();
    STM_RebootSystem();
}

void System_Shutdown() {
    int ret;
    gprintf("\n");
    gprintf("Powering Off Console\n");
    System_Deinit();
    
    if (CONF_GetShutdownMode() == CONF_SHUTDOWN_IDLE) {
        ret = CONF_GetIdleLedMode();
        if (ret >= CONF_LED_OFF && ret <= CONF_LED_BRIGHT)
            ret = STM_SetLedMode(ret);
        
        ret = STM_ShutdownToIdle();
    } else {
        ret = STM_ShutdownToStandby();
    }
}
