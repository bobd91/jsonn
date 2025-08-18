#include <stdlib.h>

static void *(*jsonpg_alloc)(size_t) = malloc;
static void *(*jsonpg_realloc)(void *, size_t) = realloc;
static void (*jsonpg_dealloc)(void *) = free;

void jsonpg_set_allocators(
                void *(*malloc)(size_t),
                void *(*realloc)(void *, size_t),
                void (*free)(void *))
{
        jsonpg_alloc = malloc;
        jsonpg_realloc = realloc;
        jsonpg_dealloc = free;
}
