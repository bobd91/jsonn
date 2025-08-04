#include <stddef.h>

#define JSN_BLOCK_SIZE    4096
#define JSN_MAX_ITEM_SIZE JSN_BLOCK_SIZE - sizeof(struct block_s); 

#ifdef JSN_BLOCK_TESTING
void *jsn_alloc(size_t size);
void jsn_dealloc(void *ptr);
#endif

typedef struct block_s *block;

struct block_s {
        block next;
        block current;
        int size;
        int free;
};

static int block_max_item_size()
{
        return JSN_MAX_ITEM_SIZE;
}

static int items_per_block(size_t size)
{
        return JSN_MAX_ITEM_SIZE / size;
}

static void *next_item_ptr(block blk)
{
        int size = blk->size;
        int free = blk->free;
        int max = items_per_block(size);

        return ((void *)blk) + sizeof(struct block_s) + size * (max - free); 
}

static block block_root(void *root_item)
{
        return root_item - sizeof(struct block_s);
}

static block block_new(size_t size)
{
        block blk  = jsn_alloc(JSN_BLOCK_SIZE);
        if(!blk) return NULL;

        blk->next = NULL;
        blk->current = blk;
        blk->size = size;
        blk->free = items_per_block(size);

        return blk;
}

static void *block_item_new(block root_block) 
{
        block current = root_block->current;

        if(!current->free) {
                block new  = block_new(current->size);
                if(!new) return NULL;

                current->next = new;
                root_block->current = new;
                current = new;
        }

        void *item = next_item_ptr(current);
        current->free--;
        return item;
}

static void block_for_items(block root_block, void (*fn)(void *))
{
        block blk = root_block;
        int ipb = items_per_block(blk->size);
        while(blk) {
                void *item = ((void *)root_block) + sizeof(struct block_s);
                void *end = item + (ipb - blk->free);
                while(item < end) {
                        fn(item);
                        item++;
                }
                blk = blk->next;
        }
}

static void block_free(block root_block)
{
        block blk = root_block;
        block next;
        while(blk) {
                next = blk->next;
                jsn_dealloc(blk);
                blk = next;
        }
}


#ifdef JSN_BLOCK_TESTING
#include <stdio.h>
#include <stdlib.h>

void *jsn_alloc(size_t size)
{
        return malloc(size);
}

void jsn_dealloc(void *ptr)
{
        free(ptr);
}

static void print_block(block blk)
{
        printf("{\n");
        printf("Block   : %p\n", blk);
        printf("Next    : %p\n", blk->next);
        printf("Current : %p\n", blk->current);
        printf("Free    : %d\n", blk->free);
        printf("}\n");
}

static void print_block_info(void *root_item)
{
        block blk = block_root(root_item);
        while(blk) {
                print_block(blk);
                blk = blk->next;
        }
}


typedef struct block_test_s {
        char t[2000];
} *block_test;



int main(int argc, char *argv[])
{

        printf("Size of block   : %ld\n", sizeof(struct block_s));
        printf("Size of item    : %ld\n", sizeof(struct block_test_s));
        printf("Items per block : %d\n", items_per_block(sizeof(struct block_test_s)));

        block root_block = block_new(sizeof(struct block_test_s));
        block_test b1 = block_item_new(root_block);
        block_test b2 = block_item_new(root_block);
        block_test b3 = block_item_new(root_block);

        print_block_info(b1);

        block_free(root_block);
}

#endif
