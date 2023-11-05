#include <gccore.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#include "AnyRegionChanger.h"
#include "cert_sys.h"
#include "controller.h"
#include "detect_settings.h"
#include "dopios.h"
#include "id.h"
#include "nand.h"
#include "Settings.h"
#include "sysconf.h"
#include "tools.h"
#include "video.h"

int lang, area, game, video, country, eula;

const char languages[][11] = { "Japanese", "English", "German", "French",
    "Spanish", "Italian", "Dutch", "S. Chinese", "T. Chinese", "Korean" };
const char areas[][10] = { "Japan", "USA", "Europe", "Australia", "Brazil",
    "Taiwan", "China", "Korea", "Hong Kong", "Asia", "Latin Am.", "S. Africa" };
const char regions_names[][7] = { "Japan", "USA", "Europe", "Korea" };
const char vmodes[][5] = { "NTSC", "PAL", "MPAL" };
const char eulas[][7] = { "Unread", "Read" };
const int languagesAvailable[] = {1,26,126,512};
const int languagesError[] = {896,896,896,256};

typedef struct Preset {
    int lang;
    int area;
    int game;
    int video;
    int country;
} Preset;

const Preset presets[4] = {
    {0,0,0,0,1}, //Japanese,Japan,Japan,NTSC,Japan
    {1,1,1,0,49}, //English,USA,USA,NTSC,United States
    {1,2,2,1,110}, //English,Europe,Europe,PAL,United Kingdom
    {9,7,3,0,136}, //Korean,Korea,Korea,NTSC,Korea
};

s32 getSettings(void) {
    lang = wiiSettings.lang;
    area = wiiSettings.area;
    game = wiiSettings.game;
    video = wiiSettings.video;
    eula = wiiSettings.eula;
    country = wiiSettings.country;
    return 0;
}

void loadPreset(int idx) {
    lang = presets[idx].lang;
    area = presets[idx].area;
    game = presets[idx].game;
    video = presets[idx].video;
    country = presets[idx].country;
}

s32 saveSettings(void) {
    s32 ret = 0;
    u8 sadr[SADR_LENGTH];
    ret = Identify_SUSilent(false);
    if (ret < 0) {
        printf("Unexpected Error: %s Value: %d\n", "Identify_SU", ret);
        return -1;
    }
    ret = Nand_Init();
    if (ret < 0) {
        printf("Unexpected Error: %s Value: %d\n", "Nand_Init", ret);
        return -1;
    }
    ret = SYSCONF_Init();
    if (ret < 0) {
        printf("Unexpected Error: %s Value: %d\n", "SYSCONF_Init", ret);
        return -1;
    }
    if (lang != wiiSettings.lang) ret = SYSCONF_SetLanguage(lang);
    if (ret) {
        printf("Unexpected Error: %s Value: %d\n", "SYSCONF_SetLanguage", ret);
        return -1;
    }
    if (area != wiiSettings.area) ret = SYSCONF_SetArea(area);
    if (ret) {
        printf("Unexpected Error: %s Value: %d\n", "SYSCONF_SetArea", ret);
        return -1;
    }
    if (game != wiiSettings.game) ret = SYSCONF_SetRegion(game);
    if (ret) {
        printf("Unexpected Error: %s Value: %d\n", "SYSCONF_SetRegion", ret);
        return -1;
    }
    if (video != wiiSettings.video) ret = SYSCONF_SetVideo(video);
    if (ret) {
        printf("Unexpected Error: %s Value: %d\n", "SYSCONF_SetVideo", ret);
        return -1;
    }
    if (eula != wiiSettings.eula) ret = SYSCONF_SetEULA(eula);
    if (ret) {
        printf("Unexpected Error: %s Value: %d\n", "SYSCONF_SetEULA", ret);
        return -1;
    }

    if (country != wiiSettings.country) {
        memset(sadr, 0, SADR_LENGTH);
        sadr[0] = country;
        ret = SYSCONF_Set("IPL.SADR", sadr, SADR_LENGTH);
        if (ret) {
            printf("Unexpected Error: %s Value: %d\n", "SYSCONF_Set", ret);
            return -1;
        }
    }
    printf("\nSaving...");
    ret = SYSCONF_SaveChanges();
    if (ret < 0) {
        printf("Unexpected Error: %s Value: %d\n", "SYSCONF_SaveChanges", ret);
        return ret;
    }
    printf("OK!\n");
    ISFS_Deinitialize();
    updateRegionSettings();
    return 0;
}

int getRegionIntFromChar( char region ) {
    if ('J' == region) {
        return 0;
    } else if ('U' == region) {
        return 1;
    } else if ('E' == region) {
        return 2;
    } else if ('K' == region) {
        return 3;
    }
    return 0;
}

int getPresetVal() {
    return getRegionIntFromChar(wiiSettings.sysMenuRegion);
}

bool checkLanguageMatchesArea(s32 thelang, s32 thearea) {
    return ((int)pow(2,thelang) & languagesAvailable[getRegionIntFromChar(AREAtoSysMenuRegion(thearea))])>0;
}

bool checkLanguageAreaError(s32 thelang, s32 thearea) {
    return ((int)pow(2,thelang) & languagesError[getRegionIntFromChar(AREAtoSysMenuRegion(thearea))])>0;
}

void regionChangerMenu(void) {
    static int menuItem = 0;
    static bool override = false;
    bool unsavedSettings = false;
    int iKnow = (override)?2:0;
    u32 button = 0;
    int ret = -1;
    int preset = getPresetVal();

    getSettings();
    
    initNextBuffer();
    PrintBanner();
    printf("\n\n\n\t\t" WARNING_SIGN " IMPORTANT BRICK INFORMATION " WARNING_SIGN "\n");
    printf("\n\t\tSemi Bricks are caused by the Console Area setting not matching\n");
    printf("\t\tyour System Menu region. A semi-brick in itself is not terribly\n");
    printf("\t\tharmful, but it can easily deteriorate into a full brick--there\n");
    printf("\t\tare multiple simple triggers to do so.\n");
    printf("\n\t\tIn order to practice proper safety when using this application, \n");
    printf("\t\tplease make sure that your Console Area and System Menu region \n");
    printf("\t\tare congruent before rebooting your system. A warning will be\n");
    printf("\t\tdisplayed if they are not in agreement.\n");
    flushBuffer();
    PromptAnyKeyContinue();
    
    while (!(button & WPAD_BUTTON_B)) {
        initNextBuffer();
        PrintBanner();
        if(wiiSettings.regionChangedKoreanWii) {
            Console_SetColors(BLACK, 0, YELLOW, 0);
            printf("\nYou have a region changed Wii that was originally Korean\n");
            printf("You are at risk of Error 003 brick if you keep the current configuration\n");
            Console_SetColors(BLACK, 0, WHITE, 0);
        } else {
            printf("\n\n\n");
        }
        printf("%s",(unsavedSettings)?"\t\t\t\t   \x1b[31;0m*\x1b[32;0mYou have unsaved settings you need to save\x1b[37;0m\n":"\n");
        printf("\t\t\t\t%3s%sLanguage Setting: %s\n", (menuItem == 0)
            ? "-->" : " ", (wiiSettings.lang!=lang)?"\x1b[31;0m*\x1b[37;0m":" ", languages[lang]);
        if (wiiSettings.sysMenuRegion != AREAtoSysMenuRegion(area)) {
            printf("\t\t\t\t" WARNING_SIGN " \x1b[41;1mWARNING: AREA/SysMenu MISMATCH!\x1b[40m " WARNING_SIGN "\n");
        } else {
            printf("\n");
        }
        printf("\t\t\t\t%3s%sConsole Area Setting: %s\n", (menuItem == 1)
            ? "-->" : " ", (wiiSettings.area!=area)?"\x1b[31;0m*\x1b[37;0m":" ", areas[area]);
        printf("\t\t\t\t%3s%sGame Region Setting: %s\n", (menuItem == 2) ? "-->"
            : " ", (wiiSettings.game!=game)?"\x1b[31;0m*\x1b[37;0m":" ", regions_names[game]);
        printf("\t\t\t\t%3s%sConsole Video Mode: %s\n", (menuItem == 3) ? "-->"
            : " ", (wiiSettings.video!=video)?"\x1b[31;0m*\x1b[37;0m":" ", vmodes[video]);
        printf("\t\t\t\t\tCountry Codes: 1=JPN 49=USA 110=UK 136=KOR\n");
        printf("\t\t\t\t%3s%sShop Country Code: %d\n", (menuItem == 4) ? "-->"
            : " ", (wiiSettings.country!=country)?"\x1b[31;0m*\x1b[37;0m":" ", country);
        printf("\t\t\t\t%3s%sServices EULA: %s\n", (menuItem == 5) ? "-->" : " ",
            (wiiSettings.eula!=eula)?"\x1b[31;0m*\x1b[37;0m":" ",
            (eula == SYSCONF_ENOENT) ? "Disabled " : eulas[eula]);
        printf("\n\t\t\t\t%3s Preset: %s\n", (menuItem == 6) ? "-->" : " ", regions_names[preset]);
        printf("\t\t\t\t%3s Load Preset", (menuItem == 7) ? "-->" : " ");
        if (wiiSettings.sysMenuRegion != AREAtoSysMenuRegion(area)) {
            Console_SetFgColor(RED, BOLD_NONE);
            printf("\tRecommended\n");
            Console_SetFgColor(WHITE, BOLD_NONE);
        } else {
            printf("\n");
        }
        printf("\n\t\t\t\t%3s Reload Current Settings\n", (menuItem == 8)
            ? "-->" : " ");
        printf("\t\t\t\t%3s%sSave Settings\n", (menuItem == 9) ? "-->" : " ",
            (unsavedSettings)?"\x1b[32;0m*\x1b[37;0m":" ");
        if( iKnow == 2 || override ) {
            printf("\t\t\t\t%3s I know what I'm doing: %s\n", (menuItem == 10) ? "-->" : " ",
                (override)?"True":"False");
        }
        SetToInputLegendPos();
        horizontalLineBreak();
        printf("\t(%s)/(%s) (%s)/(%s)\t\tChange Selection\n", UP_ARROW, DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW);
        printf("\t(A)\t\t\t\t\tSelect\t\t\t\t\t(B)\tBack\n");
        printf("\t(HOME)/GC:(START)\t\tExit");
        flushBuffer();
        
        button = WaitButtons();
        if (ExitMainThread) {
            return;
        }
        if (button & WPAD_BUTTON_HOME) {
            ReturnToLoader();
        }
        if (button & WPAD_BUTTON_A) {
            if (menuItem == 8) {
                getSettings();
            } else if (menuItem == 9) {
                ret = 1;
                if( wiiSettings.sysMenuRegion != AREAtoSysMenuRegion(area) && !override) {
                    PrintBanner();
                    printf("You are trying to save your settings while your area and SysMenu do not match.\n");
                    printf("This could leave you with a brick if you don't know what you are doing.\n");
                    if (!PromptContinue()) {
                        ret = 0;
                    }
                }
                if( checkLanguageAreaError(lang, area)) {
                    PrintBanner();
                    printf("You are trying to save your settings while your area and language do not match.\n");
                    printf("This could leave you with a brick if you don't know what you are doing.\n");
                    if (!PromptContinue()) {
                        ret = 0;
                    }
                }
                if (ret > 0) {
                    ret = saveSettings();
                }
                if (ret < 0) {
                    ISFS_Deinitialize();
                    PromptAnyKeyContinue();
                    return;
                }
            } else if (menuItem == 7) {
                loadPreset(preset);
            }
        }

        if (button & WPAD_BUTTON_LEFT) {
            if (menuItem == 0) {
                lang--;
                if( !override ) {

                    while( !checkLanguageMatchesArea(lang, area) ) {
                        lang--;
                        if (lang < 0) lang = 9;
                    }
                }
            } else if (menuItem == 1) {
                area--;
            } else if (menuItem == 2) {
                game--;
            } else if (menuItem == 3) {
                video--;
            } else if (menuItem == 4) {
                country--;
                if (eula >= 0) eula = 0;
            } else if (menuItem == 5) {
                if (eula >= 0) eula = !eula;
            } else if (menuItem == 6) {
                preset--;
            } else if ( menuItem == 10 ) {
                override = !override;
            }
        }

        if (button & WPAD_BUTTON_RIGHT) {
            if (menuItem == 0) {
                lang++;
                if( !override ) {
                    while( !checkLanguageMatchesArea(lang, area) ) {
                        lang++;
                        if (lang > 9) lang = 0;
                    }
                }
            } else if (menuItem == 1) {
                area++;
            } else if (menuItem == 2) {
                game++;
            } else if (menuItem == 3) {
                video++;
            } else if (menuItem == 4) {
                country++;
                if (eula >= 0) eula = 0;
            } else if (menuItem == 5) {
                if (eula >= 0) eula = !eula;
            } else if (menuItem == 6) {
                preset++;
            } else if ( menuItem == 10 ) {
                override = !override;
            }
        }

        if (button & WPAD_BUTTON_DOWN) menuItem++;
        if (button & WPAD_BUTTON_UP) {
            if( !override ) {
                iKnow = 0;
            }
            menuItem--;
        }
        
        if (menuItem > 9 || (iKnow && menuItem > 10)) {
            if(iKnow==0) {
                iKnow = 1;
                menuItem = 0;
            } else if (iKnow==1) {
                iKnow = 2;
            } else if( menuItem > 10 ) {
                menuItem = 0;
            }
        }
        if (menuItem < 0) {
            if( iKnow == 2 ) {
                menuItem = 10;
            } else {
                menuItem = 9;
            }
        }
        
        if (lang < 0) lang = 9;
        if (area < 0) area = 11;
        if (game < 0) game = 3;
        if (video < 0) video = 2;
        if (country < 0) country = 255;
        if (preset < 0) preset = 3;

        if (lang > 9) lang = 0;
        if (area > 11) area = 0;
        if (game > 3) game = 0;
        if (video > 2) video = 0;
        if (country > 255) country = 0;
        if (preset > 3) preset = 0;

        if( !override ) {
            while( !checkLanguageMatchesArea(lang, area) ) {
                lang++;
                if (lang > 9) lang = 0;
            }
        }

        if( wiiSettings.lang!=lang || wiiSettings.area!=area || wiiSettings.game!=game || wiiSettings.video!=video ||
                wiiSettings.country!=country || wiiSettings.eula!=eula ) {
            unsavedSettings = true;
        } else {
            unsavedSettings = false;
        }
    }
}
