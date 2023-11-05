#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <wiiuse/wpad.h>

#include "controller.h"
#include "detect_settings.h"
#include "dopios.h"
#include "IOSCheck.h"
#include "IOSMenu.h"
#include "nand.h"
#include "network.h"
#include "title_install.h"
#include "SystemMenuMenu.h"
#include "tools.h"
#include "video.h"

#define SYSTEMMENUS_LEN_PPD (sizeof(SystemMenus) / sizeof(SystemMenu))

int getIOSRevForSystemMenu(int sysVersion);
void InstallTheChosenSystemMenu(int region, int menu, bool chooseVersion, bool chooseReqIOS, int source);

typedef struct SystemMenu {
    const char Name[30];
} SystemMenu;

//These two need to stay in sync
typedef enum {
    SYSTEM_MENU_2_0,
    SYSTEM_MENU_2_1,
    SYSTEM_MENU_2_2,
    SYSTEM_MENU_3_0,
    SYSTEM_MENU_3_1,
    SYSTEM_MENU_3_2,
    SYSTEM_MENU_3_3,
    SYSTEM_MENU_3_4,
    SYSTEM_MENU_3_5,
    SYSTEM_MENU_4_0,
    SYSTEM_MENU_4_1,
    SYSTEM_MENU_4_2,
    SYSTEM_MENU_4_3,
} SYSTEM_MENUS;

const struct SystemMenu SystemMenus[] = { { "System Menu 2.0" }, {
    "System Menu 2.1" }, { "System Menu 2.2" }, { "System Menu 3.0" }, {
    "System Menu 3.1" }, { "System Menu 3.2" }, { "System Menu 3.3" }, {
    "System Menu 3.4" }, { "System Menu 3.5 (Korea Only)" }, {
    "System Menu 4.0" }, { "System Menu 4.1" }, { "System Menu 4.2" }, {
    "System Menu 4.3" } };
const int SYSTEMMENUS_LEN = SYSTEMMENUS_LEN_PPD;
const int SYSTEMMENUS_HI = SYSTEMMENUS_LEN_PPD - 1;

int getIOSRevForSystemMenu(int sysVersion) {
    if (sysVersion == 97 || sysVersion == 128 || sysVersion == 130
            || sysVersion == 192) {
        return 10;//2.0, 2.1E
    } else if (sysVersion == 192 || sysVersion == 193 || sysVersion == 194) {
        return 12;//2.2
    } else if (sysVersion == 224 || sysVersion == 225 || sysVersion == 226) {
        return 1037;//3.0
    } else if (sysVersion == 256 || sysVersion == 257 || sysVersion == 258) {
        return 1039;//3.1
    } else if (sysVersion == 289 || sysVersion == 290 || sysVersion == 288) {
        return 1040;//3.2
    } else if (sysVersion == 353 || sysVersion == 354 || sysVersion == 352) {
        return 2576;//3.3U, 3.3E, 3.3J
    } else if (sysVersion == 326) {
        return 2320;//3.3K
    } else if (sysVersion == 385 || sysVersion == 386 || sysVersion == 384) {
        return 4889;//3.4
    } else if (sysVersion == 390) {
        return 5661;//3.5K
    } else if (sysVersion == 417 || sysVersion == 418 || sysVersion == 416
            || sysVersion == 449 || sysVersion == 450 || sysVersion == 448
            || sysVersion == 454) {
        return 6174;//4.0, 4.1
    } else if (sysVersion == 481 || sysVersion == 482 || sysVersion == 480
            || sysVersion == 486) {
        return 6687;//4.2
    } else if (sysVersion == 514 || sysVersion == 513 || sysVersion == 518
            || sysVersion == 512) {
        return 6943;//4.3
    }
    return 1040;
}

u16 chooseNewVersion(u16 cur) {
    u16 any = cur;
    u16 orig = cur;
    static s32 lastchoice = 4;
    s32 menuSelection = lastchoice;
    u32 button, selected = 0;

    while (!selected) {
        initNextBuffer();
        PrintBanner();
        printf("\n\nSelect System Menu Version\n");
        printf("%3s 2\n", (menuSelection == 0) ? "-->" : " ");
        printf("%3s %u\n", (menuSelection == 1) ? "-->" : " ", orig);
        printf("%3s 54321\n", (menuSelection == 2) ? "-->" : " ");
        printf("%3s %u\n", (menuSelection == 3) ? "-->" : " ", USHRT_MAX);
        printf("\n%3s Any IOS %u\n", (menuSelection == 4) ? "-->" : " ", any);
        printf("%3s Go by 10s\n", (menuSelection == 5) ? "-->" : " ");
        printf("%3s Go by 100s\n", (menuSelection == 6) ? "-->" : " ");
        printf("%3s Go by 1000s\n", (menuSelection == 7) ? "-->" : " ");
        flushBuffer();
        while (1) {
            button = WaitButtons();
            if (ExitMainThread) return 0;
            if (button & WPAD_BUTTON_B) {
                return 0;
            }
            if (button & WPAD_BUTTON_A) {
                if (menuSelection == 0) cur = 2;
                if (menuSelection == 1) cur = orig;
                if (menuSelection == 2) cur = 54321;
                if (menuSelection == 3) cur = USHRT_MAX;
                if (menuSelection == 4) cur = any;
                selected = 1;
                if (menuSelection > 4) selected = 0;
                break;
            }
            if (button & WPAD_BUTTON_DOWN) {
                menuSelection += 1;
                if (menuSelection > 7) {
                    menuSelection = 0;
                }
                break;
            }
            if (button & WPAD_BUTTON_UP) {
                menuSelection -= 1;
                if (menuSelection < 0) {
                    menuSelection = 7;
                }
                break;
            }
            if (button & WPAD_BUTTON_LEFT) {
                if (menuSelection > 3) {
                    any -= (int) pow(10, menuSelection - 4);
                }
                break;
            }
            if (button & WPAD_BUTTON_RIGHT) {
                if (menuSelection > 3) {
                    any += (int) pow(10, menuSelection - 4);
                }
                break;
            }
            if (button & WPAD_BUTTON_HOME) ReturnToLoader();
        }
    }
    lastchoice = menuSelection;
    return cur;
}

void InstallTheChosenSystemMenu(int region, int menu, bool chooseVersion, bool chooseReqIOS, int source) {
    s32 ret = -1;
    u64 sysTid = TITLE_ID(1, 2);
    u16 sysVersion = 0;
    int hasIOS = 0;

    if( chooseVersion || chooseReqIOS ) {
        ret = get_certs();
        if( ret < 0 ) {
            printf("\nFailed to get certs ret = %d\n", ret);
            PromptAnyKeyContinue();
            ret = 0;
        } else {
            ret = check_fakesig();
        }
        if (ret == -1036) {
            //This error is ok for what we need to do.
            ret = 0;
        }
        if (ret < 0) {
            initNextBuffer();
            PrintBanner();
            printf(
                "\n\nFake Sign Patch required, check your IOSs for a valid IOS\n");
            flushBuffer();
            PromptAnyKeyContinue();
            return;
        }
    }
    ret = -1;

    //Initialize Network
    Network_Init();
    Nand_Init();
    if (region == REGION_U) {//North America
        if (menu == SYSTEM_MENU_2_0)
            sysVersion = 97; //2.0U
        else if (menu == SYSTEM_MENU_2_2)
            sysVersion = 193; //2.2U
        else if (menu == SYSTEM_MENU_3_0)
            sysVersion = 225; //3.0U
        else if (menu == SYSTEM_MENU_3_1)
            sysVersion = 257; //3.1U
        else if (menu == SYSTEM_MENU_3_2)
            sysVersion = 289; //3.2U
        else if (menu == SYSTEM_MENU_3_3)
            sysVersion = 353; //3.3U
        else if (menu == SYSTEM_MENU_3_4)
            sysVersion = 385; //3.4U
        else if (menu == SYSTEM_MENU_4_0)
            sysVersion = 417; //4.0U
        else if (menu == SYSTEM_MENU_4_1)
            sysVersion = 449; //4.1U
        else if (menu == SYSTEM_MENU_4_2)
            sysVersion = 481; //4.2U
        else if (menu == SYSTEM_MENU_4_3)
            sysVersion = 513; //4.3U
    } else if (region == REGION_E) {//Europe
        if (menu == SYSTEM_MENU_2_0)
            sysVersion = 130; //2.0E
        else if (menu == SYSTEM_MENU_2_1)
            sysVersion = 162; //2.1E
        else if (menu == SYSTEM_MENU_2_2)
            sysVersion = 194; //2.2E
        else if (menu == SYSTEM_MENU_3_0)
            sysVersion = 226; //3.0E
        else if (menu == SYSTEM_MENU_3_1)
            sysVersion = 258; //3.1E
        else if (menu == SYSTEM_MENU_3_2)
            sysVersion = 290; //3.2E
        else if (menu == SYSTEM_MENU_3_3)
            sysVersion = 354; //3.3E
        else if (menu == SYSTEM_MENU_3_4)
            sysVersion = 386; //3.4E
        else if (menu == SYSTEM_MENU_4_0)
            sysVersion = 418; //4.0E
        else if (menu == SYSTEM_MENU_4_1)
            sysVersion = 450; //4.1E
        else if (menu == SYSTEM_MENU_4_2)
            sysVersion = 482; //4.2E
        else if (menu == SYSTEM_MENU_4_3)
            sysVersion = 514; //4.3E
    } else if (region == REGION_J) {//Japan
        if (menu == SYSTEM_MENU_2_0)
            sysVersion = 128; //2.0J
        else if (menu == SYSTEM_MENU_2_2)
            sysVersion = 192; //2.2J
        else if (menu == SYSTEM_MENU_3_0)
            sysVersion = 224; //3.0J
        else if (menu == SYSTEM_MENU_3_1)
            sysVersion = 256; //3.1J
        else if (menu == SYSTEM_MENU_3_2)
            sysVersion = 288; //3.2J
        else if (menu == SYSTEM_MENU_3_3)
            sysVersion = 352; //3.3J
        else if (menu == SYSTEM_MENU_3_4)
            sysVersion = 384; //3.4J
        else if (menu == SYSTEM_MENU_4_0)
            sysVersion = 416; //4.0J
        else if (menu == SYSTEM_MENU_4_1)
            sysVersion = 448; //4.1J
        else if (menu == SYSTEM_MENU_4_2)
            sysVersion = 480; //4.2J
        else if (menu == SYSTEM_MENU_4_3)
            sysVersion = 512; //4.3J
    } else if (region == REGION_K) {//Korea
        if (menu == SYSTEM_MENU_3_3)
            sysVersion = 326; //3.3K
        else if (menu == SYSTEM_MENU_3_5)
            sysVersion = 390; //3.5K
        else if (menu == SYSTEM_MENU_4_1)
            sysVersion = 454; //4.1K
        else if (menu == SYSTEM_MENU_4_2)
            sysVersion = 486; //4.2K
        else if (menu == SYSTEM_MENU_4_3)
            sysVersion = 518; //4.3K
    }

    u32 reqIOS = 0;
    int checkForIOS = getIOSVerForSystemMenu(sysVersion);
    u16 checkForRev = getIOSRevForSystemMenu(sysVersion);
    if( chooseReqIOS ) {
        reqIOS = chooseiosloc("Choose Required IOS\n", checkForIOS);
        if( reqIOS != 0 ) {
            checkForIOS = reqIOS;
        }
    }
    if( !chooseReqIOS || checkForIOS > 0 ) {
        hasIOS = CheckForIOS(checkForIOS, checkForRev, sysVersion, chooseVersion, chooseReqIOS);
    }
    
    if (hasIOS >= 0) {//Download the System Menu
        ret = 0;
        u16 installversion = (chooseVersion)?chooseNewVersion(sysVersion):sysVersion;
        if( installversion > 0 ) {
            if( source >= 0  && source < 3) {
                ret = loadSystemMenuAndInstallAndChangeVersion(source, sysVersion, installversion, reqIOS);
            } else if ( source == 3 ) {
                ret = downloadAndInstallAndChangeVersion(sysTid, sysVersion, "System Menu", installversion, reqIOS);
            }
            if( source >=0 ) {
                if (ret < 0) {
                    printf("\nError Installing System Menu. Ret: %d\n", ret);
                    PromptAnyKeyContinue();
                    ret = 0;
                } else {
                    updateTitles();
                    updateSysMenuVersion();
                    ret = 1;
                }
            }
        } else {
            updateTitles();
        }
    }
    if( hasIOS == 1 && ret != 1) {
        updateTitles();
    }
    //Close the NAND
    ISFS_Deinitialize();
}

int systemMenuMenu() {
    static int menuItem = 0, systemSelection = SYSTEM_MENU_4_1, neverSet = 0,
            regionSelection = 0, source = 3;
    bool chooseVersion = false, chooseReqIOS = false;
    u32 button = 0;
    char *optionsstring[4] = { "SD", "USB", "SMB", "NUS"};

    if (neverSet == 0) {
        neverSet = 1;
        regionSelection = getRegion();
    }

    while (!(button & WPAD_BUTTON_B)) {
        initNextBuffer();
        if (regionSelection == REGION_K && systemSelection == 0) {
            systemSelection = SYSTEM_MENU_3_3;
        }
        PrintBanner();

        if( wiiSettings.regionChangedKoreanWii ) {
            Console_SetColors(BLACK, 0, YELLOW, 0);
            printf("\nYou have a region changed Wii that was originally Korean\n");
            printf("You are at risk of Error 003 brick if you keep the current configuration\n");
            Console_SetColors(BLACK, 0, WHITE, 0);
        } else {
            printf("\n\n\n");
        }
        printf("\t\t\t\t%3s Install System Menu:   %s\n", (menuItem == 0
            ? "-->" : " "), SystemMenus[systemSelection].Name);
        printf("\t\t\t\t%3s Region:                %s\n", (menuItem == 1
            ? "-->" : " "), Regions[regionSelection].Name);
        printf("\t\t\t\t%3s Choose Version:        %s\n", (menuItem == 2
            ? "-->" : " "), (chooseVersion)?"Yes":"No");
        printf("\t\t\t\t%3s Choose Required IOS:   %s\n", (menuItem == 3
            ? "-->" : " "), (chooseReqIOS)?"Yes":"No");
        printf("\t\t\t\t%3s Choose install source: %s\n", (menuItem == 4
            ? "-->" : " "),optionsstring[source] );
        
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s) (%s)/(%s)\t\tChange Selection\n", UP_ARROW,
            DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        printf("\t(HOME)/GC:(START)\t\tExit");
        flushBuffer();
        button = WaitButtons();
        if (ExitMainThread) return 0;
        if (button & WPAD_BUTTON_HOME) ReturnToLoader();
        if (button & WPAD_BUTTON_A) {
            initNextBuffer();
            PrintBanner();
            flushBuffer();
            if (PromptContinue()) {
                InstallTheChosenSystemMenu(regionSelection, systemSelection, chooseVersion, chooseReqIOS, source);
            }
        }
        if (button & WPAD_BUTTON_DOWN) menuItem++;
        if (button & WPAD_BUTTON_UP) menuItem--;
        
        if (menuItem > 4) menuItem = 0;
        if (menuItem < 0) menuItem = 4;
        
        if (button & WPAD_BUTTON_LEFT) {
            if (menuItem == 0) systemSelection--;
            if (menuItem == 1) regionSelection--;
            if (menuItem == 2) chooseVersion = !chooseVersion;
            if (menuItem == 3) chooseReqIOS = !chooseReqIOS;
            if (menuItem == 4) source--;
            
            if (systemSelection < 0) systemSelection = SYSTEMMENUS_HI;
            if (regionSelection < 0) regionSelection = REGIONS_HI;
            if (source < 0) source = 3;
            
            //Only let 3.5 appear if Korea is the selected region
            if (systemSelection == SYSTEM_MENU_3_5 && regionSelection
                    != REGION_K) {
                systemSelection--;
            }

            //2.1 is only E
            if (systemSelection == SYSTEM_MENU_2_1 && regionSelection
                    != REGION_E) {
                systemSelection--;
            }

            //Prevent Error 003
            if( wiiSettings.regionChangedKoreanWii && regionSelection != REGION_K &&
                    systemSelection > SYSTEM_MENU_4_1 ) {
                systemSelection = SYSTEM_MENU_4_1;
            }

            //Get rid of certain menus from Korea selection
            while (regionSelection == REGION_K && (systemSelection
                    < SYSTEM_MENU_3_3 || systemSelection == SYSTEM_MENU_3_4
                    || systemSelection == SYSTEM_MENU_4_0)) {
                systemSelection--;
                if (systemSelection < 0) systemSelection = SYSTEMMENUS_HI;
            }
            
            //3.3K is not on nus
            if (systemSelection == SYSTEM_MENU_3_3 && regionSelection
                    == REGION_K && source==3 ) {
                if(menuItem == 4){
                    source--;
                } else {
                    source = 0;
                }
            }
        }
        if (button & WPAD_BUTTON_RIGHT) {
            if (menuItem == 0) systemSelection++;
            if (menuItem == 1) regionSelection++;
            if (menuItem == 2) chooseVersion = !chooseVersion;
            if (menuItem == 3) chooseReqIOS = !chooseReqIOS;
            if (menuItem == 4) source++;
            
            if (systemSelection > SYSTEMMENUS_HI) systemSelection = 0;
            if (regionSelection > REGIONS_HI) regionSelection = 0;
            if (source > 3) source = 0;
            
            //Only let 3.5 appear if Korea is the selected region
            if (systemSelection == SYSTEM_MENU_3_5 && regionSelection
                    != REGION_K) {
                systemSelection++;
            }

            //Only let 2.1 appear if Europe is the selected region
            if (systemSelection == 1 && regionSelection != REGION_E) {
                systemSelection++;
            }

            //Prevent Error 003
            if( wiiSettings.regionChangedKoreanWii && regionSelection != REGION_K &&
                    systemSelection > SYSTEM_MENU_4_1 ) {
                systemSelection = 0;
            }

            //Get rid of certain menus from Korea selection
            while (regionSelection == REGION_K && (systemSelection
                    < SYSTEM_MENU_3_3 || systemSelection == SYSTEM_MENU_3_4
                    || systemSelection == SYSTEM_MENU_4_0)) {

                systemSelection++;
                if (systemSelection > SYSTEMMENUS_HI) systemSelection = 0;
            }

            //3.3K is not on nus
            if (systemSelection == SYSTEM_MENU_3_3 && regionSelection
                    == REGION_K && source==3 ) {
                source=0;
            }
        }
        if (systemSelection < 0) systemSelection = SYSTEMMENUS_HI;
        if (systemSelection > SYSTEMMENUS_HI) systemSelection = 0;
        if (regionSelection < 0) regionSelection = REGIONS_HI;
        if (regionSelection > REGIONS_HI) regionSelection = 0;
    }
    return 0;
}
