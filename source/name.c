/*-------------------------------------------------------------
 
 name.c -- functions for determining the name of a title
 
 Copyright (C) 2009 MrClick
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
#include <gccore.h>
#include <unistd.h>
#include <ogcsys.h>
#include <string.h>
#include <stdio.h>
#include <fat.h>
#include <malloc.h>

#include "name.h"
#include "Settings.h"
#include "title_install.h"
#include "tools.h"

// Max number of entries in the database
#define MAX_DB 		1024

// Max name length
#define MAX_LINE	80

// Contains all title ids (e.g.: "HAC")
char **__db_i;
// Contains all title names (e.g.: "Mii Channel")
char **__db;
// Contains the number of entries in the database
u32 __db_cnt = 0;

// Contains the selected mode for getTitle_Name()
u8 __selected_mode = 0;
// The names of the different modes for getTitle_Name()
const char __modeStrings[][10] = { "SD > NAND", "NAND > SD", " SD only ",
    "NAND only" };

// Contains the selected display mode
u8 __selected_disp = 2;
// The names of the different display modes
const char __dispStrings[][10] = { "TITLE", "SHORT", "FULL" };

s32 loadDatabase(char filename[]) {
    FILE *fdb;
    
    fdb = fopen(filename, "r");
    if (fdb == NULL) return -1;

    // Allocate memory for the database
    __db_i = calloc(MAX_DB, sizeof(char*));
    __db = calloc(MAX_DB, sizeof(char*));
    
    // Define the line buffer. Each line in the db file will be stored here first
    char line[MAX_LINE];
    line[sizeof(line) - 1] = 0;
    
    // Generic char buffer and counter variable
    char byte;
    u32 i = 0;
    
    // Read each character from the file
    do {
        byte = fgetc(fdb);
        // In case a line was longer than MAX_LINE
        if (i == -1) {
            // Read bytes till a new line is hit
            if (byte == 0x0A) i = 0;
            // In case were still good with the line length
        } else {
            // Add the new byte to the line buffer
            line[i] = byte;
            i++;
            // When a new line is hit or MAX_LINE is reached
            if (byte == 0x0A || i == sizeof(line) - 1) {
                // Terminate finished line to create a string
                line[i] = 0;
                // When the line is not a comment or not to short
                if (line[0] != '#' && i > 5) {
                    
                    // Allocate and copy title id to database
                    __db_i[__db_cnt] = calloc(4, sizeof(char*));
                    memcpy(__db_i[__db_cnt], line, 3);
                    __db_i[__db_cnt][3] = 0;
                    // Allocate and copy title name to database
                    __db[__db_cnt] = calloc(i - 4, sizeof(char*));
                    memcpy(__db[__db_cnt], line + 4, i - 4);
                    __db[__db_cnt][i - 5] = 0;
                    
                    // Check that the next line does not overflow the database
                    __db_cnt++;
                    if (__db_cnt == MAX_DB) break;
                }
                // Set routine to ignore all bytes in the line when MAX_LINE is reached
                if (byte == 0x0A)
                    i = 0;
                else
                    i = -1;
            }
        }
    } while (!feof(fdb));

    // Close database file; we are done with it
    fclose(fdb);
    
    return 0;
}

void freeDatabase() {
    u32 i = 0;
    for (; i < __db_cnt; i++) {
        free(__db_i[i]);
        free(__db[i]);
    }
    free(__db_i);
    free(__db);
}

s32 getDatabaseCount() {
    return __db_cnt;
}

s32 addTitleToDatabase(char *tid, char *name) {
    char entry[128];
    snprintf(entry, sizeof(entry), "%3.3s-%s", tid, name);
    
    FILE *fdb;
    fatInitDefault();
    fdb = fopen(dbfile3, "a");
    if (fdb == NULL) return -1;

    if (fseek(fdb, 0, SEEK_END) < 0) {
        fclose(fdb);
        return -2;
    }

    if (fwrite(entry, 1, strnlen(entry, 128), fdb) < 0) {
        fclose(fdb);
        return -3;
    }

    if (fwrite("\n", 1, 1, fdb) < 0) {
        fclose(fdb);
        return -4;
    }

    fclose(fdb);
    printf("Wrote \"%s\" to database\n", entry);
    return 0;
}

s32 getTitle_Name(char* name, TITLE *curTitle) {
    char *buf;

    s32 r = -1;
    // Determine the title's name accoring to the selected naming mode
    switch (__selected_mode) {
        case 0:
            if (!curTitle->failedToReadDB) {
                buf = curTitle->nameDB;
                r = 0;
            } else if (!curTitle->failedToReadBN) {
                buf = curTitle->nameBN;
                r = 1;
            } else {
                buf = curTitle->name00;
                r = 2;
            }
            break;
        case 1:
            if (!curTitle->failedToReadBN) {
                buf = curTitle->nameBN;
                r = 1;
            } else if (!curTitle->failedToRead00) {
                buf = curTitle->name00;
                r = 2;
            } else {
                buf = curTitle->nameDB;
                r = 0;
            }
            break;
        case 2:
            buf = curTitle->nameDB;
            r = 0;
            break;
        case 3:
            if (!curTitle->failedToReadBN) {
                buf = curTitle->nameBN;
                r = 1;
            } else {
                buf = curTitle->name00;
                r = 2;
            }
            break;
    }
    switch (r) {
        // In case a name was found in the database
        case 0:
            snprintf(name, 256, "%s", buf);
            break;
            // In case a name was found in the banner.bin
        case 1:
            snprintf(name, 256, "*%s*", buf);
            break;
            // In case a name was found in the 00000000.app
        case 2:
            snprintf(name, 256, "+%s+", buf);
            break;
            // In case no proper name was found return a ?
        default:
            snprintf(name, 256, "?");
            break;
    }

    return 0;
}

s32 getNameDB(char* name, char* id) {
    // Return fixed values for special entries
    if (strncmp(id, "IOS", 3) == 0) {
        snprintf(name, 256, "Operating System %s", id);
        return 0;
    }
    if (strncmp(id, "MIOS", 3) == 0) {
        snprintf(name, 256, "Gamecube Compatibility Layer");
        return 0;
    }
    if (strncmp(id, "SYSMENU", 3) == 0) {
        snprintf(name, 256, "System Menu");
        return 0;
    }
    if (strncmp(id, "BC", 2) == 0) {
        snprintf(name, 256, "BC");
        return 0;
    }

    // Create an ? just in case the function aborts prematurely
    snprintf(name, 256, "?");
    
    u32 i;
    u8 db_found = 0;
    // Compare each id in the database to the title id
    for (i = 0; i < __db_cnt; i++) {
        if (strncmp(id, __db_i[i], 3) == 0) {
            db_found = 1;
            break;
        }
    }
    
    if (db_found == 0) {
        // Return -1 if no mathcing entry was found
        return -1;
    } else {
        // Get name from database once a matching id was found	
        snprintf(name, 256, __db[i]);

        for (i = 0; i < 256; i += 1) {
            if (name[i] == '\r') {
                name[i] = '\0';
            }
        }
        return 0;
    }
}

s32 getNameBN(char* name, u64 id, bool silent) {
    // Create a string containing the absolute filename
    char file[256];
    sprintf(file, "/title/%08x/%08x/data/banner.bin", TITLE_UPPER(id), TITLE_LOWER(id));

    // Try to open file
    s32 fh = ISFS_Open(file, ISFS_OPEN_READ);
    
    // If a title does not have a banner.bin bail out
    if (fh == -106) return -2;

    // If the file won't open 
    if (fh < 0) return fh;

    // Seek to 0x20 where the name is stored
    ISFS_Seek(fh, 0x20, 0);

    // Read a chunk of 256 bytes from the banner.bin
    u8 *data = memalign(32, 0x100);
    s32 ret = ISFS_Read(fh, data, 0x100);
    // Close the banner.bin
    ISFS_Close(fh);
    if (ret < 0) {
        free(data);
        return -3;
    }

    __convertWiiString(name, data, 0xFF);
    free(data);

    // Job well done
    return 1;
}

//Bushing
s32 getapp(u64 req, u32 *outBuf) {
    //gprintf("\n\tgetBootIndex(%016llx)",req);
    u32 tmdsize;
    u64 tid = req;

    if (ES_GetStoredTMDSize(tid, &tmdsize) < 0) return WII_EINSTALL;

    signed_blob *TMD = (signed_blob *) AllocateMemory(tmdsize);
    if (TMD == NULL) return WII_EINSTALL;
    memset(TMD, 0, tmdsize);
    
    ES_GetStoredTMD(tid, TMD, tmdsize);

    tmd *tmd = SIGNATURE_PAYLOAD(TMD);

    tmd_content *content = &tmd->contents[0];
    *outBuf = content->cid;
    free(TMD);
    return 1;
}

s32 getName00(char* name, u64 id) {
    int lang = CONF_GetLanguage();
    
    //languages
    //0jap
    //2eng
    //4german
    //6french
    //8spanish
    //10italian
    //12dutch

    //find the .app file we need
    u32 app = 0;
    s32 ret = getapp(id, &app);
    if (ret < 1) return ret;
    // Create a string containing the absolute filename
    char file[256];
    snprintf(file, sizeof(file), "/title/%08x/%08x/content/%08x.app", (u32)(id
            >> 32), (u32) id, app);
    
    s32 fh = ISFS_Open(file, ISFS_OPEN_READ);
    
    // If the title does not have 00000000.app bail out
    if (fh == -106) return fh;

    // In case there is some problem with the permission
    if (fh == -102) {
        fh = ISFS_Open(file, ISFS_OPEN_READ);
    }

    if (fh < 0) return fh;

    // Jump to start of the name entries
    ISFS_Seek(fh, 0x9C, 0);
    
    // Read a chunk of 0x22 * 0x2B bytes from 00000000.app
    u8 *data = memalign(32, 2048);
    s32 r = ISFS_Read(fh, data, 0x22 * 0x2B);
    //printf("%s %d\n", file, r);wait_anyKey();
    ISFS_Close(fh);
    if (r < 0) {
        free(data);
        return -4;
    }

    // Take the entries apart
    char str[0x22][0x2B];
    u8 i = 0;
    // Convert the entries to ASCII strings
    for (; i < 0x22; i++) {
        __convertWiiString(str[i], data + (i * 0x2A), 0x2A);
    }

    // Clean up
    free(data);
    
    // Assemble name
    char tempstr[80];
    //first try the language we were trying to get
    if (strlen(str[lang]) > 1) {
        snprintf(name, 256, "%s", str[lang]);

        //if there is a part of the name in () add it
        if (strlen(str[lang + 1]) > 1) {
            snprintf(tempstr, sizeof(tempstr), "%s (%s)", name, str[lang + 1]);
            snprintf(name, 256, "%s", tempstr);
        }
    } else {
        //default to english
        snprintf(name, 256, "%s", str[2]);
        //if there is a part of the name in () add it
        if (strlen(str[3]) > 1) {
            snprintf(tempstr, sizeof(tempstr), "%s (%s)", name, str[3]);
            snprintf(name, 256, "%s", tempstr);
        }
    }
    // Job well done
    return 2;
}

s32 printContent(u64 tid) {
    char dir[256];
    snprintf(dir, sizeof(dir), "/title/%08x/%08x/content", (u32)(tid >> 32), (u32) tid);
    
    u32 num = 64;
    
    static char list[8000] __attribute__((aligned(32)));
    ISFS_ReadDir(dir, list, &num);
    
    char *ptr = list;
    u8 br = 0;
    for (; strlen(ptr) > 0; ptr += strlen(ptr) + 1) {
        printf("\t %-12.12s", ptr);
        br++;
        if (br == 4) {
            br = 0;
            printf("\n");
        }
    }
    if (br != 0) {
        printf("\n");
    }
    
    return num;
}

char *getNamingMode() {
    return (char *) __modeStrings[__selected_mode];
}

void changeNamingMode() {
    // Switch through the 4 modes
    __selected_mode++;
    if (__selected_mode >= 4) __selected_mode = 0;
}

u8 getDispMode() {
    return __selected_disp;
}

char *getDispModeName() {
    return (char *) __dispStrings[__selected_disp];
}

void changeDispMode() {
    // Switch through the 3 modes
    __selected_disp++;
    if (__selected_disp >= 3) {
        __selected_disp = 0;
    }
}

char *titleText(u32 kind, u32 title) {
    static char text[10];
    
    if (kind == 1) {
        // If we're dealing with System Titles, use custom names
        switch (title) {
            case 1:
                snprintf(text, sizeof(text), "%7s", "BOOT2");
                break;
            case 2:
                strcpy(text, "SYSMENU");
                break;
            case 0x100:
                snprintf(text, sizeof(text), "%7s", "BC");
                break;
            case 0x101:
                snprintf(text, sizeof(text), "%7s", "MIOS");
                break;
            default:
                snprintf(text, sizeof(text), "IOS %3u", title);
                break;
        }
    } else {
        // Otherwise, just convert the title to ASCII
        int i = 32, j = 0;
        do {
            u8 temp;
            i -= 8;
            temp = (title >> i) & 0x000000FF;
            if (temp < 32 || temp > 126)
                text[j] = '.';
            else
                text[j] = temp;
            j++;
        } while (i > 0);
        text[4] = 0;
    }
    return text;
}

s32 __convertWiiString(char *str, u8 *data, u32 cnt) {
    u32 i = 0;
    for (; i < cnt; data += 2) {
        u16 *chr = (u16*) data;
        if (*chr == 0) {
            break;
        } else if (*chr >= 0x20 && *chr <= 0x7E) {
            // ignores all but ASCII characters
            str[i] = *chr;
        } else {
            str[i] = '.';
        }
        i++;
    }
    str[i] = 0;

    return 0;
}

void printh(void* p, u32 cnt) {
    u32 i = 0;
    for (; i < cnt; i++) {
        if (i % 16 == 0) printf("\n %04X: ", (u8) i);
        u8 *b = p + i;
        printf("%02X ", *b);
    }
    printf("\n");
}

void printa(void* p, u32 cnt) {
    u32 i = 0;
    for (; i < cnt; i++) {
        if (i % 32 == 0) printf("\n %04X: ", (u8) i);
        u8 *c = p + i;
        if (*c > 0x20 && *c < 0xF0)
            printf("%c", *c);
        else
            printf(".");
    }
    printf("\n");
}
