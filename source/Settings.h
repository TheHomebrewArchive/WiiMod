
#ifndef SETTINGS_H_
#define SETTINGS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FILE_PATH_LEN   1024

typedef struct {
    s32 SUIdentified;
    s32 skipdisclaimer;
    s32 outputios;
    s32 neverSet;
    s32 AutoLoadIOS;
    s32 AutoLoadOnInstall;
    s32 DisableSetAHBPROT;
    s32 DisableControllerButtonHold;
    s32 DeleteDefault;
    s32 haveHBC;
    s32 printiosdetails;
    s32 skipIOSSelect;
    s32 loadoldcsv;
    s32 filterwads;
    s32 onlyCheckIOSOnDemand;
    s32 fatDeviceIndex;
    u64 hbcRequiredTID[4];
    char startupPath[256];
    char SMB_USER[20]; /*** Your share user ***/
    char SMB_PWD[40]; /*** Your share user password ***/
    char SMB_SHARE[40]; /*** Share name on server ***/
    char SMB_IP[20]; /*** IP Address of share server ***/
} SETTINGS;

extern SETTINGS theSettings;
extern char dbfile[];
extern char dbfile2[];
extern char dbfile3[];

s32 initSettings();

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H_ */
