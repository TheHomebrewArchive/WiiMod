#ifndef _IOSPatcher_H_
#define _IOSPatcher_H_

#ifdef __cplusplus
extern "C" {
#endif

#define ISALIGNED(x)    ((((u32)x)&0x1F)==0)

typedef struct {
    u32 iosVersion;
    u16 iosRevision;
    bool esTruchaPatch;
    bool esIdentifyPatch;
    bool nandPatch;
    bool addticketPatch;
    bool setuidcheckPatch;
    bool koreanKeyPatch;
    bool haveFakeSign;
    bool haveNandPerm;
    bool setAHBPROTPatch;
    int esOtherPatches;
    int diPatches;
    u32 location;
    u16 newrevision;
} patchSettings;

typedef struct {
    signed_blob *certs;
    signed_blob *ticket;
    signed_blob *tmd;
    signed_blob *crl;

    u32 certs_size;
    u32 ticket_size;
    u32 tmd_size;
    u32 crl_size;

    u8 **encrypted_buffer;
    u8 **decrypted_buffer;
    u32 *buffer_size;

    u32 content_count;
}ATTRIBUTE_PACKED IOS;

extern const u8 identify_check[];
extern const u8 identify_patch[];
extern const u8 addticket_vers_check[];
extern const u8 setuid_check[];
extern const u8 setuid_patch[];
extern const u8 isfs_perms_check[];
extern const u8 e0_patch[];
extern const u8 es_set_ahbprot_check[];
extern const u8 es_set_ahbprot_patch[];

extern const u32 identify_check_size;
extern const u32 identify_patch_size;
extern const u32 addticket_vers_check_size;
extern const u32 setuid_check_size;
extern const u32 setuid_patch_size;
extern const u32 isfs_perms_check_size;
extern const u32 e0_patch_size;
extern const u32 es_set_ahbprot_check_size;
extern const u32 es_set_ahbprot_patch_size;

void get_title_key(signed_blob *s_tik, u8 *key);
void decrypt_IOS(IOS *ios);
s32 Init_IOS(IOS **iosptr);
void free_IOS(IOS **iosptr);
s32 get_IOS(IOS **iosptr, u32 iosnr, u16 revision);
s32 set_content_count(IOS *ios, u32 count);
s32 IosInstall(patchSettings settings);
s32 IosInstallUnpatched(u32 iosversion, u32 revision);
s32 IosDowngrade(u32 iosVersion, u16 highRevision, u16 lowRevision);
s32 Downgrade_TMD_Revision(const signed_blob* ptmd, u32 tmd_size, void* certs,
    u32 certs_size);
int patch_fake_sign_check(u8 *buf, u32 size);
int patch_identify_check(u8 *buf, u32 size);
s32 brute_tik(tik *p_tik);
void forge_tmd(signed_blob *s_tmd);
void forge_tik(signed_blob *s_tik);
void encrypt_buffer(u16 index, u8 *source, u8 *dest, u32 len);
void clearPatchSettings(patchSettings* settings);
int configInstallSettings(patchSettings* settings, int sigPatch, int esIdentPatch,
    int applynandPatch, int chooseloc);
void change_ticket_title_id(signed_blob *s_tik, u32 titleid1, u32 titleid2);
void change_tmd_title_id(signed_blob *s_tmd, u32 titleid1, u32 titleid2);
void change_tmd_title_version(signed_blob *s_tmd, u16 newversion);
void change_tmd_required_ios(signed_blob *s_tmd, u32 ios);
u64 get_tmd_required_ios(signed_blob *s_tmd);
void encrypt_IOS(IOS *ios);
s32 install_IOS(IOS *ios, bool skipticket);
s32 IOSMove(patchSettings settings);
int applyPatches( IOS *ios, patchSettings settings);
u32 apply_patch(const char *name, u8 *buf, const u32 buf_size, const u8 *old, u32 old_size, const u8 *patch,
    u32 patch_size, u32 patch_offset);

#ifdef __cplusplus
}
#endif
#endif
