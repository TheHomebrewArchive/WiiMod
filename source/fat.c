#include <fat.h>
#include <sdcard/gcsd.h>
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fat.h"
#include "video.h"

/* FAT device */
fatDevice *fdev = NULL;

/* FAT device list  */
fatDevice fdevList[] = {
    { "sd", "Wii SD Slot", &__io_wiisd },
    { "usb", "USB Mass Storage Device", &__io_usbstorage },
    { "smb", "SMB Share", &__io_wiisd },
    { "gcsda", "SD Gecko (Slot A)", &__io_gcsda },
    { "gcsdb", "SD Gecko (Slot B)", &__io_gcsdb }, };

#define NB_FAT_DEVICES_PPD      (sizeof(fdevList) / sizeof(fatDevice))

const int NB_FAT_DEVICES = NB_FAT_DEVICES_PPD;

int fatInitDefaulted = 0;

s32 Fat_Mount(fatDevice *dev) {
    s32 retry = 0, fatMounted = 0;

    Fat_Unmount(dev);

    while (!fatInitDefaulted && fatMounted < 1 && (retry < 15)) {
        fatMounted = 0;
        if( 0 != retry ) {
            printf(".");
            flushBuffer();
            sleep(1);
        }
        if( !dev->interface->startup() ) {
            fatMounted = -1;
        }
        if( 0 == fatMounted && !dev->interface->isInserted() ) {
            fatMounted = -2;
        }
        if( 0 == fatMounted && !fatMountSimple(dev->mount, dev->interface) ) {
            fatMounted = -3;
        } else {
            fatMounted = 1;
        }
        if(fatMounted <= 0 && fatInitDefault()) {
            fatMounted = 1;
            fatInitDefaulted = 1;
        }
        retry++;
    }
    return fatMounted;
}

void Fat_Unmount(fatDevice *dev) {
    // Unmount device
    fatUnmount(dev->mount);

    // Shutdown interface
    dev->interface->shutdown();
}
