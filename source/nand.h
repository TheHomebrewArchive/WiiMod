#ifndef _NAND_H_
#define _NAND_H_

#ifdef __cplusplus
extern "C" {
#endif

/* 'NAND Device' structure */
typedef struct {
    /* Device name */
    char *name;

    /* Mode value */
    u32 mode;

    /* Un/mount command */
    u32 mountCmd;
    u32 umountCmd;
} nandDevice;

int Nand_Init();
s32 Nand_CreateDir( u64);
s32 Nand_CreateFile( u64, const char *);
s32 Nand_OpenFile( u64, const char *, u8);
s32 Nand_CloseFile( s32);
s32 Nand_ReadFile( s32, void *, u32);
s32 Nand_WriteFile( s32, void *, u32);
s32 Nand_RemoveDir( u64);
s32 Nand_Mount(nandDevice *);
s32 Nand_Unmount(nandDevice *);
s32 Nand_Enable(nandDevice *);
s32 Nand_Disable(void);

#ifdef __cplusplus
}
#endif

#endif
