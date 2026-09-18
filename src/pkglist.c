#include <stdlib.h>
#include <string.h>

#include "pkglist.h"

void pl_init(PkgList *l) { l->items = NULL; l->count = 0; l->cap = 0; }

void pl_clear(PkgList *l) {
    for (int i = 0; i < l->count; i++) {
        free(l->items[i].stat);
        free(l->items[i].name);
        free(l->items[i].vers);
        free(l->items[i].desc);
    }
    l->count = 0;
}

void pl_push(PkgList *l, const char *stat, const char *name, const char *vers, const char *desc) {
    if (l->count >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 256;
        l->items = realloc(l->items, (size_t)l->cap * sizeof(Pkg));
    }
    l->items[l->count].stat = strdup(stat ? stat : "");
    l->items[l->count].name = strdup(name ? name : "");
    l->items[l->count].vers = strdup(vers ? vers : "");
    l->items[l->count].desc = strdup(desc ? desc : "");
    l->count++;
}