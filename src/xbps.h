#ifndef XBPS_TUI_XBPS_H
#define XBPS_TUI_XBPS_H

#include "pkglist.h"

char *getpackinfo(const char *pack, const char *prop);
char *getpacksize(const char *pack);
char *addpack(PkgList *target, const char *lin_in);
void reload_pl(void);
void apply_filters(void);
int check_xbps_available(void);

#endif