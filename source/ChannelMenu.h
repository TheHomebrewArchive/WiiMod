#ifndef CHANNELMENU_H
#define CHANNELMENU_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHOP_CHANNEL,
    PHOTO_CHANNEL_1_0,
    PHOTO_CHANNEL_1_1,
    MII_CHANNEL,
    INTERNET_CHANNEL,
    NEWS_CHANNEL,
    WEATHER_CHANNEL,
    WII_SPEAK_CHANNEL,
    EULA_TITLE,
    BC_TITLE,
    MIOS_TITLE,
    REGION_SELECT_TITLE,
    DIGICAM_PRINT_CHANNEL,
    JAPAN_FOOD_CHANNEL,
    SHASHIN_CHANNEL,
    TV_FRIEND_CHANNEL,
    WII_NO_MA_CHANNEL,
} THE_CHANNELS;

extern int InstalledChannelVersion[];
extern const int CHANNELS_LEN;
int channelMenu();

#ifdef __cplusplus
}
#endif

#endif
