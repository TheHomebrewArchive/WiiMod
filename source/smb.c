/****************************************************************************
 * Samba Operations
 *
 ****************************************************************************/

#include <network.h>
#include <ogcsys.h>
#include <ogc/machine/processor.h>
#include <smb.h>
#include <stdio.h>
#include <string.h>

#include "network.h"
#include "smb.h"
#include "Settings.h"
#include "tools.h"

static int connected = 0;

/****************************************************************************
 * Connect to SMB
 ***************************************************************************/
bool ConnectSMB() {
    if (connected == 0) {
        SpinnerStart();
        if (Network_Init() < 0) {
            printf("init network failed\n");
            return false;
        }
        SpinnerStop();
        /*printf("\n|%s|\n|%s|\n|%s|\n|%s|\n", theSettings.SMB_USER,
            theSettings.SMB_PWD, theSettings.SMB_SHARE, theSettings.SMB_IP);*/
        SpinnerStart();
        if (smbInitDevice("smb", theSettings.SMB_USER,
            theSettings.SMB_PWD, theSettings.SMB_SHARE, theSettings.SMB_IP)) {
            connected = 1;
            //printf("connected\n");
        }
        SpinnerStop();
    }
    return (connected == 1);
}

void CloseSMB() {
    if (connected) {
        smbClose("smb");
    }
    connected = 0;
}
