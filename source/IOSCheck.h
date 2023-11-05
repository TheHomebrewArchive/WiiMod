#ifndef _IOSCHECK_H_
#define _IOSCHECK_H_

#ifdef __cplusplus
extern "C" {
#endif

int get_certs(void);
int check_fakesig(void);
int check_identify(void);
int check_flash(void);
int check_usb2(void);
int check_boot2(void);
int check_nandp(void);
u16 check_sysmenuver(void);
bool isIOSstub( u8);
int checkIOSMenu();
u8 *get_ioslist(u32 *cnt);
int checkmyios(bool displayIOS, bool skipcheckingIOSs, bool dumpHashes, bool keepAHBPROTOverride);
void checkIOSConditionally();
int loadiosfromcsv();
u16 GetTitleVersion(u64 tid);
int ClearPermDataIOS(u32 loc);
void checkPatches(int i);

bool flagOverrideIOSCheck;
int ios_flash[256];
int ios_fsign[256];
int ios_ident[256];
int ios_usb2m[256];
int ios_boot2[256];
int ios_nandp[256];
u16 ios_sysmen[256];
extern u16 ios_found[256];

#ifdef __cplusplus
}
#endif

#endif
