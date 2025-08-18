#include <stdlib.h>

static void *(*jsonpg_alloc)(size_t) = malloc;
static void *(*jsonpg_realloc)(void *, size_t) = realloc;
static void (*jsonpg_dealloc)(void *) = free;
