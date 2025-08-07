#include <stddef.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int block_max_item_size()
{
        return JSONN_MAX_ITEM_SIZE;
}
#pragma GCC diagnostic pop

static int items_per_block(size_t item_size)
{
        return JSONN_MAX_ITEM_SIZE / item_size;
}

static int item_count(block blk)
{
        return items_per_block(blk->item_size) - blk->slots_free;
}

static void *next_item_ptr(block blk)
{
        int size = blk->item_size;
        int free = blk->slots_free;
        int max = items_per_block(size);

        return ((void *)blk) + sizeof(struct block_s) + size * (max - free); 
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static block block_root(void *root_item)
{
        return root_item - sizeof(struct block_s);
}
#pragma GCC diagnostic pop

static block block_new(size_t item_size)
{
        block blk  = jsonn_alloc(JSONN_BLOCK_SIZE);
        if(!blk) return NULL;

        blk->next = NULL;
        blk->current = blk;
        blk->item_size = item_size;
        blk->slots_free = items_per_block(item_size);

        return blk;
}

static void *block_item_new(block block_root) 
{
        block current = block_root->current;

        if(!current->slots_free) {
                block new  = block_new(current->item_size);
                if(!new) return NULL;

                current->next = new;
                block_root->current = new;
                current = new;
        }

        void *item = next_item_ptr(current);
        current->slots_free--;
        return item;
}

static void *first_item(block blk)
{
        return ((void *)blk) + sizeof(struct block_s);
}

static void block_for_items(block block_root, void (*fn)(void *, void *), void *ctx)
{
        block blk = block_root;
        int ipb = items_per_block(blk->item_size);
        while(blk) {
                void *item = ((void *)blk) + sizeof(struct block_s);
                void *end = item + (ipb - blk->slots_free);
                while(item < end) {
                        fn(item, ctx);
                        item += blk->item_size;
                }
                blk = blk->next;
        }
}

static void block_root_free(block block_root)
{
        block blk = block_root;
        block next;
        while(blk) {
                next = blk->next;
                jsonn_dealloc(blk);
                blk = next;
        }
}

