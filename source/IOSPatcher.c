#include <fat.h>
#include <gccore.h>
#include <limits.h>
#include <malloc.h>
#include <math.h>
#include <network.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiilight.h>
#include <wiiuse/wpad.h>

#include "cert_sys.h"
#include "controller.h"
#include "detect_settings.h"
#include "dopios.h"
#include "Error.h"
#include "FakeSignInstaller.h"
#include "fat.h"
#include "FileSystem.h"
#include "gecko.h"
#include "http.h"
#include "IOSCheck.h"
#include "IOSMenu.h"
#include "IOSPatcher.h"
#include "kkey_bin.h"
#include "nand.h"
#include "network.h"
#include "rijndael.h"
#include "Settings.h"
#include "sha1.h"
#include "smb.h"
#include "title.h"
#include "title_install.h"
#include "tools.h"
#include "video.h"
#include "wad.h"

u8 commonkey[16] = { 0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48,
    0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7 };

const u8 identify_check[] = { 0x28, 0x03, 0xD1, 0x23 };
const u8 identify_patch[] = { 0x00, 0x00 };
const u8 addticket_vers_check[] = { 0xD2, 0x01, 0x4E, 0x56 };
const u8 setuid_check[] = { 0xD1, 0x2A, 0x1C, 0x39 };
const u8 setuid_patch[] = { 0x46, 0xC0 };
const u8 isfs_perms_check[] = { 0x42, 0x8B, 0xD0, 0x01, 0x25, 0x66 };
const u8 e0_patch[] = { 0xE0 };
const u8 es_set_ahbprot_check[] = { 0x68, 0x5B, 0x22, 0xEC, 0x00, 0x52, 0x18,
    0x9B, 0x68, 0x1B, 0x46, 0x98, 0x07, 0xDB };
const u8 es_set_ahbprot_patch[] = { 0x01 };

const u32 identify_check_size = sizeof(identify_check);
const u32 identify_patch_size = sizeof(identify_patch);
const u32 addticket_vers_check_size = sizeof(addticket_vers_check);
const u32 setuid_check_size = sizeof(setuid_check);
const u32 setuid_patch_size = sizeof(setuid_patch);
const u32 isfs_perms_check_size = sizeof(isfs_perms_check);
const u32 e0_patch_size = sizeof(e0_patch);
const u32 es_set_ahbprot_check_size = sizeof(es_set_ahbprot_check);
const u32 es_set_ahbprot_patch_size = sizeof(es_set_ahbprot_patch);

const u8 koreankey_check[] = { 0x28, 0x00, 0xD0, 0x0A, 0x20, 0x3A, 0x1C, 0x21 };
const u8 koreankey_data[] = { 0x27, 0xB8, 0xA5, 0xF2, 0x00, 0x00, 0x00, 0x00 };

int GetNusObject(u32 titleid1, u32 titleid2, u16 *version, char *content,
    u8 **outbuf, u32 *outlen) {
    char buf[200] = "";
    char filename[256] = "";
    int ret = 0;
    uint httpStatus = 200;
    FILE *fp = NULL;

    void _extractTmdVersion() {
        gprintf("Getting Version from TMD File\n");
        tmd *pTMD = (tmd *) SIGNATURE_PAYLOAD((signed_blob *) *outbuf);
        *version = pTMD->title_version;
        char tempstr[80];
        sprintf(tempstr, "%s.%u", content, *version);
        sprintf(content, "%s", tempstr);
        snprintf(filename, sizeof(filename), "sd:/%08X/%08X/v%u/%s", titleid1, titleid2, *version, content);
        gprintf("New Filename = %s\n", filename);
    }

    gprintf("\nTitleID1 = %08X, TitleID2 = %08X, Content = %s, Version = %u\n", titleid1, titleid2,
            content, *version);

    // If version is = 0, check for existence sd:/XXX/XXX/tmd
    // This is used for NUS offline mode
    if (strcmpi(content, "TMD") == 0 && *version == 0 && Fat_Mount(&fdevList[FAT_SD])) {
        snprintf(filename, sizeof(filename), "sd:/%08X/%08X/tmd", titleid1, titleid2);
        gprintf("Looking on SD card for (%s).\n", filename);
        if (FileExists(filename)) {
            ret = FileReadBinary(filename, outbuf, outlen);
            _extractTmdVersion();
            if (ret > 0) {
                Fat_Unmount(&fdevList[FAT_SD]);
                return ret;
            }
        } else {
            gprintf("File (%s) not found.\n", filename);
        }
        Fat_Unmount(&fdevList[FAT_SD]);
    }
    //else gprintf("No Card Found or content != TMD or Version != 0\n");

    // If Version != 0 then we know what version of a file we want
    // so continue normal operations
    snprintf(filename, sizeof(filename), "sd:/%08X/%08X/v%u/%s", titleid1, titleid2, *version, content);

    if (*version != 0 && Fat_Mount(&fdevList[FAT_SD])) {
        ret = FileReadBinary(filename, outbuf, outlen);
        Fat_Unmount(&fdevList[FAT_SD]);
        if (ret > 0) {
            return ret;
        }
        gprintf("\n* %s not found on the SD card.", filename);
        gprintf("\n* Trying to download it from the internet...\n");
    }
    //else gprintf("No Card Found or Version == 0\n");

    gprintf("NusGetObject::NetworkInit...");
    NetworkInit();
    gprintf("Done\n");

    snprintf(buf, 128, "http://%s%s%08x%08x/%s", NusHostname, NusPath, titleid1, titleid2, content);
    gprintf("Attemping to download %s\n\n", buf);

    int retry = 5;
    while (1) {
        ret = http_request(buf, (u32)(1 << 31));
        //gprintf("http_request = %d\n", ret);
        if (!ret) {
            retry--;
            printf("Error making HTTP request. Trying Again. \n");
            gprintf("Request: %s Ret: %d\n", buf, ret);
            sleep(1);
            if (retry < 0) return -1;
        } else
            break;
    }
    ret = http_get_result(&httpStatus, outbuf, outlen);

    //gprintf("http_get_result = %d\n", ret);

    // if version = 0 and file is TMD file then lets extract the version and return it
    // so that the rest of the files will save to the correct folder
    if (strcmpi(content, "TMD") == 0 && *version == 0 && *outbuf != NULL
            && *outlen != 0) {
        _extractTmdVersion();
    }

    if (*version != 0 && Fat_Mount(&fdevList[FAT_SD])) {
        char folder[300] = "";
        snprintf(folder, sizeof(folder), "sd:/%08X/%08X/v%u", titleid1, titleid2, *version);
        gprintf("FolderCreateTree() = %s ", (FolderCreateTree(folder)? "True" : "False"));

        gprintf("\nWriting File (%s)\n", filename);

        fp = fopen(filename, "wb");
        if (fp) {
            fwrite(*outbuf, *outlen, 1, fp);
            fclose(fp);
        } else {
            gprintf("\nCould not write file %s to the SD Card. \n", filename);
        }
        Fat_Unmount(&fdevList[FAT_SD]);
    }

    if (((int) *outbuf & 0xF0000000) == 0xF0000000) return (int) *outbuf;

    return 0;
}

void get_title_key(signed_blob *s_tik, u8 *key) {
    static u8 iv[16] ATTRIBUTE_ALIGN(0x20);
    static u8 keyin[16] ATTRIBUTE_ALIGN(0x20);
    static u8 keyout[16] ATTRIBUTE_ALIGN(0x20);

    const tik *p_tik;
    p_tik = (tik*) SIGNATURE_PAYLOAD(s_tik);
    u8 *enc_key = (u8 *) &p_tik->cipher_title_key;
    memcpy(keyin, enc_key, sizeof keyin);
    memset(keyout, 0, sizeof keyout);
    memset(iv, 0, sizeof iv);
    memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);

    aes_set_key(commonkey);
    aes_decrypt(iv, keyin, keyout, sizeof keyin);

    memcpy(key, keyout, sizeof keyout);
}

void change_ticket_title_id(signed_blob *s_tik, u32 titleid1, u32 titleid2) {
    static u8 iv[16] ATTRIBUTE_ALIGN(0x20);
    static u8 keyin[16] ATTRIBUTE_ALIGN(0x20);
    static u8 keyout[16] ATTRIBUTE_ALIGN(0x20);

    tik *p_tik;
    p_tik = (tik*) SIGNATURE_PAYLOAD(s_tik);
    u8 *enc_key = (u8 *) &p_tik->cipher_title_key;
    memcpy(keyin, enc_key, sizeof keyin);
    memset(keyout, 0, sizeof keyout);
    memset(iv, 0, sizeof iv);
    memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);

    aes_set_key(commonkey);
    aes_decrypt(iv, keyin, keyout, sizeof keyin);
    p_tik->titleid = (u64) titleid1 << 32 | (u64) titleid2;
    memset(iv, 0, sizeof iv);
    memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);

    aes_encrypt(iv, keyout, keyin, sizeof keyout);
    memcpy(enc_key, keyin, sizeof keyin);
}

void change_tmd_title_id(signed_blob *s_tmd, u32 titleid1, u32 titleid2) {
    tmd *p_tmd;
    u64 title_id = titleid1;
    title_id <<= 32;
    title_id |= titleid2;
    p_tmd = (tmd*) SIGNATURE_PAYLOAD(s_tmd);
    p_tmd->title_id = title_id;
}

void change_tmd_title_version(signed_blob *s_tmd, u16 newversion) {
    tmd *p_tmd;
    p_tmd = (tmd*) SIGNATURE_PAYLOAD(s_tmd);
    p_tmd->title_version = newversion;
}

void change_tmd_required_ios(signed_blob *s_tmd, u32 ios) {
    tmd *p_tmd;
    u64 title_id = 1;
    title_id <<= 32;
    title_id |= ios;
    p_tmd = (tmd*) SIGNATURE_PAYLOAD(s_tmd);
    p_tmd->sys_version = title_id;
}

u64 get_tmd_required_ios(signed_blob *s_tmd) {
    tmd *p_tmd;
    p_tmd = (tmd*) SIGNATURE_PAYLOAD(s_tmd);
    return p_tmd->sys_version;
}

void zero_sig(signed_blob *sig) {
    u8 *sig_ptr = (u8 *) sig;
    memset(sig_ptr + 4, 0, SIGNATURE_SIZE(sig) - 4);
}

s32 brute_tmd(tmd *p_tmd) {
    u16 fill;
    for (fill = 0; fill < 65535; fill++) {
        p_tmd->fill3 = fill;
        sha1 hash;
        //gprintf("SHA1(%p, %x, %p)\n", p_tmd, TMD_SIZE(p_tmd), hash);
        SHA1((u8 *) p_tmd, TMD_SIZE(p_tmd), hash);
        if (hash[0] == 0) {
            gprintf("setting fill3 to %04hx\n", fill);
            return 0;
        }
    }
    printf("Unable to fix tmd :(\n");
    return -1;
}

s32 brute_tik(tik *p_tik) {
    u16 fill;
    for (fill = 0; fill < 65535; fill++) {
        p_tik->padding = fill;
        sha1 hash;
        //    gprintf("SHA1(%p, %x, %p)\n", p_tmd, TMD_SIZE(p_tmd), hash);
        SHA1((u8 *) p_tik, sizeof(tik), hash);
        if (hash[0] == 0) return 1;
    }
    printf("Unable to fix tik :(\n");
    return -1;
}

void forge_tmd(signed_blob *s_tmd) {
    gprintf("forging tmd sig\n");
    zero_sig(s_tmd);
    brute_tmd(SIGNATURE_PAYLOAD(s_tmd));
}

void forge_tik(signed_blob *s_tik) {
    gprintf("forging tik sig\n");
    zero_sig(s_tik);
    brute_tik(SIGNATURE_PAYLOAD(s_tik));
}

int patch_fake_sign_check(u8 *buf, u32 size) {
    u32 i;
    u32 match_count = 0;
    u8 new_hash_check[] = { 0x20, 0x07, 0x4B, 0x0B };
    u8 old_hash_check[] = { 0x20, 0x07, 0x23, 0xA2 };
    
    for (i = 0; i < size - 4; i++) {
        if (!memcmp(buf + i, new_hash_check, sizeof new_hash_check)) {
            gcprintf("\n\t- Found Hash check @ 0x%x, patching... ", i);
            buf[i + 1] = 0;
            match_count++;
            break;
        }
        if (!memcmp(buf + i, old_hash_check, sizeof old_hash_check)) {
            buf[i + 1] = 0;
            match_count++;
            break;
        }
    }
    return match_count;
}

u32 apply_patch(const char *name, u8 *buf, const u32 buf_size, const u8 *old,
    u32 old_size, const u8 *patch, u32 patch_size, u32 patch_offset) {
    u32 found = 0;
    u32 i;

    for (i = 0; i < buf_size - old_size; i++) {
        if (!memcmp(buf + i, old, old_size)) {
            memcpy(buf + i + patch_offset, patch, patch_size);
            found++;
            printf("\n\t- Found %s @ 0x%x, patching... ", name, i);
            break;
        }
    }
    return found;
}

int patch_ES_module(u8* buf, u32 size) {
    const u8 delete_check[] = { 0xD8, 0x00, 0x4A, 0x04 };
    const u8 opentitlecontent_check[] = { 0x9D, 0x05, 0x42, 0x9D, 0xD0, 0x03 };
    const u8 readcontent_check[] = { 0xFC, 0x0F, 0xB5, 0x30, 0x1C, 0x14, 0x1C,
        0x1D, 0x4B, 0x0E, 0x68, 0x9B, 0x2B, 0x00, 0xD0, 0x03, 0x29, 0x00, 0xDB,
        0x01, 0x29, 0x0F, 0xDD, 0x01 };
    const u8 closecontent_check[] = { 0xB5, 0x10, 0x4B, 0x10, 0x68, 0x9B, 0x2B,
        0x00, 0xD0, 0x03, 0x29, 0x00, 0xDB, 0x01, 0x29, 0x0F, 0xDD, 0x01 };
    const u8 iosdelete_check[] = { 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFC,
        0x07, 0xB5, 0xF0 };
    const u8 new_iosdelete[] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0xB5, 0xF0 };
    
    int ret = 0;
    u32 i = 0;
    bool found_addticket = false, found_delete = false, found_opentitlecontent =
            false, found_readcontent = false, found_closecontent = false,
            found_iosdelete = false;
    
    printf("Patching ES Module....");
    
    while (i < size - sizeof readcontent_check && !(found_addticket
            && found_delete && found_opentitlecontent && found_readcontent)) {
        if (!found_delete
                && !memcmp(buf + i, delete_check, sizeof(delete_check))) {
            printf("\n\t- Found delete check @ 0x%x, patching... ", i);
            buf[i] = 0xE0;
            found_delete = true;
            ret += 1;
        }
        if (!found_opentitlecontent
                && !memcmp(buf + i, opentitlecontent_check, sizeof(opentitlecontent_check))) {
            printf("\n\t- Found opentitlecontent check @ 0x%x, patching... ", i);
            buf[i + 4] = 0xE0;
            found_opentitlecontent = true;
            ret += 1;
        }
        if (!found_readcontent
                && !memcmp(buf + i, readcontent_check, sizeof(readcontent_check))) {
            printf("\n\t- Found readcontent check @ 0x%x, patching... ", i);
            *(u16*) (&buf[i + 14]) = 0x46C0;
            *(u16*) (&buf[i + 18]) = 0x46C0;
            buf[i + 22] = 0xE0;
            found_readcontent = true;
            ret += 1;
        }
        if (!found_closecontent
                && !memcmp(buf + i, closecontent_check, sizeof(closecontent_check))) {
            printf("\n\t- Found closecontent check @ 0x%x, patching... ", i);
            *(u16*) (&buf[i + 8]) = 0x46C0;
            *(u16*) (&buf[i + 12]) = 0x46C0;
            found_closecontent = true;
            ret += 1;
        }
        if (!found_iosdelete
                && !memcmp(buf + i, iosdelete_check, sizeof iosdelete_check)) {
            printf("\n\t- Found IOS deletion check @ 0x%x, patching... ", i);
            memcpy(buf + i, new_iosdelete, sizeof(new_iosdelete));
            buf += sizeof(new_iosdelete);
            found_iosdelete = true;
            ret += 1;
        }
        i++;
    }

    printf("\b\b! \n");
    
    if (!(found_addticket || found_delete || found_opentitlecontent
            || found_readcontent)) {
        printf("Couldn't find patch locations!\n");
        return 0;
    }
    
    return ret;
}

int patch_DI_module(u8* buf, u32 size) {
    const u8 readlimit_old_table[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x46, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x7E, 0xD4, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x08 };
    const u8 dvdread_newlimit[] = { 0x7E, 0xD4, 0x00, 0x00 };
    u32 i = 0;
    bool found_enabledvd = false, found_readlimit = false;
    
    printf("Patching DI module....");
    
    while (i < size - sizeof readlimit_old_table && !(found_enabledvd
            && found_readlimit)) {
        if (!found_readlimit
                && !memcmp(buf + i, readlimit_old_table, sizeof readlimit_old_table)) {
            memcpy(buf + i + 12, dvdread_newlimit, sizeof dvdread_newlimit);
            printf("\bPatched  ");
            found_enabledvd = true;
            i += sizeof dvdread_newlimit;
        }
        i++;
    }

    printf("\b\b! \n");
    
    if (!(found_enabledvd || found_readlimit)) {
        printf("Couldn't find patch locations!\n");
        return 0;
    }
    
    return 1;
}

static void display_tag(u8 *buf) {
    printf("Firmware version: %s Builder: %s\n", buf, buf + 0x30);
}

static void display_ios_tags(u8 *buf, u32 size) {
    u32 i;
    char *ios_version_tag = "$IOSVersion:";
    
    if (size == 64) {
        display_tag(buf);
        return;
    }
    
    for (i = 0; i < (size - 64); i++) {
        if (!strncmp((char *) buf + i, ios_version_tag, 10)) {
            char version_buf[128], *date;
            while (buf[i + strlen(ios_version_tag)] == ' ')
                i++; // skip spaces
            strlcpy(version_buf, (char *) buf + i + strlen(ios_version_tag), sizeof version_buf);
            date = version_buf;
            strsep(&date, "$");
            date = version_buf;
            strsep(&date, ":");
            printf("%s (%s)\n", version_buf, date);
            i += 64;
        }
    }
}

bool contains_module(u8 *buf, u32 size, char *module) {
    u32 i;
    char *ios_version_tag = "$IOSVersion:";
    
    for (i = 0; i < (size - 64); i++) {
        if (!strncmp((char *) buf + i, ios_version_tag, 10)) {
            char version_buf[128];
            while (buf[i + strlen(ios_version_tag)] == ' ')
                i++; // skip spaces
            strlcpy(version_buf, (char *) buf + i + strlen(ios_version_tag), sizeof version_buf);
            i += 64;
            if (strncmp(version_buf, module, strlen(module)) == 0) return true;
        }
    }
    return false;
}

s32 module_index(IOS *ios, char *module) {
    int i;
    for (i = 0; i < ios->content_count; i++) {
        if (!ios->decrypted_buffer[i] || !ios->buffer_size[i]) return -1;
        if (contains_module(ios->decrypted_buffer[i], ios->buffer_size[i], module)) return i;
    }
    return -1;
}

bool contains_module2(u8 *buf, u32 size, char *module, u32 modsize) {
    u32 i;
    for (i = 0; i < (size - modsize); i++) {
        if (!strncmp((char *) buf + i, module, modsize)) {
            return true;
        }
    }
    return false;
}

s32 module_index2(IOS *ios, char *module, int modsize) {
    int i;
    for (i = 0; i < ios->content_count; i++) {
        if (!ios->decrypted_buffer[i] || !ios->buffer_size[i]) return -1;
        if (contains_module2(ios->decrypted_buffer[i], ios->buffer_size[i], module, modsize)) return i;
    }
    return -1;
}

static void decrypt_buffer(u16 index, u8 *source, u8 *dest, u32 len) {
    static u8 iv[16];
    memset(iv, 0, 16);
    memcpy(iv, &index, 2);
    aes_decrypt(iv, source, dest, len);
}

void encrypt_buffer(u16 index, u8 *source, u8 *dest, u32 len) {
    static u8 iv[16];
    if (!source) {
        printf("decrypt_buffer: invalid source paramater\n");
    }
    if (!dest) {
        printf("decrypt_buffer: invalid dest paramater\n");
    }
    memset(iv, 0, 16);
    memcpy(iv, &index, 2);
    aes_encrypt(iv, source, dest, len);
}

void decrypt_IOS(IOS *ios) {
    u8 key[16];
    get_title_key(ios->ticket, key);
    aes_set_key(key);

    int i;
    for (i = 0; i < ios->content_count; i++) {
        decrypt_buffer(i, ios->encrypted_buffer[i], ios->decrypted_buffer[i], ios->buffer_size[i]);
    }
}

void encrypt_IOS(IOS *ios) {
    u8 key[16];
    get_title_key(ios->ticket, key);
    aes_set_key(key);

    int i;
    for (i = 0; i < ios->content_count; i++) {
        encrypt_buffer(i, ios->decrypted_buffer[i], ios->encrypted_buffer[i], ios->buffer_size[i]);
    }
}

void display_tags(IOS *ios) {
    int i;
    for (i = 0; i < ios->content_count; i++) {
        printf("Content %2u:  ", i);
        display_ios_tags(ios->decrypted_buffer[i], ios->buffer_size[i]);
    }
}

s32 Init_IOS(IOS **iosptr) {
    *iosptr = memalign(32, sizeof(IOS));
    if (iosptr == NULL) return -1;

    (*iosptr)->content_count = 0;
    
    (*iosptr)->certs = NULL;
    (*iosptr)->certs_size = 0;
    (*iosptr)->ticket = NULL;
    (*iosptr)->ticket_size = 0;
    (*iosptr)->tmd = NULL;
    (*iosptr)->tmd_size = 0;
    (*iosptr)->crl = NULL;
    (*iosptr)->crl_size = 0;
    
    (*iosptr)->encrypted_buffer = NULL;
    (*iosptr)->decrypted_buffer = NULL;
    (*iosptr)->buffer_size = NULL;
    return 0;
}

void free_IOS(IOS **ios) {
    if (ios && *ios) {
        if ((*ios)->certs) free((*ios)->certs);
        if ((*ios)->ticket) free((*ios)->ticket);
        if ((*ios)->tmd) free((*ios)->tmd);
        if ((*ios)->crl) free((*ios)->crl);
        
        int i;
        for (i = 0; i < (*ios)->content_count; i++) {
            if ((*ios)->encrypted_buffer && (*ios)->encrypted_buffer[i]) free((*ios)->encrypted_buffer[i]);
            if ((*ios)->decrypted_buffer && (*ios)->decrypted_buffer[i]) free((*ios)->decrypted_buffer[i]);
        }

        if ((*ios)->encrypted_buffer) free((*ios)->encrypted_buffer);
        if ((*ios)->decrypted_buffer) free((*ios)->decrypted_buffer);
        if ((*ios)->buffer_size) free((*ios)->buffer_size);
        free(*ios);
    }
}

s32 set_content_count(IOS *ios, u32 count) {
    int i;
    if (ios->content_count > 0) {
        for (i = 0; i < ios->content_count; i++) {
            if (ios->encrypted_buffer && ios->encrypted_buffer[i]) free(ios->encrypted_buffer[i]);
            if (ios->decrypted_buffer && ios->decrypted_buffer[i]) free(ios->decrypted_buffer[i]);
        }

        if (ios->encrypted_buffer) free(ios->encrypted_buffer);
        if (ios->decrypted_buffer) free(ios->decrypted_buffer);
        if (ios->buffer_size) free(ios->buffer_size);
    }

    ios->content_count = count;
    if (count > 0) {
        ios->encrypted_buffer = memalign(32, 4 * count);
        ios->decrypted_buffer = memalign(32, 4 * count);
        ios->buffer_size = memalign(32, 4 * count);
        
        for (i = 0; i < count; i++) {
            if (ios->encrypted_buffer) ios->encrypted_buffer[i] = NULL;
            if (ios->decrypted_buffer) ios->decrypted_buffer[i] = NULL;
        }
        
        if (!ios->encrypted_buffer || !ios->decrypted_buffer
                || !ios->buffer_size) {
            return -1;
        }
    }
    return 0;
}

s32 install_IOS(IOS *ios, bool skipticket) {
    int ret;
    int cfd;
    
    if (!skipticket) {
        ret
                = ES_AddTicket(ios->ticket, ios->ticket_size, ios->certs, ios->certs_size, ios->crl, ios->crl_size);
        if (ret < 0) {
            gcprintf("ERROR: ES_AddTicket: %s\n", EsErrorCodeString(ret));
            ES_AddTitleCancel();
            return ret;
        }
    }
    gcprintf("\b..");
    
    ret
            = ES_AddTitleStart(ios->tmd, ios->tmd_size, ios->certs, ios->certs_size, ios->crl, ios->crl_size);
    if (ret < 0) {
        gcprintf("\nERROR: ES_AddTitleStart: %s\n", EsErrorCodeString(ret));
        ES_AddTitleCancel();
        return ret;
    }
    gcprintf("\b..");
    
    tmd *tmd_data = (tmd *) SIGNATURE_PAYLOAD(ios->tmd);
    
    int i;
    for (i = 0; i < ios->content_count; i++) {
        tmd_content *content = &tmd_data->contents[i];
        
        cfd = ES_AddContentStart(tmd_data->title_id, content->cid);
        if (cfd < 0) {
            gcprintf("\nERROR: ES_AddContentStart for content #%u cid %u returned: %s\n", i, content->cid, EsErrorCodeString(cfd));
            ES_AddTitleCancel();
            return cfd;
        }

        ret
                = ES_AddContentData(cfd, ios->encrypted_buffer[i], ios->buffer_size[i]);
        if (ret < 0) {
            gcprintf("\nERROR: ES_AddContentData for content #%u cid %u returned: %s\n", i, content->cid, EsErrorCodeString(ret));
            ES_AddTitleCancel();
            return ret;
        }

        ret = ES_AddContentFinish(cfd);
        if (ret < 0) {
            gcprintf("\ERROR: nES_AddContentFinish for content #%u cid %u returned: %s\n", i, content->cid, EsErrorCodeString(ret));
            ES_AddTitleCancel();
            return ret;
        }
        gcprintf("\b..");
    }

    ret = ES_AddTitleFinish();
    if (ret < 0) {
        gcprintf("\nERROR: ES_AddTitleFinish: %s\n", EsErrorCodeString(ret));
        ES_AddTitleCancel();
        return ret;
    }
    gcprintf("\b..");
    return 0;
}

s32 GetCerts(signed_blob** Certs, u32* Length) {
    if (cert_sys_size != 2560) return -1;
    *Certs = memalign(32, 2560);
    
    if (*Certs == NULL) {
        printf("Out of memory\n");
        return -1;
    }
    memcpy(*Certs, cert_sys, cert_sys_size);
    *Length = 2560;
    return 0;
}

s32 Download_IOS(IOS **iosptr, u32 iosnr, u16 revision) {
    s32 ret;
    sha1 hash;
    int i;
    tmd *tmd_data = NULL;
    u32 cnt;
    char buf[32];
    u16 version = revision;
    ret = Init_IOS(iosptr);
    if (ret < 0) {
        gcprintf("Out of memory\n");
        free_IOS(iosptr);
        return -1;
    }

    gcprintf("Loading certs...");
    ret = GetCerts(&(*iosptr)->certs, &(*iosptr)->certs_size);
    if (ret < 0) {
        gcprintf("Loading certs from nand failed, ret = %d\n", ret);
        free_IOS(iosptr);
        return -1;
    }

    if ((*iosptr)->certs == NULL || (*iosptr)->certs_size == 0) {
        gcprintf("certs error\n");
        free_IOS(iosptr);
        return -1;
    }

    if (!IS_VALID_SIGNATURE((*iosptr)->certs)) {
        gcprintf("Error: Bad certs signature!\n");
        free_IOS(iosptr);
        return -1;
    }
    printf("\b.Done\n");

    gcprintf("Loading TMD...");
    snprintf(buf, sizeof(buf), "tmd.%u", version);
    u8 *tmd_buffer = NULL;
    SpinnerStart();
    ret
            = GetNusObject(1, iosnr, &version, buf, &tmd_buffer, &((*iosptr)->tmd_size));
    SpinnerStop();
    if (ret < 0) {
        gcprintf("Loading tmd failed, ret = %u\n", ret);
        free_IOS(iosptr);
        return -1;
    }

    if (tmd_buffer == NULL || (*iosptr)->tmd_size == 0) {
        gcprintf("TMD error\n");
        free_IOS(iosptr);
        return -1;
    }

    (*iosptr)->tmd_size = SIGNED_TMD_SIZE((signed_blob *) tmd_buffer);
    (*iosptr)->tmd = memalign(32, (*iosptr)->tmd_size);
    if ((*iosptr)->tmd == NULL) {
        gcprintf("Out of memory\n");
        free_IOS(iosptr);
        return -1;
    }
    memcpy((*iosptr)->tmd, tmd_buffer, (*iosptr)->tmd_size);
    free(tmd_buffer);
    
    if (!IS_VALID_SIGNATURE((*iosptr)->tmd)) {
        gcprintf("Error: Bad TMD signature!\n");
        free_IOS(iosptr);
        return -1;
    }
    printf("\b.Done\n");
    
    gcprintf("Loading ticket...");
    u8 *ticket_buffer = NULL;
    SpinnerStart();
    ret
            = GetNusObject(1, iosnr, &version, "cetk", &ticket_buffer, &((*iosptr)->ticket_size));
    SpinnerStop();
    if (ret < 0) {
        gcprintf("Loading ticket failed, ret = %u\n", ret);
        free_IOS(iosptr);
        return -1;
    }

    if (ticket_buffer == NULL || (*iosptr)->ticket_size == 0) {
        gcprintf("ticket error\n");
        free_IOS(iosptr);
        return -1;
    }

    (*iosptr)->ticket_size = SIGNED_TIK_SIZE((signed_blob *) ticket_buffer);
    (*iosptr)->ticket = memalign(32, (*iosptr)->ticket_size);
    if ((*iosptr)->ticket == NULL) {
        gcprintf("Out of memory\n");
        free_IOS(iosptr);
        return -1;
    }
    memcpy((*iosptr)->ticket, ticket_buffer, (*iosptr)->ticket_size);
    free(ticket_buffer);
    
    if (!IS_VALID_SIGNATURE((*iosptr)->ticket)) {
        gcprintf("Error: Bad ticket signature!\n");
        free_IOS(iosptr);
        return -1;
    }
    printf("\b.Done\n");
    
    /* Get TMD info */
    tmd_data = (tmd *) SIGNATURE_PAYLOAD((*iosptr)->tmd);
    
    gcprintf("Checking titleid and revision...");
    if (TITLE_UPPER(tmd_data->title_id) != 1 || TITLE_LOWER(tmd_data->title_id)
            != iosnr) {
        gcprintf("IOS has titleid: %08x%08x but expected was: %08x%08x\n", TITLE_UPPER(tmd_data->title_id), TITLE_LOWER(tmd_data->title_id), 1, iosnr);
        free_IOS(iosptr);
        return -1;
    }

    if (tmd_data->title_version != version) {
        gcprintf("IOS has revision: %u but expected was: %u\n", tmd_data->title_version, version);
        free_IOS(iosptr);
        return -1;
    }

    ret = set_content_count(*iosptr, tmd_data->num_contents);
    if (ret < 0) {
        gcprintf("Out of memory\n");
        free_IOS(iosptr);
        return -1;
    }
    printf("\b.Done\n");
    
    printf("Press B to cancel\n");
    gcprintf("Loading contents...");

    u32 buttons;
    tmd_content *content;
    SpinnerStart();
    for (cnt = 0; cnt < tmd_data->num_contents; cnt++) {
        content = &tmd_data->contents[cnt];
        
        /* Encrypted content size */
        (*iosptr)->buffer_size[cnt] = round_up((u32)content->size, 64);
        snprintf(buf, sizeof(buf), "%08x", content->cid);

        ScanPads(&buttons);
        if (buttons & WPAD_BUTTON_B) {
            free_IOS(iosptr);
            SpinnerStop();
            return 0;
        }

        ret = GetNusObject(1, iosnr, &version, buf, &((*iosptr)->encrypted_buffer[cnt]), &((*iosptr)->buffer_size[cnt]));
        ScanPads(&buttons);
        if (buttons & WPAD_BUTTON_B) {
            free_IOS(iosptr);
            SpinnerStop();
            return 0;
        }

        // SD Loading may have occured so let's not add a dot to the end
        if (ret == 0) printf("\b. ");
        
        if ((*iosptr)->buffer_size[cnt] % 16) {
            gcprintf("Content %u size is not a multiple of 16\n", cnt);
            free_IOS(iosptr);
            SpinnerStop();
            return -1;
        }

        if ((*iosptr)->buffer_size[cnt] < (u32) content->size) {
            gcprintf("Content %u size is too small\n", cnt);
            free_IOS(iosptr);
            SpinnerStop();
            return -1;
        }

        (*iosptr)->decrypted_buffer[cnt]
                = memalign(32, (*iosptr)->buffer_size[cnt]);
        if (!(*iosptr)->decrypted_buffer[cnt]) {
            gcprintf("Out of memory\n");
            free_IOS(iosptr);
            SpinnerStop();
            return -1;
        }
    }
    SpinnerStop();
    printf("\b.Done\n");
    
    gcprintf("Reading file into memory complete.\n");
    gcprintf("Decrypting IOS...");
    decrypt_IOS(*iosptr);
    gcprintf("Done\n");
    
    tmd_content *p_cr = TMD_CONTENTS(tmd_data);
    
    gcprintf("Checking hashes...");
    for (i = 0; i < (*iosptr)->content_count; i++) {
        SHA1((*iosptr)->decrypted_buffer[i], (u32) p_cr[i].size, hash);
        if (memcmp(p_cr[i].hash, hash, sizeof hash) != 0) {
            gcprintf("Wrong hash for content #%u\n", i);
            free_IOS(iosptr);
            return -1;
        }
    }
    gcprintf("Done\n");
    ret = 1;
    return ret;
}

s32 get_IOS(IOS **ios, u32 iosnr, u16 revision) {
    char buf[64];
    u32 button;
    static int lastiosused = 0;
    u32 i;
    int found = -1, found2 = -1, ret, selection = lastiosused;
    char *optionsstring[5] = { "<Load IOS WAD from sd card>",
        "<Load IOS WAD from usb storage>", "<Load IOS WAD from SMB>",
        "<Download IOS from NUS>", "<Exit>" };
    
    found = GrandIOSListLookup[iosnr];
    if (found < 0) {
        return 0;
    }
    for (i = 0; i < GrandIOSList[found].cnt; i += 1) {
        if (GrandIOSList[found].revs[i].num == revision) {
            found2 = i;
            break;
        }
    }
    if (selection == 3 && (GrandIOSList[found].revs[found2].mode
            == IOS_NotonNus || GrandIOSList[found].revs[found2].mode
            == IOS_NoNusstub)) {
        selection = 0;
    }
    while (1) {
        ClearLine();
        
        set_highlight(true);
        printf(optionsstring[selection]);
        set_highlight(false);
        
        while (1) {
            button = WaitButtons();
            if (button & WPAD_BUTTON_HOME) ReturnToLoader();

            if (button & WPAD_BUTTON_LEFT) {
                if (selection > 0)
                    selection--;
                else
                    selection = 4;
                if (selection == 3 && (GrandIOSList[found].revs[found2].mode
                        == IOS_NotonNus
                        || GrandIOSList[found].revs[found2].mode
                                == IOS_NoNusstub)) {
                    selection = 2;
                }
            }

            if (button & WPAD_BUTTON_RIGHT) {
                if (selection < 4) {
                    selection++;
                } else {
                    selection = 0;
                }
                if (selection == 3 && (GrandIOSList[found].revs[found2].mode
                        == IOS_NotonNus
                        || GrandIOSList[found].revs[found2].mode
                                == IOS_NoNusstub)) {
                    selection = 4;
                }
            }

            if (button & WPAD_BUTTON_A) {
                printf("\n");
                WiiLightControl(WII_LIGHT_OFF);
                usleep(500000);
                WiiLightControl(WII_LIGHT_ON);
                usleep(500000);
                WiiLightControl(WII_LIGHT_OFF);
                if (selection == 0) {
                    if (Fat_Mount(&fdevList[FAT_SD]) <= 0) printf("Could not load SD Card\n");
                    sprintf(buf, "sd:/IOS%u-64-v%u.wad", iosnr, revision);
                    ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
                    if (ret < 0) {
                        sprintf(buf, "sd:/wad/IOS%u-64-v%u.wad", iosnr, revision);
                        ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
                        if (ret < 0) {
                            sprintf(buf, "sd:/wad/wiimod/IOS%u-64-v%u.wad", iosnr, revision);
                            ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
                            if (ret < 0) {
                                Fat_Unmount(&fdevList[FAT_SD]);
                                break;
                            }
                        }
                    }
                    lastiosused = selection;
                    Fat_Unmount(&fdevList[FAT_SD]);
                    return ret;
                }

                if (selection == 1) {
                    if (Fat_Mount(&fdevList[FAT_USB]) <= 0) printf("Could not load USB Drive\n");
                    sprintf(buf, "usb:/IOS%u-64-v%u.wad", iosnr, revision);
                    ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
                    if (ret < 0) {
                        sprintf(buf, "usb:/wad/IOS%u-64-v%u.wad", iosnr, revision);
                        ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
                        if (ret < 0) {
                            Fat_Unmount(&fdevList[FAT_USB]);
                            break;
                        }
                    }
                    lastiosused = selection;
                    Fat_Unmount(&fdevList[FAT_USB]);
                    return ret;
                }

                if (selection == 2) {
                    if (!ConnectSMB()) {
                        printf("Could not load SMB\n");
                        break;
                    }
                    sprintf(buf, "smb:/IOS%u-64-v%u.wad", iosnr, revision);
                    ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
                    if (ret < 0) {
                        sprintf(buf, "smb:/wad/IOS%u-64-v%u.wad", iosnr, revision);
                        ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
                        if (ret < 0) {
                            CloseSMB();
                            break;
                        }
                    }
                    lastiosused = selection;
                    CloseSMB();
                    return ret;
                }

                if (selection == 3) {
                    lastiosused = selection;
                    return Download_IOS(ios, iosnr, revision);
                }

                if (selection == 4) {
                    lastiosused = 0;
                    return 0;
                }
            }
            if (button & WPAD_BUTTON_B) {
                lastiosused = selection;
                return 0;
            }
            if (button) break;
        }
    }
}

int IosInstallUnpatched(u32 iosVersion, u32 revision) {
    int ret;
    IOS *ios = NULL;
    
    gcprintf("\n Getting IOS%u revision %u...\n", iosVersion, revision);
    ret = get_IOS(&ios, iosVersion, revision);
    if (ret < 0) {
        gcprintf("Error reading IOS into memory\n");
        return ret;
    } else if (ret == 0) {
        return 0;
    }

    gcprintf("Installing IOS %u Rev %u...", iosVersion, revision);
    SpinnerStart();
    ret = install_IOS(ios, false);
    SpinnerStop();
    if (ret < 0) {
        gcprintf("Error: Could not install IOS%u Rev %u\n", iosVersion, revision);
        free_IOS(&ios);
        return ret;
    }
    gcprintf("\n");
    
    free_IOS(&ios);
    return 1;
}

void clearPatchSettings(patchSettings* settings) {
    settings->iosVersion = 36;
    settings->iosRevision = IOS36version;
    settings->esTruchaPatch = false;
    settings->esIdentifyPatch = false;
    settings->nandPatch = false;
    settings->addticketPatch = false;
    settings->setuidcheckPatch = false;
    settings->koreanKeyPatch = false;
    settings->haveFakeSign = false;
    settings->haveNandPerm = false;
    settings->setAHBPROTPatch = false;
    settings->esOtherPatches = 0, settings->diPatches = 0, settings->location
            = 36, settings->newrevision = IOS36version;
}

u16 chooserevision(u32 ver, u16 cur) {
    int i, found = GrandIOSListLookup[ver], low = 0, high = 0;
    u16 any = cur;
    static s32 lastchoice = 0;
    s32 menuSelection = 0;
    u32 button, selected = 0;
    if (found < 0) {
        found = 0;
    }

    if (lastchoice == 3) {
        menuSelection = 0;
    } else if (lastchoice == USHRT_MAX) {
        menuSelection = GrandIOSList[found].cnt + 1;
    } else {
        menuSelection = GrandIOSList[found].cnt + 2;
    }
    while (!selected) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n");
        printf("Select IOS Revision\n");
        printf("%3s 2\n", (menuSelection == 0) ? "-->" : " ");
        for (i = 0; i < GrandIOSList[found].cnt; i += 1) {
            printf("%3s %u\n", (menuSelection == i + 1) ? "-->" : " ", GrandIOSList[found].revs[i].num);
        }
        printf("%3s %u\n", (menuSelection == GrandIOSList[found].cnt + 1)
            ? "-->" : " ", USHRT_MAX);
        printf("\n%3s Any IOS %u\n", (menuSelection == GrandIOSList[found].cnt
                + 2) ? "-->" : " ", any);
        printf("%3s Go by 10s\n", (menuSelection == GrandIOSList[found].cnt + 3)
            ? "-->" : " ");
        printf("%3s Go by 100s\n", (menuSelection == GrandIOSList[found].cnt
                + 4) ? "-->" : " ");
        printf("%3s Go by 1000s\n", (menuSelection == GrandIOSList[found].cnt
                + 5) ? "-->" : " ");
        flushBuffer();
        while (1) {
            button = WaitButtons();
            if (ExitMainThread) return 0;
            if (button & WPAD_BUTTON_B) {
                return 0;
            }
            if (button & WPAD_BUTTON_A) {
                if (menuSelection == 0) cur = 2;
                if (menuSelection == 1) cur = low;
                if (menuSelection == 2) cur = high;
                if (menuSelection > 0 && menuSelection
                        <= GrandIOSList[found].cnt) {
                    cur = GrandIOSList[found].revs[menuSelection - 1].num;
                }
                if (menuSelection == GrandIOSList[found].cnt + 1) cur
                        = USHRT_MAX;
                if (menuSelection == GrandIOSList[found].cnt + 2) cur = any;
                selected = 1;
                if (menuSelection > GrandIOSList[found].cnt + 2) selected = 0;
                break;
            }
            if (button & WPAD_BUTTON_DOWN) {
                menuSelection += 1;
                if (menuSelection > 5 + GrandIOSList[found].cnt) {
                    menuSelection = 0;
                }
                break;
            }
            if (button & WPAD_BUTTON_UP) {
                menuSelection -= 1;
                if (menuSelection < 0) {
                    menuSelection = 5 + GrandIOSList[found].cnt;
                }
                break;
            }
            if (button & WPAD_BUTTON_LEFT) {
                if (menuSelection > GrandIOSList[found].cnt + 1) any
                        -= (int) pow(10, menuSelection
                                - (GrandIOSList[found].cnt + 2));
                break;
            }
            if (button & WPAD_BUTTON_RIGHT) {
                if (menuSelection > GrandIOSList[found].cnt + 1) any
                        += (int) pow(10, menuSelection
                                - (GrandIOSList[found].cnt + 2));
                break;
            }
            if (button & WPAD_BUTTON_HOME) ReturnToLoader();
        }
    }
    lastchoice = cur;
    return cur;
}

int configInstallSettings(patchSettings* settings, int sigPatch,
    int esIdentPatch, int applynandPatch, int chooseloc) {
    static int menuItem = 0;
    get_certs();
    int fakesign = check_fakesig();
    int nandp = check_nandp();
    u32 button = 0;
    if (fakesign == -1036) {
        //This error is ok for what we need to do.
        fakesign = 0;
    }
    u32 ver = settings->iosVersion;
    settings->haveFakeSign = (fakesign >= 0);
    settings->haveNandPerm = (nandp >= 0);

    u16 newrevision = (chooseloc && settings->haveFakeSign)
        ? chooserevision(ver, settings->iosRevision) : settings->iosRevision;
    if (newrevision < 2) {
        return 0;
    }
    settings->newrevision = newrevision;
    if (settings->haveFakeSign && (ver == 9 || ver == 12 || ver == 13 || ver
            == 14 || ver == 15 || ver == 17 || ver > 20)) {
        while (!(button & WPAD_BUTTON_A)) {
            initNextBuffer();
            PrintBanner();
            printf("\n\n\n\t\t\t\tChoose Patches for IOS %u\n", settings->location);
            if (sigPatch == 0) printf("\t\t\t\t%3s Apply FakeSign patch:\t\t\t\t  \x1b[%u;%dm%s\x1b[%u;%dm\n", (menuItem
                    == 0) ? "-->" : " ", ((settings->esTruchaPatch) ? CYAN
                : RED) + 30, 0, (settings->esTruchaPatch) ? "yes" : "no", WHITE
                    + 30, 0);
            if (applynandPatch == 0) printf("\t\t\t\t%3s Apply NAND Permissions patch:\t\t  \x1b[%u;%dm%s\x1b[%u;%dm\n", (menuItem
                    == 1) ? "-->" : " ", ((settings->nandPatch) ? CYAN : RED)
                    + 30, 0, (settings->nandPatch) ? "yes" : "no", WHITE + 30, 0);
            if (esIdentPatch == 0) printf("\t\t\t\t%3s Apply ES_Identify patch:\t\t  \x1b[%u;%dm%s\x1b[%u;%dm\n", (menuItem
                    == 2) ? "-->" : " ", ((settings->esIdentifyPatch) ? CYAN
                : RED) + 30, 0, (settings->esIdentifyPatch) ? "yes" : "no", WHITE
                    + 30, 0);
            printf("\t\t\t\t%3s Apply ticket version check patch:\t\x1b[%u;%dm%s\x1b[%u;%dm\n", (menuItem
                    == 3) ? "-->" : " ", ((settings->addticketPatch) ? CYAN
                : RED) + 30, 0, (settings->addticketPatch) ? "yes" : "no", WHITE
                    + 30, 0);
            printf("\t\t\t\t%3s Apply Setuid check patch:\t\t\t  \x1b[%u;%dm%s\x1b[%u;%dm\n", (menuItem
                    == 4) ? "-->" : " ", ((settings->setuidcheckPatch) ? CYAN
                : RED) + 30, 0, (settings->setuidcheckPatch) ? "yes" : "no", WHITE
                    + 30, 0);
            printf("\t\t\t\t%3s Apply Korean Key patch:\t\t\t  \x1b[%u;%dm%s\x1b[%u;%dm\n", (menuItem
                    == 5) ? "-->" : " ", ((settings->koreanKeyPatch) ? CYAN
                : RED) + 30, 0, (settings->koreanKeyPatch) ? "yes" : "no", WHITE
                    + 30, 0);
            printf("\t\t\t\t%3s Apply Set AHBPROT patch:\t\t  \x1b[%u;%dm%s\x1b[%u;%dm\n", (menuItem
                    == 6) ? "-->" : " ", ((settings->setAHBPROTPatch) ? CYAN
                : RED) + 30, 0, (settings->setAHBPROTPatch) ? "yes" : "no", WHITE
                    + 30, 0);
            SetToInputLegendPos();
            horizontalLineBreak();
            printf("\t(%s)/(%s) (%s)/(%s)\t\tChange Selections\n", UP_ARROW, DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW);
            printf("\t(A)\t\t\t\t\tContinue\t\t\t\t\t(B)\tBack\n");
            printf("\t(HOME)/GC:(START)\t\tExit");
            WIILIGHT_SetLevel(63);
            WIILIGHT_TurnOn();
            flushBuffer();
            button = WaitButtons();
            WiiLightControl(WII_LIGHT_OFF);
            if (ExitMainThread || button & WPAD_BUTTON_B) return 0;
            if (button & WPAD_BUTTON_HOME) ReturnToLoader();
            if (button & WPAD_BUTTON_DOWN) {
                menuItem++;
                if (menuItem > 6) menuItem = 0;
                if (menuItem == 0 && sigPatch > 0) menuItem++;
                if (menuItem == 1 && applynandPatch > 0) menuItem++;
                if (menuItem == 2 && esIdentPatch > 0) menuItem++;
            }
            if (button & WPAD_BUTTON_UP) {
                menuItem--;
                if (menuItem == 2 && esIdentPatch > 0) menuItem--;
                if (menuItem == 1 && applynandPatch > 0) menuItem--;
                if (menuItem == 0 && sigPatch > 0) menuItem--;
                if (menuItem < 0) menuItem = 6;
            }
            if (button & WPAD_BUTTON_LEFT || button & WPAD_BUTTON_RIGHT) {
                if (menuItem == 0) settings->esTruchaPatch
                        = !settings->esTruchaPatch;
                if (menuItem == 1) settings->nandPatch = !settings->nandPatch;
                if (menuItem == 2) settings->esIdentifyPatch
                        = !settings->esIdentifyPatch;
                if (menuItem == 3) settings->addticketPatch
                        = !settings->addticketPatch;
                if (menuItem == 4) settings->setuidcheckPatch
                        = !settings->setuidcheckPatch;
                if (menuItem == 5) {
                    if( !settings->koreanKeyPatch && ( 70 == settings->iosVersion || 80 == settings->iosVersion ) && 'K' != wiiSettings.sysMenuRegion) {
                        PrintBanner();
                        printf("You are trying to apply the korean key patch to IOS %d but your\n", settings->iosVersion);
                        printf("system menu is not korean.\n");
                        printf("This could leave you with a Error 003 brick if you don't know\n");
                        printf("what you are doing.\n");
                        if(PromptContinue()) {
                            settings->koreanKeyPatch = !settings->koreanKeyPatch;
                        }
                    } else {
                        settings->koreanKeyPatch = !settings->koreanKeyPatch;
                    }
                }
                if (menuItem == 6) settings->setAHBPROTPatch
                        = !settings->setAHBPROTPatch;
            }
        }
        if (sigPatch == 2) settings->esTruchaPatch = true;
        if (applynandPatch == 2) settings->nandPatch = true;
        if (esIdentPatch == 2) settings->esIdentifyPatch = true;
        //printf("\nApply other patches to ES_Module in IOS%u?\n", ver);
        //settings->esOtherPatches = PromptYesNo();
        //printf("\nApply patch to DI_Module in IOS%u?\n", ver);
        //settings->diPatch = PromptYesNo();
    }
    return 1;
}

int applyPatches(IOS *ios, patchSettings settings) {
    bool tmd_dirty = false, tik_dirty = false;
    int index;
    tmd *p_tmd = (tmd*) SIGNATURE_PAYLOAD(ios->tmd);
    tmd_content *p_cr = TMD_CONTENTS(p_tmd);
    if (settings.esTruchaPatch || settings.esIdentifyPatch
            || settings.nandPatch || settings.addticketPatch
            || settings.setuidcheckPatch || settings.koreanKeyPatch
            || settings.setAHBPROTPatch || settings.esOtherPatches) {
        index = module_index2(ios, "ES:", 3);
        if (index < 0) {
            gcprintf("Could not identify ES module\n");
            free_IOS(&ios);
            PromptAnyKeyContinue();
            return -1;
        }
        int tmp = 0;
        int cnt = 0;
        printf("Identified ES module\n");

        if (settings.esOtherPatches) {
            gcprintf("Patching Other Patches in ES module(#%u)...", index);
            tmp
                    = patch_ES_module(ios->decrypted_buffer[index], ios->buffer_size[index]);
            gcprintf("\n\t- %u patches applied\n", tmp);
            cnt += tmp;
        }
        if (settings.esTruchaPatch) {
            gcprintf("Patching trucha bug into ES module(#%u)...", index);
            tmp
                    = patch_fake_sign_check(ios->decrypted_buffer[index], ios->buffer_size[index]);
            gcprintf("\n\t- patched %u hash check(s)\n", tmp);
            cnt += tmp;
        }
        if (settings.esIdentifyPatch) {
            gcprintf("Patching ES_Identify in ES module(#%u)...", index);
            tmp
                    = apply_patch("ES Identify check", ios->decrypted_buffer[index], ios->buffer_size[index], identify_check, sizeof(identify_check), identify_patch, sizeof(identify_patch), 2);
            gcprintf("\n\t- patch applied %u time(s)\n", tmp);
            cnt += tmp;
        }
        if (settings.nandPatch) {
            gcprintf("Patching nand permissions in ES module(#%u)...", index);
            tmp
                    = apply_patch("NAND Permission check", ios->decrypted_buffer[index], ios->buffer_size[index], isfs_perms_check, sizeof(isfs_perms_check), e0_patch, sizeof(e0_patch), 2);
            gcprintf("\n\t- patch applied %u time(s)\n", tmp);
            cnt += tmp;
        }
        if (settings.addticketPatch) {
            gcprintf("Patching add ticket version check in ES module(#%u)...", index);
            tmp
                    = apply_patch("Found addticket vers check", ios->decrypted_buffer[index], ios->buffer_size[index], addticket_vers_check, sizeof(addticket_vers_check), e0_patch, sizeof(e0_patch), 0);
            gcprintf("\n\t- patch applied %u time(s)\n", tmp);
            cnt += tmp;
        }
        if (settings.setuidcheckPatch) {
            gcprintf("Patching setuid check in ES module(#%u)...", index);
            tmp
                    = apply_patch("setuid check", ios->decrypted_buffer[index], ios->buffer_size[index], setuid_check, sizeof(setuid_check), setuid_patch, sizeof(setuid_patch), 0);
            gcprintf("\n\t- patch applied %u time(s)\n", tmp);
            cnt += tmp;
        }
        if (settings.setAHBPROTPatch) {
            gcprintf("Patching Set AHBPROT in ES module(#%u)...", index);
            tmp
                    = apply_patch("Set AHBPROT", ios->decrypted_buffer[index], ios->buffer_size[index], es_set_ahbprot_check, sizeof(es_set_ahbprot_check), es_set_ahbprot_patch, sizeof(es_set_ahbprot_patch), 25);
            gcprintf("\n\t- patch applied %u time(s)\n", tmp);
            cnt += tmp;
        }
        if (settings.koreanKeyPatch) {
            gcprintf("Patching Korean Key int ES module(#%u)...", index);

            tmp
                    = apply_patch("Korean Key Check", ios->decrypted_buffer[index], ios->buffer_size[index], koreankey_check, sizeof(koreankey_check), e0_patch, sizeof(e0_patch), 2);
            if (tmp > 0) {
                tmp
                        += apply_patch("Korean Key Data", ios->decrypted_buffer[index], ios->buffer_size[index], koreankey_data, sizeof(koreankey_data), kkey_bin, kkey_bin_size, 0x30);
            }
            gcprintf("\n\t- patch applied %u time(s)\n", tmp);
            cnt += tmp;
        }

        if (cnt > 0) {
            sha1 hash;
            SHA1(ios->decrypted_buffer[index], (u32) p_cr[index].size, hash);
            memcpy(p_cr[index].hash, hash, sizeof hash);
            tmd_dirty = true;
        }
    }
    if (settings.diPatches > 0) {
        //0x6000  = 00 01 40
        index = module_index2(ios, "DI driver:", 10);
        if (index < 0) {
            gcprintf("Could not identify DI module\n");
            free_IOS(&ios);
            return -1;
        }
        int diinstall = 0;
        printf("Identified DI module\n");
        if (settings.diPatches == 1) {
            gcprintf("Patching DI patch in ES module(#%u)...", index);
            diinstall
                    = patch_DI_module(ios->decrypted_buffer[index], ios->buffer_size[index]);
            gcprintf("\n\t- patch applied %u time(s)\n", diinstall);
        }
        if (diinstall > 0) {
            sha1 hash;
            SHA1(ios->decrypted_buffer[index], (u32) p_cr[index].size, hash);
            memcpy(p_cr[index].hash, hash, sizeof hash);
            tmd_dirty = true;
        }
    }

    if (settings.iosVersion != settings.location) {
        change_ticket_title_id(ios->ticket, 1, settings.location);
        change_tmd_title_id(ios->tmd, 1, settings.location);
        tmd_dirty = true;
        tik_dirty = true;
    }
    if (settings.iosRevision != settings.newrevision) {
        p_tmd->title_version = settings.newrevision;
        tmd_dirty = true;
    }
    if (tmd_dirty) {
        gcprintf("Trucha signing the tmd...\n");
        forge_tmd(ios->tmd);
    }
    if (tik_dirty) {
        gcprintf("Trucha signing the ticket..\n");
        forge_tik(ios->ticket);
    }
    return 1;
}

int IosInstall(patchSettings settings) {
    int ret = 0;
    
    IOS *ios = NULL;
    IOS *highios = NULL;
    u32 i;

    if (settings.iosRevision < ios_found[settings.location]
            && ios_clean[settings.location] != 1 && !settings.haveNandPerm) {
        printf("\nNand Patch required, check your IOSs for a valid IOS\n");
        PromptAnyKeyContinue();
        return 0;
    }
    if (settings.esTruchaPatch || settings.esIdentifyPatch
            || settings.nandPatch || settings.addticketPatch
            || settings.setuidcheckPatch || settings.koreanKeyPatch
            || settings.setAHBPROTPatch || settings.esOtherPatches
            || settings.diPatches) {
        if (!settings.haveFakeSign) {
            printf("\nFake Sign Patch required, check your IOSs for a valid IOS\n");
            PromptAnyKeyContinue();
            return 0;
        }
    }

    u64 titleId = TITLE_ID(1, settings.location);
    u16 curversion = ios_found[settings.location];

    gcprintf("Getting IOS %u revision %u installing to %u revision %u...\n", settings.iosVersion, settings.iosRevision, settings.location, settings.newrevision);
    WiiLightControl(WII_LIGHT_ON);
    ret = get_IOS(&ios, settings.iosVersion, settings.iosRevision);
    if (ret < 0) {
        gcprintf("Error reading IOS into memory %d\n", ret);
        WiiLightControl(WII_LIGHT_ON);
        usleep(333000);
        WiiLightControl(WII_LIGHT_OFF);
        usleep(333000);
        WiiLightControl(WII_LIGHT_ON);
        usleep(333000);
        WiiLightControl(WII_LIGHT_OFF);
        usleep(333000);
        WiiLightControl(WII_LIGHT_ON);
        usleep(333000);
        WiiLightControl(WII_LIGHT_OFF);
        PromptAnyKeyContinue();
        return -1;
    } else if (ret == 0) {
        return 0;
    }

    s32 loc = GrandIOSListLookup[settings.iosVersion];
    u32 hash[5];
    SHA1((u8 *) ios->tmd, ios->tmd_size, (unsigned char *) hash);
    for (i = 0; loc >= 0 && i < GrandIOSList[loc].cnt; i += 1) {
        if (GrandIOSList[loc].revs[i].num == settings.iosRevision) {
            if (memcmp((u32 *) GrandIOSList[loc].revs[i].hash, (u32 *) hash, sizeof(sha1))
                    != 0) {
                printf("IOS failed hash check\n");
                PromptAnyKeyContinue();
                free_IOS(&ios);
                return -1;
            }
            break;
        }
    }
    if (curversion > settings.newrevision && (ios_clean[settings.location] != 1
            || (theSettings.DeleteDefault == 1 && settings.haveNandPerm))) {
        printf("Higher version found, removing higher version %d to install new version %u\n", curversion, settings.newrevision);
        ISFS_Initialize();
        ret = Uninstall_FromTitle(titleId);
        if (ret < 0) {
            PromptAnyKeyContinue();
            return ret;
        }
        printf("\n");
        ISFS_Deinitialize();
    }
    if (curversion > settings.newrevision && ios_clean[settings.location] == 1
            && (theSettings.DeleteDefault != 1 || !settings.haveNandPerm)) {
        printf("\nHigher version found, removing higher version %d\n", curversion);
        printf("to install new version %u\n", settings.newrevision);
        printf("Getting IOS %d...\n", settings.location);
        ret = get_IOS(&highios, settings.location, curversion);
        if (ret <= 0) {
            if (ret < 0) {
                gcprintf("Error reading IOS into memory\n");
            }
            free_IOS(&ios);
            ES_AddTitleCancel();
            return ret;
        }
        printf("Installing ticket...\n");
        ret
                = ES_AddTicket(highios->ticket, highios->ticket_size, highios->certs, highios->certs_size, highios->crl, highios->crl_size);
        if (ret < 0) {
            printf("ES_AddTicket returned: %d\n", ret);
            free_IOS(&ios);
            free_IOS(&highios);
            ES_AddTitleCancel();
            return ret;
        }
        ret
                = Downgrade_TMD_Revision(highios->tmd, highios->tmd_size, highios->certs, highios->certs_size);
        if (ret < 0) {
            printf("Error: Could not set the revision to 0\n");
            free_IOS(&ios);
            free_IOS(&highios);
            return ret;
        }
    }

    applyPatches(ios, settings);
    gcprintf("Encrypting IOS...\n");
    encrypt_IOS(ios);

    gcprintf("Preparations complete\n\n");

    gcprintf("Installing...");
    SpinnerStart();
    ret = install_IOS(ios, false);
    SpinnerStop();
    if (ret < 0) {
        free_IOS(&ios);
        if (ret == -1017 || ret == -2011) {
            gcprintf("You need to use an IOS with trucha bug.\n");
            gcprintf("That's what the IOS15 downgrade is good for...\n");
        } else if (ret == -1035) {
            gcprintf("Has your installed IOS%u a higher revison than %u?\n", settings.iosVersion, settings.iosRevision);
        }
        PromptAnyKeyContinue();
        return -1;
    }
    gcprintf("\b.Done\n");

    free_IOS(&ios);
    return 1;
}

s32 Downgrade_TMD_Revision(const signed_blob* ptmd, u32 tmd_size, void* certs,
    u32 certs_size) {
    // The revison of the tmd used as paramter here has to be >= the revision of the installed tmd
    s32 file, ret;
    char *tmd_path = "/tmp/title.tmd";
    u8 *tmd;
    
    gcprintf("Setting the revision to 0...\n");
    
    ret = ES_AddTitleStart(ptmd, tmd_size, certs, certs_size, NULL, 0);
    
    if (ret < 0) {
        gcprintf("ERROR: ES_AddTitleStart: %s\n", EsErrorCodeString(ret));
        //if (ret == -1035) gcprintf("Error: ES_AddTitleStart returned %d, maybe you need an updated Downgrader\n", ret);
        //else gcprintf("Error: ES_AddTitleStart returned %d\n", ret);
        ES_AddTitleCancel();
        return ret;
    }

    ret = ISFS_Initialize();
    if (ret < 0) {
        gcprintf("Error: ISFS_Initialize returned %d\n", ret);
        ES_AddTitleCancel();
        return ret;
    }

    ret = ISFS_Delete(tmd_path);
    if (ret < 0) {
        printf("Error: ISFS_Delete returned %d\n", ret);
        ES_AddTitleCancel();
        ISFS_Deinitialize();
        return ret;
    }
    ret = ISFS_CreateFile(tmd_path, 0, 3, 3, 3);
    if (ret < 0) {
        printf("Error: ISFS_CreateFile returned %d\n", ret);
        ES_AddTitleCancel();
        ISFS_Deinitialize();
        return ret;
    }

    file = ISFS_Open(tmd_path, ISFS_OPEN_RW);
    if (!file) {
        gcprintf("Error: ISFS_Open returned %d\n", file);
        ES_AddTitleCancel();
        ISFS_Deinitialize();
        return -1;
    }

    tmd = (u8 *) ptmd;
    tmd[0x1dc] = 0;
    tmd[0x1dd] = 0;
    
    ret = ISFS_Write(file, (u8 *) ptmd, tmd_size);
    if (!ret) {
        gcprintf("Error: ISFS_Write returned %d\n", ret);
        ISFS_Close(file);
        ES_AddTitleCancel();
        ISFS_Deinitialize();
        return ret;
    }

    ISFS_Close(file);
    ISFS_Deinitialize();
    ES_AddTitleFinish();
    return 1;
}

s32 IosDowngrade(u32 iosVersion, u16 highRevision, u16 lowRevision) {
    gcprintf("\n");
    gcprintf("Preparing downgrade of IOS%u: Using %u to downgrade to %u\n", iosVersion, highRevision, lowRevision);

    int ret;
    IOS *highIos = NULL;
    IOS *lowIos = NULL;

    gcprintf("Getting IOS %u revision %u...\n", iosVersion, highRevision);
    ret = get_IOS(&highIos, iosVersion, highRevision);
    if (ret < 0) {
        gcprintf("Error reading IOS into memory\n");
        return ret;
    } else if (ret == 0) {
        return 0;
    }

    gcprintf("Getting IOS %u revision %u...\n", iosVersion, lowRevision);
    ret = get_IOS(&lowIos, iosVersion, lowRevision);
    if (ret < 0) {
        gcprintf("Error reading IOS into memory\n");
        free_IOS(&highIos);
        return ret;
    } else if (ret == 0) {
        free_IOS(&highIos);
        return 0;
    }

    gcprintf("\n");
    gcprintf("Step 1: Set the revison to 0\n");
    gcprintf("Installing ticket...\n");
    ret
            = ES_AddTicket(highIos->ticket, highIos->ticket_size, highIos->certs, highIos->certs_size, highIos->crl, highIos->crl_size);
    if (ret < 0) {
        gcprintf("ES_AddTicket returned: %d\n", ret);
        free_IOS(&highIos);
        free_IOS(&lowIos);
        ES_AddTitleCancel();
        return ret;
    }

    ret
            = Downgrade_TMD_Revision(highIos->tmd, highIos->tmd_size, highIos->certs, highIos->certs_size);
    if (ret < 0) {
        gcprintf("Error: Could not set the revision to 0\n");
        free_IOS(&highIos);
        free_IOS(&lowIos);
        return ret;
    }

    gcprintf("Revision set to 0, step 1 complete.\n");
    gcprintf("\n");
    gcprintf("Step 2: Install IOS with low revision\n");

    gcprintf("Installing IOS%u Rev %u...", iosVersion, lowRevision);
    SpinnerStart();
    ret = install_IOS(lowIos, true);
    SpinnerStop();
    if (ret < 0) {
        gcprintf("Error: Could not install IOS%u Rev %u\n", iosVersion, lowRevision);
        free_IOS(&highIos);
        free_IOS(&lowIos);
        return ret;
    }
    printf("\b.Done\n");

    gcprintf("IOS%u downgrade to revision: %u complete.\n", iosVersion, lowRevision);
    free_IOS(&highIos);
    free_IOS(&lowIos);

    return 1;
}

s32 IOSMove(patchSettings settings) {
    u64 fromtid = TITLE_ID(1,settings.iosVersion);
    int ret;
    IOS *ios = NULL;

    if (settings.iosRevision < ios_found[settings.location]
            && ios_clean[settings.location] != 1 && !settings.haveNandPerm) {
        printf("\nNand Patch required, check your IOSs for a valid IOS\n");
        return 0;
    }
    if (settings.esTruchaPatch || settings.esIdentifyPatch
            || settings.nandPatch || settings.addticketPatch
            || settings.setuidcheckPatch || settings.koreanKeyPatch
            || settings.setAHBPROTPatch || settings.esOtherPatches
            || settings.diPatches) {
        if (!settings.haveFakeSign) {
            printf("\nFake Sign Patch required, check your IOSs for a valid IOS\n");
            return 0;
        }
    }
    ret = Init_IOS(&ios);

    printf("Moving IOS %d to %d\n", settings.iosVersion, settings.location);
    fflush(stdout);

    tmd *tmd_data = NULL;
    tmd_content *content = NULL;
    u8 key[16];

    u32 cnt = 0;

    printf("Reading Certs... ");
    fflush(stdout);

    ios->certs_size = GetCert2(&ios->certs);
    printf("done\n");
    if (ios->certs_size == 0) {
        printf("Error getting Certs\n");
        return ret;
    }
    printf("Reading Ticket... ");
    fflush(stdout);
    ios->ticket_size = GetTicket2(fromtid, &ios->ticket);
    printf("done\n");
    fflush(stdout);
    if (ios->ticket_size == 0) {
        printf("Error getting Ticket\n");
        return ret;
    }

    printf("Reading TMD... ");
    fflush(stdout);
    ios->tmd_size = GetTMD2(fromtid, &ios->tmd);
    printf("done\n");
    fflush(stdout);
    if (ios->tmd_size == 0) {
        printf("Error getting TMD\n");
        return ret;
    }
    printf("Decrypting AES Title Key... ");
    fflush(stdout);

    get_title_key(ios->ticket, (u8 *) key);
    aes_set_key(key);
    printf("done\n");
    fflush(stdout);

    tmd_data = (tmd *) SIGNATURE_PAYLOAD(ios->tmd);
    u32 len2;

    ret = set_content_count(ios, tmd_data->num_contents);
    if (ret < 0) {
        gcprintf("Out of memory\n");
        ret = -1;
    }

    for (cnt = 0; ret >= 0 && cnt < tmd_data->num_contents; cnt++) {
        content = &tmd_data->contents[cnt];

        /* Encrypted content size */
        ios->buffer_size[cnt] = round_up((u32)content->size, 64);

        ios->encrypted_buffer[cnt] = AllocateMemory(ios->buffer_size[cnt]);
        ios->decrypted_buffer[cnt] = AllocateMemory(ios->buffer_size[cnt]);
        if (!ios->encrypted_buffer[cnt] || !ios->decrypted_buffer[cnt]) {
            printf("Out of memory\n");
            ret = -1;
            break;
        }

        printf("Processing content %u - %02d\t", cnt, content->cid);
        fflush(stdout);

        switch (content->type) {
            case 0x0001:
                len2
                        = GetContent2((u8 **) &ios->encrypted_buffer[cnt], fromtid, content->cid, content->index);
                ret = check_not_0(len2, "Error reading content\n");
                break;
            case 0x8001:
                ret
                        = get_shared2((u8 **) &ios->encrypted_buffer[cnt], content->index, content->hash);
                break;
            default:
                printf("Unknown Content Type  %04x... Aborting\n", content->type);
                fflush(stdout);
                ret = -1;
                break;
        }
        if (ret < 0) {
            return ret;
        }
    }
    printf("Reading file into memory complete.\n");

    printf("Decrypting IOS...\n");
    decrypt_IOS(ios);

    applyPatches(ios, settings);

    if (ret >= 0) {
        gcprintf("Encrypting IOS...\n");
        encrypt_IOS(ios);

        if (ios_found[settings.location] > 0) {
            Uninstall_FromTitle(TITLE_ID(1, settings.location));
        }
        gcprintf("Preparations complete\n\n");

        gcprintf("Installing...");
        SpinnerStart();
        ret = install_IOS(ios, false);
        if (ret >= 0) {
            ret = 1;
        }
        SpinnerStop();
    }
    if (ios) free_IOS(&ios);
    return ret;
}
