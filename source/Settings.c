#include <ctype.h>
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "detect_settings.h"
#include "fat.h"
#include "IOSCheck.h"
#include "name.h"
#include "Settings.h"
#include "tools.h"

#define CONFIG_SD_FILE_PATH "sd:/apps/wiimod/wiimod.txt"
#define CONFIG_USB_FILE_PATH "usb:/apps/wiimod/wiimod.txt"
#define MAX_FAT_DEVICE_LENGTH   10
#define WAD_ROOT_DIRECTORY  "/wad"

char dbfile[] = "sd:/apps/wiimod/database.txt";
char dbfile2[] = "sd:/database.txt";
char dbfile3[] = "usb:/apps/wiimod/database.txt";
char dbfile4[] = "usb:/database.txt";
char dbfile5[] = "/database.txt";

SETTINGS theSettings;

int GetIntParam(char *);
int GetStartupPath(char *, char *);
int GetStringParam(char *, char *, int);
int ReadConfigFile(char *configFilePath, int usb);

s32 initSettings() {
    theSettings.SUIdentified = -1;
    theSettings.neverSet = 0;
    theSettings.outputios = 1;
    theSettings.AutoLoadIOS = 0;
    theSettings.AutoLoadOnInstall = 0;
    theSettings.DisableSetAHBPROT = 0;
    theSettings.DisableControllerButtonHold = 0;
    theSettings.printiosdetails = 1;
    theSettings.loadoldcsv = 1;
    theSettings.filterwads = 1;
    theSettings.onlyCheckIOSOnDemand = 0;
    theSettings.DeleteDefault = 0;
    theSettings.haveHBC = 0;
    flagOverrideIOSCheck = false;

    memset(ios_ident, -1, 256 * sizeof(int));
    memset(ios_fsign, -1, 256 * sizeof(int));
    memset(ios_flash, -1, 256 * sizeof(int));
    memset(ios_boot2, -1, 256 * sizeof(int));
    memset(ios_usb2m, -1, 256 * sizeof(int));
    memset(ios_nandp, -1, 256 * sizeof(int));
    memset(ios_sysmen, 0, 256 * sizeof(u16));

    u32 hash[5] = { 0x8e669eb1, 0xf72a1f6f, 0x36b19cc6, 0x1743a3f, 0xc860f5cb };//BootMii IOS
    memcpy( hashesSkipIOS[skipHashesCnt++], hash, sizeof(sha1));
    u32 hash2[5] = { 0x58c5d8b1, 0xccaa035b, 0x14d3e159, 0x3209c037, 0xa915552a };//sorg ios 253 NANDEmu
    memcpy( hashesSkipIOS[skipHashesCnt++], hash2, sizeof(sha1));
    u32 hash3[5] = { 0xf636b921, 0x9cfb52b9, 0x54109707, 0xb090454c, 0xf84ecef9 };//cboot252
    memcpy( hashesSkipIOS[skipHashesCnt++], hash3, sizeof(sha1));
    u32 hash4[5] = { 0xe8806745, 0x638de7c2, 0x5d1f5183, 0xf77a26c1, 0xad2f56c1 };//obcd ios 253 bootsneek
    memcpy( hashesSkipIOS[skipHashesCnt++], hash4, sizeof(sha1));
    u32 hash5[5] = { 0xe7db8c44, 0xb68964e1, 0x6debfc12, 0x78d6b148, 0x117a4cf8 }; //BootMii IOS
    memcpy( hashesSkipIOS[skipHashesCnt++], hash5, sizeof(sha1));

    // Default startup folder
    strcpy(theSettings.startupPath, WAD_ROOT_DIRECTORY);

    theSettings.fatDeviceIndex = -1; // Means that user has to select

    memset(theSettings.SMB_USER, 0, 20 * sizeof(char));
    memset(theSettings.SMB_PWD, 0, 40 * sizeof(char));
    memset(theSettings.SMB_SHARE, 0, 40 * sizeof(char));
    memset(theSettings.SMB_IP, 0, 20 * sizeof(char));
    memset(theSettings.hbcRequiredTID, 0, 4 * sizeof(u64));

    int i;
    for( i = 0; i < 256; i += 1 ) {
        skipios[i]=false;
    }

    if( ReadConfigFile(CONFIG_SD_FILE_PATH, 0) < 0 ) {
        ReadConfigFile(CONFIG_USB_FILE_PATH, 1);
    }

    Fat_Mount(&fdevList[FAT_SD]);
    if (loadDatabase(dbfile) < 0) {
        if (loadDatabase(dbfile2) < 0) {
            Fat_Mount(&fdevList[FAT_USB]);
            if (loadDatabase(dbfile3) < 0) {
                if (loadDatabase(dbfile4) < 0) {
                    loadDatabase(dbfile5);
                }
            }
            Fat_Unmount(&fdevList[FAT_USB]);
        }
    }
    Fat_Unmount(&fdevList[FAT_SD]);
    return 0;
}

int ReadConfigFile(char *configFilePath, int usb) {
    FILE *fptr;
    char *tmpStr = malloc(MAX_FILE_PATH_LEN);
    char tmpOutStr[40];

    if (tmpStr == NULL) return -1;

    s32 ret = Fat_Mount(&fdevList[(usb)?FAT_USB:FAT_SD]);

    if(usb) ret = 1;
    if (ret <= 0) {
        printf("Failed to read from sd card (ret = %d)\n", ret);
        ret = -1;
    } else {
        ret = -1;
        // Read the file
        fptr = fopen(configFilePath, "rb");
        if (fptr != NULL) {
            ret = 1;
            // Read the options
            char done = 0;
            int i;

            while (!done) {
                if (fgets(tmpStr, MAX_FILE_PATH_LEN, fptr) == NULL) {
                    done = 1;
                } else if (isalpha((int) tmpStr[0])) {
                    // Get the password
                    char str[60];
                    if (strncmp(tmpStr, "StartupPath", 11) == 0) {// Get startup path
                        // Get startup Path
                        GetStartupPath(theSettings.startupPath, tmpStr);
                    } else if (strncmp(tmpStr, "FatDevice", 9) == 0) { // FatDevice
                        // Get fatDevice
                        GetStringParam(tmpOutStr, tmpStr, MAX_FAT_DEVICE_LENGTH);
                        for (i = 0; i < 5; i++) {
                            if (strncmp(fdevList[i].mount, tmpOutStr, 2) == 0) {
                                theSettings.fatDeviceIndex = i;
                            }
                        }
                    } else if (strncmp(tmpStr, "outputios", 9) == 0) {
                        // Get outputios
                        theSettings.outputios = (u8) GetIntParam(tmpStr);
                    } else if (strncmp(tmpStr, "printiosdetails", 15) == 0) {
                        // Get printiosdetails
                        theSettings.printiosdetails = (u8) GetIntParam(tmpStr);
                    } else if (strncmp(tmpStr, "AutoLoadIOS", 11) == 0) {
                        // Get AutoLoadIOS
                        theSettings.AutoLoadIOS = (u8) GetIntParam(tmpStr);
                    } else if (strncmp(tmpStr, "AutoLoadOnInstall", 17) == 0) {
                        // Get AutoLoadOnInstall
                        theSettings.AutoLoadOnInstall = (u8) GetIntParam(tmpStr);
                    } else if (strncmp(tmpStr, "DisableSetAHBPROT", 17) == 0) {
                        // Get DisableSetAHBPROT
                        theSettings.DisableSetAHBPROT = (u8) GetIntParam(tmpStr);
                    } else if (strncmp(tmpStr, "DisableControllerButtonHold", 27) == 0) {
                        // Get DisableControllerButtonHold
                        theSettings.DisableControllerButtonHold = (u8) GetIntParam(tmpStr);
                    } else if (strncmp(tmpStr, "loadoldcsv", 10) == 0) {
                        // Get loadoldcsv
                        theSettings.loadoldcsv = (u8) GetIntParam(tmpStr);
                    } else if (strncmp(tmpStr, "DeleteDefaultInsteadRemove", 26) == 0) {
                        // Get DeleteDefaultInsteadRemove
                        theSettings.DeleteDefault = (u8) GetIntParam(tmpStr);
                    } else if (strncmp(tmpStr, "skipios", 7) == 0) {
                        // skipios
                        int ios = GetIntParam(tmpStr);
                        if( ios >2 && ios < 255 ) {
                            skipios[ios] = true;
                        }
                    } else if (strncmp(tmpStr, "skiphash", 8) == 0 && skipHashesCnt < 100) {
                        // skiphash
                        snprintf(str, sizeof(str), "%s", tmpStr + 9);
                        sscanf(str, "%x,%x,%x,%x,%x\n", &hashesSkipIOS[skipHashesCnt][0],
                            &hashesSkipIOS[skipHashesCnt][1], &hashesSkipIOS[skipHashesCnt][2], &hashesSkipIOS[skipHashesCnt][3],
                            &hashesSkipIOS[skipHashesCnt][4]);
                        skipHashesCnt++;
                    } else if (strncmp(tmpStr, "SMB_USER", 8) == 0) {
                        // Get SMB_USER
                        snprintf(str, sizeof(str), "%s", tmpStr + 9);
                        snprintf(theSettings.SMB_USER,
                            sizeof(theSettings.SMB_USER), "%s", trim(str));
                    } else if (strncmp(tmpStr, "SMB_PWD", 7) == 0) {
                        // Get SMB_PWD
                        snprintf(str, sizeof(str), "%s", tmpStr + 8);
                        snprintf(theSettings.SMB_PWD,
                            sizeof(theSettings.SMB_PWD), "%s", trim(str));
                    } else if (strncmp(tmpStr, "SMB_SHARE", 9) == 0) {
                        // Get SMB_SHARE
                        snprintf(str, sizeof(str), "%s", tmpStr + 10);
                        snprintf(theSettings.SMB_SHARE,
                            sizeof(theSettings.SMB_SHARE), "%s", trim(str));
                    } else if (strncmp(tmpStr, "SMB_IP", 6) == 0) {
                        snprintf(str, sizeof(str), "%s", tmpStr + 7);
                        snprintf(theSettings.SMB_IP,
                            sizeof(theSettings.SMB_IP), "%s", trim(str));
                    } else if (strncmp(tmpStr, "filterwads", 10) == 0) {
                        // Get filterwads
                        theSettings.filterwads = (u8) GetIntParam(tmpStr);
                    } else if (strncmp(tmpStr, "onlyCheckIOSOnDemand", 20) == 0) {
                        // Get onlyCheckIOSOnDemand
                        theSettings.onlyCheckIOSOnDemand = (u8) GetIntParam(tmpStr);
                    }
                }
            } // EndWhile

            // Close the config file
            fclose(fptr);
        }
        Fat_Unmount(&fdevList[(usb)?FAT_USB:FAT_SD]);
    }

    // Free memory
    free(tmpStr);

    return ret;
} // ReadConfig

int GetIntParam(char *inputStr) {
    int retval = 0;
    int i = 0;
    int len = strlen(inputStr);
    char outParam[40];

    // Find the "="
    while ((inputStr[i] != '=') && (i < len)) {
        i++;
    }
    i++;

    // Get to the first alpha numeric character
    while ((isdigit((int) inputStr[i]) == 0) && (i < len)) {
        i++;
    }

    // Get the string param
    int outCount = 0;
    while ((isdigit((int) inputStr[i])) && (i < len) && (outCount < 40)) {
        outParam[outCount++] = inputStr[i++];
    }
    outParam[outCount] = 0; // NULL terminate
    retval = atoi(outParam);

    return (retval);
} // GetIntParam


int GetStartupPath(char *startupPath, char *inputStr) {
    int i = 0;
    int len = strlen(inputStr);

    // Find the "="
    while ((inputStr[i] != '=') && (i < len)) {
        i++;
    }
    i++;

    // Get to the "/"
    while ((inputStr[i] != '/') && (i < len)) {
        i++;
    }

    // Get the startup Path
    int count = 0;
    while (isascii((int) inputStr[i]) && (i < len) && (inputStr[i] != '\n')
            && (inputStr[i] != '\r') && (inputStr[i] != ' ')) {
        startupPath[count++] = inputStr[i++];
    }
    startupPath[count] = 0; // NULL terminate

    return (0);
} // GetStartupPath

int GetStringParam(char *outParam, char *inputStr, int maxChars) {
    int i = 0;
    int len = strlen(inputStr);

    // Find the "="
    while ((inputStr[i] != '=') && (i < len)) {
        i++;
    }
    i++;

    // Get to the first alpha character
    while ((isalpha((int) inputStr[i]) == 0) && (i < len)) {
        i++;
    }

    // Get the string param
    int outCount = 0;
    while ((isalnum((int) inputStr[i])) && (i < len) && (outCount < maxChars)) {
        outParam[outCount++] = inputStr[i++];
    }
    outParam[outCount] = 0; // NULL terminate

    return (0);
} // GetStringParam
