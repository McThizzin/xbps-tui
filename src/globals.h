#ifndef XBPS_TUI_GLOBALS_H
#define XBPS_TUI_GLOBALS_H

#include <signal.h>

#include "hashmap.h"
#include "pkglist.h"

#define VERSION "1.0.3"

extern PkgList rawpl;             /* unfiltered data for the current list kind (== bkpl_*) */
extern PkgList view;              /* filtered data actually shown            (== pl_*)   */

extern HashMap pl_namecache;      /* name -> last known description */
extern HashMap pl_sizecache;      /* name -> human readable installed size */
extern HashMap pl_instcache;      /* set of currently-installed package names */

extern int pack_maxnlen, pack_maxvlen;
extern int pack_maxn;
extern int pack_curr, pack_top;

extern int screen_width, screen_height;
extern int maxdlen;
extern int list_height;
extern const int scr_fixrows;

extern int list_kind;
extern char filtname[256];
extern char filtdesc[256];

extern int doredowin;

extern const char *list_kind_desc[6];

#define K_LISTCHANGE 0x0c /* ^L */
#define K_EXIT       0x11 /* ^Q */
#define K_REMOVE     0x12 /* ^R */
#define K_KILL       0x0b /* ^K */
#define K_UPGRADE    0x15 /* ^U */
#define K_INFO       0x10 /* ^P */
#define K_FUNCTION   0x06 /* ^F */
#define K_FILTNAME   0x0e /* ^N */
#define K_FILTDESC   0x04 /* ^D */

#define KEY_RESIZE_SENTINEL (-2)

extern volatile sig_atomic_t g_resized;

#endif
