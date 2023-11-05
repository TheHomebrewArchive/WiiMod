#ifndef _SYS_H_
#define _SYS_H_

#ifdef __cplusplus
extern "C" {
#endif

bool Shutdown;
void System_Init();
void System_Deinit();
void System_Shutdown();
void Sys_Reboot(void);

#ifdef __cplusplus
}
#endif

#endif
