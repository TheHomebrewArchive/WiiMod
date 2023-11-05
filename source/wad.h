#ifndef _WAD_H_
#define _WAD_H_

#include "IOSPatcher.h"

#ifdef __cplusplus
extern "C" {
#endif


/* 'WAD Header' structure */
typedef struct {
    /* Header length */
    u32 header_len;

    /* WAD type */
    u16 type;

    u16 padding;

    /* Data length */
    u32 certs_len;
    u32 crl_len;
    u32 tik_len;
    u32 tmd_len;
    u32 data_len;
    u32 footer_len;
}ATTRIBUTE_PACKED wadHeader;

int installedIOSorSM;

bool create_folders(char *path);
s32 Wad_Install(FILE *);
s32 Wad_Uninstall(FILE *);
s32 Wad_Dump(u64 id, char *path);
u32 GetCert2(signed_blob **cert);
int check_not_0(size_t ret, char *error);
u32 GetTicket2(u64 id, signed_blob **tik);
u32 GetTMD2(u64 id, signed_blob **tmd);
u32 GetContent2(u8 **out, u64 id, u16 content, u16 index);
int get_shared2(u8 **outbuf, u16 index, sha1 hash);
s32 DumpIOStoSD(u8 loc);
s32 Wad_Read_into_memory(char *filename, IOS **iosptr, u32 iosnr, u16 revision);


#ifdef __cplusplus
}
#endif

#endif
