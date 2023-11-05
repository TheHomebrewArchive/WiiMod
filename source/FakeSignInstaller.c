#include <stdio.h>
#include <wiiuse/wpad.h>

#include "detect_settings.h"
#include "FakeSignInstaller.h"
#include "fat.h"
#include "gecko.h"
#include "IOSCheck.h"
#include "IOSPatcher.h"
#include "tools.h"
#include "video.h"

int FakeSignInstall(void) {
    int ret;
    PrintBanner();
    // Let's see if we are already on IOS36, if not then reload to IOS36
    if (IOS_GetVersion() != 36) {
        gcprintf("Current IOS is not 36. Loading IOS 36...\n");
        ret = ReloadIos(36);
        
        if (ret < 0) {
            gcprintf("ERROR: Failed to load IOS 36 (%d)", ret);
            return ret;
        }
    }
    gprintf("Getting certs.sys from the NAND to test for FakeSign...");
    ret = get_certs();
    if (ret < -1) {
        gcprintf("Error (%d)\n", ret);
        return ret;
    }
    gprintf("\n");

    // Lets see if version is already fakesigned
    gprintf("Checking FakeSign on IOS36\n");
    ret = check_fakesig();
    if (ret >= 0) {
        gcprintf("\n\nIOS 36 already has FakeSign applied. ret = %d\n", ret);
        return -1;
    }
    gprintf("CheckFakeSign = %d", ret);

    if (wiiSettings.sysMenuIOS > 3 && wiiSettings.sysMenuIOS < 255) {
        ret = ReloadIos(wiiSettings.sysMenuIOS);
        if (ret < 0) {
            gcprintf("\nERROR: Failed to Load IOS %d (%d)", wiiSettings.sysMenuIOS, ret);
            return ret;
        }
    } else {
        gcprintf("\nERROR: Failed to find usable system menu ios.\n");
        return -1;
    }

    printf("IOS 15 is currently %u\n", ios_found[15]);
    u16 curIOSversion = ios_found[15];
    // Downgrade IOS 15
    if (curIOSversion != 257) {
        gcprintf("*** Downgrading IOS 15 to r257 ***\n");
        ret = IosDowngrade(15, curIOSversion, 257);
        if (ret <= 0) {
            if (ret < 0) gcprintf("ERROR: Downgrade IOS 15 Failed (%d)\n", ret);
            return ret;
        }
        ios_found[15] = 257;
    }

    // Install IOS36 but ask if they want ES_Identity and NAND Patches installed.
    gcprintf("\n*** Installing IOS 36 (r%d) w/FakeSign ***\n", IOS36version);
    // Load IOS 15
    gcprintf("Loading IOS15 (r257)...");
    SpinnerStart();
    ret = ReloadIos(15);
    SpinnerStop();
    if (ret < 0) {
        printf("\nERROR: Failed to Load IOS 15 (%d)", ret);
        return -1;
    }
    gcprintf("\b.Done\n");
    
    printf("\nApply ES_Identify Patch to IOS36?\n");
    bool esIdentifyPatch = PromptYesNo();
    
    printf("\nApply NAND Permission Patch to IOS36?\n");
    bool nandPatch = PromptYesNo();
    
    printf("\nDo you want to change the install location to IOS 236?\n");
    bool installto236 = PromptYesNo();

    gcprintf("\n");

    patchSettings settings;
    clearPatchSettings(&settings);
    settings.iosVersion = 36;
    settings.iosRevision = IOS36version;
    settings.esTruchaPatch = true;
    settings.esIdentifyPatch = esIdentifyPatch;
    settings.nandPatch = nandPatch;
    settings.newrevision = IOS36version;
    settings.haveFakeSign = true;
    if (installto236) {
        settings.location = 236;
    } else {
        settings.location = 36;
    }

    ret = IosInstall(settings);
    if (ret <= 0) {
        if (ret < 0) gcprintf("Failed to install IOS %d (%d)", settings.location, ret);
        return ret;
    }

    // Load IOS 36 to restore IOS 15
    gcprintf("\n*** Restoring IOS 15 (r%d) ***\n", curIOSversion);
    gcprintf("Loading IOS %d (r%d)...", settings.location, IOS36version);
    SpinnerStart();
    ret = ReloadIos(settings.location);
    SpinnerStop();
    if (ret < 0) {
        gcprintf("ERROR: Failed to Load IOS %d (%d)", settings.location, ret);
        return ret;
    }
    gcprintf("\b.Done\n");
    
    if (curIOSversion != 257) {
        ret = IosInstallUnpatched(15, curIOSversion);
        if (ret <= 0) {
            if (ret < 0) gcprintf("Failed to reinstall IOS 15. (%d)", ret);
            return ret;
        }
    }

    ios_found[settings.location] = IOS36version;
    ios_fsign[settings.location] = 1;
    ios_ident[settings.location] = (esIdentifyPatch) ? 1 : -1;
    ios_nandp[settings.location] = (nandPatch) ? 1 : -1;
    updateTitles();
    ClearScreen();
    PrintBanner();
    printf("Installation of IOS %d (r%d) w/FakeSign was completed successfully!!!\n", settings.location, IOS36version);
    printf("You may now use IOS %d to install anything else.\n", settings.location);
    PromptAnyKeyContinue();
    return 1;
}

int onclickFakeSignInstall(void) {
    int ret, loadsd = 0, loadusb = 0;
    initNextBuffer();
    PrintBanner();
    printf("Are you sure you would like to install an IOS that accepts fakesigning?...\n");
    flushBuffer();
    if (!PromptYesNo()) {
        return 0;
    }
    if (ios_found[15] != 257) {
        if (Fat_Mount(&fdevList[FAT_SD])) {
            loadsd = 1;
        } else {
            printf("sd load error\n");
        }
        if(Fat_Mount(&fdevList[FAT_USB])){
            loadusb = 1;
        }
        printf("Downgrading IOS 15...");
        ret = IosDowngrade(15, ios_found[15], 257);
        if (ret <= 0) {
            if (ret < 0) {
                printf("Downgrade failed. Exiting...");
                ret = -1;
            }
            if (loadsd) Fat_Unmount(&fdevList[FAT_SD]);
            if (loadusb) Fat_Unmount(&fdevList[FAT_USB]);
            return ret;
        }
        if (loadsd) {
            Fat_Unmount(&fdevList[FAT_SD]);
            loadsd = 0;
        }
        if (loadusb) {
            Fat_Unmount(&fdevList[FAT_USB]);
            loadusb = 0;
        }
        ios_found[15] = 257;
    }
    ret = ReloadIos(15);
    if (ret < 0) {
        printf("ReloadIOS 15 failed! %d\n", ret);
        return -1;
    }
    if (Fat_Mount(&fdevList[FAT_SD])) {
        loadsd = 1;
    } else {
        printf("sd load error\n");
    }
    if(Fat_Mount(&fdevList[FAT_USB])){
        loadusb = 1;
    }
    printf("IOS 15 successfully downgraded!\n");
    printf("Installing fakesign accepting IOS 36.\n");
    
    patchSettings settings;
    clearPatchSettings(&settings);
    settings.iosVersion = 36;
    settings.iosRevision = IOS36version;
    settings.esTruchaPatch = true;
    settings.esIdentifyPatch = true;
    settings.nandPatch = true;
    settings.location = 36;
    settings.newrevision = IOS36version;
    settings.haveFakeSign = true;
    
    ret = IosInstall(settings);
    if (ret <= 0) {
        if (ret < 0) {
            printf("Installing fakesign accepting IOS failed. Exiting...");
            ret = -1;
        }
        if (loadsd) Fat_Unmount(&fdevList[FAT_SD]);
        if (loadusb) Fat_Unmount(&fdevList[FAT_USB]);
        return ret;
    }
    printf("IOS 36 installed as an IOS that can accept fakesigning!\n");
    printf("Now loading IOS 36 ...\n");
    if (loadsd) {
        Fat_Unmount(&fdevList[FAT_SD]);
        loadsd = 0;
    }
    if (loadusb) {
        Fat_Unmount(&fdevList[FAT_USB]);
        loadusb = 0;
    }
    ret = ReloadIos(36);
    if (ret < 0) {
        printf("ReloadIOS %d failed!\n", 36);
        return -1;
    }
    ios_found[36] = IOS36version;
    ios_fsign[36] = 1;
    ios_ident[36] = 1;
    ios_nandp[36] = 1;
    updateTitles();
    printf("You now have IOS 36 successfully installed as an IOS that can accept fakesigning!\n");
    PromptAnyKeyContinue();
    return 1;
}
