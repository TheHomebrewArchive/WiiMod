#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "Error.h"

static char __esErrorMessage[115] = { };
char* EsErrorCodeString(int errorCode) {
    memset(__esErrorMessage, 0, sizeof(__esErrorMessage));
    snprintf(__esErrorMessage, sizeof(__esErrorMessage), "Success");
    if (errorCode > -1) return __esErrorMessage;

    switch (errorCode) {
        case -106:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-106) Invalid TMD when using ES_OpenContent or access denied.");
            break;
        case -1009:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1009) Read failure (short read).");
            break;
        case -1010:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1010) Write failure (short write).");
            break;
        case -1012:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1012) Invalid signature type.");
            break;
        case -1015:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1015) Invalid value for byte at 0x180 in ticket (valid:0,1,2)");
            break;
        case -1017:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1017) Wrong IN or OUT size, wrong size for a part of the vector, vector alignment problems, non-existant ioctl.");
            break;
        case -1020:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1020) ConsoleID mismatch");
            break;
        case -1022:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1022) Content did not match hash in TMD.");
            break;
        case -1024:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1024) Memory allocation failure.");
            break;
        case -1026:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1026) Incorrect access rights.");
            break;
        case -1028:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1028) No ticket installed.");
            break;
        case -1029:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1029) Installed Ticket/TMD is invalid");
            break;
        case -1035:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1035) Title with a higher version is already installed.");
            break;
        case -1036:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-1036) Required sysversion(IOS) is not installed.");
            break;
        case -2008:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-2008) Invalid parameter(s).");
            break;
        case -2011:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-2011) Signature check failed.");
            break;
        case -2013:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-2013) Keyring is full (contains 0x20 keys).");
            break;
        case -2014:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-2014) Bad has length (!=20)");
            break;
        case -2016:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-2016) Unaligned Data.");
            break;
        case -4100:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(-4100) Wrong Ticket-, Cert size or invalid Ticket-, Cert data");
            break;
        default:
            snprintf(__esErrorMessage, sizeof(__esErrorMessage), "(%d) Unknown Error", errorCode);
    }
    
    return __esErrorMessage;
}

static char __fsErrorMessage[50] = { };
char* FsErrorCodeString(int errorCode) {
    memset(__fsErrorMessage, 0, sizeof(__fsErrorMessage));
    snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "Success");
    if (errorCode > -1) return __fsErrorMessage;

    switch (errorCode) {
        case -1:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-1) Access Denied.");
            break;
        case -2:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-2) File Exists.");
            break;
        case -4:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-4) Invalid Argument.");
            break;
        case -6:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-6) File not found.");
            break;
        case -8:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-8) Resource Busy.");
            break;
        case -12:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-12) Returned on ECC error.");
            break;
        case -22:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-22) Alloc failed during request.");
            break;
        case -102:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-102) Permission denied.");
            break;
        case -103:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-103) Returned for \"corrupted\" NAND.");
            break;
        case -105:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-105) File exists.");
            break;
        case -106:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-106) File not found.");
            break;
        case -107:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-107) Too many fds open.");
            break;
        case -108:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-108) Memory is full.");
            break;
        case -109:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-190) Too many fds open.");
            break;
        case -110:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-110) Path Name is too long.");
            break;
        case -111:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-111) FD is already open.");
            break;
        case -114:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-114) Returned on ECC error.");
            break;
        case -115:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-115) Directory not empty.");
            break;
        case -116:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-116) Max Directory Depth Exceeded.");
            break;
        case -118:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-118) Resource busy.");
            break;
        case -119:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(-119) Fatal Error.");
            break;
        default:
            snprintf(__fsErrorMessage, sizeof(__fsErrorMessage), "(%d) Unknown Error", errorCode);
    }

    return __fsErrorMessage;
}
