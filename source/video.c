#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "detect_settings.h"
#include "dopios.h"
#include "gecko.h"
#include "video.h"

/* Video variables */
static unsigned int *frontBuffer[2];
static int fbi = 1;
static GXRModeObj *vmode = NULL;
int ConsoleRows;
int ConsoleCols;
int ScreenWidth;
int ScreenHeight;

void Console_SetPosition(u8 row, u8 column);
void PrintCenter(char *text, int width);

void PrintBanner() {
    ClearScreen();
    int errors = checkErrors();

    Console_SetColors(BLUE, 0, WHITE, 0);
    if (errors > 0) {
        Console_SetColors(RED, 0, WHITE, 0);
    } else if (wiiSettings.missingIOSwarning || wiiSettings.regionChangedKoreanWii) {
        Console_SetColors(YELLOW, 0, WHITE, 0);
    }
    char text[ConsoleCols];
    snprintf(text, sizeof(text), "Wii Mod v%.1lf", WII_MOD_VERSION);
    PrintCenter(text, ConsoleCols);
    Console_SetColors(BLACK, 0, WHITE, 0);
    printf("\n");
}

void initNextBuffer() {
    if( fbi ) {
        fbi = 0;
    } else {
        fbi = 1;
    }
    console_init(frontBuffer[fbi], 0, 0, ScreenWidth, ScreenHeight, ScreenWidth * VI_DISPLAY_PIX_SZ);
}

void flushBuffer() {
    VIDEO_SetNextFramebuffer(frontBuffer[fbi]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void ClearScreen() {
    /* Clear console */
    printf("\x1b[2J");
    fflush(stdout);
}

void ClearLine() {
    printf("\r\x1b[2K\r");
    fflush(stdout);
}

void PrintCenter(char *text, int width) {
    int textLen = strlen(text);
    int leftPad = (width - textLen) / 2;
    int rightPad = (width - textLen) - leftPad;
    printf("%*s%s%*s", leftPad, " ", text, rightPad, " ");
}

void Console_SetFgColor(u8 color, u8 bold) {
    printf("\x1b[%u;%dm", color + 30, bold);
}

void Console_SetBgColor(u8 color, u8 bold) {
    printf("\x1b[%u;%dm", color + 40, bold);
}

void Console_SetColors(u8 bgColor, u8 bgBold, u8 fgColor, u8 fgBold) {
    Console_SetBgColor(bgColor, bgBold);
    Console_SetFgColor(fgColor, fgBold);
}

void Console_SetPosition(u8 row, u8 column) {
    // The console understands VT terminal escape codes
    // This positions the cursor on row 2, column 0
    // we can use variables for this with format codes too
    // e.g. printf ("\x1b[%d;%dH", row, column );
    printf("\x1b[%u;%uH", row, column);
}

void Console_SetBottomRow() {
    Console_SetPosition(ConsoleRows - 1, 0);
}

void SetToInputLegendPos() {
    Console_SetPosition(ConsoleRows - 4, 0);
}

void Video_Init() {
    // Initialise the video system
    VIDEO_Init();

    // Obtain the preferred video mode from the system
    // This will correspond to the settings in the Wii menu
    vmode = VIDEO_GetPreferredMode(NULL);

    // Fixes Screen Resolution
    if (vmode->viTVMode == VI_NTSC || CONF_GetEuRGB60() || CONF_GetProgressiveScan()) {
        GX_AdjustForOverscan(vmode, vmode, 0, (u16)(vmode->viWidth * 0.026));
    }

    // Set up the video registers with the chosen mode
    VIDEO_Configure(vmode);

    ScreenWidth = vmode->viWidth;
    ScreenHeight = vmode->viHeight;

    // Allocate memory for the display in the uncached region
    frontBuffer[0] = (u32 *) MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
    frontBuffer[1] = (u32 *) MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
    
    VIDEO_ClearFrameBuffer(vmode, frontBuffer[0], COLOR_BLACK);
    VIDEO_ClearFrameBuffer(vmode, frontBuffer[1], COLOR_BLACK);

    // Tell the video hardware where our display memory is	
    VIDEO_SetNextFramebuffer(frontBuffer[0]);

    // Make the display visible
    VIDEO_SetBlack(FALSE);

    // Flush the video register changes to the hardware
    VIDEO_Flush();

    // Wait for Video setup to complete
    VIDEO_WaitVSync();
    if (vmode->viTVMode & VI_NON_INTERLACE) {
        VIDEO_WaitVSync();
    } else {
        while (VIDEO_GetNextField()) {
            VIDEO_WaitVSync();
        }
    }

    // Initialise the console, required for printf
    initNextBuffer();
    CON_GetMetrics(&ConsoleCols, &ConsoleRows);
    gprintf("Console Metrics: Cols = %u, Rows = %u\n", ConsoleCols, ConsoleRows);
}
