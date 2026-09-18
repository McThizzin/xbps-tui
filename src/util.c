#include <stdlib.h>
#include <string.h>

#include <ncurses.h>

#include "util.h"

char *fixlen(int num, const char *s) {
    if (num < 0) num = 0;
    char *r = malloc((size_t)num + 1);
    size_t len = strlen(s);
    if ((int)len >= num) {
        memcpy(r, s, (size_t)num);
    } else {
        memcpy(r, s, len);
        memset(r + len, ' ', (size_t)num - len);
    }
    r[num] = '\0';
    return r;
}

void show(int x, int y, const char *what) {
    if (x != 0) move(y - 1, x - 1);
    if (what) addstr(what);
}

void showcont(const char *what) { addstr(what); }

/* style bits: 0=normal,1=white,2=green,3=red ; +4=bold ; +8=reverse(+pad) */
void put_styled(int style_code, const char *s) {
    int col = style_code & 3;
    int attrs = A_NORMAL;
    if (style_code & 4) attrs |= A_BOLD;
    if (col == 1) attrs |= COLOR_PAIR(1);
    if (col == 2) attrs |= COLOR_PAIR(2);
    if (col == 3) attrs |= COLOR_PAIR(3);
    if (style_code & 8) {
        attrs |= A_REVERSE;
        attron(attrs);
        addch(' ');
        addstr(s);
        addch(' ');
        attroff(attrs);
    } else {
        attron(attrs);
        addstr(s);
        attroff(attrs);
    }
}

void scrclear(void) {
    attrset(A_NORMAL);
    erase();
    move(0, 0);
}

void trim_eol(char *s) {
    size_t l = strlen(s);
    while (l > 0 && (s[l - 1] == '\n' || s[l - 1] == '\r')) s[--l] = 0;
}

void split_name_version(const char *namebuf, char *name, size_t namesz, char *version, size_t versz) {
    const char *dash = strrchr(namebuf, '-');
    if (dash) {
        strncpy(version, dash + 1, versz - 1); version[versz - 1] = 0;
        size_t nlen = (size_t)(dash - namebuf);
        if (nlen >= namesz) nlen = namesz - 1;
        memcpy(name, namebuf, nlen); name[nlen] = 0;
    } else {
        strncpy(name, namebuf, namesz - 1); name[namesz - 1] = 0;
        version[0] = 0;
    }
}

char *stripkmenu(const char *title) {
    const char *ex = strchr(title, '!');
    if (!ex) return strdup(title);
    size_t i = (size_t)(ex - title);
    size_t len = strlen(title);
    if (i + 1 >= len) return strdup(title);
    if (title[i + 1] == ' ') return strdup(title);
    char *r = malloc(len);
    memcpy(r, title, i);
    strcpy(r + i, title + i + 1);
    return r;
}