#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include "AnyTitleManager.h"
#include "controller.h"
#include "detect_settings.h"
#include "dopios.h"
#include "fat.h"
#include "name.h"
#include "nand.h"
#include "Settings.h"
#include "title_install.h"
#include "tools.h"
#include "video.h"
#include "wad.h"

#define ITEMS_PER_PAGE      18
#define MAX_TITLES_RES      (30 * 12)
#define NUM_TYPES           7
#define TITLE_NAME_WIDTH    49
#define MAX_TITLES 256
#define BRICK_PROTECTION 1
const int ITEMS_PER_PAGE_HALF_MIN_ONE = (ITEMS_PER_PAGE / 2) - 1;

s32 uninstallChecked(u32 thi, u32 tlo) {
    u64 tid = TITLE_ID(thi, tlo);
    s32 ret;
    u32 i, j;
    for (i = 0; i < 7; i += 1) {
        if (types[i] == thi) {
            break;
        }
    }
    if (i == 7) return -1;
    for (j = 0; j < typeCnt[i]; j += 1) {
        if (installed_titles[i][j].tid == tid) break;
    }
    if (j == typeCnt[i]) {
        printf("COULD NOT FIND TITLE!!!!\n");
        PromptAnyKeyContinue();
        return -1;
    }
    TITLE * curTitle = &installed_titles[i][j];
    bool has_db = !curTitle->failedToReadDB,
            has_bn = !curTitle->failedToReadBN, has_00 =
                    !curTitle->failedToRead00;
    ret = 1;
    
    initNextBuffer();
    PrintBanner();
    printf("\x1b[2J\n\n");
#ifndef BRICK_PROTECTION
    printf("WARNING!!! BRICK PROTECTION DISABLED!! WARNING!!! BRICK PROTECTION DISABLED!!\n");
#endif
    printf("+- Title -------------------------------------------------------------------+\n");
    if (has_db) printf(" Name DB  : %s\n", curTitle->nameDB);
    if (has_bn) printf(" Name BAN : %s\n", curTitle->nameBN);
    if (has_00) printf(" Name APP : %s\n", curTitle->name00);
    printf(" Title ID : %s (%08x/%08x)\n", curTitle->text, thi, tlo);
    printf(" IOS req. : %d\n", curTitle->requiredIOS);
    printf(" Version  : %d\n", curTitle->version);
    printf(" Num of Contents  : %d\n", curTitle->num_contents);
    printf("+---------------------------------------------------------------------------+\n");
    printf("+- Contents ----------------------------------------------------------------+\n");
    printContent(tid);
    printf("+---------------------------------------------------------------------------+\n\n");

        if ( tid == TITLE_ID(0x00010001,0xAF1BF516) || // HBC v1.0.7+
            tid == TITLE_ID(0x00010001,0x4a4f4449) || // HBC JODI
            tid == TITLE_ID(0x00010001,0x48415858) ) {// HBC HAXX
            printf("\tBrick WARNING! You are trying to delete HBC!\n");
            printf("Are you sure you want to do this?\n");
            if( !PromptYesNo() ) {
                return 1;
            }
        } else if ( tid == TITLE_ID(0x00010001,0x4d415549) ) {
            // HBC MAUI (Backup HBC)
            printf("\tWARNING: You may want to keep this.\n\tYou are deleting a backup of Homebrew Channel\n");
        }
        // Display a warning if you're deleting the Homebrew Channel's IOS
        if (tid == theSettings.hbcRequiredTID[3]) { //HBC MAUI (Backup HBC)
            printf("\tWARNING: This is the IOS used by the Homebrew Channel!\n\t- Deleting it will cause the Homebrew Channel to stop working!\n");
        }
        if (tid == theSettings.hbcRequiredTID[2]) { //HBC v1.0.7+
            printf("\tWARNING: This is the IOS used by the Homebrew Channel!\n\t- Deleting it will cause the Homebrew Channel to stop working!\n");
        }
        if (tid == theSettings.hbcRequiredTID[1]) {//HBC JODI
            printf("\tWARNING: This is the IOS used by the Homebrew Channel!\n\t- Deleting it will cause the Homebrew Channel to stop working!\n");
        }
        if (tid == theSettings.hbcRequiredTID[0]) {//HBC HAXX
            printf("\tWARNING: This is the IOS used by the Homebrew Channel!\n\t- Deleting it will cause the Homebrew Channel to stop working!\n");
        }

        printf("\t               (-) Uninstall this title\n");
        printf("\t               (B) Abort\n\n");
        flushBuffer();
        int button;
        button = WaitButtons();
        if (ExitMainThread) return 1;
        if (button & WPAD_BUTTON_HOME) ReturnToLoader();
        if (button & WPAD_BUTTON_B) {
            ret = 1;
        } else if (button & WPAD_BUTTON_MINUS) {
            ret = Uninstall_FromTitle(tid);
            PromptAnyKeyContinue();
        }
    return ret;
}

int manageTitles(int menu_index) {
    static int menuItem = 0;
    u32 button = 0, tlo, thi = types[menu_index];
    u64 tid;
    unsigned int num_titles = typeCnt[menu_index];
    int i, ret = 0, startTitles = 0;
    char name[256], name2[256];
    bool notRegion = false, failedGetTMDSize = false;
    u8 mode;
    TITLE *curTitle;
    if (menu_index < 0 || menu_index > 6) return -1;
    while (num_titles > 0 && !(button & WPAD_BUTTON_B)) {
        initNextBuffer();
        PrintBanner();

        if (menuItem > num_titles) {
            menuItem = 0;
            startTitles = 0;
        }
        ret = 1;
        mode = getDispMode();
        printf("\tTitles in %08x\t\titem ", thi);
        Console_SetFgColor(GREEN, 0);
        printf("%2d", menuItem + startTitles + 1);
        Console_SetFgColor(WHITE, 0);
        printf(" of %2d\t(%-5s) (%-9s)\n", num_titles, getDispModeName(), getNamingMode());
        for (i = 0; i < ITEMS_PER_PAGE; i += 1) {
            curTitle = &installed_titles[menu_index][startTitles + i];
            tlo = TITLE_LOWER(curTitle->tid);
            if ((startTitles + i) >= num_titles) {
                break;
            }
            getTitle_Name(name, curTitle);
            u8 len = strlen(name);
            if (len > TITLE_NAME_WIDTH) {
                len = TITLE_NAME_WIDTH;
            }
            memcpy(name2, name, len);
            name2[len] = '\0';
            if (mode == 0) {
                printf("\t%3s %s", (menuItem == i) ? "-->" : " ", name2);
            } else if (mode == 1) {
                printf("\t%3s ", (menuItem == i) ? "-->" : " ");
                Console_SetFgColor(GREEN, 0);
                printf("%s", curTitle->text);
                Console_SetFgColor(WHITE, 0);
                printf(" %s", name2);
            } else {
                printf("\t%3s ", (menuItem == i) ? "-->" : " ");
                Console_SetFgColor(GREEN, 0);
                printf("%s", curTitle->text);
                Console_SetFgColor(WHITE, 0);
                printf(" (");
                Console_SetFgColor(CYAN, 0);
                printf("%08x", tlo);
                Console_SetFgColor(WHITE, 0);
                printf(") %s", name2);
            }
            if ((TITLE_UPPER(curTitle->tid) != 0x1 //not System Title
                    && (curTitle->tid & Region_IDs[getRegion()])
                            != Region_IDs[getRegion()] //not region
                    && (curTitle->tid & 0x41) != 0x41)) { //not general region
                Console_SetFgColor(YELLOW, 0);
                printf("#");
                Console_SetFgColor(WHITE, 0);
                notRegion = true;
            }
            if (curTitle->failedToGetTMDSize) {
                Console_SetFgColor(MAGENTA, 0);
                printf("*");
                Console_SetFgColor(WHITE, 0);
                failedGetTMDSize = true;
            }
            printf("\n");
        }
        if (notRegion) {
            Console_SetFgColor(YELLOW, 0);
            printf("\t\t# Title does not match region(Normal for fakesigned titles/forwarders)\n");
            Console_SetFgColor(WHITE, 0);
        } else {
            printf("\n");
        }
        if (failedGetTMDSize) {
            Console_SetFgColor(MAGENTA, 0);
            printf("\t\t* Failed to get TMD size possibly a bad title\n");
            Console_SetFgColor(WHITE, 0);
        } else {
            printf("\n");
        }
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s) (%s)/(%s) Change Selection\t(B) Back\t(HOME)/GC:(START) Exit\n", UP_ARROW, DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW);
        printf("\t(-)/GC:(L)\tDelete\t\t\t\t(+)/GC:(R)\tExtract to Wad\n");
        printf("\t(1)/GC:(Y)\tChange Display Mode\t(2)/GC:(Z)\tChange Naming Mode");
        flushBuffer();
        button = WaitButtons();
        if (ExitMainThread) return -1;
        curTitle = &installed_titles[menu_index][startTitles + menuItem];
        tlo = TITLE_LOWER(curTitle->tid);
        tid = TITLE_ID(thi, tlo);
        
        if (button & WPAD_BUTTON_HOME) {
            ReturnToLoader();
        } else if (button & WPAD_BUTTON_UP) {
            if (num_titles < ITEMS_PER_PAGE) {
                if (menuItem > 0 && menuItem < num_titles) {
                    menuItem--;
                } else {
                    menuItem = num_titles - 1;
                }
            } else {
                if (menuItem > ITEMS_PER_PAGE_HALF_MIN_ONE) {
                    menuItem--;
                } else if (startTitles > 0) {
                    startTitles--;
                } else if (menuItem > 0) {
                    menuItem--;
                } else {
                    startTitles = num_titles - ITEMS_PER_PAGE;
                    menuItem = ITEMS_PER_PAGE - 1;
                }
            }
        } else if (button & WPAD_BUTTON_DOWN) {
            if (num_titles < ITEMS_PER_PAGE) {
                if (menuItem >= 0 && menuItem + 1 < num_titles) {
                    menuItem++;
                } else {
                    menuItem = 0;
                }
            } else {
                if (menuItem < ITEMS_PER_PAGE_HALF_MIN_ONE) {
                    menuItem++;
                } else if (startTitles + ITEMS_PER_PAGE < num_titles) {
                    startTitles++;
                } else if (startTitles + menuItem + 1 < num_titles) {
                    menuItem++;
                } else {
                    startTitles = 0;
                    menuItem = 0;
                }
            }
        } else if (button & WPAD_BUTTON_LEFT && num_titles > ITEMS_PER_PAGE) {
            if (startTitles + menuItem <= ITEMS_PER_PAGE_HALF_MIN_ONE) {
                startTitles = 0;
                menuItem = 0;
            } else if (menuItem >= ITEMS_PER_PAGE_HALF_MIN_ONE) {
                menuItem -= ITEMS_PER_PAGE_HALF_MIN_ONE;
            } else if (startTitles <= ITEMS_PER_PAGE) {
                startTitles = 0;
                menuItem = ITEMS_PER_PAGE - 1;
            } else {
                startTitles -= ITEMS_PER_PAGE - 1;
                menuItem = ITEMS_PER_PAGE - 1;
            }
        } else if (button & WPAD_BUTTON_RIGHT && num_titles > ITEMS_PER_PAGE) {
            if (startTitles + menuItem + ITEMS_PER_PAGE_HALF_MIN_ONE
                    >= num_titles) {
                startTitles = num_titles - ITEMS_PER_PAGE;
                menuItem = ITEMS_PER_PAGE - 1;
            } else if (menuItem + ITEMS_PER_PAGE_HALF_MIN_ONE < ITEMS_PER_PAGE) {
                menuItem += ITEMS_PER_PAGE_HALF_MIN_ONE;
            } else if (startTitles + menuItem + ITEMS_PER_PAGE >= num_titles) {
                startTitles = num_titles - ITEMS_PER_PAGE;
                menuItem = ITEMS_PER_PAGE - 1;
            } else {
                startTitles += ITEMS_PER_PAGE - 1;
                menuItem = 0;
            }
        } else if (button & WPAD_BUTTON_1) {
            changeDispMode();
        } else if (button & WPAD_BUTTON_2) {
            changeNamingMode();
        } else if (button & WPAD_BUTTON_PLUS) {
            PrintBanner();
            char testid[256];
            ret = Fat_Mount(&fdevList[FAT_SD]);
            if (ret < 0) {
                continue;
            }
            if (curTitle->tid == TITLE_ID(1, 2)) {
                snprintf(testid, sizeof(testid), "sd:/WAD/wiimod/RVL-WiiSystemmenu-v%u.wad", curTitle->version);
            } else if (curTitle->tid == TITLE_ID(1, 0x100)) {
                snprintf(testid, sizeof(testid), "sd:/WAD/wiimod/RVL-bc-v%d.wad", curTitle->version);
            } else if (curTitle->tid == TITLE_ID(1, 0x101)) {
                snprintf(testid, sizeof(testid), "sd:/WAD/wiimod/RVL-mios-v%d.wad", curTitle->version);
            } else if (thi == 1 && tlo > 2 && tlo < 255) {
                snprintf(testid, sizeof(testid), "sd:/WAD/wiimod/IOS%d-64-v%d.wad", tlo, curTitle->version);
            } else {
                snprintf(testid, sizeof(testid), "sd:/WAD/wiimod/%08x-%08x.wad", thi, tlo);
            }
            Wad_Dump(tid, testid);
            Fat_Unmount(&fdevList[FAT_SD]);
            PromptAnyKeyContinue();
        } else if (button & WPAD_BUTTON_MINUS) {
            PrintBanner();
            ret = uninstallChecked(thi, tlo);
            if (ret == 0) {
                updateTitles();
                theSettings.neverSet = 0;
                num_titles = typeCnt[menu_index];
            }
        }
    }
    return ret;
}

void anyTitleManagerMenu() {
    // The name of the database file
    static int menuItem = 0;
    u32 button = 0;
    int ret;

    PrintBanner();

    if (wiiSettings.reRunWithSU) {
        updateTitles();
    }

    while (!(button & WPAD_BUTTON_B)) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n\n\t\t\t\t%3s 00000001 - System Titles\n", (menuItem == 0)
            ? "-->" : " ");
        printf("\t\t\t\t%3s 00010000 - Disc Game Titles (and saves)\n", (menuItem
                == 1) ? "-->" : " ");
        printf("\t\t\t\t%3s 00010001 - Installed Channel Titles\n", (menuItem
                == 2) ? "-->" : " ");
        printf("\t\t\t\t%3s 00010002 - System Channel Titles\n", (menuItem == 3)
            ? "-->" : " ");
        printf("\t\t\t\t%3s 00010004 - Games that use Channels (Channel+Save)\n", (menuItem
                == 4) ? "-->" : " ");
        printf("\t\t\t\t%3s 00010005 - Downloadable Game Content\n", (menuItem
                == 5) ? "-->" : " ");
        printf("\t\t\t\t%3s 00010008 - Hidden Channels\n", (menuItem == 6)
            ? "-->" : " ");
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s)\t\t\t\tChange Selection\n", UP_ARROW, DOWN_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        printf("\t(HOME)/GC:(START)\t\tExit");
        flushBuffer();
        
        button = WaitButtons();
        if (ExitMainThread) break;
        if (button & WPAD_BUTTON_HOME) ReturnToLoader();
        if (button & WPAD_BUTTON_A) {
            Nand_Init();
            ret = manageTitles(menuItem);
            ISFS_Deinitialize();
            if (ret < 1) {
                PrintBanner();
                printf("\nNo titles installed");
                PromptAnyKeyContinue();
            }
        }

        if (button & WPAD_BUTTON_DOWN) menuItem++;
        if (button & WPAD_BUTTON_UP) menuItem--;
        
        if (menuItem > 6) menuItem = 0;
        if (menuItem < 0) menuItem = 6;
    }
}
