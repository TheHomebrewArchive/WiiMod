#ifndef _DOPIOS_H_
#define _DOPIOS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define WII_MOD_VERSION 3.2

typedef struct Region {
    const char Name[30];
} Region;

//These two need to stay in sync
typedef enum {
    REGION_U, REGION_E, REGION_J, REGION_K,
} THE_REGIONS;
extern const Region Regions[];
extern const u64 Region_IDs[];
extern const int REGIONS_LEN;
extern const int REGIONS_HI;

int sortCallback(const void *, const void *);
int ios_selectionmenu(int default_ios);
int getsuplist(int * list2);

#ifdef __cplusplus
}
#endif

#endif
