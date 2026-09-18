#include <stdlib.h>
#include <string.h>

#include <ncurses.h>

#include "globals.h"
#include "menu.h"
#include "ui.h"
#include "util.h"

static void print_hilite_menu_item(int maxlen, const char *title, int reversed) {
    if (reversed) attron(A_REVERSE);
    const char *ex = strchr(title, '!');
    if (!ex || *(ex + 1) == '\0' || *(ex + 1) == ' ') {
        char *padded = fixlen(maxlen, title);
        addstr(padded);
        free(padded);
    } else {
        size_t i = (size_t)(ex - title);
        char before[300];
        size_t blen = i < sizeof(before) - 1 ? i : sizeof(before) - 1;
        memcpy(before, title, blen);
        before[blen] = 0;
        addstr(before);
        if (reversed) attroff(A_REVERSE);
        attron(A_BOLD | COLOR_PAIR(2));
        char hchar[2] = { ex[1], 0 };
        addstr(hchar);
        attroff(A_BOLD | COLOR_PAIR(2));
        if (reversed) attron(A_REVERSE);
        const char *rest = ex + 2;
        int restlen = maxlen - (int)blen - 1;
        if (restlen < 0) restlen = 0;
        char *padded = fixlen(restlen, rest);
        addstr(padded);
        free(padded);
    }
    if (reversed) attroff(A_REVERSE);
}

static void clearmenu_box(int x, int y, int maxlen, int n) {
    for (int i = 0; i <= n + 2; i++) {
        move(y + i - 1, x - 1);
        for (int c = 0; c < maxlen + 4; c++) addch(' ');
    }
    refresh();
}

/* choosemenu: returns 0 for cancel, else 1-based index into chlist */
int choosemenu(int x, int y, const char *title, const char **chlist, int n, void (*redraw_fn)(void)) {
    int maxlen = (int)strlen(title) + 6;
    for (int i = 0; i < n; i++) {
        int l = (int)strlen(chlist[i]);
        if (l > maxlen) maxlen = l;
    }

    int choice = 0;
    for (;;) {
        char *bar = fixlen(maxlen + 4, "");
        memset(bar, '-', (size_t)(maxlen + 4));
        show(x, y, bar);
        free(bar);

        show(x, y + 1, "| ");
        char titleline[400];
        snprintf(titleline, sizeof(titleline), "  %s", title);
        char *tl = fixlen(maxlen, titleline);
        showcont(tl);
        free(tl);
        showcont(" |");

        for (int i = 0; i < n; i++) {
            show(x, y + i + 2, "| ");
            print_hilite_menu_item(maxlen, chlist[i], i == choice);
            showcont(" |");
        }
        char *bar2 = fixlen(maxlen + 4, "");
        memset(bar2, '-', (size_t)(maxlen + 4));
        show(x, y + n + 2, bar2);
        free(bar2);
        refresh();

        int inp;
        for (;;) {
            inp = inkey(redraw_fn, 1);
            if (inp == KEY_RESIZE_SENTINEL) continue;
            break;
        }
        if (inp == 27) { clearmenu_box(x, y, maxlen, n); return 0; }
        if (inp == '\n' || inp == '\r' || inp == KEY_ENTER) { clearmenu_box(x, y, maxlen, n); return choice + 1; }
        if (inp == KEY_UP || inp == 'k') { if (choice > 0) choice--; continue; }
        if (inp == KEY_DOWN || inp == 'j') { if (choice < n - 1) choice++; continue; }
        if (inp == KEY_PPAGE) { choice -= 6; if (choice < 0) choice = 0; continue; }
        if (inp == KEY_NPAGE) { choice += 6; if (choice >= n) choice = n - 1; continue; }

        if (inp > 0 && inp < 256) {
            int vl = inp;
            if (vl < 32) vl += 64;
            if (vl > 96) vl -= 32;
            int found = -1;
            for (int i = 0; i < n; i++) {
                char pat1[3] = { '!', (char)vl, 0 };
                char pat2[3] = { '!', (char)(32 + vl), 0 };
                if (strstr(chlist[i], pat1) || strstr(chlist[i], pat2)) { found = i; break; }
            }
            if (found >= 0) { choice = found; clearmenu_box(x, y, maxlen, n); return found + 1; }
        }
    }
}

void scrmsg(const char **msglist, int n) {
    int maxlen = 24;
    for (int i = 0; i < n; i++) { int l = (int)strlen(msglist[i]); if (l > maxlen) maxlen = l; }
    int x = (screen_width - maxlen) / 2;
    int y = (screen_height - n - 4) / 2;

    attron(A_BOLD);
    char *bar = fixlen(maxlen + 4, ""); memset(bar, '-', (size_t)(maxlen + 4));
    show(x, y, bar);
    char *blank = fixlen(maxlen, "");
    char line[500]; snprintf(line, sizeof(line), "| %s |", blank);
    show(x, y + 1, line);
    for (int i = 0; i < n; i++) {
        show(x, y + i + 2, "| ");
        char *padded = fixlen(maxlen, msglist[i]);
        showcont(padded);
        free(padded);
        showcont(" |");
    }
    show(x, y + n + 2, line);
    show(x, y + n + 3, bar);
    free(bar); free(blank);
    attroff(A_BOLD);
    refresh();
}