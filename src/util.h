#ifndef XBPS_TUI_UTIL_H
#define XBPS_TUI_UTIL_H

char *fixlen(int num, const char *s);
void show(int x, int y, const char *what);
void showcont(const char *what);
void put_styled(int style_code, const char *s);
void scrclear(void);
void trim_eol(char *s);
void split_name_version(const char *namebuf, char *name, size_t namesz, char *version, size_t versz);
char *stripkmenu(const char *title);

#endif