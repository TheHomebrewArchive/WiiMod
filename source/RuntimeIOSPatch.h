#ifndef _RUNTIMEIOSPATCH_H_
#define _RUNTIMEIOSPATCH_H_

#ifdef __cplusplus
extern "C" {
#endif

u32 SetAHBPROTAndReload();
u32 patchSetAHBPROT();
u32 runtimePatchApply();

#ifdef __cplusplus
}
#endif

#endif
