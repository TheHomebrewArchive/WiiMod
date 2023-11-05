#ifndef _TOOLS_H_
#define _TOOLS_H_

#define ReloadIosNoInit(version) __reloadIos(version, false, true);
#define ReloadIos(version) __reloadIos(version, true, true);
#define ReloadIosNoInitNoPatch(version) __reloadIos(version, false, false);
#define max(x1,x2) ((x1) > (x2) ? (x1):(x2))

#ifdef __cplusplus
extern "C" {
#endif

// For the WiiLight
#define WII_LIGHT_OFF                0
#define WII_LIGHT_ON                 1
#define HAVE_AHBPROT ((*(vu32*)0xcd800064 == 0xFFFFFFFF) ? 1 : 0)
    #define WARNING_SIGN "\x1b[30;1m\x1b[43;1m/!\\\x1b[37;1m\x1b[40m"

/*
 This will shutdown the controllers, SD & USB then reload the IOS.
 */
int __reloadIos(int version, bool initWPAD, bool patchPerms);
void checkMemUsage();
void *AllocateMemory(u32 size);
void set_highlight(bool highlight);
void ReturnToLoader();
void SpinnerStart();
void SpinnerStop();
bool PromptContinue();
bool PromptYesNo();
void PromptAnyKeyContinue();
void clearLine();
void ExitMainThreadNow();
void horizontalLineBreak();
void WiiLightControl(int);
char *trim(char *str);

bool ExitMainThread;

#ifdef __cplusplus
}
#endif

#endif
