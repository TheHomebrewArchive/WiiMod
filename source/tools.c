#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiilight.h>
#include <wiiuse/wpad.h>
#include <unistd.h>

#include "controller.h"
#include "detect_settings.h"
#include "gecko.h"
#include "sys.h"
#include "title.h"
#include "RuntimeIOSPatch.h"
#include "Settings.h"
#include "tools.h"
#include "video.h"

extern void udelay(int us);
static volatile u32 tickcount = 0;

static lwp_t spinnerThread = LWP_THREAD_NULL;
static bool spinnerRunning = false;

static void * spinner(void *args) {
    char *spinnerChars = (char*) "/-\\|";
    int spin = 0;
    int flash = 0;
    int cnt = 0;
    int ratio = 5;
    int flashIntensity[24] = { 15, 31, 47, 63, 79, 95, 111, 127, 159, 191, 223,
        255, 223, 191, 159, 127, 111, 95, 79, 63, 47, 31, 15, 0 };
    while (1) {
        if (!spinnerRunning) break;
        printf("\b%c", spinnerChars[spin++]);
        cnt++;
        if (!spinnerChars[spin]) spin = 0;
        fflush(stdout);
        if (!spinnerRunning) break;
        if (cnt >= ratio) {
            cnt = 0;
            WIILIGHT_SetLevel(flashIntensity[flash++]);
            WIILIGHT_TurnOn();
        }
        if (flash >= 24) {
            flash = 0;
        }
        usleep(50000);
    }
    return NULL;
}

void SpinnerStart() {
    if (spinnerThread != LWP_THREAD_NULL) return;
    spinnerRunning = true;
    LWP_CreateThread(&spinnerThread, spinner, NULL, NULL, 0, 80);
}

void SpinnerStop() {
    if (spinnerRunning) {
        spinnerRunning = false;
        LWP_JoinThread(spinnerThread, NULL);
        spinnerThread = LWP_THREAD_NULL;
        WIILIGHT_SetLevel(0);
        WIILIGHT_TurnOn();
        WIILIGHT_Toggle();
    }
}

void ReturnToLoader() {
    if(theSettings.DisableSetAHBPROT) {
        //temp fix for HBC's AHBPROT no net bug
        ReloadIos(IOS_GetVersion());
    }
    System_Deinit();
    initNextBuffer();
    gprintf("\nReturning to Loader");
    PrintBanner();
    Console_SetBottomRow();
    ClearLine();
    printf("Returning To Loader");
    fflush(stdout);
    flushBuffer();
    exit(0);
}

void ExitMainThreadNow() {
    ExitMainThread = true;
}

void *AllocateMemory(u32 size) {
    return memalign(32, round_up(size, 32));
}

void set_highlight(bool highlight) {
    if (highlight) {
        printf("\x1b[%u;%um", 47, false);
        printf("\x1b[%u;%um", 30, false);
    } else {
        printf("\x1b[%u;%um", 37, false);
        printf("\x1b[%u;%um", 40, false);
    }
}

/*
 This will shutdown the controllers, SD & USB then reload the IOS.
 */

int __reloadIos(int version, bool initWPAD, bool patchPerms) {
    gprintf("Reloading IOS%d...", version);
    int ret;
    s32 curIOS = IOS_GetVersion();
    // The following needs to be shutdown before reload	
    System_Deinit();
    udelay(5000);
    ret = IOS_ReloadIOS(version);
    if( ret < 0 ) {
        //if at first you don't succeed
        ret = IOS_ReloadIOS(version);
        if( ret < 0 ) {
            //something must be wrong load last ios
            ret = IOS_ReloadIOS(curIOS);
        }
    }
    if( HAVE_AHBPROT && ret >=0 ) {
        patchSetAHBPROT();
        if( patchPerms ) {
            runtimePatchApply();
        }
    }
    wiiSettings.ahbprot = HAVE_AHBPROT;
    theSettings.SUIdentified = -1;

    if (initWPAD) {
        udelay(5000);
        WPAD_Init();
    }

    gprintf("Done\n");
    return ret;
}

void checkMemUsage() {
    initNextBuffer();
    PrintBanner();
    static double mem1last = 0, mem2last = 0;
    double mem1 = ((float)((char*)SYS_GetArena1Hi()-(char*)SYS_GetArena1Lo()))/0x100000;
    double mem2 = ((float)((char*)SYS_GetArena2Hi()-(char*)SYS_GetArena2Lo()))/0x100000;
    if( mem1last == 0 && mem2last == 0 ) {
        mem1last = mem1;
        mem2last = mem2;
    }
    printf("m1(%.4f) m2(%.4f) lastm1(%.4f) lastm2(%.4f)\n",mem1,mem2,mem1last,mem2last);
    printf("mem1 %s\n",((mem1 > mem1last)?"Went up":(mem1 == mem1last)?"Stayed same":"Went down"));
    printf("mem2 %s\n",((mem2 > mem2last)?"Went up":(mem2 == mem2last)?"Stayed same":"Went down"));
    mem1last = mem1;
    mem2last = mem2;
    flushBuffer();
    //PromptAnyKeyContinue();
    SpinnerStart();
    sleep(3);
    SpinnerStop();
    sleep(2);
}

bool PromptYesNo() {
    u32 button = 0;
    printf("\t(A)\tYes\t\t(B)\tNo\t\t(HOME)/GC:(START)\tExit\n");
    flushBuffer();
    while (!(button & WPAD_BUTTON_HOME)) {
        button = WaitButtons();
        if (button & WPAD_BUTTON_A) return true;
        if (button & WPAD_BUTTON_B) return false;
    }
    ReturnToLoader();
    return false;
}

bool PromptContinue() {
    printf("Are you sure you want to continue?\n");
    return PromptYesNo();
}

void PromptAnyKeyContinue() {
    printf("\n\nPress any key to continue.\n");
    flushBuffer();
    WaitButtons();
}

void clearLine() {
    printf("\r                                                                               \r");
}

void horizontalLineBreak() {
    printf(
        "________________________________________________________________________________");
}

void WiiLightControl(int state) {
    switch (state) {
        case WII_LIGHT_ON:
            /* Turn on Wii Light */
            WIILIGHT_SetLevel(255);
            WIILIGHT_TurnOn();
            break;

        case WII_LIGHT_OFF:
        default:
            /* Turn off Wii Light */
            WIILIGHT_SetLevel(0);
            WIILIGHT_TurnOn();
            WIILIGHT_Toggle();
            break;
    }
} // WiiLightControl

char *trim(char *str) {

    size_t len = 0;
    char *frontp = str - 1;
    char *endp = NULL;

    if (str == NULL) return NULL;

    if (str[0] == '\0') return str;

    len = strlen(str);
    endp = str + len;

    /* Move the front and back pointers to address
     * the first non-whitespace characters from
     * each end.
     */
    while (isspace((int) *(++frontp)))
        ;
    while (isspace((int) *(--endp)) && endp != frontp)
        ;

    if (str + len - 1 != endp)
        *(endp + 1) = '\0';
    else if (frontp != str && endp == frontp) *str = '\0';

    /* Shift the string so that it starts at str so
     * that if it's dynamically allocated, we can
     * still free it on the returned pointer.  Note
     * the reuse of endp to mean the front of the
     * string buffer now.
     */
    endp = str;
    if (frontp != str) {
        while (*frontp)
            *endp++ = *frontp++;
        *endp = '\0';
    }

    return str;
}
