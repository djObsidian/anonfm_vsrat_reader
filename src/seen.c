#include "seen.h"

#include <stdlib.h>
#include <string.h>

struct afm_slot {
    unsigned long hash;     /* 0 means empty */
    char         *key;      /* malloc'd copy, NUL-terminated */
    size_t        key_len;
};

struct afm_seen {
    struct afm_slot *slots;
    size_t           cap;   /* power of two */
    size_t           used;
};

static unsigned long afm_fnv1a(const char *data, size_t len)
{
    /* 32-bit FNV-1a, kept in unsigned long (>=32 bits per C89). */
    unsigned long h = 2166136261UL;
    size_t        i;
    for (i = 0; i < len; ++i) {
        h ^= (unsigned char)data[i];
        h *= 16777619UL;
        h &= 0xFFFFFFFFUL;
    }
    if (h == 0) h = 1;  /* avoid sentinel */
    return h;
}

struct afm_seen *afm_seen_new(void)
{
    struct afm_seen *s = (struct afm_seen *)malloc(sizeof(*s));
    if (s == NULL) return NULL;
    s->cap   = 64;
    s->used  = 0;
    s->slots = (struct afm_slot *)calloc(s->cap, sizeof(*s->slots));
    if (s->slots == NULL) { free(s); return NULL; }
    return s;
}

void afm_seen_free(struct afm_seen *s)
{
    size_t i;
    if (s == NULL) return;
    for (i = 0; i < s->cap; ++i) free(s->slots[i].key);
    free(s->slots);
    free(s);
}

static int afm_seen_grow(struct afm_seen *s)
{
    size_t           old_cap = s->cap;
    struct afm_slot *old     = s->slots;
    size_t           new_cap = old_cap * 2;
    struct afm_slot *ns      = (struct afm_slot *)calloc(new_cap, sizeof(*ns));
    size_t           i;
    if (ns == NULL) return -1;
    s->slots = ns;
    s->cap   = new_cap;
    s->used  = 0;
    for (i = 0; i < old_cap; ++i) {
        if (old[i].hash != 0) {
            size_t        idx  = (size_t)(old[i].hash & (new_cap - 1));
            while (s->slots[idx].hash != 0) idx = (idx + 1) & (new_cap - 1);
            s->slots[idx] = old[i];
            ++s->used;
        }
    }
    free(old);
    return 0;
}

int afm_seen_check_and_add(struct afm_seen *s, const char *key, size_t key_len)
{
    unsigned long h;
    size_t        idx;
    size_t        mask;
    char         *copy;

    if (s == NULL || key == NULL) return -1;

    if (s->used * 2 >= s->cap) {
        if (afm_seen_grow(s) != 0) return -1;
    }

    h    = afm_fnv1a(key, key_len);
    mask = s->cap - 1;
    idx  = (size_t)(h & mask);
    while (s->slots[idx].hash != 0) {
        if (s->slots[idx].hash == h
            && s->slots[idx].key_len == key_len
            && memcmp(s->slots[idx].key, key, key_len) == 0) {
            return 1;  /* already present */
        }
        idx = (idx + 1) & mask;
    }

    copy = (char *)malloc(key_len + 1);
    if (copy == NULL) return -1;
    memcpy(copy, key, key_len);
    copy[key_len] = '\0';

    s->slots[idx].hash    = h;
    s->slots[idx].key     = copy;
    s->slots[idx].key_len = key_len;
    ++s->used;
    return 0;
}
