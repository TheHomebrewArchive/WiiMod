/*-------------------------------------------------------------
 
 name.h -- functions for determining the name of a title
 
 Copyright (C) 2009 MrClick
 Unless other credit specified
 
 This software is provided 'as-is', without any express or implied
 warranty.  In no event will the authors be held liable for any
 damages arising from the use of this software.
 
 Permission is granted to anyone to use this software for any
 purpose, including commercial applications, and to alter it and
 redistribute it freely, subject to the following restrictions:
 
 1.The origin of this software must not be misrepresented; you
 must not claim that you wrote the original software. If you use
 this software in a product, an acknowledgment in the product
 documentation would be appreciated but is not required.
 
 2.Altered source versions must be plainly marked as such, and
 must not be misrepresented as being the original software.
 
 3.This notice may not be removed or altered from any source
 distribution.
 
 -------------------------------------------------------------*/

#ifndef _NAME_H_
#define _NAME_H_

#include "detect_settings.h"

// Load the database from SD card
s32 loadDatabase(char filename[]);

// Free the database on exit
void freeDatabase();

// Get the number of entries in the database
s32 getDatabaseCount();

// Get the name of database file
s32 addTitleToDatabase(char *tid, char *name);

// Get the name of a title
s32 getTitle_Name(char* name, TITLE *curTitle);

// Get the name of a title from the database located on the SD card
s32 getNameDB(char *name, char* id);

// Get the name of a title from its banner.bin in NAND
s32 getNameBN(char *name, u64 id, bool silent);

// Get the name of a title from its 00000000.app in NAND
s32 getName00(char *name, u64 id);

// Print the content of the title's content directory
s32 printContent(u64 id);

// Get the name of the selected mode
char *getNamingMode();

// Change naming mode
void changeNamingMode();

// Get the number of the dispay mode
u8 getDispMode();

// Get the name of the dispay mode
char *getDispModeName();

// Change diplay mode
void changeDispMode();

// Get string representation of lower title id
char *titleText(u32 kind, u32 title);

// Converts a 16 bit Wii string to a printable 8 bit string
s32 __convertWiiString(char *str, u8 *data, u32 cnt);

#endif
