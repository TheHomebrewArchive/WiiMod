#include <gccore.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef NO_DEBUG

bool geckoinit = false;

//using the gprintf from crediar because it is smaller than mine
void gprintf(const char *fmt, ...) {
    if (!(geckoinit)) return;
    char astr[4096];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(astr, fmt, ap);
    va_end(ap);
    usb_sendbuffer_safe(1, astr, strlen(astr));
    fflush(stdout);
}

void InitGecko() {
    if (usb_isgeckoalive(EXI_CHANNEL_1)) {
        usb_flush(EXI_CHANNEL_1);
        geckoinit = true;
    }
}

#endif /* NO_DEBUG */

void gcprintf(const char *fmt, ...) {
    char astr[4096];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(astr, fmt, ap);
    va_end(ap);
    gprintf(astr);
    printf(astr);
}
