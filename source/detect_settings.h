/*-------------------------------------------------------------
 
 detect_settings.h -- detects various system settings
 
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

#ifndef __SYSMENU_DETECT_H_
#define __SYSMENU_DETECT_H_

#define SADR_LENGTH 0x1007+1

typedef struct {
    u32 deviceId;
    u32 hollywoodVersion;
    u32 boot2version;
    u16 sysMenuVer;
    s32 sysMenuIOS;
    bool sysMenuIOSisStub;
    bool regionChangedKoreanWii;
    double sysMenuNinVersion;
    char sysMenuRegion;
    char regionFromSerial;
    s32 lang;
    s32 area;
    s32 game;
    s32 video;
    s32 eula;
    s32 country;
    bool reRunWithSU;
    bool missingIOSwarning;
    bool SMRegionMismatchWarning;
    bool failSMContentRead;
    u16 bcVersion;
    u16 miosVersion;
    u32 titleCnt;
    u32 iosCount;
    bool ahbprot;
} SYSSETTINGS;

typedef struct {
    u64 tid;
    u32 type;
    u64 requiredIOStid;
    u32 requiredIOS;
    u16 version;
    char nameDB[256];
    bool failedToReadDB;
    char nameBN[256];
    bool failedToReadBN;
    char name00[256];
    bool failedToRead00;
    bool failedToGetTMDSize;
    char text[15];
    u16 num_contents;
} TITLE;

typedef struct {
    char name[ISFS_MAXPATH + 1];
    int type;
} dirent_t;

extern const u32 types[7];
TITLE **installed_titles;
int typeCnt[7];
u16 ios_found[256];
u32 ios_isStub[256];
u32 ios_used[256];
u32 ios_used2[256];
char **namesTitleUsingIOS[256];

extern SYSSETTINGS wiiSettings;
extern u32 ios_hash[256][5];
extern s32 ios_clean[256];
extern bool skipios[256];

extern u32 hashesSkipIOS[100][5];
extern u32 skipHashesCnt;

//Get the IOS version of a given title
u64 get_required_ios_tid(u32 tidh, u32 tidl);
s32 getRegion();
void updateSysMenuVersion();
s32 getIOSVerForSystemMenu(s32 sysVersion);
int detectSettings();
void updateRegionSettings();
int checkErrors();
char AREAtoSysMenuRegion(int area);
int checkRegionSystemMenuMismatch(s32 cc);
int checkSystemMenuIOSisNotStub(int cc);
int checkMissingSystemMenuIOS( int cc );
void updateTitles();

char getSystemMenuRegionFromContent();
char getSystemMenuRegionFromContentES();

#endif
