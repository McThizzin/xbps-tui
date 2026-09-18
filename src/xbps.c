#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>

#include "globals.h"
#include "hashmap.h"
#include "pkglist.h"
#include "proc.h"
#include "util.h"
#include "xbps.h"

char *getpackinfo(const char *pack, const char *prop) {
    const char *remote = (list_kind == 3 || list_kind == 4) ? "R" : "";
    char flag[8]; snprintf(flag, sizeof(flag), "-S%s", remote);
    char *argv[] = { "xbps-query", flag, "--property", (char *)prop, (char *)pack, NULL };
    return run_capture_trim(argv);
}

char *getpacksize(const char *pack) {
    const char *cached = hm_get(&pl_sizecache, pack);
    if (cached && cached[0] != '\0') return strdup(cached);

    char *tmp = getpackinfo(pack, "installed_size");
    size_t len = strlen(tmp);
    char out[32];

    if (len <= 4) {
        char a[3] = {0}, b[3] = {0};
        size_t alen = len >= 2 ? 2 : len;
        strncpy(a, tmp, alen);
        size_t rem = len > 2 ? len - 2 : 0;
        size_t blen = rem >= 2 ? 2 : rem;
        strncpy(b, tmp + (len > 2 ? 2 : len), blen);
        snprintf(out, sizeof(out), " %s %s", a, b);
    } else if (len < 6) {
        char a[4] = {0}, b[4] = {0};
        strncpy(a, tmp, 3);
        size_t blen = len - 3 < 3 ? len - 3 : 3;
        strncpy(b, tmp + 3, blen);
        snprintf(out, sizeof(out), "%s %s", a, b);
    } else if (len == 6) {
        char newun = tmp[4];
        if (newun == 'K' || newun == 'M') {
            char numbuf[5]; strncpy(numbuf, tmp, 4); numbuf[4] = 0;
            long val = atol(numbuf);
            long newsize = val / 100;
            char nsz[32]; snprintf(nsz, sizeof(nsz), "%ld", newsize);
            size_t nl = strlen(nsz);
            char d0 = nl > 0 ? nsz[0] : '0';
            char d1 = nl > 1 ? nsz[1] : '0';
            snprintf(out, sizeof(out), "%c.%c %s", d0, d1, newun == 'K' ? "MB" : "GB");
        } else {
            strncpy(out, tmp, sizeof(out) - 1); out[sizeof(out) - 1] = 0;
        }
    } else {
        strncpy(out, tmp, sizeof(out) - 1); out[sizeof(out) - 1] = 0;
    }
    free(tmp);
    hm_set(&pl_sizecache, pack, out);
    return strdup(out);
}

/* returns strdup(name) on success (and pushes into target), NULL on skip */
char *addpack(PkgList *target, const char *lin_in) {
    char lin[2048];
    strncpy(lin, lin_in, sizeof(lin) - 1); lin[sizeof(lin) - 1] = 0;
    trim_eol(lin);

    char *sp = strchr(lin, ' ');
    if (!sp) return NULL;
    size_t slen = (size_t)(sp - lin);
    char stat[16]; if (slen > sizeof(stat) - 1) slen = sizeof(stat) - 1;
    memcpy(stat, lin, slen); stat[slen] = 0;

    char *rest = sp + 1;
    char *sp2 = strchr(rest, ' ');
    if (!sp2) return NULL;
    size_t namelen = (size_t)(sp2 - rest);
    char namebuf[300]; if (namelen >= sizeof(namebuf)) namelen = sizeof(namebuf) - 1;
    memcpy(namebuf, rest, namelen); namebuf[namelen] = 0;

    char *dash = strrchr(namebuf, '-');
    char name[300] = "", version[128] = "";
    if (dash) {
        strncpy(version, dash + 1, sizeof(version) - 1);
        size_t nlen = (size_t)(dash - namebuf);
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, namebuf, nlen); name[nlen] = 0;
    } else {
        strncpy(name, namebuf, sizeof(name) - 1);
    }

    if (list_kind == 3 || list_kind == 4) {
        if (hm_contains(&pl_instcache, name)) return NULL;
    }
    if (list_kind == 1 || list_kind == 4) {
        if (strncmp(name, "lib", 3) == 0) return NULL;
    }

    char *desc = sp2;
    while (*desc == ' ') desc++;
    char descbuf[1024];
    strncpy(descbuf, desc, sizeof(descbuf) - 1); descbuf[sizeof(descbuf) - 1] = 0;
    size_t dl = strlen(descbuf);
    while (dl > 0 && (descbuf[dl - 1] == ' ')) descbuf[--dl] = 0;

    pl_push(target, stat, name, version, descbuf);
    if ((int)strlen(name) > pack_maxnlen) pack_maxnlen = (int)strlen(name);
    if ((int)strlen(version) > pack_maxvlen) pack_maxvlen = (int)strlen(version);
    hm_set(&pl_namecache, name, descbuf);
    if (!hm_contains(&pl_sizecache, name)) hm_set(&pl_sizecache, name, "");
    return strdup(name);
}

void reload_pl(void) {
    pl_clear(&rawpl);
    pack_maxnlen = 0;
    pack_maxvlen = 0;

    if (list_kind <= 1) {
        hm_clear(&pl_instcache);
        char *argv[] = { "xbps-query", "-l", NULL };
        Pipe p = run_pipe(argv);
        if (p.f) {
            char *line = NULL; size_t cap = 0; ssize_t n;
            while ((n = getline(&line, &cap, p.f)) > 0) {
                char *inst = addpack(&rawpl, line);
                if (inst) { hm_set(&pl_instcache, inst, "1"); free(inst); }
            }
            free(line);
        }
        close_pipe(&p);
    }

    if (list_kind == 2) {
        char *argv[] = { "xbps-query", "-O", NULL };
        Pipe p = run_pipe(argv);
        if (p.f) {
            char *line = NULL; size_t cap = 0; ssize_t n;
            while ((n = getline(&line, &cap, p.f)) > 0) {
                trim_eol(line);
                char name[300] = "", version[128] = "";
                split_name_version(line, name, sizeof(name), version, sizeof(version));
                if ((int)strlen(name) > pack_maxnlen) pack_maxnlen = (int)strlen(name);
                if ((int)strlen(version) > pack_maxvlen) pack_maxvlen = (int)strlen(version);
                const char *desc = hm_get(&pl_namecache, name);
                pl_push(&rawpl, "o", name, version, desc ? desc : "??");
                if (!hm_contains(&pl_sizecache, name)) hm_set(&pl_sizecache, name, "");
            }
            free(line);
        }
        close_pipe(&p);
    }

    if (list_kind == 3 || list_kind == 4) {
        char *argv[] = { "xbps-query", "-Rs", "*", NULL };
        Pipe p = run_pipe(argv);
        if (p.f) {
            char *line = NULL; size_t cap = 0; ssize_t n;
            while ((n = getline(&line, &cap, p.f)) > 0) {
                char *r = addpack(&rawpl, line);
                if (r) free(r);
            }
            free(line);
        }
        close_pipe(&p);
    }

    if (list_kind == 5) {
        HashMap upgr_name, upgr_oper, upgr_nver;
        hm_init(&upgr_name); hm_init(&upgr_oper); hm_init(&upgr_nver);

        {
            char *argv[] = { "xbps-install", "-nu", NULL };
            Pipe p = run_pipe(argv);
            if (p.f) {
                char *line = NULL; size_t cap = 0; ssize_t n;
                while ((n = getline(&line, &cap, p.f)) > 0) {
                    trim_eol(line);
                    char *sp = strchr(line, ' ');
                    if (!sp) continue;
                    size_t namelen = (size_t)(sp - line);
                    char namebuf[300]; if (namelen >= sizeof(namebuf)) namelen = sizeof(namebuf) - 1;
                    memcpy(namebuf, line, namelen); namebuf[namelen] = 0;
                    char name[300] = "", version[128] = "";
                    split_name_version(namebuf, name, sizeof(name), version, sizeof(version));
                    hm_set(&upgr_name, name, "1");
                    hm_set(&upgr_nver, name, version);
                    if ((int)strlen(name) > pack_maxnlen) pack_maxnlen = (int)strlen(name);
                    if ((int)strlen(version) > pack_maxvlen) pack_maxvlen = (int)strlen(version);

                    char *r2 = sp; while (*r2 == ' ') r2++;
                    char *sp3 = strchr(r2, ' ');
                    size_t oplen = sp3 ? (size_t)(sp3 - r2) : strlen(r2);
                    char operbuf[32]; if (oplen >= sizeof(operbuf)) oplen = sizeof(operbuf) - 1;
                    memcpy(operbuf, r2, oplen); operbuf[oplen] = 0;
                    if (strcmp(operbuf, "hold") == 0) strcpy(operbuf, " HOLD ");
                    hm_set(&upgr_oper, name, operbuf);
                }
                free(line);
            }
            close_pipe(&p);
        }
        {
            char *argv[] = { "xbps-query", "-l", NULL };
            Pipe p = run_pipe(argv);
            if (p.f) {
                char *line = NULL; size_t cap = 0; ssize_t n;
                while ((n = getline(&line, &cap, p.f)) > 0) {
                    trim_eol(line);
                    char *sp = strchr(line, ' '); if (!sp) continue;
                    char *rest = sp + 1;
                    char *sp2 = strchr(rest, ' '); if (!sp2) continue;
                    size_t namelen = (size_t)(sp2 - rest);
                    char namebuf[300]; if (namelen >= sizeof(namebuf)) namelen = sizeof(namebuf) - 1;
                    memcpy(namebuf, rest, namelen); namebuf[namelen] = 0;
                    char name[300] = "", version[128] = "";
                    split_name_version(namebuf, name, sizeof(name), version, sizeof(version));
                    if (!hm_contains(&upgr_name, name)) continue;

                    char *dstart = sp2; while (*dstart == ' ') dstart++;
                    char descbuf[1024];
                    strncpy(descbuf, dstart, sizeof(descbuf) - 1); descbuf[sizeof(descbuf) - 1] = 0;
                    size_t dl = strlen(descbuf);
                    while (dl > 0 && descbuf[dl - 1] == ' ') descbuf[--dl] = 0;

                    const char *oper = hm_get(&upgr_oper, name);
                    const char *nver = hm_get(&upgr_nver, name);
                    char finaldesc[1200];
                    snprintf(finaldesc, sizeof(finaldesc), "(%s) %s", oper ? oper : "", descbuf);
                    pl_push(&rawpl, version, name, nver ? nver : "", finaldesc);
                    if (!hm_contains(&pl_sizecache, name)) hm_set(&pl_sizecache, name, "");
                }
                free(line);
            }
            close_pipe(&p);
        }
        hm_clear(&upgr_name); hm_clear(&upgr_oper); hm_clear(&upgr_nver);
    }

    pack_maxnlen += 1;
    if (pack_maxnlen > 26) pack_maxnlen = 26;
    pack_maxvlen += 1;

    apply_filters();
}

void apply_filters(void) {
    pl_clear(&view);
    for (int i = 0; i < rawpl.count; i++) {
        const char *name = rawpl.items[i].name;
        const char *desc = rawpl.items[i].desc;
        if (filtname[0] && !strcasestr(name, filtname)) continue;
        if (filtdesc[0] && !strstr(desc, filtdesc)) continue;
        pl_push(&view, rawpl.items[i].stat, name, rawpl.items[i].vers, desc);
    }
    pack_maxn = view.count - 1;
    pack_curr = 0;
    pack_top = 0;
}

int check_xbps_available(void) {
    int devnull = open("/dev/null", O_WRONLY);
    pid_t pid = fork();
    if (pid == 0) {
        if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); }
        execlp("xbps-query", "xbps-query", "-V", (char *)NULL);
        _exit(127);
    }
    if (devnull >= 0) close(devnull);
    int status = 0;
    if (pid > 0) waitpid(pid, &status, 0);
    return pid > 0 && WIFEXITED(status) && WEXITSTATUS(status) != 127;
}