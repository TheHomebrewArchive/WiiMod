#include <dirent.h>
#include <malloc.h>
#include <ogcsys.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "detect_settings.h"
#include "Error.h"
#include "fat.h"
#include "gecko.h"
#include "key_bin.h"
#include "nand.h"
#include "rijndael.h"
#include "Settings.h"
#include "sha1.h"
#include "title.h"
#include "title_install.h"
#include "tools.h"
#include "wad.h"
#include "video.h"

#define round64(x)      round_up(x,0x40)
#define round32(x)      round_up(x,0x20)
#define round16(x)      round_up(x,0x10)
#define BLOCKSIZE 2048
#define SD_BLOCKSIZE 1024 * 32

static u8 wadBuffer[BLOCK_SIZE] ATTRIBUTE_ALIGN(32);

s32 __Wad_ReadFile(FILE *fp, void *outbuf, u32 offset, u32 len) {
    s32 ret;
    
    /* Seek to offset */
    fseek(fp, offset, SEEK_SET);
    /* Read data */
    ret = fread(outbuf, len, 1, fp);
    if (ret >= 0) {
        ret = 0;
    }

    return ret;
}

s32 __Wad_ReadAlloc(FILE *fp, void **outbuf, u32 offset, u32 len) {
    void *buffer = NULL;
    s32 ret;
    
    /* Allocate memory */
    buffer = memalign(32, len);
    if (!buffer) return -1;

    /* Read file */
    ret = __Wad_ReadFile(fp, buffer, offset, len);
    if (ret < 0) {
        free(buffer);
    } else {
        *outbuf = buffer;
        ret = 0;
    }
    return ret;
}

s32 Wad_Read_into_memory(char *filename, IOS **iosptr, u32 iosnr, u16 revision) {
    s32 ret;
    FILE *fp = NULL;
    wadHeader *header = NULL;

    tmd *tmd_data = NULL;
    u32 cnt, offset = 0;

    ret = Init_IOS(iosptr);
    if (ret < 0) {
        printf("Out of memory\n");
        free_IOS(iosptr);
        return ret;
    }

    fp = fopen(filename, "rb");
    if (!fp) {
        printf("Could not open file: %s\n", filename);
        free_IOS(iosptr);
        return -1;
    }

    /* WAD header */
    ret = __Wad_ReadAlloc(fp, (void *) &header, offset, sizeof(wadHeader));
    if (ret < 0) {
        printf("Error reading the header, ret = %d\n", ret);
        free_IOS(iosptr);
        if (fp) fclose(fp);
        return ret;
    } else {
        offset += round_up(header->header_len, 64);
    }

    if (header->certs_len == 0 || header->tik_len == 0 || header->tmd_len == 0) {
        printf("Error: Certs, ticket and/or tmd has size 0\n");
        printf("Certs size: %u, ticket size: %u, tmd size: %u\n",
            header->certs_len, header->tik_len, header->tmd_len);
        free_IOS(iosptr);
        free(header);
        if (fp) fclose(fp);
        return -1;
    }

    /* WAD certificates */
    (*iosptr)->certs_size = header->certs_len;
    ret = __Wad_ReadAlloc(fp, (void *) &(*iosptr)->certs, offset,
        header->certs_len);
    if (ret < 0) {
        printf("Error reading the certs, ret = %d\n", ret);
        free_IOS(iosptr);
        free(header);
        if (fp) fclose(fp);
        return ret;
    } else {
        offset += round_up(header->certs_len, 64);
    }

    if (!IS_VALID_SIGNATURE((signed_blob *) (*iosptr)->certs)) {
        printf("Error: Bad certs signature!\n");
        free_IOS(iosptr);
        free(header);
        if (fp) fclose(fp);
        return -1;
    }

    /* WAD crl */
    (*iosptr)->crl_size = header->crl_len;
    if (header->crl_len) {
        ret = __Wad_ReadAlloc(fp, (void *) &(*iosptr)->crl, offset,
            header->crl_len);
        if (ret < 0) {
            printf("Error reading the crl, ret = %d\n", ret);
            free_IOS(iosptr);
            free(header);
            if (fp) fclose(fp);
            return ret;
        } else
            offset += round_up(header->crl_len, 64);
    } else {
        (*iosptr)->crl = NULL;
    }

    (*iosptr)->ticket_size = header->tik_len;
    /* WAD ticket */
    ret = __Wad_ReadAlloc(fp, (void *) &(*iosptr)->ticket, offset, header->tik_len);
    if (ret < 0) {
        printf("Error reading the ticket, ret = %d\n", ret);
        free_IOS(iosptr);
        free(header);
        if (fp) fclose(fp);
        return ret;
    } else {
        offset += round_up(header->tik_len, 64);
    }

    if (!IS_VALID_SIGNATURE((signed_blob *) (*iosptr)->ticket)) {
        printf("Error: Bad ticket signature!\n");
        free_IOS(iosptr);
        free(header);
        if (fp) fclose(fp);
        return -1;
    }

    (*iosptr)->tmd_size = header->tmd_len;
    /* WAD TMD */
    ret = __Wad_ReadAlloc(fp, (void *) &(*iosptr)->tmd, offset, header->tmd_len);
    if (ret < 0) {
        printf("Error reading the tmd, ret = %d\n", ret);
        free_IOS(iosptr);
        free(header);
        if (fp) fclose(fp);
        return ret;
    } else {
        offset += round_up(header->tmd_len, 64);
    }
    free(header);

    if (!IS_VALID_SIGNATURE((signed_blob *) (*iosptr)->tmd)) {
        printf("Error: Bad TMD signature!\n");
        free_IOS(iosptr);
        if (fp) fclose(fp);
        return -1;
    }

    /* Get TMD info */
    tmd_data = (tmd *) SIGNATURE_PAYLOAD((*iosptr)->tmd);

    printf("Checking titleid and revision...\n");
    if (TITLE_UPPER(tmd_data->title_id) != 1 || TITLE_LOWER(tmd_data->title_id)
            != iosnr) {
        gcprintf("IOS wad has titleid: %08x%08x but expected was: %08x%08x\n",
            TITLE_UPPER(tmd_data->title_id), TITLE_LOWER(tmd_data->title_id),
            1, iosnr);
        free_IOS(iosptr);
        if (fp) fclose(fp);
        return -1;
    }

    if (tmd_data->title_version != revision) {
        gcprintf("IOS wad has revision: %u but expected was: %u\n",
            tmd_data->title_version, revision);
        free_IOS(iosptr);
        if (fp) fclose(fp);
        return -1;
    }

    ret = set_content_count(*iosptr, tmd_data->num_contents);
    if (ret < 0) {
        gcprintf("Out of memory\n");
        free_IOS(iosptr);
        if (fp) fclose(fp);
        return ret;
    }

    printf("Loading contents %d", tmd_data->num_contents);
    for (cnt = 0; cnt < tmd_data->num_contents; cnt++) {
        printf(".");
        tmd_content *content = &tmd_data->contents[cnt];

        /* Encrypted content size */
        (*iosptr)->buffer_size[cnt] = round_up((u32)content->size, 64);

        (*iosptr)->encrypted_buffer[cnt] = memalign(32, (*iosptr)->buffer_size[cnt]);
        (*iosptr)->decrypted_buffer[cnt] = memalign(32, (*iosptr)->buffer_size[cnt]);

        if (!(*iosptr)->encrypted_buffer[cnt] || !(*iosptr)->decrypted_buffer[cnt]) {
            printf("Out of memory\n");
            free_IOS(iosptr);
            if (fp) fclose(fp);
            return -1;
        }

        ret = __Wad_ReadFile(fp, (*iosptr)->encrypted_buffer[cnt], offset,
            (*iosptr)->buffer_size[cnt]);
        if (ret < 0) {
            printf("Error reading content #%u, ret = %d\n", cnt, ret);
            free_IOS(iosptr);
            if (fp) fclose(fp);
            return ret;
        }

        offset += (*iosptr)->buffer_size[cnt];
    }
    printf("done\n");

    printf("Reading file into memory complete.\n");

    printf("Decrypting IOS...\n");
    decrypt_IOS(*iosptr);

    tmd_content *p_cr = TMD_CONTENTS(tmd_data);
    sha1 hash;
    int i;

    printf("Checking hashes...");
    for (i = 0; i < (*iosptr)->content_count; i++) {
        SHA1((*iosptr)->decrypted_buffer[i], (u32) p_cr[i].size, hash);
        if (memcmp(p_cr[i].hash, hash, sizeof hash) != 0) {
            printf("Wrong hash for content #%u\n", i);
            free_IOS(iosptr);
            if (fp) fclose(fp);
            return -1;
        }
    }
    printf("\n");

    if (fp) fclose(fp);
    return 1;
}

s32 __Wad_GetTitleID(FILE *fp, wadHeader *header, u64 *tid) {
    signed_blob *p_tik = NULL;
    tik *tik_data = NULL;

    u32 offset = 0;
    s32 ret;

    /* Ticket offset */
    offset += round_up(header->header_len, 64);
    offset += round_up(header->certs_len, 64);
    offset += round_up(header->crl_len, 64);

    /* Read ticket */
    ret = __Wad_ReadAlloc(fp, (void *) &p_tik, offset, header->tik_len);
    if (ret < 0) return ret;

    /* Ticket data */
    tik_data = (tik *) SIGNATURE_PAYLOAD(p_tik);

    /* Copy title ID */
    *tid = tik_data->titleid;

    /* Free memory */
    if (p_tik) free(p_tik);

    return ret;
}

void __Wad_FixTicket(signed_blob *p_tik) {
    u8 *data = (u8 *) p_tik;
    u8 *ckey = data + 0x1F1;

    if (*ckey > 1) {
        /* Set common key */
        *ckey = 0;

        /* Fakesign ticket */
        brute_tik((tik *) p_tik);
    }
}

s32 Wad_Install(FILE *fp) {
    wadHeader *header = NULL;
    signed_blob *p_certs = NULL, *p_crl = NULL, *p_tik = NULL, *p_tmd = NULL;
    tmd *tmd_data = NULL;
    tmd_content *content = NULL;

    u32 cnt, idx, len, offset = 0, size, tidh, tidl;
    s32 cfd, ret;

    bool isInstalled = false, autoLoadIOSIsSet = false, wasInstalled = false;

    if( theSettings.AutoLoadIOS > 2 && theSettings.AutoLoadIOS < 255 ) {
        autoLoadIOSIsSet = true;
        isInstalled = (ios_found[theSettings.AutoLoadIOS] > 0);
    }

    printf("\t\t>> Reading WAD data...");

    // WAD header
    ret = __Wad_ReadAlloc(fp, (void *) &header, offset, sizeof(wadHeader));
    if (ret >= 0) {
        offset += round_up(header->header_len, 64);

        // WAD certificates
        ret = __Wad_ReadAlloc(fp, (void *) &p_certs, offset, header->certs_len);
    }

    if (ret >= 0) {
        offset += round_up(header->certs_len, 64);

        // WAD crl
        if (header->crl_len) {
            ret = __Wad_ReadAlloc(fp, (void *) &p_crl, offset, header->crl_len);
            if (ret >= 0) {
                offset += round_up(header->crl_len, 64);
            }
        }
    }

    // WAD ticket
    if (ret >= 0) ret = __Wad_ReadAlloc(fp, (void *) &p_tik, offset,
        header->tik_len);

    if (ret >= 0) {
        offset += round_up(header->tik_len, 64);
        // WAD TMD
        ret = __Wad_ReadAlloc(fp, (void *) &p_tmd, offset, header->tmd_len);
    }

    if (ret >= 0) {
        offset += round_up(header->tmd_len, 64);
        // Fix ticket
        __Wad_FixTicket(p_tik);

        printf("\n\t\t>> Installing ticket...");

        // Install ticket
        ret = ES_AddTicket(p_tik, header->tik_len, p_certs, header->certs_len,
            p_crl, header->crl_len);
    }

    if (ret >= 0) {
        printf("\n\t\t>> Installing title...");
        // Install title
        ret = ES_AddTitleStart(p_tmd, header->tmd_len, p_certs,
            header->certs_len, p_crl, header->crl_len);
    }

    // Get TMD info
    if (ret >= 0) {
        tmd_data = (tmd *) SIGNATURE_PAYLOAD(p_tmd);

        tidh = TITLE_UPPER(tmd_data->title_id);
        tidl = TITLE_LOWER(tmd_data->title_id);

        if (tidh == 1 && tidl < 256) {
            if( autoLoadIOSIsSet && tidl == theSettings.AutoLoadIOS ) {
                wasInstalled = true;
            }
            installedIOSorSM = 1;
        }
    }

    printf("\n");
    // Install contents
    for (cnt = 0; ret >= 0 && cnt < tmd_data->num_contents; cnt++) {
        content = &tmd_data->contents[cnt];
        idx = 0;

        clearLine();
        printf("\t\t>> Installing content #%02d...", content->cid);

        // Encrypted content size
        len = round_up(content->size, 64);

        // Install content
        cfd = ES_AddContentStart(tmd_data->title_id, content->cid);
        if (cfd < 0) {
            ret = cfd;
            break;
        }

        // Install content data
        while (idx < len) {

            // Data length
            size = (len - idx);
            if (size > BLOCK_SIZE) size = BLOCK_SIZE;

            // Read data
            ret = __Wad_ReadFile(fp, &wadBuffer, offset, size);
            if (ret < 0) break;

            // Install data
            ret = ES_AddContentData(cfd, wadBuffer, size);
            if (ret < 0) break;

            // Increase variables
            idx += size;
            offset += size;
        }
        if (ret < 0) {
            break;
        }
        // Finish content installation
        ret = ES_AddContentFinish(cfd);
    }
    if (ret < 0) {
        ES_AddTitleCancel();
    } else {
        printf("\n\t\t>> Finishing installation...");
        ret = ES_AddTitleFinish();
    }
    if (ret < 0) {
        printf("Error: %s\nTry with AHBPROT mode or an IOS with the correct permissions.\n",
            EsErrorCodeString(ret));
    } else {
        printf(" OK!\n");
    }
    fflush(stdout);
    // Free memory
    if (header) free(header);
    if (p_certs) free(p_certs);
    if (p_crl) free(p_crl);
    if (p_tik) free(p_tik);
    if (p_tmd) free(p_tmd);

    if( ret >=0 && !isInstalled && wasInstalled && theSettings.AutoLoadOnInstall ) {
        printf("AutoLoadIOS was installed Reloading %d...\n", theSettings.AutoLoadIOS);
        fflush(stdout);
        Fat_Unmount(fdev);
        ReloadIos(theSettings.AutoLoadIOS);
        Fat_Mount(fdev);
    }

    return ret;
}

s32 Wad_Uninstall(FILE *fp) {
    wadHeader *header = NULL;
    u64 tid = 0;
    u32 tidl, tidh;
    s32 ret;

    printf("\t\t>> Reading WAD data...");

    /* WAD header */
    ret = __Wad_ReadAlloc(fp, (void *) &header, 0, sizeof(wadHeader));
    if (ret >= 0) {
        /* Get title ID */
        ret = __Wad_GetTitleID(fp, header, &tid);
    }
    if (header) {
        free(header);
    }

    if (ret >= 0) {
        ret = Uninstall_FromTitle(tid);
        tidh = TITLE_UPPER(tid);
        tidl = TITLE_LOWER(tid);
        if (tidh == 1 && tidl < 256) {
            installedIOSorSM = 1;
        }
    }
    return ret;
}

void setHeader(wadHeader *header) {
    header->header_len = 0x20;
    header->type = 0x4973;
    header->padding = 0;
    header->certs_len = 0;
    header->crl_len = 0;
    header->tik_len = 0;
    header->tmd_len = 0;
    header->data_len = 0;
    header->footer_len = 0;
}

u32 pad_data_32(u8 *in, u32 len, u8 **out) {
    u32 new_size = round64(len);
    
    u8 *buffer = memalign(32, new_size);
    if (buffer == NULL) {
        return 0;
    }

    memcpy(buffer, in, len);
    memset(buffer + len, 0, new_size - len);

    free(in);
    *out = buffer;
    
    return new_size;
}

u32 read_isfs(char *path, u8 **out) {
    fstats *status;
    u32 size;
    s32 ret, fd = ISFS_Open(path, ISFS_OPEN_READ);
    if (fd < 0) {
        printf("ISFS_OPEN for %s failed %d\n", path, fd);
        return 0;
    }
    status = AllocateMemory(sizeof(fstats));
    if (status == NULL) {
        printf("Error allocating memory for status\n");
        return 0;
    }
    ret = ISFS_GetFileStats(fd, status);
    if (ret < 0) {
        printf("\nISFS_GetFileStats(fd) returned %d\n", ret);
        if(status) free(status);
        ISFS_Close(fd);
        return 0;
    }
    u32 fullsize = status->file_length;
    u8 *out2 = AllocateMemory(fullsize);
    if (out2 == NULL) {
        printf("Error allocating memory for out2\n");
        free(status);
        ISFS_Close(fd);
        return 0;
    }
    u32 restsize = status->file_length;
    u32 writeindex = 0;
    while (restsize > 0) {
        if (restsize >= BLOCKSIZE) {
            size = BLOCKSIZE;
        } else {
            size = restsize;
        }
        ret = ISFS_Read(fd, &(out2[writeindex]), size);
        if (ret < 0) {
            printf("\nISFS_Read(%d, %d) returned %d\n", fd, size, ret);
            free(status);
            ISFS_Close(fd);
            return 0;
        }
        writeindex = writeindex + size;
        restsize -= size;
    }
    free(status);
    ISFS_Close(fd);
    *out = out2;
    return fullsize;
}

u32 GetISFSFile(char *path, signed_blob **ptr) {
    u8 *buffer;
    u32 size = read_isfs(path, &buffer);
    *ptr = (signed_blob *) buffer;
    return size;
}

u32 GetISFSFileAndWrite(FILE *f, char *path, signed_blob **ptr) {
    u32 ret = GetISFSFile(path, ptr);
    if( ret == 0 ) return ret;
    u32 size2 = pad_data_32((u8 *)*ptr, ret, (u8 **)ptr);
    fwrite((u8 *)*ptr, 1, size2, f);
    return ret;
}

u32 GetCert(FILE *f) {
    char path[ISFS_MAXPATH];
    u8 *buffer = NULL;
    snprintf(path, sizeof(path), "/sys/cert.sys");
    u32 ret = GetISFSFileAndWrite(f, path, (signed_blob **)&buffer);
    if(ret > 0 && buffer) free(buffer);
    return ret;
}

u32 GetCert2(signed_blob **cert) {
    char path[ISFS_MAXPATH];
    snprintf(path, sizeof(path), "/sys/cert.sys");
    return GetISFSFile(path, cert);
}

u32 GetTicket(FILE *f, u64 id, signed_blob **tik) {
    char path[ISFS_MAXPATH];
    snprintf(path, sizeof(path), "/ticket/%08x/%08x.tik", TITLE_UPPER(id),
        TITLE_LOWER(id));
    return GetISFSFileAndWrite(f, path, tik);
}

u32 GetTicket2(u64 id, signed_blob **tik) {
    char path[ISFS_MAXPATH];
    snprintf(path, sizeof(path), "/ticket/%08x/%08x.tik", TITLE_UPPER(id),
        TITLE_LOWER(id));
    return GetISFSFile(path, tik);
}

u32 GetTMD(FILE *f, u64 id, signed_blob **tmd) {
    char path[ISFS_MAXPATH];
    snprintf(path, sizeof(path), "/title/%08x/%08x/content/title.tmd",
        TITLE_UPPER(id), TITLE_LOWER(id));
    return GetISFSFileAndWrite(f, path, tmd);
}

u32 GetTMD2(u64 id, signed_blob **tmd) {
    char path[ISFS_MAXPATH];
    snprintf(path, sizeof(path), "/title/%08x/%08x/content/title.tmd",
        TITLE_UPPER(id), TITLE_LOWER(id));
    return GetISFSFile(path, tmd);
}

bool MakeDir(const char *Path) {
    //logfile("makedir path = %s\n", Path);
    // Open Target Folder
    DIR* dir = opendir(Path);
    int ret;

    // Already Exists?
    if (dir == NULL) {
        // Create
        mode_t Mode = 0777;
        ret = mkdir(Path, Mode);
        printf("mkdir %s == %d\n", Path, ret);
        sleep(7);
        // Re-Verify
        closedir(dir);
        dir = opendir(Path);
        if (dir == NULL) return false;
    }

    // Success
    closedir(dir);
    return true;
}

bool create_folders(char *path) {
    // Creates the required folders for a filepath
    // Example: Input "sd:/BlueDump/00000001/test.bin" creates "sd:/BlueDump" and "sd:/BlueDump/00000001"
    char *last = strrchr(path, '/');
    char *next = strchr(path, '/');
    if (last == NULL) {
        return true;
    }
    char buf[256];
    
    while (next != last) {
        next = strchr((char *) (next + 1), '/');
        strncpy(buf, path, (u32)(next - path));
        buf[(u32)(next - path)] = 0;
        if (!MakeDir(buf)) {
            return false;
        }
    }
    return true;
}

u32 GetContent2(u8 **out, u64 id, u16 content, u16 index) {
    char path[ISFS_MAXPATH];
    u8 *buffer;
    u32 size, size2;

    snprintf(path, sizeof(path), "/title/%08x/%08x/content/%08x.app",
        TITLE_UPPER(id), TITLE_LOWER(id), content);
    printf("Adding regular content...\n");
    size = read_isfs(path, &buffer);
    if (size == 0) {
        printf("Reading content failed, size = 0\n");
        return 0;
    }
    size2 = pad_data_32(buffer, size, &buffer);
    if (size2 == 0) {
        printf("Failed to allocate memory %d\n", size);
        return 0;
    }

    encrypt_buffer(index, buffer, *out, size2);
    free(buffer);
    printf("Adding content done\n");
    return size2;
}

u32 GetContent(FILE *f, u64 id, u16 content, u16 index, bool shared,
    wadHeader *header) {
    char path[ISFS_MAXPATH];
    u8 *buffer;
    u8 *encryptedcontentbuf;
    u32 size, size2;
    
    if (shared) {
        snprintf(path, sizeof(path), "/shared1/%08x.app", content);
        printf("Adding shared content...\n");
    } else {
        snprintf(path, sizeof(path), "/title/%08x/%08x/content/%08x.app",
            TITLE_UPPER(id), TITLE_LOWER(id), content);
        printf("Adding regular content...\n");
    }
    size = read_isfs(path, &buffer);
    if (size == 0) {
        printf("Reading content failed, size = 0\n");
        return 0;
    }

    size2 = pad_data_32(buffer, size, &buffer);
    if (size2 == 0) {
        printf("Failed to allocate memory %d\n", size);
        return 0;
    }
    encryptedcontentbuf = AllocateMemory(size2);
    if (encryptedcontentbuf == NULL) {
        printf("Error encryptedcontentbuf was NULL\n");
        return 0;
    }
    encrypt_buffer(index, buffer, encryptedcontentbuf, size2);
    free(buffer);
    u32 writeindex = 0;
    u32 restsize = size2;
    while (restsize > 0) {
        if (restsize >= SD_BLOCKSIZE) {
            fwrite(&(encryptedcontentbuf[writeindex]), 1, SD_BLOCKSIZE, f);
            restsize = restsize - SD_BLOCKSIZE;
            writeindex = writeindex + SD_BLOCKSIZE;
        } else {
            fwrite(&(encryptedcontentbuf[writeindex]), 1, restsize, f);
            restsize = 0;
        }
    }
    free(encryptedcontentbuf);
    header->data_len += size2;
    printf("Adding content done\n");
    return size2;
}

int check_not_0(size_t ret, char *error) {
    if (ret == 0) {
        printf(error);
        return -1;
    }
    return 1;
}

int get_shared2(u8 **outbuf, u16 index, sha1 hash) {
    u32 i;
    s32 ret;
    fstats *status = AllocateMemory(sizeof(fstats));
    printf("Adding shared content...");
    s32 fd = ISFS_Open("/shared1/content.map", ISFS_OPEN_READ);
    ret = ISFS_GetFileStats(fd, status);
    if (ret < 0) {
        printf("\nISFS_GetFileStats(fd) returned %d\n", ret);
        free(status);
        return -1;
    }
    u32 fullsize = status->file_length;
    u8 *out2 = AllocateMemory(fullsize);
    u32 restsize = status->file_length;
    u32 writeindex = 0;
    u32 size = 0;
    while (restsize > 0) {
        if (restsize >= BLOCKSIZE) {
            size = BLOCKSIZE;
        } else {
            size = restsize;
        }
        ret = ISFS_Read(fd, &(out2[writeindex]), size);
        if (ret < 0) {
            printf("\nISFS_Read(%d, %d) returned %d\n", fd, size, ret);
            free(status);
            return -1;
        }
        writeindex = writeindex + size;
        restsize -= size;
    }
    ISFS_Close(fd);
    bool found = false;
    for (i = 8; i < fullsize; i += 0x1C) {
        if (memcmp(out2 + i, hash, 20) == 0) {
            char path[ISFS_MAXPATH];
            snprintf(path, sizeof(path), "/shared1/%.8s.app", (out2 + i) - 8);
            u8 *out;
            u32 size_out = read_isfs(path, &out);
            if( size_out ==0 ) {
                continue;
            }
            u32 size2 = pad_data_32(out, size_out, &out);
            encrypt_buffer(index, out, *outbuf, size2);
            free(out);
            found = true;
            free(out2);
            break;
        }
    }
    if (found == false) {
        printf("\nCould not find the shared content, no hash did match !\n");
        return -1;
    }
    printf("done\n");
    return 1;
}

int get_shared(FILE *f, u16 index, sha1 hash, wadHeader *header) {
    u32 i;
    s32 ret;
    fstats *status = AllocateMemory(sizeof(fstats));
    printf("Adding shared content...");
    s32 fd = ISFS_Open("/shared1/content.map", ISFS_OPEN_READ);
    ret = ISFS_GetFileStats(fd, status);
    if (ret < 0) {
        printf("\nISFS_GetFileStats(fd) returned %d\n", ret);
        free(status);
        return -1;
    }
    u32 fullsize = status->file_length;
    u8 *out2 = AllocateMemory(fullsize);
    u32 restsize = status->file_length;
    u32 writeindex = 0;
    u32 size = 0;
    while (restsize > 0) {
        if (restsize >= BLOCKSIZE) {
            size = BLOCKSIZE;
        } else {
            size = restsize;
        }
        ret = ISFS_Read(fd, &(out2[writeindex]), size);
        if (ret < 0) {
            printf("\nISFS_Read(%d, %d) returned %d\n", fd, size, ret);
            free(status);
            return -1;
        }
        writeindex = writeindex + size;
        restsize -= size;
    }
    ISFS_Close(fd);
    bool found = false;
    for (i = 8; i < fullsize; i += 0x1C) {
        if (memcmp(out2 + i, hash, 20) == 0) {
            char path[ISFS_MAXPATH];
            snprintf(path, sizeof(path), "/shared1/%.8s.app", (out2 + i) - 8);
            u8 *out;
            u32 size_out = read_isfs(path, &out);
            if(size_out ==0 ){
                continue;
            }
            u32 size2 = pad_data_32(out, size_out, &out);
            u8 *encryptedcontentbuf = AllocateMemory(size2);
            if (encryptedcontentbuf == NULL) {
                printf("\nError encryptedcontentbuf was NULL\n");
                return -1;
            }
            encrypt_buffer(index, out, encryptedcontentbuf, size2);
            free(out);
            writeindex = 0;
            restsize = size2;
            while (restsize > 0) {
                if (restsize >= SD_BLOCKSIZE) {
                    fwrite(&(encryptedcontentbuf[writeindex]), 1, SD_BLOCKSIZE, f);
                    restsize = restsize - SD_BLOCKSIZE;
                    writeindex = writeindex + SD_BLOCKSIZE;
                } else {
                    fwrite(&(encryptedcontentbuf[writeindex]), 1, restsize, f);
                    restsize = 0;
                }
            }
            header->data_len += size2;
            found = true;
            free(out2);
            free(encryptedcontentbuf);
            break;
        }
    }
    if (found == false) {
        printf("\nCould not find the shared content, no hash did match !\n");
        return -1;
    }
    printf("done\n");
    return 1;
}

s32 DumpIOStoSD(u8 loc) {
    PrintBanner();
    char testid[256];
    int ret = Fat_Mount(&fdevList[FAT_SD]);
    if (ret < 0) {
        return ret;
    }
    Nand_Init();
    snprintf(testid, sizeof(testid), "sd:/WAD/wiimod/IOS%d-64-v%u.wad",
        loc, ios_found[loc]);
    ret = Wad_Dump(TITLE_ID(1, loc), testid);
    ISFS_Deinitialize();
    Fat_Unmount(&fdevList[FAT_SD]);
    PromptAnyKeyContinue();
    return ret;
}

s32 Wad_Dump(u64 id, char *path) {
    wadHeader *header = AllocateMemory(sizeof(wadHeader));
    if (header == NULL) {
        printf("Error allocating memory for wadheader\n");
        return -1;
    }
    setHeader(header);
    
    /*if (id ==TITLE_ID(1, 0) || id ==TITLE_ID(1, 1) || id == TITLE_ID(1, 2)) {
     printf("\tCan't be done.\n"); //boot2 and system menu
     return -1;
     }*/

    printf("Started WAD Packing...\nPacking Title %08x-%08x\n",
        TITLE_UPPER(id), TITLE_LOWER(id));
    
    signed_blob *p_tik = NULL;
    signed_blob *p_tmd = NULL;
    
    tmd *tmd_data = NULL;
    u8 key[16];
    
    u32 cnt = 0;

    FILE *wadout;
    printf("WAD_Dump path = %s\n", path);
    if (!create_folders(path)) {
        printf("Error creating folder(s) for '%s'\n", path);
        return -1;
    }

    wadout = fopen(path, "wb");
    if (!wadout) {
        printf("fopen error %s\n", path);
        return -1;
    }

    u8 *padding_table = AllocateMemory(64);
    if (padding_table == NULL) {
        printf("Out of memory\n");
        fclose(wadout);
        return -1;
    }

    memset(padding_table, 0, 64);
    fwrite(padding_table, 1, 64, wadout);
    free(padding_table);

    printf("Reading Certs... ");
    fflush(stdout);
    
    header->certs_len = GetCert(wadout);
    printf("done\n");
    if (header->certs_len == 0) {
        printf("Error getting Certs\n");
        fclose(wadout);
        return header->certs_len;
    }
    printf("Reading Ticket... ");
    header->tik_len = GetTicket(wadout, id, &p_tik);
    printf("done\n");
    if (header->tik_len == 0) {
        printf("Error getting Ticket\n");
        fclose(wadout);
        return header->tik_len;
    }
    printf("Reading TMD... ");
    header->tmd_len = GetTMD(wadout, id, &p_tmd);
    printf("done\n");
    if (header->tmd_len == 0) {
        printf("Error getting TMD\n");
        fclose(wadout);
        return header->tmd_len;
    }
    printf("Decrypting AES Title Key... ");

    get_title_key(p_tik, (u8 *) key);
    aes_set_key(key);
    printf("done\n");
    
    tmd_data = (tmd *) SIGNATURE_PAYLOAD(p_tmd);
    int ret = 0;
    SpinnerStart();
    for (cnt = 0; cnt < tmd_data->num_contents; cnt++) {
        printf("Processing content %u of %u\n", cnt + 1, tmd_data->num_contents);
        tmd_content *content = &tmd_data->contents[cnt];
        
        u32 len2 = 0;
        
        u16 type = 0;
        
        type = content->type;
        switch (type) {
            case 0x0001:
                len2 = GetContent(wadout, id, content->cid, content->index,
                    false, header);
                ret = check_not_0(len2, "Error reading content\n");
                break;
            case 0x8001:
                ret = get_shared(wadout, content->index, content->hash, header);
                break;
            default:
                printf("Unknown Content Type  %04x... Aborting\n", type);
                ret = -1;
                break;
        }
        if (ret <= 0) {
            SpinnerStop();
            fclose(wadout);
            return ret;
        }
    }
    SpinnerStop();
    printf("Adding Header... ");
    fseek(wadout, 0, SEEK_SET);
    
    fwrite((u8 *) header, 1, 0x20, wadout);
    printf("done\n");
    fclose(wadout);
    free(header);
    return 1;
}
