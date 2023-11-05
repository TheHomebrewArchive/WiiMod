/*-------------------------------------------------------------

 dopios.c

 Dop-IOS MOD - A modification of Dop-IOS by Arikado, giantpune, Lunatik, SifJar, and PhoenixTank

 Dop-IOS - install and patch any IOS by marc

 Based on tona's shop installer (C) 2008 tona

 This software is provided 'as-is', without any express or implied
 warranty.  In no event will the authors be held liable for any
 damages arising from the use of this software.

 Permission is granted to anyone to use this software for any
 purpose, including commercial applications, and to alter it and
 redistribute it freely, subject to the following restrictions:

 1.The origin of this software must not be misrepresented; you
 must not claim that you wrote the original software. If you use
 this software in a product, an acknowledgment in the product
 documentation would be appreciated but is not required.

 2.Altered source versions must be plainly marked as such, and
 must not be misrepresented as being the original software.

 3.This notice may not be removed or altered from any source
 distribution.

 -------------------------------------------------------------*/
#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include "AnyRegionChanger.h"
#include "AnyTitleManager.h"
#include "AppLoader.h"
#include "ChannelMenu.h"
#include "controller.h"
#include "detect_settings.h"
#include "dopios.h"
#include "FakeSignInstaller.h"
#include "IOSCheck.h"
#include "IOSMenu.h"
#include "network.h"
#include "RuntimeIOSPatch.h"
#include "Settings.h"
#include "sys.h"
#include "sysconf.h"
#include "SystemMenuMenu.h"
#include "title_install.h"
#include "tools.h"
#include "video.h"
#include "wadmanager.h"

#define REGIONS_LEN_PPD     (sizeof(Regions) / sizeof(Region))
#define MAINMENU_LEN_PPD    (sizeof(mainMenu) / sizeof(Function))

extern void __exception_setreload(int t);

int otherMenu();
int advancedMenu();
int changeIOS();
int priiloaderMenu();
int disclaimer();
int show_boot2_info();
int systemInfo();

// Variables
bool ExitMainThread = false;

const u64 Region_IDs[] = { 0x0000000000000045ULL, 0x0000000000000050ULL,
    0x000000000000004AULL, 0x000000000000004BULL };

const struct Region Regions[] = { { "North America (U)" }, { "Europe (E)" }, {
    "Japan (J)" }, { "Korea (K)" } };
const int REGIONS_LEN = REGIONS_LEN_PPD;
const int REGIONS_HI = REGIONS_LEN_PPD - 1;

typedef struct Function {
    const char DisplayName[40];
    int (*fp)();
}ATTRIBUTE_PACKED Function;

//These two need to stay in sync
typedef enum {
    MAIN_MENU_IOS_MANAGER,
    MAIN_MENU_CHANNEL_MANAGER,
    MAIN_MENU_SYSTEMMENU_MANAGER,
    MAIN_MENU_WAD_MANAGER,
    MAIN_MENU_APPLOADER,
    MAIN_MENU_CHANGE_IOS,
    MAIN_MENU_CHECK_IOS,
    MAIN_MENU_OTHERMENU,
    MAIN_MENU_ADVANCED,
    MAIN_MENU_BOOT2_INFO,
    MAIN_MENU_SYS_INFO,
    MAIN_MENU_DISCLAIMER
} MAIN_MENU;

const struct Function mainMenu[] = { { "IOSs", &IOSMenu }, { "Channels",
    &channelMenu }, { "System Menu", &systemMenuMenu }, { "WAD Manager",
    &wadmanager }, { "Apploader", &appLoader }, { "Change IOS", &changeIOS }, {
    "Check IOSs", &checkIOSMenu }, { "Other Menu", &otherMenu }, {
    "Advanced Menu", &advancedMenu }, { "Display boot2 information",
    &show_boot2_info }, { "Display Wii System information", &systemInfo }, {
    "Disclaimer and Credits", &disclaimer } };
const int MAINMENU_LEN = MAINMENU_LEN_PPD;
const int MAINMENU_HI = MAINMENU_LEN_PPD - 1;

int advancedMenu() {
    static int menuItem = 0;
    u32 button = 0;
    
    PrintBanner();

    printf("This is for advanced options.\n");
    printf("If you mess up in this menu you can BRICK YOUR WII.\n");
    printf("Are you sure you want to access the advanced options?\n\n");
    if (!PromptContinue()) {
        return 0;
    }

    int fsign = get_certs();

    //Cant es_ident or anytitle will fail
    int cnand;
    if (fsign < 0) {
        printf("\nFailed to get certs ret = %d\n", fsign);
        PromptAnyKeyContinue();
        fsign = 0;
        cnand = 0;
    } else {
        cnand = check_nandp();
        fsign = check_fakesig();
    }
    if (fsign == -1036) {
        fsign = 0; //Not a problem
    }
    if (cnand < 0 || fsign < 0) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n");
        int prev = 0;
        if (cnand < 0) {
            printf("Nand Permissions");
            prev = 1;
        }
        if (fsign < 0) {
            if (prev) printf(", ");
            printf("Fake Sign Patch %d ", fsign);
            prev = 1;
        }

        printf(" required.\nCheck your IOSs for a valid IOS\n");
        flushBuffer();
        PromptAnyKeyContinue();
        return 0;
    }
    while (!(button & WPAD_BUTTON_B)) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n\n\t\t\t\t%3s ", (menuItem == 0) ? "-->" : " ");
        if (checkRegionSystemMenuMismatch(0)) {
            Console_SetColors(BLACK, 0, RED, 1);
        } else if( wiiSettings.regionChangedKoreanWii ) {
            Console_SetColors(BLACK, 0, YELLOW, 0);
        }
        printf("Region Changer\n");
        Console_SetColors(BLACK, 0, WHITE, 0);
        printf("\t\t\t\t%3s Any Title Manager\n", (menuItem == 1) ? "-->" : " ");
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s) (%s)/(%s)\t\tChange Selection\n", UP_ARROW, DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        printf("\t(HOME)/GC:(START)\t\tExit");
        flushBuffer();
        button = WaitButtons();
        if (ExitMainThread) return 0;
        if (button & WPAD_BUTTON_HOME) ReturnToLoader();
        if (button & WPAD_BUTTON_A) {
            if (menuItem == 0) {
                regionChangerMenu();
            } else if (menuItem == 1) {
                anyTitleManagerMenu();
            }
        }
        if (button & WPAD_BUTTON_DOWN) menuItem++;
        if (button & WPAD_BUTTON_UP) menuItem--;
        
        if (menuItem > 1) menuItem = 0;
        if (menuItem < 0) menuItem = 1;
    }
    return 0;
}

void LoadHBC() {
    WII_Initialize();
    if( theSettings.haveHBC & 4 ) {
        WII_LaunchTitle(TITLE_ID(0x00010001,0xAF1BF516)); // HBC v1.0.7+
    } else if( theSettings.haveHBC & 2 ) {
        WII_LaunchTitle(TITLE_ID(0x00010001,0x4a4f4449)); // HBC JODI
    } else if( theSettings.haveHBC & 1 ) {
        WII_LaunchTitle(TITLE_ID(0x00010001,0x48415858)); // HBC HAXX
    } else if( theSettings.haveHBC & 8 ) {
        WII_LaunchTitle(TITLE_ID(0x00010001,0x4d415549)); // HBC MAUI (Backup HBC)
    }
}

int otherMenu() {
    static int menuItem = 0;
    u32 button = 0;
    int ret, itemMax;
    
    while (!(button & WPAD_BUTTON_B)) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n\n\t\t\t\t%3s Install IOS36 (r%d) w/FakeSign\n", (menuItem
                == 0) ? "-->" : " ", IOS36version);
        printf("\t\t\t\t%3s Install IOS36 (r%d) w/FakeSign Old 1 Click Way\n", (menuItem
                == 1) ? "-->" : " ", IOS36version);
        printf("\t\t\t\t%3s Switch to System Menu\n", (menuItem == 2) ? "-->"
            : " ");
        printf("\t\t\t\t%3s Switch to Priiloader\n", (menuItem == 3) ? "-->"
            : " ");
        itemMax = 3;
        if( theSettings.haveHBC > 0 ) {
            printf("\t\t\t\t%3s Switch to HBC\n", (menuItem == 4) ? "-->" : " ");
            itemMax++;
        }
        
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s) (%s)/(%s)\t\tChange Selection\n", UP_ARROW, DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        printf("\t(HOME)/GC:(START)\t\tExit");
        flushBuffer();
        button = WaitButtons();
        if (ExitMainThread) return 0;
        if (button & WPAD_BUTTON_HOME) ReturnToLoader();
        if (button & WPAD_BUTTON_A) {
            if (menuItem == 0) {
                ret = FakeSignInstall();
                if (ret < 0) {
                    PromptAnyKeyContinue();
                }
            } else if (menuItem == 1) {
                ret = onclickFakeSignInstall();
                if (ret < 0) {
                    PromptAnyKeyContinue();
                    ReloadIos(36);
                }
            } else if (menuItem == 2) {
                *(vu32*) 0x8132FFFB = 0x50756e65; // "Pune" , causes priiloader to skip autoboot and load Sys Menu
                DCFlushRange((void*)0x8132FFFB, 4);
                ICInvalidateRange((void *)(0x8132FFFB), 4);
                SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
            } else if (menuItem == 3) {
                *(vu32*) 0x8132FFFB = 0x4461636f;
                DCFlushRange((void*)0x8132FFFB, 4);
                ICInvalidateRange((void *)(0x8132FFFB), 4);
                SYS_ResetSystem(SYS_RESTART, 0, 0);
            } else if (menuItem == 4) {
                LoadHBC();
            }
        }
        if (button & WPAD_BUTTON_DOWN) menuItem++;
        if (button & WPAD_BUTTON_UP) menuItem--;
        if (menuItem > itemMax) menuItem = 0;
        if (menuItem < 0) menuItem = itemMax;
    }
    return 0;
}

inline int sortCallback(const void * first, const void * second) {
    return (*(u32*) first - *(u32*) second);
}

int getsuplist(int * list2) {
    u32 i = 0, j = 1, len, len2 = 0;
    u8 * list = get_ioslist(&len);
    u8 a, b;
    if (len == 0) {
        return 0;
    }

    memset(list2, 0, 256 * sizeof(int));
    i = 0;
    while (i < len || j < GrandIOSLEN) {
        a = 255, b = 255;
        if (j < GrandIOSLEN) {
            a = GrandIOSList[j].ver;
        }
        if (i < len) {
            b = list[i];
        }
        if (a < b) {
            list2[len2++] = a;
            j++;
        } else if (b < a) {
            list2[len2++] = b;
            i++;
        } else {
            list2[len2++] = b;
            i++;
            j++;
        }
    }
    free(list);
    return len2;
}

int ios_selectionmenu(int default_ios) {
    bool exitMenu = false;
    int i, selection = 0;
    u8 *list;
    u32 button, ioscount;

    list = get_ioslist(&ioscount);
    if (ioscount == 0) {
        return 30;
    }
    for (i = 0; i < ioscount; i++) {
        // Default to default_ios if found, else the loaded IOS
        if (list[i] == default_ios) {
            selection = i;
            break;
        }
        if (list[i] == IOS_GetVersion()) {
            selection = i;
        }
    }
    while (!exitMenu) {
        initNextBuffer();
        PrintBanner();
        printf("\n\nSelect which IOS to load:\n\n");
        bool usedColor = false;
        for (i = 0; i < ioscount; i += 1) {
            if (i == selection) {
                set_highlight(true);
            }
            if (ios_ident[list[i]] >= 0) {
                Console_SetFgColor(GREEN, 0);
                usedColor = true;
            }
            if (ios_nandp[list[i]] >= 0) {
                Console_SetFgColor(BLUE, 1);
                usedColor = true;
                if (ios_ident[list[i]] >= 0) {
                    Console_SetFgColor(CYAN, 0);
                }
            }
            printf(" %3u ", list[i]);
            Console_SetFgColor(WHITE, 1);
            if (i == selection) {
                set_highlight(false);
            }
        }
        printf("\n\nPress D-PAD to select, A to load, B to go back\n");
        if (usedColor) {
            printf("IOSs in ");
            Console_SetFgColor(GREEN, 0);
            printf("Green");
            Console_SetFgColor(WHITE, 0);
            printf(" have ES_Identify patch.\nIOSs in ");
            Console_SetFgColor(BLUE, 1);
            printf("Blue");
            Console_SetFgColor(WHITE, 0);
            printf(" have Nand patch.\nIOSs in ");
            Console_SetFgColor(CYAN, 0);
            printf("Cyan");
            Console_SetFgColor(WHITE, 0);
            printf(" have both patches.\n");
        }
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s) (%s)/(%s)\t\tChange Selection\n", UP_ARROW, DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        printf("\t(HOME)/GC:(START)\t\tExit");
        flushBuffer();
        while (1) {
            button = WaitButtons();
            if (button & WPAD_BUTTON_LEFT) {
                selection -= 1;
                if (selection < 0) {
                    selection = ioscount - 1;
                }
                break;
            }
            if (button & WPAD_BUTTON_HOME) {
                ReturnToLoader();
            }
            if (button & WPAD_BUTTON_RIGHT) {
                selection += 1;
                if (selection > (ioscount - 1)) {
                    selection = 0;
                }
                break;
            }
            if (button & WPAD_BUTTON_UP) {
                selection -= 16;
                if (selection < 0) {
                    selection = ioscount - 1;
                }
                break;
            }
            if (button & WPAD_BUTTON_DOWN) {
                selection += 16;
                if (selection > (ioscount - 1)) {
                    selection = 0;
                }
                break;
            }
            if (button & WPAD_BUTTON_A) {
                exitMenu = true;
                break;
            }
            if (button & WPAD_BUTTON_B) {
                free(list);
                return 0;
            }
        }
    }
    selection = list[selection];
    free(list);
    return selection;
}

int disclaimer() {
    initNextBuffer();
    PrintBanner();
    printf("Welcome to Wii Mod - a modification of DOP-IOS/Dop-IOS MOD!\n\n");
    printf("If you have paid for this software, you have been scammed.\n\n");
    printf("If misused, this software WILL brick your Wii.\n");
    printf("The authors are not responsible if your Wii is bricked.\n\n");
    printf("Credits: Jskyboo, Marcmax, Waninkoko, Wiipower\n");
    printf("         giantpune, SifJar, PheonixTank, Lunatik\n");
    printf("         Team Twiizers, wiiNinja, Sorg, DacoTaco\n");
    printf("         mariomaniac33, Leathl, Tona, Nicksasa\n");
    printf("         Red Squirrel, MrClick, daveboal, tueidj\n");
    printf("         Wiiwu, Joostin, TeenTin, airline38, Maisto\n");
    printf("         wes11ph, Arikado, and many others\n\n");

    flushBuffer();
    PromptAnyKeyContinue();
    return 0;
}

int systemInfo() {
    initNextBuffer();
    PrintBanner();
    
    s32 curIOS = IOS_GetVersion();
    u16 titleVersion = GetTitleVersion(TITLE_ID(1, curIOS));
    printf("\n\n\n\t\t\t\tWii System Information:\n\n");
    printf("\t\t\t\t\tHollywood v0x%x Console ID: %d\n", wiiSettings.hollywoodVersion, wiiSettings.deviceId);
    printf("\t\t\t\t\tSystem menu %u\n", wiiSettings.sysMenuVer);
    printf("\t\t\t\t\tSystem menu v%.1lf%c on IOS %d\n", wiiSettings.sysMenuNinVersion, wiiSettings.sysMenuRegion, wiiSettings.sysMenuIOS);
    s8 pass[5];
    s32 ret = SYSCONF_Init();
    if (ret < 0) {
        printf("Unexpected Error: %s Value: %d\n", "SYSCONF_Init", ret);
    } else {
        ret = SYSCONF_GetParentalPassword(pass);
    }
    if (ret < 0) {
        printf("failed to get Parental PIN %d\n", ret);
    } else if (strlen((char *) pass) > 0) {
        printf("\n\t\t\t\t\tParental PIN %s\n", pass);
    }

    printf("\n\t\t\t\t\tCurrent Permissions on IOS %d v%u:\n", curIOS, titleVersion);
    
    get_certs();
    int fsig = -1, esident = -1, devflash = -1, devusb = -1;
    int devboot2 = -1, nandp = -1;
    u16 sysmen = 0;
    printf("\t\t\t\t\tFakesign Bug (Trucha bug):");
    fsig = check_fakesig();
    Console_SetFgColor((fsig >= 0) ? YELLOW : RED, 1);
    printf("%s", (fsig >= 0) ? "[Yes]" : "[No]");
    Console_SetFgColor(WHITE, 0);
    
    printf("\n\t\t\t\t\tEsIdentify (ES_DiVerify):");
    esident = check_identify();
    Console_SetFgColor((esident >= 0) ? YELLOW : RED, 1);
    printf("%s", (esident >= 0) ? "[Yes]" : "[No]");
    Console_SetFgColor(WHITE, 0);
    
    printf("\tboot2:");
    devboot2 = check_boot2();
    Console_SetFgColor((devboot2 >= 0) ? YELLOW : RED, 1);
    printf("%s\n", (devboot2 >= 0) ? "[Yes]" : "[No]");
    Console_SetFgColor(WHITE, 0);
    
    printf("\t\t\t\t\t/dev/flash (Flash access):");
    devflash = check_flash();
    Console_SetFgColor((devflash >= 0) ? YELLOW : RED, 1);
    printf("%s", (devflash >= 0) ? "[Yes]" : "[No]");
    Console_SetFgColor(WHITE, 0);
    
    printf("\tUSB2 Tree:");
    devusb = check_usb2();
    Console_SetFgColor((devusb >= 0) ? YELLOW : RED, 1);
    printf("%s\n", (devusb >= 0) ? "[Yes]" : "[No]");
    Console_SetFgColor(WHITE, 0);
    
    printf("\t\t\t\t\tNAND Permissions:");
    nandp = check_nandp();
    Console_SetFgColor((nandp >= 0) ? YELLOW : RED, 1);
    printf("%s", (nandp >= 0) ? "[Yes]" : "[No]");
    Console_SetFgColor(WHITE, 0);
    
    printf("\t\tGetSysMenuVersion:");
    sysmen = wiiSettings.sysMenuVer;
    Console_SetFgColor((sysmen > 0) ? YELLOW : RED, 1);
    printf("%s", (sysmen > 0) ? "[Yes]" : "[No]");
    Console_SetFgColor(WHITE, 0);
    printf("\n\t\t\t\t\tAHBPROT: %x\n", *(vu32*) 0xcd800064);
    ios_found[curIOS] = titleVersion;
    ios_flash[curIOS] = devflash;
    ios_fsign[curIOS] = fsig;
    ios_ident[curIOS] = esident;
    ios_usb2m[curIOS] = devusb;
    ios_boot2[curIOS] = devboot2;
    ios_nandp[curIOS] = nandp;
    ios_sysmen[curIOS] = sysmen;

    flushBuffer();
    PromptAnyKeyContinue();
    return 0;
}

int show_boot2_info() {
    initNextBuffer();
    PrintBanner();
    printf("\x1b[2;0H");
    printf("\n\nRetrieving boot2 version...\n");
    if (wiiSettings.boot2version == 0) {
        printf("Could not get boot2 version. It's possible your Wii is\n");
        printf("a boot2v4+ Wii, maybe not.\n");
    } else {
        printf("Your boot2 version is: %u\n", wiiSettings.boot2version);
        if (wiiSettings.boot2version < 4) {
            printf("This means you should not have problems.\n");
        }
    }
    printf("\nBoot2v4 is an indicator for the 'new' Wii hardware revision that \n");
    printf("prevents the execution of some old IOS. These Wiis are often called\n");
    printf("LU64+ Wiis or 'unsoftmoddable' Wiis. You MUST NOT downgrade one of these\n");
    printf("Wiis and be EXTRA careful when messing with ANYTHING on them.\n");
    printf("The downgraded IOS15 you get with the Trucha Bug Restorer should work\n");
    printf("on these Wiis and not harm Wiis in general.\n\n");
    printf("If you updated your Wii to 4.2+ through wifi or disc, your boot2\n");
    printf("was updated by this and you can't use it as indicator for this. Some\n");
    printf("discs with 4.1 will also update boot2 to v4\n\n");
    flushBuffer();
    PromptAnyKeyContinue();
    return 0;
}

int changeIOS() {
    int ios = ios_selectionmenu(IOS_GetVersion());
    if (ios != 0) {

        ReloadIos(ios);
    }
    if (wiiSettings.failSMContentRead) {
        updateSysMenuVersion();
    }
    return 0;
}

void MainThread_Execute() {
    int i, menuSelection = 0;
    u32 button;

    initSettings();
    if (HAVE_AHBPROT) {
        SetAHBPROTAndReload();
    }
    if (theSettings.loadoldcsv) {
        loadiosfromcsv();
    }
    detectSettings();
    if (!theSettings.loadoldcsv && theSettings.DisableSetAHBPROT == 0) {
        checkIOSConditionally();
        WPAD_Init();
    }
    menuSelection = 0;
    while (!ExitMainThread) {
        initNextBuffer();
        PrintBanner();
        printf("[MAIN MENU]\t\t\t\t\t\t\t\t\t\t\t\tCurrent IOS:%d v%d\n", IOS_GetVersion(), IOS_GetRevision());
        if (wiiSettings.ahbprot) {
            Console_SetColors(BLACK, 0, GREEN, 0);
            printf("\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tAHBPROT Enabled");
            Console_SetColors(BLACK, 0, WHITE, 0);
        }
        if (wiiSettings.sysMenuVer < 0) {
            Console_SetColors(BLACK, 0, RED, 0);
            printf("\t\t\t\t\t\t\t\t\t\t\t\t\tCan't get IOS version list");
            Console_SetColors(BLACK, 0, WHITE, 0);
        }
        printf("\n");
        
        for (i = 0; i < MAINMENU_LEN; i += 1) {
            printf("%3s ", (menuSelection == i) ? "-->" : " ");
            if (wiiSettings.regionChangedKoreanWii && i == MAIN_MENU_ADVANCED) {
                Console_SetColors(BLACK, 0, YELLOW, 0);
            }
            if (checkRegionSystemMenuMismatch(0) && i == MAIN_MENU_ADVANCED) {
                Console_SetColors(BLACK, 0, RED, 1);
            }
            if (wiiSettings.missingIOSwarning && i == MAIN_MENU_IOS_MANAGER) {
                Console_SetColors(BLACK, 0, YELLOW, 0);
            }
            if ((checkMissingSystemMenuIOS(0) && i == MAIN_MENU_IOS_MANAGER)
                    || (checkSystemMenuIOSisNotStub(0) && i
                            == MAIN_MENU_IOS_MANAGER)
                    || (wiiSettings.sysMenuVer < 0 && i
                            == MAIN_MENU_SYSTEMMENU_MANAGER)) {
                Console_SetColors(BLACK, 0, RED, 1);
            }
            printf("%s\n", mainMenu[i].DisplayName);
            Console_SetColors(BLACK, 0, WHITE, 0);
        }

        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s)\t\t\t\tChange Selection\t\t\t\t\t(%s)\tChange IOS\n", UP_ARROW, DOWN_ARROW, LEFT_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t\t\t\t(%s)\tWAD Manager\n", RIGHT_ARROW);
        printf("\t(HOME)/GC:(START)\t\tExit");
        flushBuffer();
        
        button = WaitButtons();
        if (button & WPAD_BUTTON_A) mainMenu[menuSelection].fp();
        if (button & WPAD_BUTTON_UP) menuSelection--;
        if (button & WPAD_BUTTON_DOWN) menuSelection++;
        if (menuSelection < 0) menuSelection = MAINMENU_HI;
        if (menuSelection > MAINMENU_HI) menuSelection = 0;
        if (button & WPAD_BUTTON_HOME) ExitMainThreadNow();
        if (button & WPAD_BUTTON_LEFT) mainMenu[MAIN_MENU_CHANGE_IOS].fp();
        if (button & WPAD_BUTTON_RIGHT) mainMenu[MAIN_MENU_WAD_MANAGER].fp();
    }
}

int main(int argc, char **argv) {
    __exception_setreload(5);

    System_Init();
    
    MainThread_Execute();
    if (Shutdown) {
        System_Shutdown();
    } else {
        ReturnToLoader();
    }
    return 0;
}
