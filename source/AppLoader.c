#include <gccore.h>
#include <malloc.h>
#include <ogc/machine/processor.h>
#include <ogc/lwp_threads.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include "AppLoader.h"
#include "controller.h"
#include "loaddol/loaddol.h"
#include "fat.h"
#include "smb.h"
#include "tools.h"
#include "video.h"
#include "wadmanager.h"

#define RERUN 1
#define ERROR -1

int getFileList(void) {
    static s32 selected = 0, start = 0;
    BROWSERENTRY *file;
    BROWSERENTRY *tmpFile;
    char *tmpCurPath;
    f32 filesize;
    s32 index, len, spaces, x;
    u32 cnt, fileCnt = 0;
    
    if (browserList) {
        free(browserList);
        browserList = NULL;
    }
    fileCnt = ParseDirectory(1);
    /* No files */
    if (fileCnt <= 0 || fileCnt == -1) {
        selected = 0;
        start = 0;
        tmpCurPath = PopCurrentDir(&selected, &start);
        if (tmpCurPath != NULL) {
            snprintf(rootdir, sizeof(rootdir), "%s", tmpCurPath);
            return RERUN;
        }
        printf(" No files found!\n");
        return ERROR;
    }

    for (;;) {
        /* Clear console */
        initNextBuffer();
        PrintBanner();
        fflush(stdout);
        
        /** Print entries **/
        cnt = strlen(rootdir);
        if (cnt > MAXDISPLAY) {
            index = cnt - MAXDISPLAY;
        } else {
            index = 0;
        }

        printf("[+] %s:", rootdir + index);
        printf("\n\n");
        /* Print entries */
        for (cnt = start; cnt < fileCnt; cnt++) {
            file = &browserList[cnt];
            filesize = file->length / MB_SIZE;
            /* Entries per page limit */
            if ((cnt - start) >= ENTRIES_PER_PAGE) break;

            len = strlen(file->displayname);
            if (file->isdir) {
                len += 2;
            }
            spaces = MAXDISPLAY - len;
            
            if (file->isdir) { // wiiNinja
                printf("   %2s [%s]", (cnt == (start + selected)) ? ">>" : "  ", file->displayname);
            } else {
                printf("   %2s", (cnt == (start + selected)) ? ">>" : "  ");
                printf(" %s", file->displayname);
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
        printf("\t(A)\tLoad\t\t\t\t\t\t\t\t\t");
        if (gDirLevel > 1) {
            printf("(B)\tGo up directory");
        } else {
            printf("(B)\tReturn to main menu");
        }
        flushBuffer();
        /** Controls **/
        u32 buttons = WaitButtons();
        
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
        } else if (buttons & WPAD_BUTTON_RIGHT && fileCnt > ENTRIES_PER_PAGE) {
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
        } else if (buttons & WPAD_BUTTON_A) {
            tmpFile = &browserList[start + selected];
            if (tmpFile->isdir) {// wiiNinja
                if (strcmp(tmpFile->filename, "..") == 0) {
                    selected = 0;
                    start = 0;
                    // Previous dir
                    tmpCurPath = PopCurrentDir(&selected, &start);
                    if (tmpCurPath != NULL) snprintf(rootdir, sizeof(rootdir), "%s", tmpCurPath);
                    return RERUN;
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
                    return RERUN;
                }
            } else {
                tmpCurPath = PeekCurrentDir();
                if (tmpCurPath != NULL) {
                    PrintBanner();
                    char filename[80];
                    snprintf(filename, sizeof(filename), "%s/%s", tmpCurPath, tmpFile->filename);
                    printf("\n%s\n", filename);
                    loadelfdol(filename);
                    break;
                }
            }
        } else if (buttons & WPAD_BUTTON_B) {
            if (gDirLevel <= 1) {
                break;
            }
            selected = 0;
            start = 0;
            // Previous dir
            tmpCurPath = PopCurrentDir(&selected, &start);
            if (tmpCurPath != NULL) snprintf(rootdir, sizeof(rootdir), "%s", tmpCurPath);
            
            if (browserList) {
                free(browserList);
                browserList = NULL;
            }
            return RERUN;
        }
    }
    if (browserList) {
        free(browserList);
        browserList = NULL;
    }
    return 0;
}

int Menu_FileList(void) {
    s32 ret;
    
    printf("[+] Retrieving file list...");
    fflush(stdout);
    
    if (!strcmp(fdev->mount, "smb")) {
        printf("\nConnecting smb...");
        ret = ConnectSMB();
        printf("%s\n", (ret) ? "Worked" : "Failed");
        if (ret <= 0) return -1;
    }

    gDirLevel = 0;
    
    snprintf(rootdir, sizeof(rootdir), "%s:" DIRECTORY, fdev->mount);
    PushCurrentDir(rootdir, 0, 0);
    ret = RERUN;
    while (ret == RERUN) {
        ret = getFileList();
        if (ret == ERROR) {
            if (!strcmp(fdev->mount, "smb")) {
                CloseSMB();
            } else {
                Fat_Unmount(fdev);
            }
            if (browserList) {
                free(browserList);
                browserList = NULL;
            }
            PromptAnyKeyContinue();
            return -1;
        }
    }
    if (!strcmp(fdev->mount, "smb")) {
        CloseSMB();
    } else {
        Fat_Unmount(fdev);
    }
    return 0;
}

int appLoader(void) {
    int ret = 0;
    ret = Menu_FatDevice();
    if (ret < 0) {
        return -1;
    }
    ret = Menu_FileList();
    return ret;
}
