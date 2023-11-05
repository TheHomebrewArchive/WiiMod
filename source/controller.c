#include <ogc/pad.h>
#include <stdio.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include "controller.h"
#include "dopios.h"
#include "Settings.h"
#include "tools.h"

#define MAX_WIIMOTES    4

u32 convertGCToWii(u32 pressed) {
    if (pressed & PAD_BUTTON_A) return WPAD_BUTTON_A;
    if (pressed & PAD_BUTTON_B) return WPAD_BUTTON_B;
    if (pressed & PAD_BUTTON_LEFT) return WPAD_BUTTON_LEFT;
    if (pressed & PAD_BUTTON_RIGHT) return WPAD_BUTTON_RIGHT;
    if (pressed & PAD_BUTTON_UP) return WPAD_BUTTON_UP;
    if (pressed & PAD_BUTTON_DOWN) return WPAD_BUTTON_DOWN;
    if (pressed & PAD_BUTTON_START) return WPAD_BUTTON_HOME;
    if (pressed & PAD_TRIGGER_L) return WPAD_BUTTON_MINUS;
    if (pressed & PAD_TRIGGER_R) return WPAD_BUTTON_PLUS;
    if (pressed & PAD_BUTTON_Y) return WPAD_BUTTON_1;
    if (pressed & PAD_BUTTON_X) return WPAD_BUTTON_2;
    return 0;
}

u32 convertClassicToWii(u32 pressed) {
    if (pressed & WPAD_CLASSIC_BUTTON_A) return WPAD_BUTTON_A;
    if (pressed & WPAD_CLASSIC_BUTTON_B) return WPAD_BUTTON_B;
    if (pressed & WPAD_CLASSIC_BUTTON_LEFT) return WPAD_BUTTON_LEFT;
    if (pressed & WPAD_CLASSIC_BUTTON_RIGHT) return WPAD_BUTTON_RIGHT;
    if (pressed & WPAD_CLASSIC_BUTTON_UP) return WPAD_BUTTON_UP;
    if (pressed & WPAD_CLASSIC_BUTTON_DOWN) return WPAD_BUTTON_DOWN;
    if (pressed & WPAD_CLASSIC_BUTTON_HOME) return WPAD_BUTTON_HOME;
    if (pressed & WPAD_CLASSIC_BUTTON_MINUS) return WPAD_BUTTON_MINUS;
    if (pressed & WPAD_CLASSIC_BUTTON_PLUS) return WPAD_BUTTON_PLUS;
    if (pressed & WPAD_CLASSIC_BUTTON_FULL_L) return WPAD_BUTTON_1;
    if (pressed & WPAD_CLASSIC_BUTTON_FULL_R) return WPAD_BUTTON_2;
    return pressed;
}

u32 convertGuitarHeroToWii(u32 pressed) {
    if (pressed & WPAD_GUITAR_HERO_3_BUTTON_GREEN) return WPAD_BUTTON_A;
    if (pressed & WPAD_GUITAR_HERO_3_BUTTON_RED) return WPAD_BUTTON_B;
    if (pressed & WPAD_GUITAR_HERO_3_BUTTON_STRUM_UP) return WPAD_BUTTON_UP;
    if (pressed & WPAD_GUITAR_HERO_3_BUTTON_STRUM_DOWN) return WPAD_BUTTON_DOWN;
    if (pressed & WPAD_GUITAR_HERO_3_BUTTON_ORANGE) return WPAD_BUTTON_HOME;
    if (pressed & WPAD_GUITAR_HERO_3_BUTTON_MINUS) return WPAD_BUTTON_MINUS;
    if (pressed & WPAD_GUITAR_HERO_3_BUTTON_PLUS) return WPAD_BUTTON_PLUS;
    return pressed;
}

u32 filterHeld(u32 pressed) {
    if( theSettings.DisableControllerButtonHold ) return 0;
    if (pressed & WPAD_BUTTON_B) return WPAD_BUTTON_B;
    if (pressed & WPAD_BUTTON_LEFT) return WPAD_BUTTON_LEFT;
    if (pressed & WPAD_BUTTON_RIGHT) return WPAD_BUTTON_RIGHT;
    if (pressed & WPAD_BUTTON_UP) return WPAD_BUTTON_UP;
    if (pressed & WPAD_BUTTON_DOWN) return WPAD_BUTTON_DOWN;
    if (pressed & WPAD_BUTTON_HOME) return WPAD_BUTTON_HOME;
    return 0;
}

u32 GetButtons(u32 (*fp)()) {
    u32 buttons = 0, cnt;
    for (cnt = 0; cnt < MAX_WIIMOTES; cnt++) {
        buttons |= fp(cnt);
    }
    return buttons;
}

void ScanButtons(u32 *buttons, u32 *buttonsHeld, u32 *buttonsGC) {
    PAD_ScanPads();
    WPAD_ScanPads();
    // GC buttons
    *buttonsGC = convertGCToWii(GetButtons((u32 (*)())&PAD_ButtonsDown));
    // Wii buttons
    *buttons = convertGuitarHeroToWii(convertClassicToWii(GetButtons(&WPAD_ButtonsDown)));
    *buttonsHeld = filterHeld(convertGuitarHeroToWii(convertClassicToWii(GetButtons(&WPAD_ButtonsHeld))));
}

int ScanPads(u32 *button) {
    u32 buttons = 0, buttonsHeld = 0;
    u32 buttonsGC = 0;

    ScanButtons(&buttons, &buttonsHeld, &buttonsGC);

    buttons |= buttonsHeld;
    buttons |= buttonsGC;

    *button = buttons;

    return buttons;
}

u32 WaitKey(u32 button) {
    u32 pressed = 0;
    
    while (!(pressed & button)) {
        if (ExitMainThread) return 0;
        VIDEO_WaitVSync();
        ScanPads(&pressed);
        if (pressed & WPAD_BUTTON_HOME) ReturnToLoader();
    }

    return pressed;
}

// Routine to wait for a button from either the Wiimote or a gamecube
// controller. The return value will mimic the WPAD buttons to minimize
// the amount of changes to the original code, that is expecting only
// Wiimote button presses. Note that the "HOME" button on the Wiimote
// is mapped to the "SELECT" button on the Gamecube Ctrl. (wiiNinja 5/15/2009)
u32 WaitButtons(void) {
    u32 buttons = 0;
    u32 buttonsHeld = 0;
    u32 buttonsGC = 0;
    
    // Wait for button pressing
    while (!buttons && !buttonsGC && !buttonsHeld && !ExitMainThread) {
        ScanButtons(&buttons, &buttonsHeld, &buttonsGC);
        VIDEO_WaitVSync();
    }

    if (buttonsHeld) {
        usleep(90000);
    }
    buttons |= buttonsHeld;
    buttons |= buttonsGC;
    return buttons;
} // WaitButtons
