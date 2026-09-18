#ifndef XBPS_TUI_HASHMAP_H
#define XBPS_TUI_HASHMAP_H

#define HM_BUCKETS 4093

typedef struct HMEntry {
    char *key;
    char *value;
    struct HMEntry *next;
} HMEntry;

typedef struct {
    HMEntry *buckets[HM_BUCKETS];
} HashMap;

void hm_init(HashMap *m);
void hm_set(HashMap *m, const char *key, const char *value);
const char *hm_get(HashMap *m, const char *key);
int hm_contains(HashMap *m, const char *key);
void hm_remove(HashMap *m, const char *key);
void hm_clear(HashMap *m);

#endif