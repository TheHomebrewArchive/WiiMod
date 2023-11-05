#ifndef _TITLE_INSTALL_H_
#define _TITLE_INSTALL_H_

#ifdef __cplusplus
extern "C" {
#endif

// Turn upper and lower into a full title ID
#define TITLE_ID(x,y)   (((u64)(x) << 32) | (y))
#define TITLE_UPPER(x)  ((u32)((x) >> 32))
#define TITLE_LOWER(x)  ((u32)(x))

/* Prototypes */
s32 Title_Download( u64, u16, signed_blob **, signed_blob **);
s32 Title_ExtractWAD(u8 *, signed_blob **, signed_blob **);
s32 Title_Install(u64 tid, signed_blob *, signed_blob *);
s32 Title_Clean(signed_blob *);
s32 Uninstall_FromTitle(const u64 tid);
int CheckAndRemoveStubs();
int downloadAndInstall(u64 tid, u16 version, char* item);
int downloadAndInstallAndChangeVersion(u64 tid, u16 version, char* item, u16 version2, u32 requiredIOS);
int loadSystemMenuAndInstallAndChangeVersion(int source, u16 version, u16 version2, u32 requiredIOS);
s32 Title_ReadNetwork(u64 tid, const char *filename, void **outbuf, u32 *outlen);
#ifdef __cplusplus
}
#endif

#endif
