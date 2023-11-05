#ifndef IOSMENU_H
#define IOSMENU_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IOS_NotonNus, IOS_NoNusstub, IOS_Stub, IOS_OK
} Iosmode;

typedef struct IOSrev {
    u16 num;
    Iosmode mode;
    u32 hash[5];
} IOSrev;

typedef struct IOSlisting {
    u32 ver;
    int onnus;
    const char Description[512];
    u32 cnt;
    IOSrev revs[30];
} IOSlisting;

extern const int GrandIOSLEN;
extern int LesserIOSLEN;
extern const IOSlisting GrandIOSList[];
extern s32 GrandIOSListLookup[256];
extern s32 GrandIOSListLookup2[256];

int CheckForIOS(int ver, u16 rev, u16 sysVersion, bool chooseVersion, bool chooseReqIOS);
u8 chooseiosloc(char *str, u8 cur);
int IOSMenu();

#ifdef __cplusplus
}
#endif

#endif
