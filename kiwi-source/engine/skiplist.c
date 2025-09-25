//Theofanis Tompolis 4855
//Athanasios Fytilis 5381
//Marinos Aristidou 5397
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include "skiplist.h"
#include "config.h"
#include "utils.h"
#include "indexer.h"

#define cmp_lt(node, key, klen) (comparator((const char *)node, key, klen) < 0)
#define cmp_eq(node, key, klen) (comparator((const char *)node, key, klen) == 0)

SkipList* skiplist_new(size_t max_count)
{
    int i;
    SkipList* self = calloc(1, sizeof(SkipList));

    if (!self)
        PANIC("NULL allocation");

    self->max_count = max_count;
    self->arena = arena_new();
    self->allocated = 0;

    self->hdr = arena_alloc(self->arena, SKIPNODE_SIZE + SKIPLIST_MAXLEVEL * sizeof(SkipNode*));
    self->level = 0;

#ifdef BACKGROUND_MERGE
    pthread_mutex_init(&self->lock, NULL);
    self->refcount = 0;
#endif

    for (i = 0; i <= SKIPLIST_MAXLEVEL; i++)
        self->hdr->forward[i] = self->hdr;

    return self;
}

void skiplist_acquire(SkipList* self)
{
#ifdef BACKGROUND_MERGE
    pthread_mutex_lock(&self->lock);
    self->refcount++;
    pthread_mutex_unlock(&self->lock);
#endif
}

void skiplist_release(SkipList* self)
{
#ifdef BACKGROUND_MERGE
    if (!self) return;

    pthread_mutex_lock(&self->lock);

    if (self->refcount <= 0) {
        // probale na paei na to kanei free diplo, bga warning kai min kaneis tipota
        fprintf(stderr, "[WARNING] Tried to release skiplist with refcount=%d\n", self->refcount);
        pthread_mutex_unlock(&self->lock);
        return;
    }

    self->refcount--;

    if (self->refcount == 0)
    {
        INFO("SkipList refcount is at 0. Freeing up the structure");

        // Ta dedomena exoun desmeutei me arena_alloc -> den kanoume free sto data
        // To arena_free tha katharisei ta panta

        skiplist_free(self);
    }

    pthread_mutex_unlock(&self->lock);
#endif
}

void skiplist_free(SkipList* self)
{
    arena_free(self->arena);
    //free(self->hdr); // arena ton eixe idi kanei alloc, oxi emείς
    free(self);
}

static inline int comparator(const char *encoded, const char *key, size_t klen)
{
    uint32_t encoded_len = 0;
    encoded = get_varint32(encoded, encoded + 5, &encoded_len);
    return string_cmp(encoded, key, encoded_len, klen);
}

static size_t skipnode_size(SkipNode* node)
{
    uint32_t encoded_len = 0;
    const char *end = get_varint32(node->data, node->data + 5, &encoded_len);
    end += encoded_len;
    end = get_varint32(end, end + 5, &encoded_len);

    if (encoded_len > 1)
        end += encoded_len - 1;

    return end - node->data;
}

int skiplist_insert(SkipList* self, const char *key, size_t klen, OPT opt, char *data)
{
    int i, new_level;
    SkipNode* update[SKIPLIST_MAXLEVEL];
    SkipNode* x;

    x = self->hdr;
    for (i = self->level; i >= 0; i--)
    {
        while (x->forward[i] != self->hdr &&
               cmp_lt(x->forward[i]->data, key, klen))
            x = x->forward[i];
        update[i] = x;
    }

    x = x->forward[0];

    if (x != self->hdr && cmp_eq(x->data, key, klen))
    {
        // Βρέθηκε ήδη αυτό το key — αγνόησέ το, ΔΕΝ κάνουμε αντικατάσταση!
        return STATUS_OK;
    }

    self->count++;

    for (new_level = 0; rand() < RAND_MAX/2 && new_level < SKIPLIST_MAXLEVEL - 1; new_level++);

    if (new_level > self->level)
    {
        for (i = self->level + 1; i <= new_level; i++)
            update[i] = self->hdr;
        self->level = new_level;
    }

    if ((x = arena_alloc(self->arena, SKIPNODE_SIZE + new_level * sizeof(SkipNode*))) == NULL)
        PANIC("NULL allocation");

    x->data = data;
    self->allocated += skipnode_size(x);

    for (i = 0; i <= new_level; i++)
    {
        x->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = x;
    }

    return STATUS_OK;
}

SkipNode* skiplist_last(SkipList* self)
{
    int i;
    SkipNode* x = self->hdr;

    for (i = self->level; i >= 0; i--)
    {
        while (x->forward[i] != self->hdr)
            x = x->forward[i];
    }

    return x;
}

SkipNode* skiplist_first(SkipList* self)
{
    return self->hdr->forward[0];
}

SkipNode* skiplist_lookup_prev(SkipList* self, char* key, size_t klen)
{
    int i;
    SkipNode* x = self->hdr;

    for (i = self->level; i >= 0; i--)
    {
        while (x->forward[i] != self->hdr &&
               cmp_lt(x->forward[i]->data, key, klen))
            x = x->forward[i];
    }

    x = x->forward[0];
    if (x != self->hdr/* && cmp_eq(x->data, key, klen)*/)
        return x;
    return NULL;
}

SkipNode* skiplist_lookup(SkipList* self, char* key, size_t klen)
{
    int i;
    SkipNode* x = self->hdr;

    for (i = self->level; i >= 0; i--)
    {
        while (x->forward[i] != self->hdr &&
               cmp_lt(x->forward[i]->data, key, klen))
            x = x->forward[i];
    }

    x = x->forward[0];
    if (x != self->hdr && cmp_eq(x->data, key, klen))
        return x;
    return NULL;
}