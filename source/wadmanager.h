#ifndef _WADMANAGER_H_
#define _WADMANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

// Constants
#define ENTRIES_PER_PAGE    17
#define MAX_DIR_LEVELS      10
#define WAD_DIRECTORY       "/"

#define KB_SIZE     1024.0
#define MB_SIZE     1048576.0
#define GB_SIZE     1073741824.0

#define MAXPATHLEN 1024
#define MAXJOLIET 255
#define MAXDISPLAY 59

typedef struct {
    unsigned int length; // file length
    bool isdir; // false - file, true - directory
    char filename[MAXJOLIET + 1]; // full filename
    char displayname[MAXDISPLAY + 1]; // name for browser display
    /* 1 = Batch Install, 2 = Batch Uninstall - Leathl */
    int install;
    int ret;
} BROWSERENTRY;

extern BROWSERENTRY * browserList;
extern char rootdir[1024];
extern u8 gDirLevel;
extern const int ENTRIES_PER_PAGE_HALF_MIN_ONE;

int ParseDirectory(int filterElfDol);
char *PopCurrentDir(s32 *Selected, s32 *Start);
bool IsListFull(void);
void WaitPrompt(char *);
char *PeekCurrentDir(void);
int PushCurrentDir(char *dirStr, s32 Selected, s32 Start);
int Menu_FatDevice(void);
int wadmanager();
void ReloadMount();

#ifdef __cplusplus
}
#endif

#endif
