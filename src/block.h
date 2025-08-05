#pragma once

#ifndef JSONN_BLOCK_SIZE
#define JSONN_BLOCK_SIZE    4096
#endif

#define JSONN_MAX_ITEM_SIZE (JSONN_BLOCK_SIZE - sizeof(struct block_s))

typedef struct block_s *block;

struct block_s {
        block next;
        block current;
        int item_size;
        int slots_free;
};
