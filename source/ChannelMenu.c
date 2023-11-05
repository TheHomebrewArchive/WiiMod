#include <stdio.h>
#include <wiiuse/wpad.h>

#include "ChannelMenu.h"
#include "controller.h"
#include "detect_settings.h"
#include "dopios.h"
#include "gecko.h"
#include "IOSCheck.h"
#include "nand.h"
#include "network.h"
#include "title_install.h"
#include "tools.h"
#include "video.h"

#define CHANNELS_LEN_PPD    (sizeof(Channels) / sizeof(Channel))

void InstallTheChosenChannel(int region, int channel, u16 version);

typedef struct Channel {
    const char Name[40];
    int cnt;
    u16 versions[15];
} Channel;

//This must stay in sync with THE_CHANNELS
const Channel Channels[] = { { "Shop Channel", 15, { 0, 3, 4, 5, 6, 7, 8, 10,
    13, 14, 16, 17, 18, 19, 20 } },// 14 is Korea Only
    { "Photo Channel 1.0", 3, { 0, 1, 2 } }, { "Photo Channel 1.1", 4, { 0, 1,
        2, 3 } }, { "Mii Channel", 6, { 0, 2, 3, 4, 5, 6 } }, {
        "Internet Channel", 6, { 0, 1, 3, 257, 512, 1024 } }, { "News Channel",
        4, { 0, 3, 6, 7 } }, { "Weather Channel", 4, { 0, 3, 6, 7 } }, {
        "Wii Speak Channel", 4, { 0, 1, 256, 512 } }, { "EULA", 4,
        { 0, 1, 2, 3 } }, { "BC", 5, { 0, 2, 4, 5, 6 } }, { "MIOS", 6, { 0, 4,
        5, 8, 9, 10 } }, { "Region Select", 3, { 0, 1, 2 } }, {
        "Digicam Print Channel", 1, { 0 } },//Japan only
    { "Japan Food Service/Demae Channel", 3, { 0, 1, 2 } },//Japan only
    { "Shashin Channel 1.0 Fukkyuu Programme", 1, { 0 } },//Japan only
    { "TV Friend Channel / G-Guide for Wii", 1, { 0 } },//Japan only
    { "Wii no Ma Channel", 1, { 0 } }//Japan only
};
const int CHANNELS_LEN = CHANNELS_LEN_PPD;
const int CHANNELS_HI = CHANNELS_LEN_PPD - 1;

int InstalledChannelVersion[CHANNELS_LEN_PPD];

void InstallTheChosenChannel(int region, int channel, u16 version) {
    s32 ret = 0;
    u64 tid = 0;
    //Initialize Network
    Network_Init();
    
    Nand_Init();
    //Shop Channel

    if (channel == SHOP_CHANNEL) {
        if (version != 14 || region == REGION_K) {
            ret
                    = downloadAndInstall(0x0001000248414241ULL, version, "Shop Channel");
        } else {
            ret = downloadAndInstall(0x0001000248414241ULL, 0, "Shop Channel");
        }
        if (region == REGION_K && ret > 0) { //Korea
            ret
                    = downloadAndInstall(0x000100024841424BULL, version, "Shop Channel");
        }
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nShop Channel successfully installed!");
        }
    } else if (channel == PHOTO_CHANNEL_1_0) {//Photo Channel 1.0
        ret
                = downloadAndInstall(0x0001000248414141ULL, version, "Photo Channel 1.0");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nPhoto Channel 1.0 successfully installed!");
        }
    } else if (channel == PHOTO_CHANNEL_1_1) {//Photo Channel 1.1
        ret
                = downloadAndInstall(0x0001000248415941ULL, version, "Photo Channel 1.1");
        if (region == REGION_K && ret > 0) {
            ret
                    = downloadAndInstall(0x000100024841594BULL, version, "Photo Channel 1.1");
        }
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nPhoto Channel 1.1 successfully installed!");
        }
    } else if (channel == MII_CHANNEL) {//Mii Channel
        ret = downloadAndInstall(0x0001000248414341ULL, version, "Mii Channel");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nMii Channel successfully installed!");
        }
    } else if (channel == INTERNET_CHANNEL) {//Internet Channel
        tid = 0x0001000148414400ULL | Region_IDs[region];
        ret = downloadAndInstall(tid, version, "Internet Channel");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nInternet Channel successfully installed!");
        }
    } else if (channel == NEWS_CHANNEL) {//News Channel
        if (3 == version) {
            ret
                    = downloadAndInstall(0x0001000248414741ULL, version, "News Channel");
        } else {
            tid = 0x0001000248414700ULL | Region_IDs[region];
            ret = downloadAndInstall(tid, version, "News Channel");
        }
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nNews Channel successfully installed!");
        }
    } else if (channel == WEATHER_CHANNEL) {//Weather Channel
        ret
                = downloadAndInstall(0x0001000248414641ULL, version, "Weather Channel");
        if (3 != version) {
            tid = 0x0001000248414600ULL | Region_IDs[region];
            ret = downloadAndInstall(tid, version, "Weather Channel");
        }
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nWeather Channel successfully installed!");
        }
    } else if (channel == WII_SPEAK_CHANNEL) {//Wii Speak Channel
        tid = 0x0001000148434600ULL | Region_IDs[region];
        ret = downloadAndInstall(tid, version, "Wii Speak Channel");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nWii Speak Channel successfully installed!");
        }
    } else if (channel == EULA_TITLE) {//EULA
        tid = 0x0001000848414B00ULL | Region_IDs[region];
        ret = downloadAndInstall(tid, version, "EULA");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nEULA successfully installed!");
        }
    } else if (channel == BC_TITLE) {//BC
        ret = downloadAndInstall(0x0000000100000100ULL, version, "BC");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nBC successfully installed!");
        }
    } else if (channel == MIOS_TITLE) {//MIOS
        ret = downloadAndInstall(0x0000000100000101ULL, version, "MIOS");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nMIOS successfully installed!");
        }
    } else if (channel == REGION_SELECT_TITLE) {//Region Select
        tid = 0x0001000848414C00ULL | Region_IDs[region];
        ret = downloadAndInstall(tid, version, "Region Select");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nRegion Select successfully installed!");
        }
    } else if (channel == DIGICAM_PRINT_CHANNEL && region == REGION_J) {
        //Digicam Print Channel - Japan only
        ret
                = downloadAndInstall(0x000100014843444AULL, version, "Region Select");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nRegion Select successfully installed!");
        }
    } else if (channel == JAPAN_FOOD_CHANNEL && region == REGION_J) {
        //Japan Food Service/Demae Channel - Japan only
        ret
                = downloadAndInstall(0x000100084843434AULL, version, "Region Select");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nRegion Select successfully installed!");
        }
    } else if (channel == SHASHIN_CHANNEL && region == REGION_J) {
        //Shashin Channel 1.0 Fukkyuu Programme - Japan only
        ret
                = downloadAndInstall(0x000100014843424AULL, version, "Region Select");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nRegion Select successfully installed!");
        }
    } else if (channel == TV_FRIEND_CHANNEL && region == REGION_J) {
        //TV Friend Channel / G-Guide for Wii - Japan only
        ret
                = downloadAndInstall(0x0001000148424E4AULL, version, "Region Select");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nRegion Select successfully installed!");
        }
    } else if (channel == WII_NO_MA_CHANNEL && region == REGION_J) {
        //Wii no Ma Channel - Japan only
        ret
                = downloadAndInstall(0x000100014843494AULL, version, "Region Select");
        if (ret < 0) {
            gcprintf("\nError: %d", ret);
        } else if (ret > 1) {
            gcprintf("\nRegion Select successfully installed!");
        }
    }
    ISFS_Deinitialize();
    if (ret != 0) {
        PromptAnyKeyContinue();
    }
}

int channelMenu() {
    static int channelver = 0, menuItem = 0, channelSelection = 0,
            neverSet = 0, regionSelection = 0;
    int i;
    u32 button = 0;

    if (neverSet == 0) {
        neverSet = 1;
        regionSelection = getRegion();
    }
    while (!(button & WPAD_BUTTON_B)) {
        initNextBuffer();
        PrintBanner();
        printf("\n\n\n\t\t\t\t%3s Install Channel: %s\n", (menuItem == 0
            ? "-->" : " "), Channels[channelSelection].Name);
        printf("\t\t\t\t%3s Region:          %s\n", (menuItem == 1 ? "-->"
            : " "), Regions[regionSelection].Name);
        printf("\t\t\t\t%3s Version: ", (menuItem == 2 ? "-->" : " "));
        for (i = 0; i < Channels[channelSelection].cnt; i += 1) {
            if (i == channelver) {
                Console_SetFgColor(BLACK, 0);
                Console_SetBgColor(WHITE, 0);
            }
            if (Channels[channelSelection].versions[i] == 0) {
                printf("LATEST");
            } else {
                printf("%d", Channels[channelSelection].versions[i]);
            }
            if (i == channelver) {
                Console_SetFgColor(WHITE, 0);
                Console_SetBgColor(BLACK, 0);
            }
            printf(" ");
        }
        printf("\n\t\t\t\t\tCurrent version:");
        if (InstalledChannelVersion[channelSelection] > 0) {
            printf(" v%d\n", InstalledChannelVersion[channelSelection]);
        } else {
            printf(" None\n");
        }
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s) (%s)/(%s)\t\tChange Selection\n", UP_ARROW, DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        printf("\t(HOME)/GC:(START)\t\tExit");
        flushBuffer();
        button = WaitButtons();
        if (ExitMainThread) return 0;
        if (button & WPAD_BUTTON_HOME) ReturnToLoader();
        if (button & WPAD_BUTTON_A) {
            PrintBanner();
            if (PromptContinue()) {
                InstallTheChosenChannel(regionSelection, channelSelection, Channels[channelSelection].versions[channelver]);
            }
        }

        if (button & WPAD_BUTTON_DOWN) menuItem++;
        if (button & WPAD_BUTTON_UP) menuItem--;
        
        if (menuItem > 2) menuItem = 0;
        if (menuItem < 0) menuItem = 2;
        
        if (button & WPAD_BUTTON_LEFT) {
            if (menuItem == 0) channelSelection--;
            if (menuItem == 1) regionSelection--;
            if (menuItem == 2) channelver--;
            if (channelSelection == SHOP_CHANNEL
                    && Channels[channelSelection].versions[channelver] == 14
                    && regionSelection != REGION_K) {
                // Korea only
                channelver--;
            }

            if (channelSelection < 0) channelSelection = CHANNELS_HI;
            while (channelSelection > 11 && regionSelection != REGION_J) {// Japan only
                channelSelection--;
            }
        }
        if (button & WPAD_BUTTON_RIGHT) {
            if (menuItem == 0) channelSelection++;
            if (menuItem == 1) regionSelection++;
            if (menuItem == 2) channelver++;
            
            if (channelSelection == SHOP_CHANNEL
                    && Channels[channelSelection].versions[channelver] == 14
                    && regionSelection != REGION_K) {
                // Korea only
                channelver++;
            }

            if (channelSelection > CHANNELS_HI) channelSelection = 0;
            while (channelSelection > 11 && regionSelection != REGION_J) {// Japan only
                channelSelection++;
                if (channelSelection > CHANNELS_HI) channelSelection = 0;
            }
        }

        if (channelSelection < 0) channelSelection = CHANNELS_HI;
        if (channelSelection > CHANNELS_HI) channelSelection = 0;
        if (regionSelection < 0) regionSelection = REGIONS_HI;
        if (regionSelection > REGIONS_HI) regionSelection = 0;
        if (channelver < 0) channelver = Channels[channelSelection].cnt - 1;
        if (channelver >= Channels[channelSelection].cnt) channelver = 0;
    }
    return 0;
}
