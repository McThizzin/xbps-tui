#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <ncurses.h>

#include "globals.h"
#include "hashmap.h"
#include "ui.h"
#include "util.h"

/* "added texts" overlay */
typedef struct { int x, y; char text[400]; } AddedText;
static AddedText addedtexts[16];
static int addedtext_count = 0;

void addtext(int x, int y, const char *text) {
    char buf[400];
    if (y < 0) {
        addedtext_count = 0;
        if (!text || !text[0]) return;
        y = -y;
    }
    if (y == screen_height) {
        size_t cap = (size_t)(screen_width - x + 1);
        size_t len = strlen(text);
        if (len > cap) len = cap;
        if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
        memcpy(buf, text, len);
        buf[len] = 0;
        text = buf;
    }
    if (addedtext_count < 16) {
        addedtexts[addedtext_count].x = x;
        addedtexts[addedtext_count].y = y;
        strncpy(addedtexts[addedtext_count].text, text, sizeof(addedtexts[0].text) - 1);
        addedtexts[addedtext_count].text[sizeof(addedtexts[0].text) - 1] = 0;
        addedtext_count++;
    }
    show(x, y, text);
}

void listpacks(void) {
    if (pack_maxn < 0) {
        for (int i = 4; i <= screen_height - 1; i++) { move(i - 1, 0); clrtoeol(); }
        show(11, 5, "Sorry, empty list");
        return;
    }
    int index = pack_top;
    for (int i = 4; i <= screen_height - 1; i++) {
        move(i - 1, 0);
        if (index > pack_maxn) {
            clrtoeol();
        } else {
            int isCurrent = (index == pack_curr);
            if (isCurrent) attron(A_REVERSE);

            char *namef = fixlen(pack_maxnlen, view.items[index].name);
            addstr(namef); free(namef);

            char *statf = (list_kind == 5) ? fixlen(pack_maxvlen, view.items[index].stat)
                                            : fixlen(4, view.items[index].stat);
            addstr(statf); free(statf);

            char *versf = fixlen(pack_maxvlen, view.items[index].vers);
            addstr(versf); free(versf);

            const char *sz = hm_get(&pl_sizecache, view.items[index].name);
            char *szf = fixlen(8, sz ? sz : "");
            addstr(szf); free(szf);

            char *descf = fixlen(maxdlen, view.items[index].desc);
            addstr(descf); free(descf);

            clrtoeol();
            if (isCurrent) attroff(A_REVERSE);
        }
        index++;
    }
}

void redowin(void) {
    getmaxyx(stdscr, screen_height, screen_width);
    erase();
    maxdlen = screen_width - pack_maxnlen - pack_maxvlen - 1 - 3 - 9;
    if (maxdlen < 1) maxdlen = 1;

    show(1, 1, "");
    char hdr[64]; snprintf(hdr, sizeof(hdr), "xbps-tui v%s - ^L List type: ", VERSION);
    put_styled(1, hdr);
    char *stripped = stripkmenu(list_kind_desc[list_kind]);
    put_styled(2, stripped); free(stripped);
    char cnt[32]; snprintf(cnt, sizeof(cnt), " (%d)", pack_maxn + 1);
    showcont(cnt);

    show(1, 2, "");
    char *namehdr = fixlen(pack_maxnlen, "Name (^N filter)");
    put_styled(1, namehdr); free(namehdr);
    if (list_kind == 5) {
        char *v1 = fixlen(pack_maxvlen, "Version"); put_styled(1, v1); free(v1);
        char *v2 = fixlen(pack_maxvlen, "New ver"); put_styled(1, v2); free(v2);
        put_styled(1, "Size    ");
    } else {
        char *v1 = fixlen(pack_maxvlen, "    Version"); put_styled(1, v1); free(v1);
        put_styled(1, "    Size    ");
    }
    char *dhdr = fixlen(maxdlen, "Description (^D filter)");
    put_styled(1, dhdr); free(dhdr);
    clrtoeol();

    show(1, 3, "");
    if (filtname[0] || filtdesc[0]) {
        if (filtname[0]) {
            char b[300]; snprintf(b, sizeof(b), ">%s", filtname);
            put_styled(2, b);
        }
        if (filtdesc[0]) {
            if (filtname[0]) put_styled(0, " ");
            char b[300]; snprintf(b, sizeof(b), ">%s", filtdesc);
            put_styled(2, b);
        }
    } else {
        for (int i = 0; i < screen_width; i++) addch('-');
    }

    listpacks();

    show(1, screen_height - 1, "");
    for (int i = 0; i < screen_width; i++) addch('-');

    show(1, screen_height, "");
    put_styled(1, "xbps-tui ready: ^Q=exit, ^H=help, ^P=PKG properties, ^F=other functions");
    clrtoeol();

    for (int i = 0; i < addedtext_count; i++)
        show(addedtexts[i].x, addedtexts[i].y, addedtexts[i].text);

    refresh();
    list_height = screen_height - scr_fixrows;
}

void relist(void) {
    listpacks();
    refresh();
}

void scrnew(const char *title, const char *secondline) {
    scrclear();
    printw("\n");
    put_styled(2, title); printw("\n");
    put_styled(1, secondline); printw("\n\n");
}

void pressakey(void) {
    printw("\n");
    put_styled(2, "--- PRESS A KEY TO CONTINUE ---");
    refresh();
    waitkey();
}

void on_resize(int sig) { (void)sig; g_resized = 1; }

void update_screen_size(void) {
    endwin();
    refresh();
    getmaxyx(stdscr, screen_height, screen_width);
}

int inkey(void (*redraw_fn)(void), int retanyway) {
    wtimeout(stdscr, 20);
    for (;;) {
        int ch = getch();
        if (ch == ERR) {
            if (g_resized) {
                g_resized = 0;
                update_screen_size();
                if (redraw_fn) redraw_fn();
                if (retanyway) return KEY_RESIZE_SENTINEL;
            }
            continue;
        }
        return ch;
    }
}

int waitkey(void) { return inkey(redowin, 0); }

int is_named_key(int ch) {
    return ch == 27 || ch == '\n' || ch == '\r' || ch == KEY_ENTER ||
           ch == KEY_BACKSPACE || ch == 127 || ch == 8 ||
           (ch >= KEY_MIN && ch <= KEY_MAX);
}
