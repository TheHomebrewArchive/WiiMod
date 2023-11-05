#ifndef _VIDEO_H_
#define _VIDEO_H_

#ifdef __cplusplus
extern "C" {
#endif

#define BLACK   0
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define MAGENTA 5
#define CYAN    6
#define WHITE   7

#define BOLD_NONE   0
#define BOLD_NORMAL 1
#define BOLD_FAINT  2

#define UP_ARROW    "\x1E"
#define DOWN_ARROW  "\x1F"
#define LEFT_ARROW  "\x11"
#define RIGHT_ARROW "\x10"

void initNextBuffer();
void flushBuffer();
void PrintBanner();
void ClearScreen();
void ClearLine();
void SetToInputLegendPos();
void Console_SetBottomRow();
void Console_SetFgColor(u8 color, u8 bold);
void Console_SetBgColor(u8 color, u8 bold);
void Console_SetColors(u8 bgColor, u8 bgBold, u8 fgColor, u8 fgBold);
void Video_Init();

#ifdef __cplusplus
}
#endif

#endif
