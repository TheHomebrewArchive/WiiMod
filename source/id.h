/*-------------------------------------------------------------
 
 id.h -- ES Identification code
 
 Copyright (C) 2008 tona
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

#ifndef _ID_H_
#define _ID_H_

// Identify as the system menu
s32 Identify_SUSilent(bool silent);
s32 Identify_SysMenu(void);
s32 Identify_tmd(tmd *tmd, u32 tmd_size, tik *tik);
s32 IdentifySilent(const u8 *certs, u32 certs_size, const u8 *idtmd, u32 idtmd_size,
    const u8 *idticket, u32 idticket_size, bool silent);

#endif
