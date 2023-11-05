#include <malloc.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <unistd.h>

#include "controller.h"
#include "detect_settings.h"
#include "Error.h"
#include "fat.h"
#include "gecko.h"
#include "IOSCheck.h"
#include "IOSPatcher.h"
#include "nand.h"
#include "network.h"
#include "smb.h"
#include "title.h"
#include "title_install.h"
#include "tools.h"
#include "video.h"
#include "wad.h"

/* Variables */
static u8 titleBuf[BLOCK_SIZE] ATTRIBUTE_ALIGN(32);

s32 Title_ReadNetwork(u64 tid, const char *filename, void **outbuf, u32 *outlen) {
    void *buffer = NULL;
    char netpath[ISFS_MAXPATH];
    u32 len;
    s32 ret;
    
    /* Generate network path */
    snprintf(netpath, sizeof(netpath), "%016llx/%s", tid, filename);
    /* Request file */
    ret = Network_Request(netpath, &len);
    if (ret < 0) return ret;
    /* Allocate memory */
    buffer = memalign(32, len);
    if (!buffer) return -1;
    /* Download file */
    ret = Network_Read(buffer, len);
    if (ret != len) {
        free(buffer);
        return -2;
    }

    /* Set values */
    *outbuf = buffer;
    if (outlen) *outlen = len;
    return 0;
}

s32 __Title_SaveContent(tik *p_tik, tmd_content *content, void *buffer, u32 len) {
    char filename[ISFS_MAXPATH];
    s32 fd = -1, ret;
    
    /* Genereate filename */
    snprintf(filename, sizeof(filename), "%08x", content->cid);
    
    /* Create file */
    ret = Nand_CreateFile(p_tik->titleid, filename);
    if (ret >= 0) {
        /* Open file */
        fd = Nand_OpenFile(p_tik->titleid, filename, ISFS_OPEN_WRITE);
        if (fd < 0) {
            ret = fd;
            ret = -1;
        } else {
            /* Write file */
            ret = Nand_WriteFile(fd, buffer, len);
            if (ret != len) {
                ret = -1;
            }
        }
    }
    if (ret > 0) {
        /* Success */
        ret = 0;
    }

    /* Close file */
    if (fd >= 0) Nand_CloseFile(fd);
    return ret;
}

s32 __Title_DownloadContent(tik *p_tik, tmd_content *content) {
    u8 *buffer = NULL;
    char filename[ISFS_MAXPATH];
    u32 len;
    s32 ret;
    
    /* Genereate filename */
    snprintf(filename, sizeof(filename), "%08x", content->cid);
    
    /* Download content file */
    ret = Title_ReadNetwork(p_tik->titleid, filename, (void *) &buffer, &len);
    if (ret < 0) return ret;
    /* Save content */
    ret = __Title_SaveContent(p_tik, content, buffer, len);
    if (ret == 0) ret = 1;
    
    /* Free memory */
    if (buffer) free(buffer);
    return ret;
}

s32 Title_Download(u64 tid, u16 version, signed_blob **p_tik,
    signed_blob **p_tmd) {
    signed_blob * s_tik = NULL, *s_tmd = NULL;
    tik *tik_data = NULL;
    tmd *tmd_data = NULL;
    tmd_content *content;
    char filename[ISFS_MAXPATH];
    s32 cnt, ret;
    
    gcprintf("\r\t\t>> Creating temp directory...");
    /* Create temp dir */
    ret = Nand_CreateDir(tid);

    if (ret == -105) {
        Nand_RemoveDir(tid);
        ret = Nand_CreateDir(tid);
    }
    if (ret >= 0) {
        ClearLine();
        gcprintf("\r\t\t>> Downloading ticket...");
        /* Download ticket */
        ret = Title_ReadNetwork(tid, "cetk", (void *) &s_tik, NULL);
    }
    if (ret >= 0) {
        ClearLine();
        gcprintf("\r\t\t>> Downloading TMD...");
        /* TMD filename */
        if (version) {
            snprintf(filename, sizeof(filename), "tmd.%d", version);
        } else {
            snprintf(filename, sizeof(filename), "tmd");
        }
        /* Download TMD */
        ret = Title_ReadNetwork(tid, filename, (void *) &s_tmd, NULL);
    }
    if (ret >= 0) {
        /* Get ticket/TMD info */
        tik_data = (tik *) SIGNATURE_PAYLOAD(s_tik);
        tmd_data = (tmd *) SIGNATURE_PAYLOAD(s_tmd);
        u32 buttons;
        
        /* Title contents */
        for (cnt = 0; cnt < tmd_data->num_contents; cnt++) {
            content = &tmd_data->contents[cnt];
            ClearLine();
            gcprintf("\t\t>> press B to cancel...\n", content->cid);
            gcprintf("\r\t\t>> Downloading content #%02d...", content->cid);
            /* Download content file */
            ScanPads(&buttons);
            if (buttons & WPAD_BUTTON_B) {
                ret = 0;
                break;
            }
            ret = __Title_DownloadContent(tik_data, content);
            ScanPads(&buttons);
            if (buttons & WPAD_BUTTON_B) {
                ret = 0;
                break;
            }
        }
        if (ret > 0) {
            /* Set pointers */
            *p_tik = s_tik;
            *p_tmd = s_tmd;
            
            ClearLine();
            gcprintf("\r\t\t>> Title downloaded successfully!\n");
            return 1;
        }
    }

    if (ret < 0) {
        gcprintf(" ERROR! (ret = %d)\n", ret);
    }
    /* Free memory */
    if (s_tik) free(s_tik);
    if (s_tmd) free(s_tmd);
    return ret;
}

s32 Title_ExtractWAD(u8 *buffer, signed_blob **p_tik, signed_blob **p_tmd) {
    wadHeader *header = (wadHeader *) buffer;
    signed_blob * s_tik = NULL, *s_tmd = NULL;
    tik *tik_data = NULL;
    tmd *tmd_data = NULL;
    u32 cnt, content_len, offset = 0;
    s32 ret;
    void *p_content;
    tmd_content *content;
    
    /* Move to ticket */
    offset += round_up(header->header_len, 64);
    offset += round_up(header->certs_len, 64);
    offset += round_up(header->crl_len, 64);
    
    gcprintf("\r\t\t>> Extracting ticket...");
    
    /* Copy ticket */
    s_tik = (signed_blob *) memalign(32, header->tik_len);
    if (!s_tik) {
        ret = -1;
    }
    if (ret >= 0) {
        memcpy(s_tik, buffer + offset, header->tik_len);
        /* Move to TMD */
        offset += round_up(header->tik_len, 64);
        ClearLine();
        gcprintf("\r\t\t>> Extracting TMD...");
        
        /* Copy TMD */
        s_tmd = (signed_blob *) memalign(32, header->tmd_len);
        if (!s_tmd) {
            ret = -1;
        }
    }
    if (ret >= 0) {
        memcpy(s_tmd, buffer + offset, header->tmd_len);
        offset += round_up(header->tmd_len, 64);
        
        /* Get ticket/TMD info */
        tik_data = (tik *) SIGNATURE_PAYLOAD(s_tik);
        tmd_data = (tmd *) SIGNATURE_PAYLOAD(s_tmd);
        
        gcprintf("\r\t\t>> Creating temp directory...");
        
        /* Create temp dir */
        ret = Nand_CreateDir(tik_data->titleid);
    }
    if (ret >= 0) {
        /* Title contents */
        for (cnt = 0; cnt < tmd_data->num_contents; cnt++) {
            content = &tmd_data->contents[cnt];
            content_len = round_up(content->size, 64);
            p_content = NULL;
            
            ClearLine();
            gcprintf("\r\t\t>> Extracting content #%02d...", content->cid);
            
            /* Allocate memory */
            p_content = memalign(32, content_len);
            if (!p_content) {
                ret = -1;
                break;
            }

            /* Extract content */
            memcpy(p_content, buffer + offset, content_len);
            
            /* Save content */
            ret
                    = __Title_SaveContent(tik_data, content, p_content,
                        content_len);
            if (ret < 0) {
                free(p_content);
                break;
            }
            /* Free memory */
            free(p_content);

            /* Move to next content */
            offset += content_len;
        }
    }
    if (ret >= 0) {
        /* Set pointers */
        *p_tik = s_tik;
        *p_tmd = s_tmd;
        ClearLine();
        gcprintf("\r\t\t>> Title extracted successfully!\n");
        return 0;
    }

    gcprintf(" ERROR! (ret = %d)\n", ret);
    /* Free memory */
    if (s_tik) free(s_tik);
    if (s_tmd) free(s_tmd);
    return ret;
}

s32 Title_Install(u64 tid, signed_blob *p_tik, signed_blob *p_tmd) {
    signed_blob *p_certs = NULL;
    tmd *tmd_data = NULL;
    u32 certs_len, size, tik_len, tmd_len;
    s32 cnt, ret, cfd = -1, err = 0, fd = -1;
    char filename[ISFS_MAXPATH];
    tmd_content *content;
    
    gcprintf("\t\t\t>> Getting certificates...");
    
    /* Get certificates */
    ret = GetCert2(&p_certs);
    if (ret == 0) {
        gcprintf(" ERROR! (ret = %d)\n", ret);
        return ret;
    }
    certs_len = ret;

    /* Get ticket lenght */
    tik_len = SIGNED_TIK_SIZE(p_tik);
    /* Get TMD length */
    tmd_len = SIGNED_TMD_SIZE(p_tmd);
    /* Get TMD info */
    tmd_data = (tmd *) SIGNATURE_PAYLOAD(p_tmd);
    
    ClearLine();
    gcprintf("\r\t\t>> Installing ticket...");
    
    /* Install ticket */
    ret = ES_AddTicket(p_tik, tik_len, p_certs, certs_len, NULL, 0);
    if (ret < 0) {
        gcprintf("ERROR! ES_AddTicket: %s\n", EsErrorCodeString(ret));
        return ret;
    }

    ClearLine();
    gcprintf("\r\t\t>> Installing title...");
    
    /* Install title */
    ret = ES_AddTitleStart(p_tmd, tmd_len, p_certs, certs_len, NULL, 0);
    if (ret < 0) {
        gcprintf("ERROR! ES_AddTitleStart: %s\n", EsErrorCodeString(ret));
        return ret;
    }

    /* Install contents */
    for (cnt = 0; cnt < tmd_data->num_contents; cnt++) {
        content = &tmd_data->contents[cnt];
        
        ClearLine();
        gcprintf("\r\t\t>> Installing content #%02d...", content->cid);
        
        /* Content filename */
        snprintf(filename, sizeof(filename), "%08x", content->cid);
        
        /* Open content file */
        fd = Nand_OpenFile(tmd_data->title_id, filename, ISFS_OPEN_READ);
        if (fd < 0) {
            gcprintf("ERROR! (ret = %d)\n", fd);
            ret = fd;
            err = -1;
        }
        if (err >= 0) {
            /* Add content */
            cfd = ES_AddContentStart(tmd_data->title_id, content->cid);
            if (cfd < 0) {
                gcprintf("ERROR! ES_AddContentStart: %s\n", EsErrorCodeString(
                    cfd));
                ret = cfd;
                err = -1;
            }
        }
        if (err >= 0) {
            /* Add content data */
            while (1) {
                /* Read content data */
                ret = Nand_ReadFile(fd, titleBuf, BLOCK_SIZE);
                if (ret < 0) {
                    gcprintf(" ERROR! (ret = %d)\n", ret);
                    err = -1;
                    break;
                }
                /* EOF */
                if (!ret) break;

                /* Block size */
                size = ret;
                
                /* Add content data */
                ret = ES_AddContentData(cfd, titleBuf, size);
                if (ret < 0) {
                    gcprintf("ERROR! ES_AddContentData: %s\n",
                        EsErrorCodeString(ret));
                    err = -1;
                    break;
                }
            }
        }
        if (err >= 0) {
            /* Finish content installation */
            ret = ES_AddContentFinish(cfd);
            if (ret < 0) {
                gcprintf("ERROR! ES_AddContentFinish: %s\n", EsErrorCodeString(
                    ret));
                err = -1;
            }
        }
        if (err >= 0) {
            /* Close content file */
            Nand_CloseFile(fd);
        }
    }
    if (err >= 0) {
        ClearLine();
        gcprintf("\r\t\t>> Finishing installation...");
        
        /* Finish title install */
        ret = ES_AddTitleFinish();
        if (ret < 0) {
            gcprintf("ERROR! ES_AddTitleFinish: %s\n", EsErrorCodeString(ret));
            err = -1;
        }
    }
    if (err >= 0) {
        gcprintf(" OK!\n");
        return 0;
    }

    /* Close content file */
    if (fd >= 0) Nand_CloseFile(fd);
    /* Finish content installation */
    if (cfd >= 0) ES_AddContentFinish(cfd);
    /* Cancel title installation */
    ES_AddTitleCancel();
    return ret;
}

s32 Title_Clean(signed_blob *p_tmd) {
    tmd *tmd_data = NULL;
    
    /* Retrieve TMD info */
    tmd_data = (tmd *) SIGNATURE_PAYLOAD(p_tmd);
    
    /* Delete title contents */
    return Nand_RemoveDir(tmd_data->title_id);
}

/* Uninstall_Remove* functions taken from Waninkoko's WAD Manager 1.0 source */
s32 Uninstall_RemoveTitleContents(u64 tid) {
    s32 ret;
    
    /* Remove title contents */
    gcprintf("\t\t- Removing title contents...");
    
    ret = ES_DeleteTitleContent(tid);
    if (ret < 0) {
        gcprintf("\n\tError! ES_DeleteTitleContent: %s\n", EsErrorCodeString(
            ret));
    } else {
        gcprintf(" OK!\n");
    }
    return ret;
}

s32 Uninstall_RemoveTitle(u64 tid) {
    s32 ret;
    /* Remove title */
    gcprintf("\t\t- Removing title...");
    
    ret = ES_DeleteTitle(tid);
    if (ret < 0) {
        gcprintf("\n\tError! ES_DeleteTitle: %s\n", EsErrorCodeString(ret));
    } else {
        printf(" OK!\n");
    }
    return ret;
}

s32 Uninstall_RemoveTicket(u64 tid) {
    static tikview viewdata[0x10] ATTRIBUTE_ALIGN(32);

    u32 cnt, views;
    s32 ret;
    
    gcprintf("\t\t- Removing tickets...");
    
    /* Get number of ticket views */
    ret = ES_GetNumTicketViews(tid, &views);
    if (ret < 0) {
        gcprintf("ERROR! ES_GetNumTicketViews: %s\n", EsErrorCodeString(ret));
        return ret;
    }

    if (!views) {
        gcprintf(" No tickets found!\n");
        return 1;
    } else if (views > 16) {
        gcprintf(" Too many ticket views! (views = %d)\n", views);
        return -1;
    }

    /* Get ticket views */
    ret = ES_GetTicketViews(tid, viewdata, views);
    if (ret < 0) {
        gcprintf(" \n\tERROR! ES_GetTicketViews (ret = %d)\n", ret);
        return ret;
    }

    /* Remove tickets */
    for (cnt = 0; cnt < views; cnt++) {
        ret = ES_DeleteTicket(&viewdata[cnt]);
        if (ret < 0) {
            gcprintf("ERROR! ES_DeleteTicket (View = %d): %s\n", cnt,
                EsErrorCodeString(ret));
            return ret;
        }
    }
    gcprintf(" OK!\n");
    return ret;
}

s32 Uninstall_DeleteTitle(u32 title_u, u32 title_l) {
    s32 ret;
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "/title/%08x/%08x", title_u, title_l);
    
    /* Remove title */
    gcprintf("\t\t- Deleting title file %s...", filepath);
    
    ret = ISFS_Delete(filepath);
    if (ret < 0) {
        gcprintf("\n\tError! ISFS_Delete(ret = %d)\n", ret);
    } else {
        printf(" OK!\n");
    }
    return ret;
}

s32 Uninstall_DeleteTicket(u32 title_u, u32 title_l) {
    s32 ret;
    char filepath[256];
    
    snprintf(filepath, sizeof(filepath), "/ticket/%08x/%08x.tik", title_u,
        title_l);
    /* Delete ticket */
    gcprintf("\t\t- Deleting ticket file %s...", filepath);
    
    ret = ISFS_Delete(filepath);
    if (ret < 0) {
        gcprintf("\n\tTicket delete failed (No ticket?) %d\n", ret);
    } else {
        printf(" OK!\n");
    }
    return ret;
}

s32 Uninstall_FromTitleISFS(const u64 tid) {
    s32 contents_ret, tik_ret, title_ret, ret;
        u32 id = tid & 0xFFFFFFFF, kind = TITLE_UPPER(tid);
        contents_ret = tik_ret = title_ret = ret = 0;

        tik_ret = Uninstall_DeleteTicket(kind, id);
        title_ret = Uninstall_DeleteTitle(kind, id);
        //printf("tik %d title %d\n",tik_ret,title_ret);
        contents_ret = title_ret;
        if (tik_ret < 0 && contents_ret < 0 && title_ret < 0) {
            ret = -1;
        } else if (tik_ret < 0 || contents_ret < 0 || title_ret < 0) {
            ret = 1;
        } else {
            ret = 0;
        }
        return ret;
}

s32 Uninstall_FromTitleES(const u64 tid) {
    s32 contents_ret, tik_ret, title_ret, ret;
    contents_ret = tik_ret = title_ret = ret = 0;

    tik_ret = Uninstall_RemoveTicket(tid);
    contents_ret = Uninstall_RemoveTitleContents(tid);
    title_ret = Uninstall_RemoveTitle(tid);
    //printf("tik %d contents %d title %d\n",tik_ret,contents_ret,title_ret);

    if (tik_ret < 0 && contents_ret < 0 && title_ret < 0) {
        ret = -1;
    } else if (tik_ret < 0 || contents_ret < 0 || title_ret < 0) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

s32 Uninstall_FromTitle(const u64 tid) {
    s32 ret = 0;

#ifdef BRICK_PROTECTION
    u32 thi = TITLE_UPPER(tid);
    u16 system_region = Region_IDs[getRegion()];
    // Don't uninstall System titles if we can't find sysmenu IOS
    if (thi == 1) {
        if (wiiSettings.sysMenuIOS == 0) {
            printf("\tSafety Check! Can't detect Sysmenu IOS, system title deletes disabled\n");
            printf("\tPlease report this to the author\n");
            flushBuffer();
            PromptAnyKeyContinue();
            return -1;
        }
    }

    // Fail for uninstalls of various titles.
    if (tid == TITLE_ID(1, 1))
        printf("\tBrick protection! Can't delete boot2!\n");
    else if (tid == TITLE_ID(1, 2))
        printf("\tBrick protection! Can't delete System Menu!\n");
    else if (tid == TITLE_ID(1, wiiSettings.sysMenuIOS))
        printf("\tBrick protection! Can't delete Sysmenu IOS!\n");
    else if (tid == TITLE_ID(1, IOS_GetVersion()))
        printf("\tBrick protection! Can't delete the currently running IOS!\n");
    else if (tid == TITLE_ID(0x10008, 0x48414B00 | system_region))
        printf("\tBrick protection! Can't delete your region's EULA!\n");
    else if (tid == TITLE_ID(0x10008, 0x48414C00 | system_region))
        printf("\tBrick protection! Can't delete your region's rgnsel!\n");
    else {
#endif
        ret = Uninstall_FromTitleES(tid);
        if( ret < 0 ) {
            ret = Uninstall_FromTitleISFS(tid);
        }
#ifdef BRICK_PROTECTION
    }
#endif
    return ret;
}

int CheckAndRemoveStubs() {
    int _RemoveStub_(int ios) {
        PrintBanner();
        printf("OK to Remove stub IOS %d?\n", ios);
        if (PromptYesNo()) {
            gprintf("\nDeleting Stub IOS %d", ios);
            Uninstall_FromTitle(TITLE_ID(1,ios));
            ClearPermDataIOS(ios);
            return 1;
        }
        return 0;
    }
    
    Nand_Init();
    u32 i;
    int count = 0;
    for (i = 3; i < 255; i += 1) {
        if (ios_isStub[i] > 1) {
            count += _RemoveStub_(i);
        }
    }
    ISFS_Deinitialize();
    return count;
}

int downloadAndInstall(u64 tid, u16 version, char* item) {
    return downloadAndInstallAndChangeVersion(tid, version, item, version, 0);
}

int loadSystemMenuAndInstallAndChangeVersion(int source, u16 version, u16 version2, u32 requiredIOS) {
    char buf[64];
    int ret = -1;
    IOS *ios = NULL;
    bool forgetmd = false;
    SpinnerStart();
    if( source == 0 ) {
        if (Fat_Mount(&fdevList[FAT_SD]) <= 0) {
            printf("Could not load SD Card\n");
        } else {
            sprintf(buf, "sd:/RVL-WiiSystemmenu-v%u.wad", version);
            ret = Wad_Read_into_memory(buf, &ios, 2, version);
            if (ret < 0) {
               sprintf(buf, "sd:/wad/RVL-WiiSystemmenu-v%u.wad", version);
               ret = Wad_Read_into_memory(buf, &ios, 2, version);
               if (ret < 0) {
                   sprintf(buf, "sd:/wad/wiimod/RVL-WiiSystemmenu-v%u.wad", version);
                   ret = Wad_Read_into_memory(buf, &ios, 2, version);
               }
            }
            Fat_Unmount(&fdevList[FAT_SD]);
        }
    } else if ( source == 1 ) {
        if (Fat_Mount(&fdevList[FAT_USB]) <= 0) {
            printf("Could not load USB Drive\n");
        } else {
            sprintf(buf, "usb:/RVL-WiiSystemmenu-v%u.wad", version);
            ret = Wad_Read_into_memory(buf, &ios, 2, version);
            if (ret < 0) {
                sprintf(buf, "usb:/wad/RVL-WiiSystemmenu-v%u.wad", version);
                ret = Wad_Read_into_memory(buf, &ios, 2, version);
            }
            Fat_Unmount(&fdevList[FAT_USB]);
        }
    } else if ( source == 2 ) {
        if (!ConnectSMB()) {
            printf("Could not load SMB\n");
        } else {
            sprintf(buf, "smb:/RVL-WiiSystemmenu-v%u.wad", version);
            ret = Wad_Read_into_memory(buf, &ios, 2, version);
            if (ret < 0) {
                sprintf(buf, "smb:/wad/RVL-WiiSystemmenu-v%u.wad", version);
                ret = Wad_Read_into_memory(buf, &ios, 2, version);
            }
            CloseSMB();
        }
    }
    if( ret > 0 ) {
        ret = -1;
        if( version != version2 ) {
            printf("\nChanging version to %d.\n", version2);
            change_tmd_title_version(ios->tmd, version2);
            forgetmd = true;
        }
        if( requiredIOS != 0 && get_tmd_required_ios(ios->tmd) != TITLE_ID(1, requiredIOS)) {
            printf("\nChanging required IOS to %d.\n", requiredIOS);
            change_tmd_required_ios(ios->tmd, requiredIOS);
            forgetmd = true;
        }
        if(forgetmd) {
            forge_tmd(ios->tmd);
        }
        printf( "Installing System menu v%u. Do NOT turn the power off. This may take awhile...\n", version);
        ret = install_IOS(ios, false);
        free_IOS(&ios);
    }
    SpinnerStop();
    return ret;
}


int downloadAndInstallAndChangeVersion(u64 tid, u16 version, char* item, u16 version2, u32 requiredIOS) {
    static signed_blob *tik = NULL, *tmd = NULL;
    int ret;
    bool forgetmd = false;
    gcprintf("\n\nDownloading the %s %u...\n", item, version);
    SpinnerStart();
    ret = Title_Download(tid, version, &tik, &tmd);
    if (ret < 0) {
        printf("\nError Downloading %s.\n", item);
    } else if (ret > 0) {
        if( version != version2 ) {
            printf("\nChanging version to %d.\n", version2);
            change_tmd_title_version(tmd, version2);
            forgetmd = true;
        }
        if( requiredIOS != 0 && get_tmd_required_ios(tmd) != TITLE_ID(1, requiredIOS)) {
            printf("\nChanging required IOS to %d.\n", requiredIOS);
            change_tmd_required_ios(tmd, requiredIOS);
            forgetmd = true;
        }
        if(forgetmd) {
            forge_tmd(tmd);
        }

        printf(
            "Installing %s. Do NOT turn the power off. This may take awhile...\n",
            item);
        ret = Title_Install(tid, tik, tmd);
    }
    SpinnerStop();
    return ret;
}
