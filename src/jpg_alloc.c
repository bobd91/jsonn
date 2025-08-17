#include <stdlib.h>

static void *(*jpg_alloc)(size_t) = malloc;
static void *(*jpg_realloc)(void *, size_t) = realloc;
static void (*jpg_dealloc)(void *) = free;
