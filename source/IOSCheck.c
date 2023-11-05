#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include "AnyRegionChanger.h"
#include "controller.h"
#include "detect_settings.h"
#include "dopios.h"
#include "Error.h"
#include "fat.h"
#include "gecko.h"
#include "IOSCheck.h"
#include "sha1.h"
#include "Settings.h"
#include "tools.h"
#include "ticket_dat.h"
#include "tmd_bin.h"
#include "video.h"
#include "wad.h"

#define BOOTMII_VERSION     254
#define CSV_VERSION         3
#define WIIMOD_CSV          "sd:/wad/wiimod/wiimod.csv"
#define ES_ERROR_1028 -1028 // No ticket installed
#define ES_ERROR_1029 -1029 // Installed Ticket/TMD is invalid
#define ES_ERROR_1035 -1035 // Title with a higher version is already installed 
#define ES_ERROR_2011 -2011 // Signature check failed (Needs Fakesign)
s32 __u8Cmp(const void *a, const void *b);

void quickViewCSVOutput();

static u8 certs[0xA00] ATTRIBUTE_ALIGN(32);
static const char certs_fs[] ATTRIBUTE_ALIGN(32) = "/sys/cert.sys";
static const char flash_fs[] ATTRIBUTE_ALIGN(32) = "/dev/flash";
static const char usb2_fs[] ATTRIBUTE_ALIGN(32) = "/dev/usb2";
static const char boot2_fs[] ATTRIBUTE_ALIGN(32) = "/dev/boot2";

int get_certs(void) {
    int fd, ret;
    fd = IOS_Open(certs_fs, 1);
    ret = IOS_Read(fd, certs, sizeof(certs));
    if (ret < sizeof(certs)) {
        ret = -1;
    } else {
        IOS_Close(fd);
    }
    return ret;
}

int check_fakesig(void) {
    int ret = 0;
    ret
            = ES_AddTitleStart((signed_blob*) tmd_bin, tmd_bin_size, (signed_blob *) certs, sizeof(certs), NULL, 0);
    if (ret >= 0) ES_AddTitleCancel();
    // we don't care if IOS bitches that our version is to low because we
    // don't even want to install this title
    if (ret == ES_ERROR_1035) ret = 0;
    if (ret == ES_ERROR_1028) ret = 0;
    return ret;
}

void zeroSignature(signed_blob *sig) {
    u8 *psig = (u8*) sig;
    memset(psig + 4, 0, SIGNATURE_SIZE(sig) - 4);
}

int bruteTMD(tmd *ptmd) {
    u16 fill;
    for (fill = 0; fill < 65535; fill++) {
        ptmd->fill3 = fill;
        sha1 hash;
        SHA1((u8 *) ptmd, TMD_SIZE(ptmd), hash);
        if (hash[0] == 0) return 0;
    }
    return -1;
}

void forgeTMD(signed_blob *signedTmd) {
    zeroSignature(signedTmd);
    bruteTMD((tmd*) SIGNATURE_PAYLOAD(signedTmd));
}

int bruteTicket(tik *ticket) {
    u16 fill;
    for (fill = 0; fill < 65535; fill++) {
        ticket->padding = fill;
        sha1 hash;
        SHA1((u8*) ticket, sizeof(ticket), hash);
        if (hash[0] == 0) return 0;
    }
    return -1;
}

void forgeTicket(signed_blob *signedTicket) {
    zeroSignature(signedTicket);
    bruteTicket((tik*) SIGNATURE_PAYLOAD(signedTicket));
}

int check_identify(void) {
    int ret = 0;
    ret
            = ES_Identify((signed_blob *) certs, sizeof(certs), (signed_blob *) tmd_bin, tmd_bin_size, (signed_blob *) ticket_dat, ticket_dat_size, NULL);

    // we don't care about invalid signatures here.
    // patched versions return -1017 before checking them.
    if (ret == ES_ERROR_2011) ret = 0;
    if (ret == ES_ERROR_1029) ret = 0;
    return ret;
}

int check_flash(void) {
    int ret;
    ret = IOS_Open(flash_fs, 0);
    if (ret >= 0) IOS_Close(ret);
    return ret;
}

int check_usb2(void) {
    int ret = IOS_Open(usb2_fs, 0);
    if (ret >= 0) IOS_Close(ret);
    return ret;
}

int check_boot2(void) {
    int ret;
    ret = IOS_Open(boot2_fs, 0);
    if (ret >= 0) IOS_Close(ret);
    return ret;
}

int check_nandp(void) {
    int ret;
    ret = IOS_Open("/title/00000001/00000002/content/title.tmd", 1);
    if (ret >= 0) IOS_Close(ret);
    return ret;
}

bool isIOSstub(u8 ios_number) {
    u32 tmd_size = 0;
    tmd *ios_tmd ATTRIBUTE_ALIGN(32);

    ES_GetStoredTMDSize(0x0000000100000000ULL | ios_number, &tmd_size);
    if (!tmd_size) {
        //getting size failed. invalid or fake tmd for sure!
        gprintf("failed to get tmd for ios %d\n",ios_number);
        return true;
    }
    signed_blob *ios_tmd_buf = (signed_blob *) AllocateMemory(tmd_size);
    memset(ios_tmd_buf, 0, tmd_size);

    ES_GetStoredTMD(0x0000000100000000ULL | ios_number, ios_tmd_buf, tmd_size);
    ios_tmd = (tmd *) SIGNATURE_PAYLOAD(ios_tmd_buf);
    free(ios_tmd_buf);
    gprintf("IOS %d is rev %d(0x%x) with tmd size off %u and %u contents\n", ios_number, ios_tmd->title_version, ios_tmd->title_version,
            tmd_size, ios_tmd->num_contents);
    /*Stubs have a few things in common¨:
     - title version : it is mostly 65280 , or even better : in hex the last 2 digits are 0.
     example : IOS 60 rev 6400 = 0x1900 = 00 = stub
     - exception for IOS21 which is active, the tmd size is 592 bytes
     - the stub ios' have 1 app of their own (type 0x1) and 2 shared apps (type 0x8001).

     eventho the 00 check seems to work fine , we'll only use other knowledge as well cause some
     people/applications install an ios with a stub rev >_> ...*/
    u8 Version = ios_tmd->title_version;
#ifdef DEBUG
    gprintf("Version = 0x%x\n", Version);
#endif
    //version now contains the last 2 bytes. as said above, if this is 00, its a stub
    if (Version == 0) {
        if ((ios_tmd->num_contents == 3) && (ios_tmd->contents[0].type == 1
                && ios_tmd->contents[1].type == 0x8001
                && ios_tmd->contents[2].type == 0x8001)) {
            gprintf("IOS %d is a stub\n", ios_number);
            return true;
        } else {
            gprintf("IOS %d is active\n", ios_number);
            return false;
        }
    }gprintf("IOS %d is active\n", ios_number);
    return false;
}

u16 GetTitleVersion(u64 tid) {
    static u32 tmd_size ATTRIBUTE_ALIGN(32);

    s32 r = ES_GetStoredTMDSize(tid, &tmd_size);
#ifdef DEBUG
    printf("ES_GetStoredTMDSize(%llX, %08X):%d\n", tid, (u32) (&tmd_size), r);
#endif
    if (r < 0) return 0;

    signed_blob *TMD = (signed_blob *) AllocateMemory(tmd_size);
    memset(TMD, 0, tmd_size);
    
    r = ES_GetStoredTMD(tid, TMD, tmd_size);
#ifdef DEBUG
    printf("ES_GetStoredTMD(%llX, %08X, %d):%d\n", tid, (u32) (TMD), tmd_size, r);
#endif
    if (r < 0) {
        free(TMD);
        return 0;
    }

    tmd *rTMD = (tmd *) (TMD + (0x140 / sizeof(tmd *)));
    u16 version = rTMD->title_version;
    free(TMD);
    return version;
}

u16 check_sysmenuver(void) {
    return GetTitleVersion(0x0000000100000002LL);
}

int checkIOSMenu() {
    static int menuItem = 0;
    u32 button = 0;
    int maxItem;
    bool displayCurIOS = false, skipChecking = false, dumpHashes = false, keepAHBPROTOverride=false;

    if (flagOverrideIOSCheck) {
        keepAHBPROTOverride = true;
        flagOverrideIOSCheck = false;
    }

    while (!(button & WPAD_BUTTON_B)) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n\n\t\t\t\t%3s Check IOSs\n", (menuItem == 0) ? "-->" : " ");
        printf("\t\t\t\t%3s View last output\n", (menuItem == 1) ? "-->" : " ");
        printf("\t\t\t\t%3s Skip checking IOSs: %d\n", (menuItem == 2) ? "-->"
            : " ", (skipChecking) ? 1 : 0);
        printf("\t\t\t\t%3s Display current IOS: %d\n", (menuItem == 3) ? "-->"
            : " ", (displayCurIOS) ? 1 : 0);
        printf("\t\t\t\t%3s Output IOS csv: %d\n", (menuItem == 4) ? "-->"
            : " ", theSettings.outputios);
        printf("\t\t\t\t%3s Dump Hashes: %d\n", (menuItem == 5) ? "-->" : " ", (dumpHashes)
            ? 1 : 0);
        maxItem = 5;
        if( wiiSettings.ahbprot ) {
            printf("\t\t\t\t%3s Check IOSs<28: %d\n", (menuItem == 6) ? "-->" : " ", (keepAHBPROTOverride)
                        ? 1 : 0);
            maxItem++;
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
                PrintBanner();
                checkmyios(displayCurIOS, skipChecking, dumpHashes, keepAHBPROTOverride);
                WPAD_Init();
                PromptAnyKeyContinue();
            } else if (menuItem == 1) {
                quickViewCSVOutput();
            }
        }
        if ((button & WPAD_BUTTON_LEFT) || (button & WPAD_BUTTON_RIGHT)) {
            if (menuItem == 2) {
                skipChecking = !skipChecking;
                if (skipChecking) {
                    displayCurIOS = false;
                }
            } else if (menuItem == 3) {
                if (!skipChecking) {
                    displayCurIOS = !displayCurIOS;
                }
            } else if (menuItem == 4) {
                theSettings.outputios = !theSettings.outputios;
                if (!theSettings.outputios) {
                    dumpHashes = false;
                }
            } else if (menuItem == 5) {
                if (theSettings.outputios) {
                    dumpHashes = !dumpHashes;
                }
            } else if (menuItem == 6) {
                keepAHBPROTOverride = !keepAHBPROTOverride;
            }
        }
        if (button & WPAD_BUTTON_DOWN) menuItem++;
        if (button & WPAD_BUTTON_UP) menuItem--;
        
        if (menuItem > maxItem) menuItem = 0;
        if (menuItem < 0) menuItem = maxItem;
    }
    return 0;
}

s32 __u8Cmp(const void *a, const void *b) {
    return *(u8 *) a - *(u8 *) b;
}

u8 *get_ioslist(u32 *cnt) {
    u32 i, icnt;
    u8 *ioses = NULL;
    
    ioses = (u8 *) malloc(sizeof(u8) * wiiSettings.iosCount);
    icnt = 0;
    for (i = 3; i < 256; i++) {
        if (ios_found[i] > 0) {
            ioses[icnt++] = i;
        }
    }

    *cnt = wiiSettings.iosCount;
    return ioses;
}

int writebackLog(bool dumpHashes) {
    int ret;
    u8 *list;
    u32 len;
    
    ret = Fat_Mount(&fdevList[FAT_SD]);
    if (ret > 0) {
        list = get_ioslist(&len);
        if (!create_folders(WIIMOD_CSV)) {
            printf("Error creating folder(s) for '%s'\n", WIIMOD_CSV);
            ret = -1;
        } else {
            FILE *logFile = fopen(WIIMOD_CSV, "wb");
            if (logFile != NULL) {
                fprintf(logFile, "%d,%d\n", CSV_VERSION, (dumpHashes) ? 1 : 0);
                fprintf(logFile, "IOS,version, ,Flash, FakeSign, ES_Ident, Usb2.0, Boot2, NAND, sysmenuver");
                if (dumpHashes) {
                    fprintf(logFile, ",hash 1,hash 2,hash 3,hash 4,hash 5\n");
                } else {
                    fprintf(logFile, "\n");
                }
                fprintf(logFile, "%u\n", len);
                int i, tmp;
                for (i = len - 1; i >= 0; i -= 1) {
                    tmp = list[i];
                    fprintf(logFile, "%d,%u, ,%d,%d,%d,%d,%d,%d,%u", tmp, ios_found[tmp], ios_flash[tmp], ios_fsign[tmp], ios_ident[tmp], ios_usb2m[tmp], ios_boot2[tmp], ios_nandp[tmp], ios_sysmen[tmp]);
                    if (dumpHashes) {
                        fprintf(logFile, ",%x,%x,%x,%x,%x\n", ios_hash[tmp][0], ios_hash[tmp][1], ios_hash[tmp][2], ios_hash[tmp][3], ios_hash[tmp][4]);
                    } else {
                        fprintf(logFile, "\n");
                    }
                    printf(".");
                }
                if (checkRegionSystemMenuMismatch(0)) {
                    fprintf(logFile, "Error: System Menu Region Error!!\n");
                }
                if (checkMissingSystemMenuIOS(0)) {
                    fprintf(logFile, "Error: Missing System Menu IOS!!\n");
                }
                if (checkSystemMenuIOSisNotStub(0)) {
                    fprintf(logFile, "Error: System Menu IOS is a stub!!\n");
                }
                if (wiiSettings.missingIOSwarning) {
                    fprintf(logFile, "Warning: Missing required IOS!\n");
                }
                if (wiiSettings.SMRegionMismatchWarning) {
                    fprintf(logFile, "Warning: System Menu region mismatch!\n");
                }
                fprintf(logFile, "\n");
                if (wiiSettings.bcVersion > 0) {
                    fprintf(logFile, "BC v%u\n", wiiSettings.bcVersion);
                }
                if (wiiSettings.miosVersion > 0) {
                    fprintf(logFile, "MIOS v%u\n", wiiSettings.miosVersion);
                }
                fprintf(logFile, "Region %s\n", regions_names[wiiSettings.game]);
                fprintf(logFile, "Hollywood v0x%x Console ID: %d\n", wiiSettings.hollywoodVersion, wiiSettings.deviceId);
                fprintf(logFile, "Boot2 v%u\n", wiiSettings.boot2version);
                fprintf(logFile, "System menu %u\n", wiiSettings.sysMenuVer);
                fprintf(logFile, "System menu v%.1lf%c on IOS%d\n", wiiSettings.sysMenuNinVersion, wiiSettings.sysMenuRegion, wiiSettings.sysMenuIOS);
                fprintf(logFile, "Found %u titles of which %u are IOSs.\n", wiiSettings.titleCnt, wiiSettings.iosCount);
                fprintf(logFile, "Wii Mod v%.1lf\n", WII_MOD_VERSION);
                fclose(logFile);
            }
            ret = 0;
            printf("\n\nWrote %s\n", WIIMOD_CSV);
        }
        Fat_Unmount(&fdevList[FAT_SD]);
        if(list) free(list);
    }
    return ret;
}

int ClearPermDataIOS(u32 loc) {
    ios_flash[loc] = -1;
    ios_boot2[loc] = -1;
    ios_usb2m[loc] = -1;
    ios_nandp[loc] = -1;
    ios_fsign[loc] = -1;
    ios_sysmen[loc] = 0;
    ios_ident[loc] = -1;
    ios_found[loc] = 0;
    return 1;
}

void checkIOSConditionally() {
    if (!theSettings.onlyCheckIOSOnDemand) {
        checkmyios(false, false, false, false);
    }
}

void checkPatches(int i) {
    ios_flash[i] = check_flash();
    ios_boot2[i] = check_boot2();
    ios_usb2m[i] = check_usb2();
    ios_nandp[i] = check_nandp();
    ios_fsign[i] = check_fakesig();
    ios_sysmen[i] = check_sysmenuver();
    ios_ident[i] = check_identify();

}

int checkmyios(bool displayIOS, bool skipcheckingIOSs, bool dumpHashes, bool keepAHBPROTOverride) {
    int i, ret, tmp;
    u8 *list;
    u32 len, version;
    list = get_ioslist(&len);
    if (len == 0) {
        if(list) {
            free(list);
        }
        return 0;
    }

    version = IOS_GetVersion();

    if (!skipcheckingIOSs) {

        if (wiiSettings.ahbprot && !keepAHBPROTOverride) {
            printf("\nHave AHBPROT, skipping IOSs < 28s\n");
            for (i = 28; i < 256; i += 1) {
                ios_ident[i] = -1;
                ios_fsign[i] = -1;
                ios_flash[i] = -1;
                ios_boot2[i] = -1;
                ios_usb2m[i] = -1;
                ios_nandp[i] = -1;
                ios_sysmen[i] = 0;
            }
        } else {
            memset(ios_ident, -1, 256 * sizeof(int));
            memset(ios_fsign, -1, 256 * sizeof(int));
            memset(ios_flash, -1, 256 * sizeof(int));
            memset(ios_boot2, -1, 256 * sizeof(int));
            memset(ios_usb2m, -1, 256 * sizeof(int));
            memset(ios_nandp, -1, 256 * sizeof(int));
            memset(ios_sysmen, 0, 256 * sizeof(16));
        }

        get_certs();
        printf("\nChecking IOSs.");
    }
    SpinnerStart();
    for (i = 0; !skipcheckingIOSs && i < len; i += 1) {
        tmp = list[i];
        if (skipios[tmp]) {
            continue;
        }
        if (wiiSettings.ahbprot && tmp < 28 && !keepAHBPROTOverride) {
            continue;
        }
        if (ios_found[tmp]) {
            if (ios_isStub[tmp] > 1) {
                printf("\b\nStub %d\n", tmp);
                fflush(stdout);
                ret = -1;
            } else {
                if (displayIOS) {
                    printf("\b %d,", tmp);
                    fflush(stdout);
                    sleep(1);
                }
                ret = ReloadIosNoInitNoPatch(tmp);
            }
            if (ret >= 0) {
                checkPatches(tmp);
            }
        }
        if (i % 2 == 0) {
            printf("\b..");
            fflush(stdout);
        }
    }
    free(list);
    ret = ReloadIosNoInit(version);
    SpinnerStop();
    printf("\b.");
    if (theSettings.outputios) {
        writebackLog(dumpHashes);
    }
    return 1;
}

void quickViewCSVOutput() {
    int ret;
    char *buffer = NULL;
    ret = Fat_Mount(&fdevList[FAT_SD]);

    if (ret <= 0) {
        printf("Failed load sd card\n");
        PromptAnyKeyContinue();
    } else {
        FILE * logFile = fopen(WIIMOD_CSV, "rb");
        if (logFile != NULL) {
            fseek(logFile, 0, SEEK_END);
            unsigned long fileLen = ftell(logFile);
            fseek(logFile, 0, SEEK_SET);
            //Allocate memory
            buffer = (char *) malloc(fileLen + 1);
            if (!buffer) {
                fprintf(stderr, "Memory error!");
                fclose(logFile);
                Fat_Unmount(&fdevList[FAT_SD]);
                return;
            }
            //Read file contents into buffer
            fread(buffer, fileLen, 1, logFile);
            fclose(logFile);
        } else {
            printf("Failed to open file\n");
            Fat_Unmount(&fdevList[FAT_SD]);
            PromptAnyKeyContinue();
            return;
        }
        Fat_Unmount(&fdevList[FAT_SD]);

        int lineNum = 0, page = 0;
        char *line;
        initNextBuffer();
        PrintBanner();
        line = strtok(buffer, "\n");
        while (line != NULL) {
            if (strlen(line) > 1 || page == 0) {
                printf("\t%s\n", line);
            }
            lineNum += 1;
            if (lineNum > 20) {
                page += 1;
                printf("\nPress A to continue");
                flushBuffer();
                initNextBuffer();
                PrintBanner();
                lineNum = 0;
                WaitKey(WPAD_BUTTON_A);
            }
            line = strtok(NULL, "\n");
        }
        flushBuffer();
        PromptAnyKeyContinue();
        if (buffer) {
            free(buffer);
        }
    }
}

int loadiosfromcsv() {
    int tmp2 = 0, ret = -1;
    char in[80];
    u32 len;
    u16 tmp;

    memset(ios_ident, -1, 256 * sizeof(int));
    memset(ios_fsign, -1, 256 * sizeof(int));
    memset(ios_flash, -1, 256 * sizeof(int));
    memset(ios_boot2, -1, 256 * sizeof(int));
    memset(ios_usb2m, -1, 256 * sizeof(int));
    memset(ios_nandp, -1, 256 * sizeof(int));
    memset(ios_sysmen, 0, 256 * sizeof(u16));

    ret = Fat_Mount(&fdevList[FAT_SD]);

    if (ret > 0) {
        FILE * logFile = fopen(WIIMOD_CSV, "rb");
        if (logFile != NULL) {
            //printf("Loading from csv...\n");
            int i, ver = 0, hashes = 0;
            fscanf(logFile, "%d,%d\n", &ver, &hashes);
            if (ver != CSV_VERSION) {
                //printf("load failed\n");
                ret = -1;
            }
            if (ret > 0) {
                len = (hashes) ? 13 : 8;
                for (i = 0; i < len; i += 1) {
                    fscanf(logFile, "%s", in);
                }
                fscanf(logFile, "%u", &len);
                for (i = len; i > 0; i -= 1) {
                    fscanf(logFile, "%d,", &tmp2);
                    if (tmp2 > 2 && tmp2 < 256) {
                        fscanf(logFile, "%hu, ,%d,%d,%d,%d,%d,%d,%hu", &ios_found[tmp2], &ios_flash[tmp2], &ios_fsign[tmp2], &ios_ident[tmp2], &ios_usb2m[tmp2], &ios_boot2[tmp2], &ios_nandp[tmp2], &ios_sysmen[tmp2]);
                        if (hashes) {
                            fscanf(logFile, ",%x,%x,%x,%x,%x\n", &ver, &ver, &ver, &ver, &ver);
                        } else {
                            fscanf(logFile, "\n");
                        }
                    } else {
                        fscanf(logFile, "%hu, ,%d,%d,%d,%d,%d,%d,%hu", &tmp, &ver, &ver, &ver, &ver, &ver, &ver, &tmp);
                        if (hashes) {
                            fscanf(logFile, ",%x,%x,%x,%x,%x\n", &ver, &ver, &ver, &ver, &ver);
                        } else {
                            fscanf(logFile, "\n");
                        }
                    }
                    //printf(".");
                }
                ret = 0;
            }
            fclose(logFile);
        }
        Fat_Unmount(&fdevList[FAT_SD]);
    }
    return ret;
}
