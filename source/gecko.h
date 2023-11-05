#ifndef _GECKO_H_
#define _GECKO_H_

#ifdef __cplusplus
extern "C" {
#endif

#define NO_DEBUG

void gcprintf(const char *fmt, ...);

#ifndef NO_DEBUG
//use this just like printf();
void gprintf(const char *fmt, ...);
bool InitGecko();
#else
#define gprintf(...)
#define InitGecko()      false
#endif

#ifdef __cplusplus
}
#endif

#endif

