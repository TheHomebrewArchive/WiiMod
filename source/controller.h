#ifndef BUTTONS_H
#define BUTTONS_H

#ifdef __cplusplus
extern "C" {
#endif

int ScanPads(u32 *button);
u32 WaitKey(u32 button);
u32 WaitButtons(void);

#ifdef __cplusplus
}
#endif

#endif
