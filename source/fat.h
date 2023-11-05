#ifndef _FAT_H_
#define _FAT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* 'FAT Device' structure */
typedef struct {
    /* Device mount point */
    char *mount;

    /* Device name */
    char *name;

    /* Device interface */
    const DISC_INTERFACE *interface;
} fatDevice;

extern fatDevice fdevList[];
extern fatDevice *fdev;
extern const int NB_FAT_DEVICES;

enum {
    FAT_SD = 0,
    FAT_USB,
    FAT_SMB,
    FAT_GECKO_A,
    FAT_GECKO_B
};

s32 Fat_Mount(fatDevice *);
void Fat_Unmount(fatDevice *);

#ifdef __cplusplus
}
#endif

#endif
