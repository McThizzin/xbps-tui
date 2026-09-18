#ifndef XBPS_TUI_PKGLIST_H
#define XBPS_TUI_PKGLIST_H

typedef struct {
    char *stat;
    char *name;
    char *vers;
    char *desc;
} Pkg;

typedef struct {
    Pkg *items;
    int count;
    int cap;
} PkgList;

void pl_init(PkgList *l);
void pl_clear(PkgList *l);
void pl_push(PkgList *l, const char *stat, const char *name, const char *vers, const char *desc);

#endif