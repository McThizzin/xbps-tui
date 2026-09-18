#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

static unsigned long hm_hash(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++)) h = ((h << 5) + h) + (unsigned long)c;
    return h % HM_BUCKETS;
}

void hm_init(HashMap *m) { memset(m->buckets, 0, sizeof(m->buckets)); }

void hm_set(HashMap *m, const char *key, const char *value) {
    unsigned long h = hm_hash(key);
    for (HMEntry *e = m->buckets[h]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            free(e->value);
            e->value = strdup(value ? value : "");
            return;
        }
    }
    HMEntry *e = malloc(sizeof(HMEntry));
    e->key = strdup(key);
    e->value = strdup(value ? value : "");
    e->next = m->buckets[h];
    m->buckets[h] = e;
}

const char *hm_get(HashMap *m, const char *key) {
    unsigned long h = hm_hash(key);
    for (HMEntry *e = m->buckets[h]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e->value;
    return NULL;
}

int hm_contains(HashMap *m, const char *key) { return hm_get(m, key) != NULL; }

void hm_remove(HashMap *m, const char *key) {
    unsigned long h = hm_hash(key);
    HMEntry **pp = &m->buckets[h];
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            HMEntry *dead = *pp;
            *pp = dead->next;
            free(dead->key);
            free(dead->value);
            free(dead);
            return;
        }
        pp = &(*pp)->next;
    }
}

void hm_clear(HashMap *m) {
    for (int i = 0; i < HM_BUCKETS; i++) {
        HMEntry *e = m->buckets[i];
        while (e) {
            HMEntry *n = e->next;
            free(e->key);
            free(e->value);
            free(e);
            e = n;
        }
        m->buckets[i] = NULL;
    }
}