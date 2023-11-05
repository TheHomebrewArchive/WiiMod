#include <gccore.h>
#include <ogc/machine/processor.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "IOSCheck.h"
#include "IOSPatcher.h"
#include "Settings.h"
#include "RuntimeIOSPatch.h"
#include "tools.h"

#define MEM_PROT 0xD8B420A

const u8 hash_old[] = { 0x20, 0x07, 0x4B, 0x0B };
const u8 hash_patch[] = { 0x00 };

u32 apply_patch2(const char *name, const u8 *old, u32 old_size, const u8 *patch,
    u32 patch_size, u32 patch_offset) {
    //printf("Applying patch %s.....", name);
    u32 * end = (u32 *) 0x80003134;
    u8 *ptr = (u8 *) *end;
    //u8 *ptr = (u8 *) 0x93400000;
    u32 i, found = 0;
    u8 *start;
    
    while ((u32) ptr < (0x94000000 - old_size)) {
        if (!memcmp(ptr, old, old_size)) {
            found++;
            start = ptr + patch_offset;
            for (i = 0; i < patch_size; i++) {
                *(start + i) = patch[i];
            }
            ptr += patch_size;
            DCFlushRange((u8 *) (((u32) start) >> 5 << 5), (patch_size >> 5
                    << 5) + 64);
            ICInvalidateRange((u8 *)(((u32)start) >> 5 << 5), (patch_size >> 5 << 5) + 64);
            break;
        }
        ptr++;
    }
    /*if (found) {
        printf("Patched %d\n", found);
    } else {
        printf("\n");
    }*/
    return found;
}

u32 SetAHBPROTAndReload() {
    u32 count = patchSetAHBPROT();
    if (count) {
        ReloadIos(IOS_GetVersion());
        get_certs();
        int fsign = check_fakesig();
        int n=0;
        while( HAVE_AHBPROT && fsign < 0 && fsign != -1036 && n<10) {
            n+=1;
            ReloadIos(IOS_GetVersion());
            fsign = check_fakesig();
        }
    }
    return count;
}

u32 patchSetAHBPROT() {
    u32 count = 0;
    if( theSettings.DisableSetAHBPROT == 0) {
        write16(MEM_PROT, 0);
        count += apply_patch2("Set AHBPROT patch", es_set_ahbprot_check,
            es_set_ahbprot_check_size, es_set_ahbprot_patch, es_set_ahbprot_patch_size, 25);
        write16(MEM_PROT, 1);
    }
    return count;
}

u32 runtimePatchApply() {
    //printf("\n\n");
    u32 count = 0;

    write16(MEM_PROT, 0);
    count += apply_patch2("New Trucha (may fail)", hash_old,
        sizeof(hash_old), hash_patch, sizeof(hash_patch), 1);
    count += apply_patch2("ES_Identify", identify_check,
        identify_check_size, identify_patch, identify_patch_size, 2);
    count += apply_patch2("NAND Permissions", isfs_perms_check,
        isfs_perms_check_size, e0_patch, e0_patch_size, 2);
    count += apply_patch2("add ticket patch", addticket_vers_check,
        addticket_vers_check_size, e0_patch, e0_patch_size, 0);
    count += apply_patch2("ES_SetUID", setuid_check, setuid_check_size,
        setuid_patch, setuid_patch_size, 0);
    //PromptAnyKeyContinue();
    write16(MEM_PROT, 1);
    return count;
}
