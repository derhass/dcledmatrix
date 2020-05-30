#ifndef DCLM_FONT_H
#define DCLM_FONT_H

#include "dclm.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 * FONTS                                                                    *
 ****************************************************************************/

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

