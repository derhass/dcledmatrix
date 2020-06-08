/*
 * Copyright (C) 2011 - 2020 by derhass <derhass@arcor.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DCLM_FONT_H
#define DCLM_FONT_H

#include "dclm.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * FONTS                                                                    *
 ****************************************************************************/

/*
 * This font is based on the font from "dcled",
 * Copyright 2009,2010,2011 Jeff Jahr <malakais@pacbell.net>,
 * under GPL license.  It is basically the X11 5x7 font with the "g" 
 * glyph replaced by a variant from Andy Scheller.
 */
extern const uint8_t dclmFontBase[256*7];

/****************************************************************************
 * FONT TO SCREEN                                                           *
 ****************************************************************************/

extern void
dclmCharBitsToScr(DCLEDMatrixScreen *scr, int x, const uint8_t *c);

extern void 
dclmCharToScr(DCLEDMatrixScreen *scr, int x, char c, const uint8_t *font);

extern void 
dclmStringToScr(DCLEDMatrixScreen *scr, int x, const char *str, size_t len, const uint8_t *font);

extern void 
dclmTextToScr(DCLEDMatrixScreen *scr, int x, const char *str, size_t len, const uint8_t *font);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif /* !DCLM_FONT_H */

