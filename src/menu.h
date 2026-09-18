#ifndef XBPS_TUI_MENU_H
#define XBPS_TUI_MENU_H

int choosemenu(int x, int y, const char *title, const char **chlist, int n, void (*redraw_fn)(void));
void scrmsg(const char **msglist, int n);

#endif