#include <ctype.h>
#include <fat.h>
#include <malloc.h>
#include <ogcsys.h>
#include <ogc/pad.h>
#include <sdcard/gcsd.h>
#include <sdcard/wiisd_io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <unistd.h>
#include <wiilight.h>
#include <wiiuse/wpad.h>

#include "controller.h"
#include "detect_settings.h"
#include "dopios.h"
#include "Error.h"
#include "fat.h"
#include "IOSCheck.h"
#include "nand.h"
#include "Settings.h"
#include "smb.h"
#include "tools.h"
#include "video.h"
#include "wad.h"
#include "wadmanager.h"

/* Macros */
#define NB_NAND_DEVICES     (sizeof(ndevList) / sizeof(nandDevice))

const int ENTRIES_PER_PAGE_HALF_MIN_ONE = (ENTRIES_PER_PAGE / 2) - 1;

void Menu_NandDevice(void);
int Menu_WadList(void);
void Menu_WadManage(BROWSERENTRY *, char *);
void CheckPassword(void);
u32 NoWaitButtons(void);
void ResetBrowser();

/* NAND device list */
//static nandDevice ndevList[] = {
nandDevice ndevList[] = { { "Disable", 0, 0x00, 0x00 }, { "SD/SDHC Card", 1,
    0xF0, 0xF1 }, { "USB 2.0 Mass Storage Device", 2, 0xF2, 0xF3 }, };

// wiiNinja: Define a buffer holding the previous path names as user
// traverses the directory tree. Max of 10 levels is define at this point
u8 gDirLevel = 0;
static char gDirList[MAX_DIR_LEVELS][MAX_FILE_PATH_LEN];
static s32 gSeleted[MAX_DIR_LEVELS];
static s32 gStart[MAX_DIR_LEVELS];

char gTmpFilePath[MAX_FILE_PATH_LEN];

BROWSERENTRY * browserList = NULL; // list of files/folders in browser

char rootdir[1024];
int hasinstalled;

int Menu_FatDevice(void) {
    s32 ret = 0, selected = 0;

    /* Unmount FAT device */
    if (fdev) Fat_Unmount(fdev);

    /* Select source device */
    if (theSettings.fatDeviceIndex < 0) {
        for (;;) {
            /* Clear console */
            initNextBuffer();
            PrintBanner();

            /* Selected device */
            fdev = &fdevList[selected];
            
            printf("\n\n\n\t\t\t\t>> Select source device: < %s >\n\n", fdev->name);

            SetToInputLegendPos();
            horizontalLineBreak();
            printf("\t(%s)/(%s)\t\t\t\tChange Selection\n", LEFT_ARROW, RIGHT_ARROW);
            printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
            printf("\t(HOME)/GC:(START)\t\tExit");
            flushBuffer();

            u32 buttons = WaitButtons();

            /* LEFT/RIGHT buttons */
            if (buttons & WPAD_BUTTON_LEFT) {
                if ((--selected) <= -1) selected = (NB_FAT_DEVICES - 1);
            }
            if (buttons & WPAD_BUTTON_RIGHT) {
                if ((++selected) >= NB_FAT_DEVICES) selected = 0;
            }

            /* HOME button */
            if (buttons & WPAD_BUTTON_HOME) ReturnToLoader();

            /* A button */
            if (buttons & WPAD_BUTTON_A) break;

            /* A button */
            if (buttons & WPAD_BUTTON_B) return -1;
        }
    } else {
        fdev = &fdevList[theSettings.fatDeviceIndex];
    }

    initNextBuffer();
    PrintBanner();
    printf("[+] Mounting device, please wait...");
    fflush(stdout);

    /* Mount FAT device */
    if (strncmp(fdev->name, "SMB", 3)) { //not smb
        ret = Fat_Mount(fdev);
    }
    if (ret < 0) {
        printf(" ERROR! (ret = %d)\n", ret);
        flushBuffer();
    } else {
        printf(" OK!\n");
        flushBuffer();
        return 1;
    }

    WiiLightControl(WII_LIGHT_OFF);
    PromptAnyKeyContinue();

    /* Prompt menu again */
    return Menu_FatDevice();
}

/* Install and/or Uninstall multiple WADs - Leathl */
int Menu_BatchProcessWads(BROWSERENTRY *files, int fileCount, char *inFilePath,
    int installCnt, int uninstallCnt) {
    int count;
    
    for (;;) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n\n\t\t\t\t");
        
        if ((installCnt > 0) & (uninstallCnt == 0)) {
            printf("[+] %d file%s marked for installation.\n", installCnt, (installCnt
                    == 1) ? "" : "s");
            printf("\t\t\t\t    Do you want to proceed?\n");
        } else if ((installCnt == 0) & (uninstallCnt > 0)) {
            printf("[+] %d file%s marked for uninstallation.\n", uninstallCnt, (uninstallCnt
                    == 1) ? "" : "s");
            printf("\t\t\t\t    Do you want to proceed?\n");
        } else {
            printf("[+] %d file%s marked for installation and %d file%s for uninstallation.\n", installCnt, (installCnt
                    == 1) ? "" : "s", uninstallCnt, (uninstallCnt == 1) ? ""
                : "s");
            printf("\t\t\t\t    Do you want to proceed?\n");
        }
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\n\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        flushBuffer();
        
        u32 buttons = WaitButtons();
        
        if (buttons & WPAD_BUTTON_A) break;
        
        if (buttons & WPAD_BUTTON_B) return 0;
    }

    WiiLightControl(WII_LIGHT_ON);
    int errors = 0, ret, mode;
    initNextBuffer();
    BROWSERENTRY *thisFile = NULL;
    FILE *fp = NULL;
    int cur = 1;
    for (count = 0; count < fileCount; count++) {
        thisFile = &files[count];
        
        if ((thisFile->install == 1) | (thisFile->install == 2)) {
            mode = thisFile->install;
            PrintBanner();
            snprintf(gTmpFilePath, sizeof(gTmpFilePath), "%s/%s", inFilePath, thisFile->filename);

            fp = fopen(gTmpFilePath, "rb");
            if (!fp) {
                printf(" Error opening %s!\n", thisFile->filename);
                errors += 1;
                flushBuffer();
                continue;
            }
            printf("[+] %d of %d %s %s please wait...\n", cur++, installCnt
                    + uninstallCnt, (mode == 2) ? "Uninstalling" : "Installing", thisFile->filename);
            flushBuffer();
            if (mode == 2) {
                ret = Wad_Uninstall(fp);
            } else {
                ret = Wad_Install(fp);
            }
            thisFile->ret = ret;
            if (ret < 0) {
                errors += 1;
            } else {
                hasinstalled = 1;
            }

            if (fp) fclose(fp);
        }
        flushBuffer();
        u32 buttons;
        ScanPads(&buttons);
        
        if (buttons & WPAD_BUTTON_B) break;
    }
    WiiLightControl(WII_LIGHT_OFF);
    
    printf("\n");
    
    if (errors > 1) {
        printf("\t%d errors occured...\n", errors);
    } else if (errors > 0) {
        printf("\t%d error occured...\n", errors);
    }
    for (count = 0; count < fileCount; count++) {
        thisFile = &files[count];
        if ((thisFile->install == 1) && thisFile->ret < 0) {
            printf("Error installing %s::\n%s", thisFile->filename, EsErrorCodeString(thisFile->ret));
        } else if ((thisFile->install == 2) && thisFile->ret < 0) {
            printf("Error uninstalling %s::\n%s", thisFile->filename, EsErrorCodeString(thisFile->ret));
        }
    }

    PromptAnyKeyContinue();
    
    return 1;
}

/* File Operations - Leathl */
int Menu_FileOperations(BROWSERENTRY *file, char *inFilePath) {
    f32 filesize = (file->length / MB_SIZE);
    
    for (;;) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n\n\t\t\t\t[+] WAD Filename : %s\n", file->filename);
        printf("\t\t\t\t    WAD Filesize : %.2f MB\n\n\n", filesize);
        printf("\t\t\t\t[+] Select action: < %s WAD >\n\n", "Delete"); //There's yet nothing else than delete
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s)\t\t\t\tChange Selection\n", LEFT_ARROW, RIGHT_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        flushBuffer();
        
        u32 buttons = WaitButtons();
        
        /* A button */
        if (buttons & WPAD_BUTTON_A) break;

        /* B button */
        if (buttons & WPAD_BUTTON_B) return 0;
    }

    ClearScreen();
    
    printf("[+] Deleting \"%s\", please wait...\n", file->filename);
    
    snprintf(gTmpFilePath, sizeof(gTmpFilePath), "%s/%s", inFilePath, file->filename);
    int error = remove(gTmpFilePath);
    if (error != 0) {
        printf("\tERROR!");
    } else {
        printf("\tSuccessfully deleted!");
    }

    PromptAnyKeyContinue();
    
    return !error;
}

int Menu_WadList(void) {
    u32 fileCnt = 0;
    s32 ret, selected = 0, start = 0;
    int installCnt = 0, installall = 0, batchFolder = 0;
    int uninstallCnt = 0, exit = 0;

    printf("[+] Retrieving file list...");
    fflush(stdout);
    
    if (!strcmp(fdev->mount, "smb")) {
        printf("\nConnecting smb...");
        ret = ConnectSMB();
        printf("%s\n", (ret) ? "Worked" : "Failed");
        if (ret <= 0) {
            PromptAnyKeyContinue();
            return -1;
        }
    } else {
        CloseSMB();
    }

    gDirLevel = 0;
    
    // wiiNinja: The root is always the primary folder
    // But if the user has a /wad directory, just go there. This makes
    // both sides of the argument win

    snprintf(rootdir, sizeof(rootdir), "%s:" WAD_DIRECTORY, fdev->mount);

    PushCurrentDir(rootdir, 0, 0);
    if (strcmp(WAD_DIRECTORY, theSettings.startupPath) != 0) {
        snprintf(rootdir, sizeof(rootdir), "%s:%s", fdev->mount, theSettings.startupPath);
        PushCurrentDir(rootdir, 0, 0); // wiiNinja
    }

    /* Retrieve filelist */

    fileCnt = ParseDirectory(0);
    char *tmpCurPath;
    u32 counter, buttons, cnt, offset;
    for (; !exit;) {
        for (;;) {
            /* No files */
            if (fileCnt <= 0) {
                selected = 0;
                start = 0;
                tmpCurPath = PopCurrentDir(&selected, &start);
                if (tmpCurPath != NULL) {
                    snprintf(rootdir, sizeof(rootdir), "%s", tmpCurPath);
                    fileCnt = ParseDirectory(0);
                    installCnt = 0;
                    uninstallCnt = 0;
                    PromptAnyKeyContinue();
                    continue;
                }
                printf(" No files found!\n");
                PromptAnyKeyContinue();
                return -1;
            }

            if (batchFolder) {
                for (counter = 0; counter < fileCnt; counter++) {
                    BROWSERENTRY *file = &browserList[counter];
                    if (!file->isdir) {
                        file->install = batchFolder;
                        if (batchFolder == 1) {
                            installCnt += 1;
                        } else {
                            uninstallCnt += 1;
                        }
                    }
                }
                batchFolder = 0;
            }

            /* Clear console */
            initNextBuffer();
            PrintBanner();
            fflush(stdout);

            /** Print entries **/
            cnt = strlen(rootdir);
            if (cnt > MAXDISPLAY) {
                offset = cnt - MAXDISPLAY;
            } else {
                offset = 0;
            }

            printf("[+] %s:\t\t\t\t\t\t\t\t\t\t\t", rootdir + offset);

            if (installCnt > 0 || uninstallCnt > 0) {
                printf("Marked : %d", (installCnt + uninstallCnt));
            }
            printf("\n\n");
            /* Print entries */
            for (cnt = start; cnt < fileCnt; cnt++) {
                BROWSERENTRY *file = &browserList[cnt];
                f32 filesize = file->length / MB_SIZE;
                /* Entries per page limit */
                if ((cnt - start) >= ENTRIES_PER_PAGE) break;

                int len = strlen(file->displayname);
                if (file->isdir) {
                    len += 2;
                }
                int x, spaces = MAXDISPLAY - len;

                if (file->isdir) { // wiiNinja
                    printf("   %2s [%s]", (cnt == start + selected) ? ">>"
                        : "  ", file->displayname);
                } else {
                    printf("   %2s", (cnt == start + selected) ? ">>" : "  ");
                    if (file->install == 1) {
                        Console_SetColors(BLACK, 0, CYAN, 0);
                        printf("+");
                        Console_SetColors(BLACK, 0, WHITE, 0);
                    } else if (file->install == 2) {
                        Console_SetColors(BLACK, 0, RED, 0);
                        printf("-");
                        Console_SetColors(BLACK, 0, WHITE, 0);
                    } else {
                        printf(" ");
                    }
                    printf("%s", file->displayname);
                }
                for (x = 0; x < spaces; x++) {
                    printf(" ");
                }
                if (file->isdir) {
                    printf("\t<DIR>\n");
                } else {
                    printf("\t(%.2f MB)\n", filesize);
                }
            }

            SetToInputLegendPos();
            horizontalLineBreak();
            printf("\t(A)\tProceed\t\t\t\t\t\t\t\t\t(B)\t");
            if (gDirLevel > 1) {
                printf("Go up directory");
            } else {
                printf("Return to main menu");
            }
            printf("\n\t(+)\tMark for batch install\t\t\t\t(-)\tMark for batch uninstall\n");
            printf("\t(1)\tInstall all wads in folder\t\t\t(2)\tFile operations");

            flushBuffer();
            /** Controls **/
            buttons = WaitButtons();

            /* DPAD buttons */
            if (buttons & WPAD_BUTTON_UP) {
                if (fileCnt < ENTRIES_PER_PAGE) {
                    if (selected > 0 && selected < fileCnt) {
                        selected--;
                    } else {
                        selected = fileCnt - 1;
                    }
                } else {
                    if (selected > ENTRIES_PER_PAGE_HALF_MIN_ONE) {
                        selected--;
                    } else if (start > 0) {
                        start--;
                    } else if (selected > 0) {
                        selected--;
                    } else {
                        start = fileCnt - ENTRIES_PER_PAGE;
                        selected = ENTRIES_PER_PAGE - 1;
                    }
                }
            } else if (buttons & WPAD_BUTTON_DOWN) {
                if (fileCnt < ENTRIES_PER_PAGE) {
                    if (selected >= 0 && selected + 1 < fileCnt) {
                        selected++;
                    } else {
                        selected = 0;
                    }
                } else {
                    if (selected < ENTRIES_PER_PAGE_HALF_MIN_ONE) {
                        selected++;
                    } else if (start + ENTRIES_PER_PAGE < fileCnt) {
                        start++;
                    } else if (start + selected + 1 < fileCnt) {
                        selected++;
                    } else {
                        start = 0;
                        selected = 0;
                    }
                }
            } else if (buttons & WPAD_BUTTON_LEFT && fileCnt > ENTRIES_PER_PAGE) {
                if (start + selected <= ENTRIES_PER_PAGE_HALF_MIN_ONE) {
                    start = 0;
                    selected = 0;
                } else if (selected >= ENTRIES_PER_PAGE_HALF_MIN_ONE) {
                    selected -= ENTRIES_PER_PAGE_HALF_MIN_ONE;
                } else if (start <= ENTRIES_PER_PAGE) {
                    start = 0;
                    selected = ENTRIES_PER_PAGE - 1;
                } else {
                    start -= ENTRIES_PER_PAGE - 1;
                    selected = ENTRIES_PER_PAGE - 1;
                }
            } else if (buttons & WPAD_BUTTON_RIGHT && fileCnt
                    > ENTRIES_PER_PAGE) {
                if (start + selected + ENTRIES_PER_PAGE_HALF_MIN_ONE >= fileCnt) {
                    start = fileCnt - ENTRIES_PER_PAGE;
                    selected = ENTRIES_PER_PAGE - 1;
                } else if (selected + ENTRIES_PER_PAGE_HALF_MIN_ONE
                        < ENTRIES_PER_PAGE) {
                    selected += ENTRIES_PER_PAGE_HALF_MIN_ONE;
                } else if (start + selected + ENTRIES_PER_PAGE >= fileCnt) {
                    start = fileCnt - ENTRIES_PER_PAGE;
                    selected = ENTRIES_PER_PAGE - 1;
                } else {
                    start += ENTRIES_PER_PAGE - 1;
                    selected = 0;
                }
            } else if (buttons & WPAD_BUTTON_HOME) {
                ReturnToLoader();
            } else if (buttons & WPAD_BUTTON_PLUS) {
                BROWSERENTRY *file = &browserList[start + selected];
                if ((!file->isdir) & (file->install == 0)) {
                    file->install = 1;
                    installCnt += 1;
                } else if ((!file->isdir) & (file->install == 1)) {
                    file->install = 0;
                    installCnt -= 1;
                } else if ((!file->isdir) & (file->install == 2)) {
                    file->install = 1;
                    installCnt += 1;
                    uninstallCnt -= 1;
                } else if (file->isdir) {
                    batchFolder = 1;
                }
            } else if (buttons & WPAD_BUTTON_MINUS) {
                BROWSERENTRY *file = &browserList[start + selected];
                if ((!file->isdir) && (file->install == 0)) {
                    file->install = 2;
                    uninstallCnt += 1;
                } else if ((!file->isdir) && (file->install == 1)) {
                    file->install = 2;
                    uninstallCnt += 1;
                    installCnt -= 1;
                } else if ((!file->isdir) && (file->install == 2)) {
                    file->install = 0;
                    uninstallCnt -= 1;
                } else if (file->isdir) {
                    batchFolder = 2;
                }
            } else if (buttons & WPAD_BUTTON_1) {
                BROWSERENTRY *file;
                for (counter = 0; counter < fileCnt; counter++) {
                    file = &browserList[counter];
                    if ((file->isdir) == false) {
                        if (file->install == 2) {
                            uninstallCnt -= 1;
                        }
                        if (file->install != 1) {
                            installCnt += 1;
                        }
                        file->install = 1;
                        installall = 1;
                    }
                }
            } else if (buttons & WPAD_BUTTON_B) {
                if (gDirLevel <= 1) {
                    return -1;
                }
                char *tmpCurPath;
                selected = 0;
                start = 0;
                // Previous dir
                tmpCurPath = PopCurrentDir(&selected, &start);
                if (tmpCurPath != NULL) snprintf(rootdir, sizeof(rootdir), "%s", tmpCurPath);
                fileCnt = ParseDirectory(0);
                installCnt = 0;
                uninstallCnt = 0;
                continue;
            } else if (buttons & WPAD_BUTTON_2) {
                BROWSERENTRY *tmpFile = &browserList[start + selected];
                if (!tmpFile->isdir) {
                    tmpCurPath = PeekCurrentDir();
                    if (tmpCurPath != NULL) {
                        int res = Menu_FileOperations(tmpFile, tmpCurPath);
                        if (res != 0) {
                            fileCnt = ParseDirectory(0);
                            continue;
                        }
                    }
                }
            }
            if (installall || batchFolder || buttons & WPAD_BUTTON_A) {
                BROWSERENTRY *tmpFile = &browserList[start + selected];
                char *tmpCurPath;
                if (tmpFile->isdir && !installall) {// wiiNinja
                    if (strcmp(tmpFile->filename, "..") == 0 && !batchFolder) {
                        selected = 0;
                        start = 0;
                        // Previous dir
                        tmpCurPath = PopCurrentDir(&selected, &start);
                        if (tmpCurPath != NULL) snprintf(rootdir, sizeof(rootdir), "%s", tmpCurPath);
                        installCnt = 0;
                        uninstallCnt = 0;
                        fileCnt = ParseDirectory(0);
                        continue;
                    } else if (IsListFull() == true) {
                        ClearScreen();
                        printf("Maximum number of directory levels is reached.");
                        PromptAnyKeyContinue();
                    } else {
                        tmpCurPath = PeekCurrentDir();
                        if (tmpCurPath != NULL) {
                            if (gDirLevel > 1) {
                                snprintf(rootdir, sizeof(rootdir), "%s/%s", tmpCurPath, tmpFile->filename);
                            } else {
                                snprintf(rootdir, sizeof(rootdir), "%s%s", tmpCurPath, tmpFile->filename);
                            }
                        }
                        // wiiNinja: Need to PopCurrentDir
                        PushCurrentDir(rootdir, selected, start);
                        selected = 0;
                        start = 0;

                        installCnt = 0;
                        uninstallCnt = 0;
                        fileCnt = ParseDirectory(0);
                        continue;
                    }
                } else { //is not dir
                    //If at least one WAD is marked, then batch install - Leathl
                    installall = 0;
                    if ((installCnt > 0) | (uninstallCnt > 0)) {
                        char *thisCurPath = PeekCurrentDir();
                        if (thisCurPath != NULL) {
                            int
                                    res =
                                            Menu_BatchProcessWads(browserList, fileCnt, thisCurPath, installCnt, uninstallCnt);

                            if (res == 1) {
                                int counter;
                                for (counter = 0; counter < fileCnt; counter++) {
                                    BROWSERENTRY *temp = &browserList[counter];
                                    temp->install = 0;
                                }
                                installCnt = 0;
                                uninstallCnt = 0;
                            }
                        }
                    }
                    //else use standard wadmanage menu - Leathl
                    else {
                        tmpCurPath = PeekCurrentDir();
                        if (tmpCurPath != NULL) Menu_WadManage(tmpFile, tmpCurPath);
                    }
                }
            }
        }
        exit = 1;
    }

    PromptAnyKeyContinue();
    return -1;
}

void Menu_WadManage(BROWSERENTRY *file, char *inFilePath) {
    FILE *fp = NULL;
    f32 filesize;

    u32 mode = 0;
    s32 ret;

    /* File size in megabytes */
    filesize = (file->length / MB_SIZE);

    for (;;) {
        /* Clear console */
        initNextBuffer();
        PrintBanner();

        printf("\n\n\n\t\t\t\t[+] WAD Filename : %s\n", file->filename);
        printf("\t\t\t\t    WAD Filesize : %.2f MB\n\n\n", filesize);
        
        printf("\t\t\t\t[+] Select action: < %s WAD >\n\n", (!mode) ? "Install"
            : "Uninstall");

        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s)\t\t\t\tChange Selection\n", LEFT_ARROW, RIGHT_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        flushBuffer();

        u32 buttons = WaitButtons();

        /* LEFT/RIGHT buttons */
        if (buttons & (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT)) mode ^= 1;

        /* A button */
        if (buttons & WPAD_BUTTON_A) break;

        /* B button */
        if (buttons & WPAD_BUTTON_B) return;
    }

    PrintBanner();

    printf("[+] Opening \"%s\", please wait...", file->filename);
    fflush(stdout);

    /* Generate filepath */
    snprintf(gTmpFilePath, sizeof(gTmpFilePath), "%s/%s", inFilePath, file->filename); // wiiNinja

    /* Open WAD */
    fp = fopen(gTmpFilePath, "rb");
    int error = 0;
    if (!fp) {
        printf(" ERROR!\n");
        error = 1;
    } else {
        printf(" OK!\n\n");
    }

    if (!error) {
        printf("[+] %s WAD, please wait...\n", (!mode) ? "Installing"
            : "Uninstalling");

        /* Do install/uninstall */
        WiiLightControl(WII_LIGHT_ON);
        if (!mode) {
            ret = Wad_Install(fp);
        } else {
            ret = Wad_Uninstall(fp);
        }
        if (ret >= 0) {
            hasinstalled = 1;
        }
        WiiLightControl(WII_LIGHT_OFF);
    }

    /* Close file */
    if (fp) fclose(fp);

    PromptAnyKeyContinue();
}

// Start of wiiNinja's added routines

int PushCurrentDir(char *dirStr, s32 Selected, s32 Start) {
    int retval = 0;

    // Store dirStr into the list and increment the gDirLevel
    // WARNING: Make sure dirStr is no larger than MAX_FILE_PATH_LEN
    if (gDirLevel < MAX_DIR_LEVELS) {
        strcpy(gDirList[gDirLevel], dirStr);
        gSeleted[gDirLevel] = Selected;
        gStart[gDirLevel] = Start;
        gDirLevel++;
    } else {
        retval = -1;
    }

    return (retval);
}

char *PopCurrentDir(s32 *Selected, s32 *Start) {
    if (gDirLevel > 1) {
        gDirLevel--;
    } else {
        gDirLevel = 0;
    }

    *Selected = gSeleted[gDirLevel];
    *Start = gStart[gDirLevel];
    return PeekCurrentDir();
}

bool IsListFull(void) {
    if (gDirLevel < MAX_DIR_LEVELS) {
        return (false);
    }
    return (true);
}

char *PeekCurrentDir(void) {
    // Return the current path
    if (gDirLevel > 0) {
        return (gDirList[gDirLevel - 1]);
    }
    return (NULL);
}

/****************************************************************************
 * FileSortCallback
 *
 * Quick sort callback to sort file entries with the following order:
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/
int FileSortCallback(const void *f1, const void *f2) {
    BROWSERENTRY * entry1 = (BROWSERENTRY *) f1;
    BROWSERENTRY * entry2 = (BROWSERENTRY *) f2;

    /* Special case for implicit directories */
    if (strcmp(entry1->filename, "..") == 0) {
        return -1;
    }
    if (strcmp(entry2->filename, "..") == 0) {
        return 1;
    }
    /* If one is a file and one is a directory the directory is first. */
    if (entry1->isdir && !entry2->isdir) {
        return -1;
    }
    if (!entry1->isdir && entry2->isdir) {
        return 1;
    }
    return stricmp(entry1->filename, entry2->filename);
}

int ParseDirectory(int filterElfDol) {
    DIR *dir = NULL;
    char fulldir[MAXPATHLEN];
    char fullpath[MAXPATHLEN];
    char filename[MAXPATHLEN];
    char filenamelow[MAXPATHLEN];
    int i;

    // reset browser
    ResetBrowser();
    // open the directory
    snprintf(fulldir, sizeof(fulldir), "%s", rootdir); // add currentDevice to path
    //printf("trying to open %s\n",fulldir);
    dir = opendir(fulldir);
    // if we can't open the dir, try opening the root dir
    if (dir == NULL) {
        char *tmpCurPath;
        tmpCurPath = PopCurrentDir(&i, &i);
        if (tmpCurPath != NULL) snprintf(rootdir, sizeof(rootdir), "%s", tmpCurPath);
        snprintf(fulldir, sizeof(fulldir), "%s", rootdir);
        dir = opendir(fulldir);
        if (dir == NULL) {
            dir = opendir(fulldir);
            if (dir == NULL) {
                printf("failed to diropen %s\n", fulldir);
                PromptAnyKeyContinue();
                return 0;
            }
        }
    }
    // index files/folders
    int entryNum = 0;
    struct dirent * dirFile = readdir(dir);
    struct stat filestat;
    while (dirFile != NULL) {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", fulldir, dirFile->d_name);
        if (stat(fullpath, &filestat) < 0) {
            dirFile = readdir(dir);
            continue;
        }
        bool isdir, iscurdir, isprevdir, iswad, iselfdol, filtercontrol = true;
        memset(filenamelow, 0, MAXPATHLEN * sizeof(char));
        snprintf(filename, sizeof(filename), "%s", dirFile->d_name);
        for (i = 0; filename[i]; i += 1) {
            filenamelow[i] = tolower((int) filename[i]);
        }
        isdir = (filestat.st_mode & _IFDIR) != 0;
        iscurdir = (strcmp(filename, ".") == 0);
        isprevdir = (strcmp(filename, "..") == 0);
        iswad = (strstr(filenamelow, ".wad") != NULL);
        iselfdol = ((strstr(filenamelow, ".elf") != NULL)
                || (strstr(filenamelow, ".dol") != NULL));
        if (theSettings.filterwads) {
            filtercontrol = iswad;
        }
        if (filterElfDol) {
            filtercontrol = iselfdol;
        }

        //Do not list mac os x metadata files
        if (strncmp(filenamelow, "._", 2) == 0) {
            filtercontrol = false;
        }

        if (!iscurdir && (isdir || isprevdir || filtercontrol)) {
            BROWSERENTRY * newBrowserList =
                    (BROWSERENTRY *) realloc(browserList, (entryNum + 1)
                            * sizeof(BROWSERENTRY));

            // failed to allocate required memory
            if (!newBrowserList) {
                ResetBrowser();
                entryNum = -1;
                break;
            } else {
                browserList = newBrowserList;
            }
            memset(&(browserList[entryNum]), 0, sizeof(BROWSERENTRY)); // clear the new entry
            strncpy(browserList[entryNum].filename, filename, MAXJOLIET);
            strncpy(browserList[entryNum].displayname, filename, MAXDISPLAY); // crop name for display
            browserList[entryNum].length = filestat.st_size;
            browserList[entryNum].isdir = isdir; // flag this as a dir
            entryNum++;
        }
        dirFile = readdir(dir);
    }
    // close directory
    closedir(dir);
    // Sort the file list
    qsort(browserList, entryNum, sizeof(BROWSERENTRY), FileSortCallback);
    return entryNum;
}

/****************************************************************************
 * ResetBrowser()
 * Clears the file browser memory, and allocates one initial entry
 ***************************************************************************/
void ResetBrowser() {
    // Clear any existing values
    if (browserList != NULL) {
        free(browserList);
        browserList = NULL;
    }
    // set aside space for 1 entry
    browserList = (BROWSERENTRY *) malloc(sizeof(BROWSERENTRY));
    memset(browserList, 0, sizeof(BROWSERENTRY));
}

int wadmanager() {
    int ret;
    hasinstalled = 0;
    installedIOSorSM = 0;
    initNextBuffer();
    PrintBanner();
    ret = get_certs();
    if (ret < 0) {
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
        printf("\nFake Sign Patch may be required, check your IOSs for a valid IOS ret = %d\n", ret);
        PromptAnyKeyContinue();
    }
    WIILIGHT_Init();
    /* FAT device menu */
    ret = Menu_FatDevice();
    if (ret >= 0) {
        /* WAD list menu */
        ret = Menu_WadList();
        if (browserList) {
            free(browserList);
            browserList = NULL;
        }
        Fat_Unmount(fdev);
        CloseSMB();
    }
    if (hasinstalled) {
        updateTitles();
        updateSysMenuVersion();
        theSettings.neverSet = 0;
        if (installedIOSorSM) {
            checkIOSConditionally();
            WPAD_Init();
        }
    }
    return hasinstalled;
}
