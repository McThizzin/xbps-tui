#ifndef XBPS_TUI_UI_H
#define XBPS_TUI_UI_H

void addtext(int x, int y, const char *text);
void listpacks(void);
void redowin(void);
void relist(void);
void scrnew(const char *title, const char *secondline);
void pressakey(void);
void on_resize(int sig);
void update_screen_size(void);
int inkey(void (*redraw_fn)(void), int retanyway);
int waitkey(void);
int is_named_key(int ch);

#endif