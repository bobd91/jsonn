#include <stdlib.h>

static void *(*jsonn_alloc)(size_t) = malloc;
static void *(*jsonn_realloc)(void *, size_t) = realloc;
static void (*jsonn_dealloc)(void *) = free;
