/*-------------------------------------------------------------

 detect_settings.c -- detects various system settings

 Copyright (C) 2008 tona
 Unless other credit specified

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
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <unistd.h>

#include "AnyRegionChanger.h"
#include "ChannelMenu.h"
#include "detect_settings.h"
#include "gecko.h"
#include "id.h"
#include "IOSMenu.h"
#include "name.h"
#include "nand.h"
#include "Settings.h"
#include "sha1.h"
#include "sysconf.h"
#include "title.h"
#include "title_install.h"
#include "tools.h"

SYSSETTINGS wiiSettings;

int check_sysmenuver(void);
u32 GetSysMenuVersion(void);
char getSystemMenuRegionFromRev(u16 sysVersion);
double getSystemMenuVersionFromRev(u16 sysVersion);
u32 GetBoot2Version(void);
u32 GetDeviceID(void);
int titleCmp(const void *a, const void *b);
s32 checkLanguageAreaMismatch(s32 cc);
void copyNamesTitlesByTypes(u32 type);
void scanIOSUsedByTypes(u32 type);

const u32 types[7] =
        { 1, 0x10000, 0x10001, 0x10002, 0x10004, 0x10005, 0x10008 };
u32 ios_hash[256][5];
s32 ios_clean[256];
bool skipios[256];

u32 hashesSkipIOS[100][5];
u32 skipHashesCnt = 0;
int wasInstalled = 0;

#define CNT_KNOWN_STUBS 21
u32 nullHash[5] = { 0x0, 0x0, 0x0, 0x0, 0x0 };
u32 hashesKnownStubs[CNT_KNOWN_STUBS][5] = {
    { 0x9e3ecc3f, 0x40e86648, 0xf387b9a8, 0xd24e9bed, 0xded4c651 }, //3, 65280
    { 0xe465142b, 0x57955f7d, 0xa52617b9, 0xf79a0067, 0xd8c943b6 }, //4, 65280
    { 0x59c468b0, 0x73874de3, 0x25f58286, 0x5e067bbe, 0xc82de307 }, //10, 768
    { 0x6eeef316, 0xfb936458, 0xed05d886, 0xfe76155c, 0xfa5791cf }, //11, 256
    { 0x3f73c30a, 0xd85ab73b, 0xa4b84b85, 0x63534fc2, 0x88c2be5d }, //16, 512
    { 0x589af879, 0x5fbadeef, 0x7afb1057, 0xe4f7e968, 0x3d3d730d }, //20, 256
    { 0x5fd69cdf, 0x8197c405, 0xa02a8419, 0xae1aaf0f, 0xd9c09190 }, //30, 2816
    { 0x12edf6e1, 0x7f49d4f9, 0xd2df5259, 0x52c79545, 0x57a028e4 }, //40, 3072
    { 0xb21c7e5a, 0x16be62e3, 0xb629f0cb, 0x10821951, 0x7eb63057 }, //50, 5120
    { 0x3c068202, 0x2006c1b6, 0x40a0c82f, 0x5bd0436b, 0x9d343e9f }, //51, 4864
    { 0xd72a8362, 0x7a40c485, 0x4e27d812, 0x720d49f1, 0xef2b4a4f }, //52, 5888
    { 0xdb83137b, 0x436b532c, 0x50f147a6, 0xc404dea4, 0x14e87233 }, //60, 6400
    { 0x7a09d761, 0x8527768c, 0xc321e460, 0x7f2fc642, 0x28efe171 }, //70, 6912
    { 0x5faf3d89, 0xb2b8ad01, 0x6c629c52, 0x64518df6, 0xc3567d73 }, //222, 65280
    { 0xbcdf9dfd, 0x12a41ae4, 0x1eb74613, 0x69b47652, 0xa6d27966 }, //223, 65280
    { 0xf76e5650, 0xa42b8208, 0xc93f6dd8, 0x22774dc4, 0xa2fe698f }, //249, 65280
    { 0xeb2d3b80, 0x3ca1a09f, 0xdf862b7d, 0xe6768430, 0xde715643 }, //250, 65280
    { 0x7b61c1f7, 0xd1c4e3fd, 0xd0dfdcb4, 0xfc633e29, 0x6b9ed121 }, //254, 2
    { 0x2cb06231, 0x9692a096, 0xc6f0db6a, 0xd9636535, 0x75c0131b }, //254, 3
    { 0xc28d6fde, 0x3371c55d, 0x28714fcf, 0xf97aaaff, 0xf9cc78d0 }, //254, 260
    { 0x2c4626ff, 0xce09dd32, 0x64244d7, 0x95e1ac32, 0x72a7ef1e } }; //254, 65280

        /* be* functions taken from Segher's wii.git */
u32 be32(const u8 *p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

u64 be64(const u8 *p) {
    return ((u64) be32(p) << 32) | be32(p + 4);
}


u64 get_required_ios_tid (u32 tidh, u32 tidl) {
    int i, j;
    u64 tid = TITLE_ID(tidh, tidl);
    for (i = 0; i < 7; i += 1) {
        if (types[i] == tidh) break;
    }
    if( i < 7 ) {
        for( j = 0; j < typeCnt[i]; j++ ) {
            if(installed_titles[i][j].tid == tid) {
                return installed_titles[i][j].requiredIOStid;
            }
        }
    }
    return 0;
}

int ctoi(char c) {
    switch(c){
        case '1': return 1;
        case '2': return 2;
        case '3': return 3;
        case '4': return 4;
        case '5': return 5;
        case '6': return 6;
        case '7': return 7;
        case '8': return 8;
        case '9': return 9;
        default:
        case '0': return 0;
    }
}

char sanitizeRegion( char region ) {
    switch (region){
        case 'U':
        case 'E':
        case 'J':
        case 'K':
            return region;
            break;
        default:
            return 'X';
            break;
    }
}

/* Get Sysmenu Region identifies the region of the system menu (not your Wii)
  by looking into it's resource content file for region information. */
char getSystemMenuRegionFromContentES(void)
{
    s32 ret, cfd;
    static u8 fbuffer[0x950] ATTRIBUTE_ALIGN(32);
    static tikview viewdata[0x10] ATTRIBUTE_ALIGN(32);
    u32 views;
    static u64 tid ATTRIBUTE_ALIGN(32) = TITLE_ID(1,2);
    u8 region = 'X', match[] = "C:\\Revolution", match2[] = "Final";

    ret = Identify_SUSilent(true);
    ret = ES_GetNumTicketViews(tid, &views);
    if (ret < 0) {
        printf(" Error! ES_GetNumTickets (ret = %d)\n", ret);
        PromptAnyKeyContinue();
        return 'X';
    }
    if (!views) {
        printf(" No tickets found!\n");
        PromptAnyKeyContinue();
        return 'X';
    } else if (views > 16) {
        printf(" Too many ticket views! (views = %d)\n", views);
        PromptAnyKeyContinue();
        return 'X';
    }

    ret = ES_GetTicketViews(tid, viewdata, 1);
    if (ret < 0) {
        printf("Error! ES_OpenTitleContent (ret = %d)\n", ret);
        PromptAnyKeyContinue();
        return 'X';
    }
    ret = ES_OpenTitleContent(tid, viewdata, 1);
    if (ret < 0) {
        //printf("Error! ES_OpenTitleContent (ret = %d)\n", ret);
        //PromptAnyKeyContinue();
        return 'X';
    }

    cfd = ret;
    region = 0;
    int i, j;
    while (!region){
        ret = ES_ReadContent(cfd,fbuffer,0x950);
        if (ret < 0) {
            printf("Error! ES_ReadContent (ret = %d)\n", ret);
            PromptAnyKeyContinue();
            return 'X';
        }
        for(i=0;i<0x950;i++) {
            if (!memcmp(&fbuffer[i], match, 13)){
                for(j=0; j<30; j++){
                    if( memcmp((fbuffer+i+j+24), match2, 5) == 0) {
                        region = fbuffer[i+j+30];
                        break;
                    }
                }
                if(wiiSettings.sysMenuNinVersion == 0.0) {
                    int first = ctoi(fbuffer[i+24]);
                    int second = ctoi(fbuffer[i+26]);
                    wiiSettings.sysMenuNinVersion = first + (0.1 * second);
                }
                break;
            }
        }

    }
    ret = ES_CloseContent(cfd);
    if (ret < 0)
    {
        printf("Error! ES_CloseContent (ret = %d)\n", ret);
        PromptAnyKeyContinue();
        return 'X';
    }
    return sanitizeRegion(region);
}

s32 getRegion() {
    switch (wiiSettings.sysMenuRegion) {
        case 'U':
            return 0;
        case 'E':
            return 1;
        case 'J':
            return 2;
        case 'K':
            return 3;
    }
    return 0;
}

s32 checkIOSisStub(s32 ios) {
    u32 i, loc;
    s32 ret = 0;
    if (ios > 2 && ios < 255 && GrandIOSListLookup[ios] >= 0) {
        loc = GrandIOSListLookup[ios];
        for (i = 0; i < GrandIOSList[loc].cnt; i += 1) {
            if (GrandIOSList[loc].revs[i].num == ios_found[ios]) {
                if (GrandIOSList[loc].revs[i].mode == IOS_Stub
                        || GrandIOSList[loc].revs[i].mode
                                == IOS_NoNusstub) {
                    ret += 1;
                }
                break;
            }
        }
        //  hashesKnownStubs
        for (i = 0; i < CNT_KNOWN_STUBS; i += 1) {
            if (memcmp((u32 *) hashesKnownStubs[i], (u32 *) &ios_hash[ios],
                sizeof(sha1)) == 0) {
                ret += 2;
                break;
            }
        }
    }
    return ret;
}

s32 __FileCmp(const void *a, const void *b)
{
        dirent_t *hdr1 = (dirent_t *)a;
        dirent_t *hdr2 = (dirent_t *)b;

        if (hdr1->type == hdr2->type)
        {
                return strcmp(hdr1->name, hdr2->name);
        } else
        {
                return 0;
        }
}

s32 getdir(char *path, dirent_t **ent, u32 *cnt)
{
    s32 res;
    u32 num = 0;

    int i, j, k;

    res = ISFS_ReadDir(path, NULL, &num);
    if(res != ISFS_OK)
    {
        printf("Error: could not get dir entry count! (result: %d)\n", res);
        return -1;
    }

    char *nbuf = (char *)AllocateMemory((ISFS_MAXPATH + 1) * num);
    char ebuf[ISFS_MAXPATH + 1];

    if(nbuf == NULL)
    {
        printf("Error: could not allocate buffer for name list!\n");
        return -2;
    }

    res = ISFS_ReadDir(path, nbuf, &num);
    if(res != ISFS_OK)
    {
        printf("Error: could not get name list! (result: %d)\n", res);
        return -3;
    }

    *cnt = num;

    *ent = AllocateMemory(sizeof(dirent_t) * num);

    for(i = 0, k = 0; i < num; i++)
    {
        for(j = 0; nbuf[k] != 0; j++, k++)
            ebuf[j] = nbuf[k];
        ebuf[j] = 0;
        k++;

        strcpy((*ent)[i].name, ebuf);
    }

    qsort(*ent, *cnt, sizeof(dirent_t), __FileCmp);

    free(nbuf);
    return 0;
}

char getRegionFromSomewhere() {
    int region = CONF_GetRegion();

    if( CONF_REGION_JP == region ) {
        return 'J';
    } else if ( CONF_REGION_EU == region ) {
        return 'E';
    } else if ( CONF_REGION_US == region ) {
        return 'U';
    } else if ( CONF_REGION_KR == region ) {
        return 'K';
    }
    return 'X';
}

char getSystemMenuRegionFromContent() {
    s32 cfd;
    s32 ret;
    u32 num;
    dirent_t *list;
    char contentpath[ISFS_MAXPATH];
    char path[ISFS_MAXPATH];
    int i, j;
    u32 cnt = 0;
    u8 *buffer;
    u8 match[] = "C:\\Revolution", match2[] = "Final";
    char region = 'X';
    Nand_Init();
    sprintf(contentpath, "/title/%08x/%08x/content", 1, 2);

    ret = getdir(contentpath, &list, &num);
    if (ret < 0) {
        printf("Reading folder of the title failed\n");
        return region;
    }

    fstats filestats;
    for(cnt=0; region == 'X' && cnt < num; cnt++) {
        sprintf(path, "/title/%08x/%08x/content/%s", 1, 2, list[cnt].name);

        cfd = ISFS_Open(path, ISFS_OPEN_READ);
        if (cfd < 0) {
            printf("ISFS_OPEN for %s failed %d\n", path, cfd);
            continue;
        }

        ret = ISFS_GetFileStats(cfd, &filestats);
        if (ret < 0) {
            printf("Error! ISFS_GetFileStats (ret = %d)\n", ret);
            continue;
        }
        buffer=(u8*)AllocateMemory(filestats.file_length);

        ret = ISFS_Read(cfd, buffer, filestats.file_length);
        if (ret < 0) {
            printf("ISFS_Read for %s failed %d\n", path, ret);
            ISFS_Close(cfd);
            continue;
        }
        ISFS_Close(cfd);
        for( i = 0; i < filestats.file_length - 49; i += 1 ) {
            if(memcmp((buffer+i), match, 13) == 0) {
                for(j=0; j<30; j++){
                    //printf("%c",buffer[i+j+24]);
                    if( memcmp((buffer+i+j+24), match2, 5) == 0) {
                        //printf("\n%c\n",buffer[i+j+30]);
                        region = buffer[i+j+30];
                        break;
                    }
                }
                if(wiiSettings.sysMenuNinVersion == 0.0) {
                    int first = ctoi(buffer[i+24]);
                    int second = ctoi(buffer[i+26]);
                    wiiSettings.sysMenuNinVersion = first + (0.1 * second);
                }
                break;
            }
        }
        free(buffer);
    }
    free(list);
    ISFS_Deinitialize();
    return sanitizeRegion(region);
}

void setSysMenuRegion() {
    char regionFromVersion = getSystemMenuRegionFromRev( wiiSettings.sysMenuVer);
    char regionFromContentES = getSystemMenuRegionFromContentES();
    char regionFromContent = 'X';
    if( 'X' == regionFromContentES ) {
        regionFromContent = getSystemMenuRegionFromContent();
        if( 'X' == regionFromContent ) {
            wiiSettings.failSMContentRead = true;
        }
    }
    if( ( regionFromContentES != regionFromVersion ) &&
            ( 'X' != regionFromContentES ) && ( 'X' != regionFromVersion ) ) {
        wiiSettings.SMRegionMismatchWarning = true;
    }
    if( ( regionFromContent != regionFromVersion ) &&
            ( 'X' != regionFromContent ) && ( 'X' != regionFromVersion ) ) {
        wiiSettings.SMRegionMismatchWarning = true;
    }
    if(  'X' != regionFromContent ) {
        wiiSettings.sysMenuRegion = regionFromContent;
    } else if( 'X' != regionFromContentES ) {
        wiiSettings.sysMenuRegion = regionFromContentES;
    } else {
        wiiSettings.sysMenuRegion = regionFromVersion;
    }
}

void updateSysMenuVersion() {
    wiiSettings.SMRegionMismatchWarning = false;
    wiiSettings.failSMContentRead = false;
    if (wiiSettings.sysMenuVer > 0) {
        wiiSettings.sysMenuNinVersion = getSystemMenuVersionFromRev(wiiSettings.sysMenuVer);
        setSysMenuRegion();
    } else {
        wiiSettings.sysMenuNinVersion = 0.0;
        wiiSettings.sysMenuRegion = 'X';
    }
}

double getSystemMenuVersionFromRev(u16 sysVersion) {
    if (sysVersion == 33) {
        return 1.0;
    } else if (sysVersion == 97 || sysVersion == 130 || sysVersion == 128) {
        return 2.0;
    } else if (sysVersion == 162) {
        return 2.1;
    } else if (sysVersion == 193 || sysVersion == 194 || sysVersion == 192) {
        return 2.2;
    } else if (sysVersion == 225 || sysVersion == 226 || sysVersion == 224) {
        return 3.0;
    } else if (sysVersion == 257 || sysVersion == 258 || sysVersion == 256) {
        return 3.1;
    } else if (sysVersion == 289 || sysVersion == 290 || sysVersion == 288) {
        return 3.2;
    } else if (sysVersion == 353 || sysVersion == 354 || sysVersion == 352) {
        return 3.3;
    } else if (sysVersion == 326) {
        return 3.3;
    } else if (sysVersion == 385 || sysVersion == 386 || sysVersion == 384) {
        return 3.4;
    } else if (sysVersion == 390) {
        return 3.5;
    } else if (sysVersion == 417 || sysVersion == 418 || sysVersion == 416) {
        return 4.0;
    } else if (sysVersion == 449 || sysVersion == 450 || sysVersion == 448
            || sysVersion == 454) {
        return 4.1;
    } else if (sysVersion == 481 || sysVersion == 482 || sysVersion == 480
            || sysVersion == 486) {
        return 4.2;
    } else if (sysVersion == 514 || sysVersion == 513 || sysVersion == 518
            || sysVersion == 512) {
        return 4.3;
    }
    return 0.0;
}

char getSystemMenuRegionFromRev(u16 sysVersion) {
    u16 regionJAP[] = { 128, 192, 224, 256, 288, 352, 384, 416, 448, 480, 512 };
    u16 regionUSA[] = { 97, 192, 225, 257, 289, 353, 385, 417, 449, 481, 513 };
    u16 regionPAL[] = { 130, 162, 194, 226, 258, 290, 354, 386, 418, 450, 482,
        514 };
    u16 regionKOR[] = { 326, 390, 454, 486, 518 };
    int lenJAP = sizeof(regionJAP) / sizeof(u16);
    int lenUSA = sizeof(regionUSA) / sizeof(u16);
    int lenPAL = sizeof(regionPAL) / sizeof(u16);
    int lenKOR = sizeof(regionKOR) / sizeof(u16);
    int cmp1 = (lenJAP > lenUSA) ? lenJAP : lenUSA;
    int cmp2 = (lenPAL > lenKOR) ? lenPAL : lenKOR;
    int i, len = (cmp1 > cmp2) ? cmp1 : cmp2;
    for (i = 0; i < len; i += 1) {
        if (i < lenJAP && regionJAP[i] == sysVersion) return 'J';
        if (i < lenUSA && regionUSA[i] == sysVersion) return 'U';
        if (i < lenPAL && regionPAL[i] == sysVersion) return 'E';
        if (i < lenKOR && regionKOR[i] == sysVersion) return 'K';
    }
    return 'X';
}

s32 getIOSVerForSystemMenu(s32 sysVersion) {
    if (sysVersion == 97 || sysVersion == 128 || sysVersion == 130
            || sysVersion == 192) {
        return 11;//2.0, 2.1E
    } else if (sysVersion == 192 || sysVersion == 193 || sysVersion == 194) {
        return 20;//2.2
    } else if (sysVersion == 224 || sysVersion == 225 || sysVersion == 226
            || sysVersion == 256 || sysVersion == 257 || sysVersion == 258
            || sysVersion == 289 || sysVersion == 290 || sysVersion == 288
            || sysVersion == 353 || sysVersion == 354 || sysVersion == 352) {
        return 30;//3.0, 3.1, 3.2, 3.3U, 3.3E, 3.3J
    } else if (sysVersion == 326) {
        return 40;//3.3K
    } else if (sysVersion == 385 || sysVersion == 386 || sysVersion == 384) {
        return 50;//3.4
    } else if (sysVersion == 390) {
        return 52;//3.5K
    } else if (sysVersion == 417 || sysVersion == 418 || sysVersion == 416
            || sysVersion == 449 || sysVersion == 450 || sysVersion == 448
            || sysVersion == 454) {
        return 60;//4.0, 4.1
    } else if (sysVersion == 481 || sysVersion == 482 || sysVersion == 480
            || sysVersion == 486) {
        return 70;//4.2
    } else if (sysVersion == 514 || sysVersion == 513 || sysVersion == 518
            || sysVersion == 512) {
        return 80;//4.3
    }
    return 30;
}

int detectSettings() {
    u32 i, j;
    installed_titles = NULL;
    wiiSettings.hollywoodVersion = *(u32 *) 0x80003138;
    wiiSettings.deviceId = GetDeviceID();
    wiiSettings.boot2version = GetBoot2Version();
    wiiSettings.ahbprot = HAVE_AHBPROT;
    wiiSettings.regionChangedKoreanWii = false;
    memset(typeCnt, 0, 7 * sizeof(int));
    memset(ios_used, 0, 256 * sizeof(u32));

    memset(GrandIOSListLookup, -1, 256 * sizeof(s32));
    LesserIOSLEN = 0;

    for( i = 1; i < GrandIOSLEN; i += 1  ) {
        if(GrandIOSList[i].onnus > 0) {
            LesserIOSLEN += 1;
        }
        GrandIOSListLookup[GrandIOSList[i].ver] = i;
    }

    for( i = skipHashesCnt; i < 100; i += 1 ) {
        for( j = 0; j < 5; j += 1 ) {
            hashesSkipIOS[i][j] = 0x0;
        }
    }

    updateTitles();
    int isInstalled = -1;
    if( theSettings.AutoLoadIOS > 2 && theSettings.AutoLoadIOS < 255 ) {
        isInstalled = (ios_found[theSettings.AutoLoadIOS] <= 0)?0:1;
    }
    if( 1 == isInstalled ) {
        ReloadIos(theSettings.AutoLoadIOS);
    }
    updateSysMenuVersion();
    updateRegionSettings();
    return 0;
}

// Get the boot2 version
u32 GetBoot2Version(void) {
    u32 boot2version = 0;
    if (ES_GetBoot2Version(&boot2version) < 0) boot2version = 0;
    return boot2version;
}

u32 GetDeviceID(void) {
    u32 deviceId = 0;
    if (ES_GetDeviceID(&deviceId) < 0) deviceId = 0;
    return deviceId;
}

char getRegionFromSerialCode() {
    char ret = 'X';

    s32 serialret = SYSCONF_GetSerialCode();
    if( SYSCONF_CODE_JPN == serialret || SYSCONF_CODE_JAPK == serialret ) {
        ret = 'J';
    } else if ( SYSCONF_CODE_USA == serialret || SYSCONF_CODE_USAK == serialret ) {
        ret = 'U';
    } else if ( SYSCONF_CODE_KOR == serialret || SYSCONF_CODE_KORK == serialret ) {
        ret = 'K';
    } else if ( SYSCONF_CODE_EURH == serialret || SYSCONF_CODE_EURM == serialret ||
            SYSCONF_CODE_EURF == serialret || SYSCONF_CODE_EURHK == serialret ||
            SYSCONF_CODE_EURMK == serialret || SYSCONF_CODE_EURFK == serialret ||
            SYSCONF_CODE_AUS == serialret || SYSCONF_CODE_AUSK == serialret ) {
        ret = 'E';
    }
    return ret;
}

void updateRegionSettings() {
    s32 ret = SYSCONF_Init();
    if (ret < 0) {
        printf("Unexpected Error: %s Value: %d\n", "SYSCONF_Init", ret);
        PromptAnyKeyContinue();
        return;
    }

    wiiSettings.lang = SYSCONF_GetLanguage();
    wiiSettings.area = SYSCONF_GetArea();
    wiiSettings.game = SYSCONF_GetRegion();
    wiiSettings.video = SYSCONF_GetVideo();
    wiiSettings.eula = SYSCONF_GetEULA();
    wiiSettings.regionFromSerial = getRegionFromSerialCode();
    if (wiiSettings.lang < 0 || wiiSettings.area < 0 || wiiSettings.game < 0
            || wiiSettings.video < 0 || (wiiSettings.eula != SYSCONF_ENOENT
            && wiiSettings.eula < 0)) {
        printf("Error getting settings! %d, %d, %d, %d, %d\n",
            wiiSettings.lang, wiiSettings.area, wiiSettings.game,
            wiiSettings.video, wiiSettings.eula);
        return;
    }
    if (SYSCONF_GetLength("IPL.SADR") != SADR_LENGTH) {
        printf("Unexpected Error: %s Value: %d\n", "IPL.SADR Length Incorrect",
            SYSCONF_GetLength("IPL.SADR"));
        return;
    }
    u8 sadr[SADR_LENGTH];
    ret = SYSCONF_Get("IPL.SADR", sadr, SADR_LENGTH);
    if (ret < 0) {
        printf("Unexpected Error: %s Value: %d\n", "SYSCONF_Get IPL.SADR", ret);
        return;
    }
    wiiSettings.country = sadr[0];

    if( 'K' == wiiSettings.regionFromSerial &&
            ('K' != AREAtoSysMenuRegion(wiiSettings.area) ||
                    'K' !=  wiiSettings.sysMenuRegion)) {
        wiiSettings.regionChangedKoreanWii = true;
    }

}

int checkMissingSystemMenuIOS(int cc) {
    int ret = cc;
    if (wiiSettings.sysMenuIOS < 3 || wiiSettings.sysMenuIOS > 255
            || ios_found[wiiSettings.sysMenuIOS] <= 0) {
        ret++;
    }
    return ret;
}

int checkSystemMenuIOSisNotStub(int cc) {
    int ret = cc;
    if (wiiSettings.sysMenuIOSisStub) {
        ret++;
    }
    return ret;
}

int checkErrors() {
    int ret = checkRegionSystemMenuMismatch(0);
    ret = checkMissingSystemMenuIOS(ret);
    ret = checkSystemMenuIOSisNotStub(ret);
    return ret;
}

char AREAtoSysMenuRegion(int area) {
    // Data based on my own tests with AREA/Sysmenu
    switch (area) {
        case 0:
        case 5:
        case 6:
            return 'J';
        case 1:
        case 4:
            // case 7:
        case 8:
        case 9:
        case 10:
        case 11:
            return 'U';
        case 2:
        case 3:
            return 'E';
        case 7:
            return 'K';
        default:
            return 0;
    }
}
s32 checkRegionSystemMenuMismatch(s32 cc) {
    s32 ret = cc;
    if (wiiSettings.sysMenuRegion != AREAtoSysMenuRegion(wiiSettings.area)) {
        ret += 1;
    }
    return ret;
}

void CheckIfHBC(TITLE *curTitle) {
    u32 tidh = TITLE_UPPER(curTitle->tid);
    u32 tidl = TITLE_LOWER(curTitle->tid);

    if( tidh == 0x00010001 && tidl == 0x48415858 ) {
        // 1 HBC HAXX
        theSettings.haveHBC |= 1;
        theSettings.hbcRequiredTID[0] = curTitle->requiredIOStid;
    } else if( tidh == 0x00010001 && tidl == 0x4a4f4449 ) {
        // 2 HBC JODI
        theSettings.haveHBC |= 2;
        theSettings.hbcRequiredTID[1] = curTitle->requiredIOStid;
    } else if( tidh == 0x00010001 && tidl == 0xAF1BF516 ) {
        // 4 HBC v1.0.7+
        theSettings.haveHBC |= 4;
        theSettings.hbcRequiredTID[2] = curTitle->requiredIOStid;
    } else if( tidh == 0x00010001 && tidl == 0x4d415549 ) {
        // 8 HBC MAUI (Backup HBC)
        theSettings.haveHBC |= 8;
        theSettings.hbcRequiredTID[3] = curTitle->requiredIOStid;
    }
}

void CheckIfChannel(TITLE *curTitle) {
    u64 maskedtid = (curTitle->tid & 0xFFFFFFFFFFFFFF00);
    s8 i = -1;
    if( 0x0001000248414200ULL == maskedtid ) {
        i = SHOP_CHANNEL;
    } else if( 0x0001000248414100ULL == maskedtid ) {
        i = PHOTO_CHANNEL_1_0;
    } else if( 0x0001000248415900ULL == maskedtid ) {
        i = PHOTO_CHANNEL_1_1;
    } else if( 0x0001000248414300ULL == maskedtid ) {
        i = MII_CHANNEL;
    } else if( 0x0001000148414400ULL == maskedtid ) {
        i = INTERNET_CHANNEL;
    } else if( 0x0001000248414700ULL == maskedtid ) {
        i = NEWS_CHANNEL;
    } else if( 0x0001000248414600ULL == maskedtid ) {
        i = WEATHER_CHANNEL;
    } else if( 0x0001000148434600ULL == maskedtid ) {
        i = WII_SPEAK_CHANNEL;
    } else if( 0x0001000848414B00ULL == maskedtid ) {
        i = EULA_TITLE;
    } else if( 0x0000000100000100ULL == curTitle->tid ) {
        i = BC_TITLE;
    } else if( 0x0000000100000101ULL == curTitle->tid ) {
        i = MIOS_TITLE;
    } else if( 0x0001000848414C00ULL == maskedtid ) {
        i = REGION_SELECT_TITLE;
    } else if( 0x000100014843444AULL == curTitle->tid ) {
        i = DIGICAM_PRINT_CHANNEL;
    } else if( 0x000100084843434AULL == curTitle->tid ) {
        i = JAPAN_FOOD_CHANNEL;
    } else if( 0x000100014843424AULL == curTitle->tid ) {
        i = SHASHIN_CHANNEL;
    } else if( 0x0001000148424E4AULL == curTitle->tid ) {
        i = TV_FRIEND_CHANNEL;
    } else if( 0x000100014843494AULL == curTitle->tid ) {
        i = WII_NO_MA_CHANNEL;
    }
    if( i >=0 ) {
        InstalledChannelVersion[i] = curTitle->version;
    }
}

void updateTitles() {
    u64 *list = NULL;
    u32 count, i, j = 0;
    s32 ret;
    ret = Title_GetList(&list, &count);
    if (ret < 0) {
        printf("Failed to get titles.\n");
        sleep(5);
        return;
    }
    wiiSettings.titleCnt = count;
    u64 tid;
    u32 tidl, tidh, tmd_size;
    wiiSettings.bcVersion = 0;
    wiiSettings.miosVersion = 0;
    wiiSettings.iosCount = 0;
    theSettings.haveHBC = 0;
    memset(theSettings.hbcRequiredTID, 0, 4 * sizeof(u64));

    if (installed_titles != NULL) {
        for (i = 0; i < 7; i += 1) {
            if (installed_titles[i] != NULL) {
                free(installed_titles[i]);
                installed_titles[i] = NULL;
            }
        }
        free(installed_titles);
        installed_titles = NULL;
    }

    for (i = 3; i < 256; i += 1) {
        if (namesTitleUsingIOS[i] != NULL) {
            for (j = 0; j < ios_used[i]; j += 1) {
                if (namesTitleUsingIOS[i][j]) {
                    free(namesTitleUsingIOS[i][j]);
                }
            }
            free(namesTitleUsingIOS[i]);
        }
    }

    memset(typeCnt, 0, 7 * sizeof(int));
    memset(InstalledChannelVersion, -1, CHANNELS_LEN * sizeof(int));
    for (i = 0; i < count; i += 1) {
        tidh = TITLE_UPPER(list[i]);
        for (j = 0; j < 7; j += 1) {
            if (types[j] == tidh) break;
        }
        if (j < 7) {
            typeCnt[j] += 1;
        }
    }
    installed_titles = (TITLE **) malloc(sizeof(TITLE*) * 7);
    for (i = 0; i < 7; i += 1) {
        installed_titles[i] = (TITLE *) malloc(sizeof(TITLE) * typeCnt[i]);
    }
    TITLE *curTitle = NULL;
    s32 r;
    int typeIndex;
    int typeCnt2[7];
    memset(typeCnt2, 0, 7 * sizeof(int));
    memset(ios_found, 0, 256 * sizeof(u16));
    memset(ios_used, 0, 256 * sizeof(u32));
    memset(ios_used2, 0, 256 * sizeof(u32));
    memset(GrandIOSListLookup2, -1, 256 * sizeof(s32));
    memset(ios_isStub, 0, 256 * sizeof(s32));
    memset(ios_clean, 0, 256 * sizeof(s32));

    ret = Identify_SUSilent(true);
    if (ret < 0) {
        wiiSettings.reRunWithSU = true;
    } else {
        wiiSettings.reRunWithSU = false;
    }

    ret = Nand_Init();
    if (ret < 0) {
        printf("Error:: Failed to get access to nand\n");
    }

    for (i = 0; i < count; i += 1) {
        tid = list[i];
        tidh = TITLE_UPPER(tid);
        tidl = TITLE_LOWER(tid);
        for (j = 0; j < 7; j += 1) {
            if (types[j] == tidh) {
                break;
            }
        }
        if (j > 6) {
            continue;
        }
        typeIndex = typeCnt2[j]++;
        if (typeIndex >= typeCnt[j] || typeIndex < 0) {
            printf("j %d typei %d max %d\n", j, typeIndex, typeCnt[j]);
            printf("tidh %u tidl %u\n", tidh, tidl);
            PromptAnyKeyContinue();
            continue;
        }
        curTitle = &installed_titles[j][typeIndex];
        curTitle->tid = tid;
        curTitle->num_contents = 0;

        memset(curTitle->nameDB, 0, 256 * sizeof(char));
        sprintf(curTitle->nameDB, "Failed to read");
        memset(curTitle->nameBN, 0, 256 * sizeof(char));
        sprintf(curTitle->nameBN, "Failed to read");
        memset(curTitle->name00, 0, 256 * sizeof(char));
        sprintf(curTitle->name00, "Failed to read");

        curTitle->failedToReadDB = true;
        curTitle->failedToReadBN = true;
        curTitle->failedToRead00 = true;

        ret = ES_GetStoredTMDSize(tid, &tmd_size);
        if (ret < 0) {
            gprintf("Error:: Failed to get TMD size. tid = %llu\n", tid);
            curTitle->failedToGetTMDSize = true;
            continue;
        }
        curTitle->failedToGetTMDSize = false;

        signed_blob *s_tmd = (signed_blob *) AllocateMemory(tmd_size);
        memset(s_tmd, 0, tmd_size);

        ret = ES_GetStoredTMD(tid, s_tmd, tmd_size);
        if (ret < 0) {
            printf("Error:: Failed to get stored TMD. tid = %llu\n",tid);
            continue;
        }
        tmd *TMD = SIGNATURE_PAYLOAD(s_tmd);
        if (TMD == NULL) continue;
        curTitle->type = TMD->title_type;
        curTitle->requiredIOS = TITLE_LOWER(TMD->sys_version);
        curTitle->requiredIOStid = TMD->sys_version;
        curTitle->version = TMD->title_version;
        curTitle->num_contents = TMD->num_contents;

        CheckIfChannel(curTitle);
        CheckIfHBC(curTitle);

        if (tidh == 1 && tidl == 0x100) {
            wiiSettings.bcVersion = curTitle->version;
        } else if (tidh == 1 && tidl == 0x101) {
            wiiSettings.miosVersion = curTitle->version;
        } else if (tidh == 1 && tidl == 2 ) {
            wiiSettings.sysMenuVer = TMD->title_version;
            wiiSettings.sysMenuIOS = curTitle->requiredIOS;
        }
        if (tidh == 1 && tidl > 2 && tidl < 256) {
            ios_found[tidl] = curTitle->version;
            u32 hash[5];
            SHA1((u8 *) s_tmd, tmd_size, (unsigned char *) hash);
            memcpy(ios_hash[tidl], hash, sizeof(sha1));
            ios_isStub[tidl] = checkIOSisStub(tidl);
            for( j = 0; j < skipHashesCnt; j += 1 ) {
                if (memcmp((u32 *) hashesSkipIOS[j], hash, sizeof(sha1)) == 0) {
                    skipios[tidl] = true;
                    break;
                }
            }
        }
        free(s_tmd);

        memset(curTitle->text, 0, 15 * sizeof(char));
        sprintf(curTitle->text, "%s", titleText(tidh, tidl));

        r = getNameDB(curTitle->nameDB, curTitle->text);
        if (r < 0) {
            memset(curTitle->nameDB, 0, 256 * sizeof(char));
            sprintf(curTitle->nameDB, "Failed to read");
        } else {
            curTitle->failedToReadDB = false;
        }
        r = getNameBN(curTitle->nameBN, tid, true);
        if (r < 0) {
            memset(curTitle->nameBN, 0, 256 * sizeof(char));
            sprintf(curTitle->nameBN, "Failed to read");
        } else {
            curTitle->failedToReadBN = false;
        }
        r = getName00(curTitle->name00, tid);
        if (r < 0) {
            memset(curTitle->name00, 0, 256 * sizeof(char));
            sprintf(curTitle->name00, "Failed to read");
        } else {
            curTitle->failedToRead00 = false;
        }
        if( curTitle->failedToReadDB && curTitle->failedToReadBN && curTitle->failedToRead00 &&
                curTitle->num_contents == 3 && tidh == 0x10001) {
            memset(curTitle->nameDB, 0, 256 * sizeof(char));
            sprintf(curTitle->nameDB, "Maybe Channel Moved to SD");
            curTitle->failedToReadDB = false;
        }
    }
    int isInstalled = -1;
    if( wasInstalled <=0 && theSettings.AutoLoadIOS > 2 && theSettings.AutoLoadIOS < 255 ) {
        isInstalled = (ios_found[theSettings.AutoLoadIOS] <= 0)?0:1;
    }
    if( 1 == isInstalled && theSettings.AutoLoadOnInstall > 0 ) {
        ReloadIos(theSettings.AutoLoadIOS);
    }
    wasInstalled = isInstalled;

    s32 loc;

    for (i = 3; i < 256; i += 1) {
        loc = GrandIOSListLookup[i];
        if (ios_found[i] > 0 ) {
            wiiSettings.iosCount++;
            for( j = 0; loc >=0 && j < GrandIOSList[loc].cnt; j += 1 ) {
                if( GrandIOSList[loc].revs[j].num == ios_found[i] ) {
                    GrandIOSListLookup2[i] = j;
                    if (memcmp((u32 *) GrandIOSList[loc].revs[j].hash, (u32 *) &ios_hash[i], sizeof(sha1)) == 0) {
                        ios_clean[i] = 1;
                    } else if (memcmp((u32 *) GrandIOSList[loc].revs[j].hash, (u32 *) nullHash, sizeof(sha1)) == 0) {
                        ios_clean[i] = 2;
                    }
                    break;
                }
            }
        }
    }

    if (wiiSettings.sysMenuIOS > 2 && wiiSettings.sysMenuIOS < 256) {
        wiiSettings.sysMenuIOSisStub = (ios_isStub[wiiSettings.sysMenuIOS]>0)?1:0;
    }

    wiiSettings.missingIOSwarning = false;
    scanIOSUsedByTypes(2);
    scanIOSUsedByTypes(3);
    scanIOSUsedByTypes(4);
    scanIOSUsedByTypes(6);

    for (i = 3; i < 256; i += 1) {
        if (ios_used[i] > 0) {
            namesTitleUsingIOS[i] = (char **) malloc(ios_used[i]
                    * sizeof(char *));
        }
    }

    copyNamesTitlesByTypes(2);
    copyNamesTitlesByTypes(3);
    copyNamesTitlesByTypes(4);
    copyNamesTitlesByTypes(6);

    ISFS_Deinitialize();
    for (i = 0; i < 7; i += 1) {
        qsort(installed_titles[i], typeCnt[i], sizeof(TITLE), titleCmp);
    }
}

void copyNamesTitlesByTypes(u32 type) {
    u32 i, iosVer = 0, strIndex;
    TITLE * curTitle;
    for (i = 0; i < typeCnt[type]; i += 1) {
        curTitle = &installed_titles[type][i];
        iosVer = curTitle->requiredIOS;
        if (iosVer > 2 && iosVer < 256) {
            strIndex = ios_used2[iosVer]++;
            if (strIndex < ios_used[iosVer]) {
                namesTitleUsingIOS[iosVer][strIndex] = (char *) malloc(256
                        * sizeof(char));
                getTitle_Name(namesTitleUsingIOS[iosVer][strIndex], curTitle);
            }
        }
    }
}

void scanIOSUsedByTypes(u32 type) {
    u32 i, iosVer = 0;
    TITLE * curTitle;
    for (i = 0; i < typeCnt[type]; i += 1) {
        curTitle = &installed_titles[type][i];
        iosVer = curTitle->requiredIOS;
        if (iosVer > 2 && iosVer < 256) {
            if (ios_found[iosVer] <= 0) {
                wiiSettings.missingIOSwarning = true;
                //printf("missing ios %u\n", iosVer);
            }
            ios_used[iosVer]++;
        }
    }
}

int titleCmp(const void *a, const void *b) {
    return memcmp(((TITLE *) a)->text, ((TITLE *) b)->text, 15);
}
