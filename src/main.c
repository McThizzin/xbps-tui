/*
 * xbps-tui.c  --  a textual front-end for the XBPS package manager (Void Linux)
 *
 * Uses ncurses, hash tables for the package caches, and fork()/execvp().
 *
 * Build:   make
 * Run:     ./xbps-tui
 */

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ncurses.h>

#include "globals.h"
#include "hashmap.h"
#include "menu.h"
#include "pkglist.h"
#include "proc.h"
#include "ui.h"
#include "util.h"
#include "xbps.h"

static void sync_repos(void) {
    scrnew("SYNCHRONIZE (update the local database of packages in repositories)",
           "Invoked: xbps-install -S");
    char *argv[] = { "xbps-install", "-S", NULL };
    char **sudoargv = sudo_wrap(argv);
    char *lastline = run_interactive_lastline(sudoargv);
    free(sudoargv);
    if (lastline && lastline[0]) printw("\n%s\n", lastline);
    free(lastline);
    pressakey();
}

int main(void) {
    if (!check_xbps_available()) {
        fprintf(stderr,
            "\nThis program is a front-end for the XBPS package manager of Void Linux,\n"
            "but the xbps-query(1) command can not be run. This program is useless\n"
            "without that command.\n\n");
        return 1;
    }

    hm_init(&pl_namecache);
    hm_init(&pl_sizecache);
    hm_init(&pl_instcache);
    pl_init(&rawpl);
    pl_init(&view);

    initscr();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_RED, -1);
    noecho();
    raw(); /* not cbreak(): raw() also disables IXON/IXOFF so that ^Q/^S
              reach the program instead of being eaten as flow control */
    keypad(stdscr, TRUE);
    curs_set(0);
    scrollok(stdscr, TRUE);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_resize;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGWINCH, &sa, NULL);

    getmaxyx(stdscr, screen_height, screen_width);

    sync_repos();
    reload_pl();

    for (;;) {
        if (doredowin) redowin();
        doredowin = 1;

        int inp = waitkey();
        if (inp == K_EXIT) break;

        int is_special = (inp == '\t') || (inp >= KEY_MIN && inp <= KEY_MAX) || inp == 127 ||
                         inp == 'j' || inp == 'k';
        if (is_special) {
            int recog = 0;
            if (inp == KEY_HOME) {
                pack_curr = 0; pack_top = 0; relist(); recog = 1;
            } else if (inp == KEY_END) {
                pack_curr = pack_maxn; pack_top = pack_maxn - list_height;
                if (pack_top < 0) pack_top = 0;
                relist(); recog = 1;
            } else if (inp == KEY_DOWN || inp == 'j') {
                if (pack_curr < pack_maxn) {
                    pack_curr++;
                    if (pack_curr - pack_top > list_height) pack_top++;
                    relist();
                }
                recog = 1;
            } else if (inp == KEY_UP || inp == 'k') {
                if (pack_curr > 0) {
                    pack_curr--;
                    if (pack_curr < pack_top) pack_top--;
                    relist();
                }
                recog = 1;
            } else if (inp == KEY_NPAGE) {
                pack_curr += list_height; pack_top += list_height;
                if (pack_curr >= pack_maxn) {
                    pack_curr = pack_maxn;
                    pack_top = pack_maxn - list_height;
                    if (pack_top < 0) pack_top = 0;
                }
                relist(); recog = 1;
            } else if (inp == KEY_PPAGE) {
                pack_curr -= list_height; pack_top -= list_height;
                if (pack_top < 0) { pack_curr = 0; pack_top = 0; }
                relist(); recog = 1;
            } else if (inp == '\t' && (list_kind == 3 || list_kind == 4) && pack_maxn >= 0) {
                char *pkg = strdup(view.items[pack_curr].name);
                char sub[400]; snprintf(sub, sizeof(sub), "Invoked: xbps-install %s", pkg);
                char title[400]; snprintf(title, sizeof(title), "Installing package %s", pkg);
                scrnew(title, sub);
                char *argv[] = { "xbps-install", pkg, NULL };
                char **sudoargv = sudo_wrap(argv);
                char *lastline = run_interactive_lastline(sudoargv);
                free(sudoargv);
                if (lastline && lastline[0]) printw("\n%s\n", lastline);
                free(lastline);
                pressakey();
                reload_pl();
                free(pkg);
                recog = 1;
            } else if (inp == KEY_BACKSPACE || inp == 127) {
                scrnew("HELP for xbps-tui", "...a textual front-end for the XBPS package manager");
                printw(
                    "- The main screen shows a list of packages: use the (Pg)Up-Down (cursor-) keys\n"
                    "  to navigate. (j and k also move down and up, vim-style).\n"
                    "- The active element (package) can be inspected with ^P (Control-P: propertiers).\n"
                    "- Depending on the kind of list, the selected package can be removed or installed.\n"
                    "- Use ^L (list) to change what packages are listed.\n\n"
                    "- The list can be refined by filtering by name (^N, case insensitive) and by\n"
                    "  description (^D, case sensitive).\n\n"
                    "- Pressing ^F pops up a menu with other functions. Use arrow keys and Enter to\n"
                    "  select or press a hilighted letter. Cancel with the Esc key.\n\n"
                    "- Synchronize\t\tmakes the repositories up to date (use it once a session)\n"
                    "- Show size...\t\tcalculates installed size of the package now in the screen\n\n");
                pressakey();
                recog = 1;
            }

            if (!recog) {
                char buf[32]; snprintf(buf, sizeof(buf), "<key %d>", inp);
                show(1, screen_height, buf);
                refresh();
                napms(1000);
            }
            continue;
        }

        /* ---- plain (non-special) key handling ---- */

        if (inp == K_FILTNAME) {
            scrollok(stdscr, FALSE);
            addtext(1, -screen_height, "Entering name filter... backspace honored, Esc to clear or Enter to confirm");
            clrtoeol();
            refresh();
            filtname[0] = 0;
            for (;;) {
                show(1, 3, "");
                char b[300]; snprintf(b, sizeof(b), ">%s", filtname);
                put_styled(2, b);
                put_styled(0, "  <- Write name filter (case insensitive)");
                clrtoeol();
                apply_filters();
                relist();
                int c = waitkey();
                if (c == 27) { filtname[0] = 0; apply_filters(); break; }
                if (c == '\n' || c == '\r' || c == KEY_ENTER) break;
                if (c == KEY_BACKSPACE || c == 127 || c == 8) {
                    size_t l = strlen(filtname);
                    if (l > 0) filtname[l - 1] = 0;
                    continue;
                }
                if (is_named_key(c)) break;
                if (c >= 32 && c < 256 && (int)strlen(filtname) < pack_maxnlen - 1) {
                    size_t l = strlen(filtname);
                    if (l < sizeof(filtname) - 1) { filtname[l] = (char)c; filtname[l + 1] = 0; }
                }
            }
            addtext(0, -1, "");
            scrollok(stdscr, TRUE);
            continue;
        }

        if (inp == K_FILTDESC) {
            scrollok(stdscr, FALSE);
            addtext(1, -screen_height, "Entering description filter... backspace honored, Esc to clear or Enter to confirm");
            clrtoeol();
            refresh();
            filtdesc[0] = 0;
            for (;;) {
                show(1, 3, "");
                char b[300]; snprintf(b, sizeof(b), ">%s", filtdesc);
                put_styled(2, b);
                put_styled(0, "  <- Write description filter (case sensitive)");
                clrtoeol();
                apply_filters();
                relist();
                int c = waitkey();
                if (c == 27) { filtdesc[0] = 0; apply_filters(); break; }
                if (c == '\n' || c == '\r' || c == KEY_ENTER) break;
                if (c == KEY_BACKSPACE || c == 127 || c == 8) {
                    size_t l = strlen(filtdesc);
                    if (l > 0) filtdesc[l - 1] = 0;
                    continue;
                }
                if (is_named_key(c)) break;
                if (c >= 32 && c < 256 && (int)strlen(filtdesc) < pack_maxnlen - 1) {
                    size_t l = strlen(filtdesc);
                    if (l < sizeof(filtdesc) - 1) { filtdesc[l] = (char)c; filtdesc[l + 1] = 0; }
                }
            }
            addtext(0, -1, "");
            scrollok(stdscr, TRUE);
            continue;
        }

        if (inp == K_LISTCHANGE) {
            int sel = choosemenu(33, 1, "Choose list type: ", list_kind_desc, 6, redowin);
            if (sel > 0) {
                list_kind = sel - 1;
                reload_pl();
                pack_curr = 0; pack_top = 0;
            }
            inp = 0;
        }

        if (inp == K_REMOVE && pack_maxn >= 0 && (list_kind == 0 || list_kind == 1 || list_kind == 2)) {
            char *pkg = strdup(view.items[pack_curr].name);
            char sub[400]; snprintf(sub, sizeof(sub), "Invoked: xbps-remove %s", pkg);
            scrnew("Remove a package", sub);
            char *argv[] = { "xbps-remove", pkg, NULL };
            char **sudoargv = sudo_wrap(argv);
            char *lastline = run_interactive_lastline(sudoargv);
            free(sudoargv);
            if (lastline && lastline[0]) printw("\n%s\n", lastline);
            free(lastline);
            if (list_kind == 2) {
                char *chk[] = { "xbps-query", "--property", "installed_size", pkg, NULL };
                Pipe p = run_pipe(chk);
                int rc = close_pipe_status(&p);
                if (rc != 0) hm_remove(&pl_instcache, pkg);
            }
            pressakey();
            reload_pl();
            if (pack_curr > pack_maxn) pack_curr = pack_maxn;
            free(pkg);
            inp = 0;
        }

        if (inp == K_KILL && pack_maxn >= 0 && (list_kind == 0 || list_kind == 1 || list_kind == 2)) {
            char *pkg = strdup(view.items[pack_curr].name);
            char sub[400]; snprintf(sub, sizeof(sub), "Invoked: xbps-remove -R %s", pkg);
            scrnew("Kill a package", sub);
            char *argv[] = { "xbps-remove", "-R", pkg, NULL };
            char **sudoargv = sudo_wrap(argv);
            char *lastline = run_interactive_lastline(sudoargv);
            free(sudoargv);
            if (lastline && lastline[0]) printw("\n%s\n", lastline);
            free(lastline);
            if (list_kind == 2) {
                char *chk[] = { "xbps-query", "--property", "installed_size", pkg, NULL };
                Pipe p = run_pipe(chk);
                int rc = close_pipe_status(&p);
                if (rc != 0) hm_remove(&pl_instcache, pkg);
            }
            pressakey();
            reload_pl();
            if (pack_curr > pack_maxn) pack_curr = pack_maxn;
            free(pkg);
            inp = 0;
        }

        if (inp == K_UPGRADE && list_kind == 5 && pack_maxn >= 0) {
            char *pkg = strdup(view.items[pack_curr].name);
            char title[400]; snprintf(title, sizeof(title), "Upgrading package %s", pkg);
            char sub[400]; snprintf(sub, sizeof(sub), "Invoked: xbps-install %s", pkg);
            scrnew(title, sub);
            char *argv[] = { "xbps-install", pkg, NULL };
            char **sudoargv = sudo_wrap(argv);
            char *lastline = run_interactive_lastline(sudoargv);
            free(sudoargv);
            if (lastline && lastline[0]) printw("\n%s\n", lastline);
            free(lastline);
            pressakey();
            reload_pl();
            if (pack_curr > pack_maxn) pack_curr = pack_maxn;
            free(pkg);
            inp = 0;
        }

        if (inp == K_UPGRADEALL && list_kind == 5) {
            scrnew("Upgrade all packages", "Invoked: xbps-install -u");
            char *argv[] = { "xbps-install", "-u", NULL };
            char **sudoargv = sudo_wrap(argv);
            char *lastline = run_interactive_lastline(sudoargv);
            free(sudoargv);
            if (lastline && lastline[0]) printw("\n%s\n", lastline);
            free(lastline);
            pressakey();
            reload_pl();
            if (pack_curr > pack_maxn) pack_curr = pack_maxn;
            inp = 0;
        }

        if (inp == K_INFO && pack_maxn >= 0) {
            char *mypack = strdup(view.items[pack_curr].name);
            char *mydesc = strdup(view.items[pack_curr].desc);
            scrclear();
            printw("\n");
            put_styled(2, "Properties"); printw("\n");
            printw("about package ");
            put_styled(3, mypack);
            printw(" (%s)\n\n", mydesc);

            const char *remote = (list_kind == 3 || list_kind == 4) ? "R" : "";
            char flagbuf[8];

            char **depon = NULL; int depon_n = 0;
            snprintf(flagbuf, sizeof(flagbuf), "-x%s", remote);
            {
                char *argv1[] = { "xbps-query", flagbuf, mypack, NULL };
                Pipe p = run_pipe(argv1);
                if (p.f) {
                    char *line = NULL; size_t cap = 0; ssize_t n;
                    while ((n = getline(&line, &cap, p.f)) > 0) {
                        trim_eol(line);
                        depon = realloc(depon, sizeof(char *) * (size_t)(depon_n + 1));
                        depon[depon_n++] = strdup(line);
                    }
                    free(line);
                }
                close_pipe(&p);
            }

            char **theydep = NULL; int theydep_n = 0;
            snprintf(flagbuf, sizeof(flagbuf), "-X%s", remote);
            {
                char *argv2[] = { "xbps-query", flagbuf, mypack, NULL };
                Pipe p = run_pipe(argv2);
                if (p.f) {
                    char *line = NULL; size_t cap = 0; ssize_t n;
                    while ((n = getline(&line, &cap, p.f)) > 0) {
                        trim_eol(line);
                        theydep = realloc(theydep, sizeof(char *) * (size_t)(theydep_n + 1));
                        theydep[theydep_n++] = strdup(line);
                    }
                    free(line);
                }
                close_pipe(&p);
            }

            char **instfiles = NULL; int instfiles_n = 0;
            snprintf(flagbuf, sizeof(flagbuf), "-f%s", remote);
            {
                char *argv3[] = { "xbps-query", flagbuf, mypack, NULL };
                Pipe p = run_pipe(argv3);
                if (p.f) {
                    char *line = NULL; size_t cap = 0; ssize_t n;
                    while ((n = getline(&line, &cap, p.f)) > 0) {
                        trim_eol(line);
                        instfiles = realloc(instfiles, sizeof(char *) * (size_t)(instfiles_n + 1));
                        instfiles[instfiles_n++] = strdup(line);
                    }
                    free(line);
                }
                close_pipe(&p);
            }

            int usrbin_any = 0;
            for (int i = 0; i < instfiles_n; i++) {
                if (strncmp(instfiles[i], "/usr/bin", 8) == 0) {
                    if (!usrbin_any) { printw("Files in /usr/bin/:"); usrbin_any = 1; }
                    printw(" %s", instfiles[i] + 9);
                }
            }
            if (usrbin_any) printw("\n");

            int bin_any = 0;
            for (int i = 0; i < instfiles_n; i++) {
                if (strncmp(instfiles[i], "/usr/bin", 8) != 0 && strstr(instfiles[i], "/bin/")) {
                    if (!bin_any) { printw("Other bin: "); bin_any = 1; }
                    printw("%s", instfiles[i] + (strlen(instfiles[i]) > 9 ? 9 : 0));
                }
            }
            if (bin_any) printw("\n");
            if (usrbin_any) printw("\n");

            char **already = NULL; int already_n = 0;
            int head = 0;
            for (int i = 0; i < instfiles_n; i++) {
                if (strstr(instfiles[i], "/bin/")) continue;
                char *lastslash = strrchr(instfiles[i], '/');
                char dirbuf[600] = "";
                if (lastslash) {
                    size_t l = (size_t)(lastslash - instfiles[i]);
                    if (l >= sizeof(dirbuf)) l = sizeof(dirbuf) - 1;
                    memcpy(dirbuf, instfiles[i], l); dirbuf[l] = 0;
                }
                if (strncmp(dirbuf, "/usr/share/locale/", 19) == 0) strcpy(dirbuf, "/usr/share/locale");
                if (strncmp(dirbuf, "/usr/share/man/", 15) == 0) strcpy(dirbuf, "/usr/share/man");
                if (strncmp(dirbuf, "/usr/include/", 13) == 0) strcpy(dirbuf, "/usr/include");
                int found = 0;
                for (int k = 0; k < already_n; k++) if (strcmp(already[k], dirbuf) == 0) { found = 1; break; }
                if (!found) {
                    already = realloc(already, sizeof(char *) * (size_t)(already_n + 1));
                    already[already_n++] = strdup(dirbuf);
                    if (!head) { printw("Other dirs:"); head = 1; }
                    printw(" %s", dirbuf);
                }
            }
            if (head) printw("\n\n");

            printw("Size = ");
            refresh();
            char *sz = getpacksize(mypack);
            printw("%s", sz);
            char *repo = getpackinfo(mypack, "repository");
            printw(" (repository %s)\n\n", repo);
            free(sz); free(repo);

            printw("Depends on:\n");
            for (int i = 0; i < depon_n; i++) printw("%s  ", depon[i]);
            printw("\n\n");
            printw("Required by:\n");
            for (int i = 0; i < theydep_n; i++) printw("%s  ", theydep[i]);
            printw("\n");

            pressakey();

            for (int i = 0; i < depon_n; i++) free(depon[i]);
            free(depon);
            for (int i = 0; i < theydep_n; i++) free(theydep[i]);
            free(theydep);
            for (int i = 0; i < instfiles_n; i++) free(instfiles[i]);
            free(instfiles);
            for (int i = 0; i < already_n; i++) free(already[i]);
            free(already);
            free(mypack); free(mydesc);
            inp = 0;
        }

        if (inp == K_FUNCTION) {
            const char *funcs[2] = {
                "Synchronize (!update) repositories",
                "Show !size of visible packages"
            };
            int sel = choosemenu(36, 4, "Choose extended function: ", funcs, 2, redowin);
            if (sel == 1) {
                sync_repos();
                reload_pl();
                if (pack_curr > pack_maxn) pack_curr = pack_maxn;
            } else if (sel == 2) {
                const char *msgs[2] = { "Wait: calculating sizes...", "using xbps-query -S <pack>" };
                scrmsg(msgs, 2);
                int index = pack_top;
                for (int pszs = 4; pszs < screen_height - 1; pszs++) {
                    if (index > pack_maxn) break;
                    char *sz = getpacksize(view.items[index].name);
                    free(sz);
                    index++;
                }
            }
            inp = 0;
        }

        if (inp > 0 && inp < 32) {
            char hexbuf[16]; snprintf(hexbuf, sizeof(hexbuf), "0x%x", inp);
            show(1, screen_height, hexbuf);
            refresh();
            napms(1000);
        }
    }

    scrclear();
    refresh();
    endwin();
    return 0;
}
