#define JSONN_BLOCK_SIZE 4096

typedef struct node_block_s *node_block;

struct node_block_s {
        node_block next;
        node_block current;
        int free_nodes;
};

static const int nodes_per_block = 
                (JSONN_BLOCK_SIZE - sizeof(struct node_block_s))
                 / sizeof(struct jsonn_node_s);

static node_block root_block(jsonn_node root)
{
        return ((node_block)root) - 1;
}

static jsonn_node node_add(jsonn_node root, jsonn_type type) 
{
        node_block block = root_block(root);
        node_block current = block->current;

        if(!current->free_nodes) {
                node_block new  = jsonn_alloc(JSONN_BLOCK_SIZE);
                if(!new) return NULL;

                new->next = NULL;
                new->free_nodes = nodes_per_block;
                current->next = new;
                block->current = new;
                current = new;
        }
        jsonn_node n = ((jsonn_node)(current + 1)) 
                + (nodes_per_block - current->free_nodes);

        current->free_nodes--;
        n->type = type;
        return n;
}

static jsonn_node jsonn_root_node()
{
        node_block block  = jsonn_alloc(JSONN_BLOCK_SIZE);
        if(!block) return NULL;

        block->next = NULL;
        block->current = block;
        block->free_nodes = nodes_per_block - 1;

        jsonn_node root = (jsonn_node)(block + 1);

        root->type = JSONN_ROOT;

        return root;
}

static int node_null(void *root)
{       
        return NULL != node_add((jsonn_node)root, JSONN_NULL);
}

static int node_boolean(void *root, int boolean)
{
        return NULL != node_add((jsonn_node)root, boolean ? JSONN_TRUE : JSONN_FALSE);
}

static int node_integer(void *root, long integer)
{
        jsonn_node n = node_add((jsonn_node)root, JSONN_INTEGER);
        if(!n) return 1;

        n->is.number.integer = integer;
        return 0;
}


static int node_real(void *root, double real)
{
        jsonn_node n = node_add((jsonn_node)root, JSONN_REAL);
        if(!n) return 1;

        n->is.number.real = real;
        return 0;
}

static int node_string(void *root, uint8_t *bytes, size_t length)
{
        jsonn_node n = node_add((jsonn_node)root, JSONN_STRING);
        if(!n) return 1;

        n->is.string.bytes = bytes;
        n->is.string.length = length;
        return 0;
}

static int node_key(void *root, uint8_t *bytes, size_t length)
{
        jsonn_node n = node_add((jsonn_node)root, JSONN_KEY);
        if(!n) return 1;

        n->is.string.bytes = bytes;
        n->is.string.length = length;
        return 0;
}

static int node_begin_array(void *root)
{
        return NULL != node_add((jsonn_node)root, JSONN_BEGIN_ARRAY);
}

static int node_end_array(void *root)
{
        return NULL != node_add((jsonn_node)root, JSONN_END_ARRAY);
}

static int node_begin_object(void *root)
{
        return NULL != node_add((jsonn_node)root, JSONN_BEGIN_OBJECT);
}

static int node_end_object(void *root)
{
        return NULL != node_add((jsonn_node)root, JSONN_END_OBJECT);
}

static jsonn_callbacks node_callbacks = {
        .boolean = node_boolean,
        .null = node_null,
        .integer = node_integer,
        .real = node_real,
        .string = node_string,
        .key = node_key,
        .begin_array = node_begin_array,
        .end_array = node_end_array,
        .begin_object = node_begin_object,
        .end_object = node_end_object
};      

jsonn_visitor jsonn_tree_builder() {
        jsonn_visitor visitor = {
                .callbacks = &node_callbacks,
                .ctx = jsonn_root_node()
        };
        return visitor;
}

jsonn_node jsonn_parse_tree(jsonn_parser p,
                uint8_t *json,
                size_t length)
{
        jsonn_visitor visitor = jsonn_tree_builder();
        jsonn_node root = (jsonn_node)(visitor.ctx);
        if(!root) {
                error(p, JSONN_ERROR_ALLOC);
                return NULL;
        };

        jsonn_type type = jsonn_parse(p, json, length, &visitor);
        if(type != JSONN_EOF) {
                jsonn_tree_free(root);
                return NULL;
        }

        return root;
}

int jsonn_tree_visit(jsonn_node root, jsonn_visitor *visitor)
{
        node_block block = root_block(root);
        int abort = 0;
        while(block) {
                jsonn_node node = (jsonn_node)(block + 1);
                int nodes = nodes_per_block - block->free_nodes;
                for(int i = 0 ; i < nodes ; i++) {
                        abort = visit(visitor, node->type, &node->is);
                        if(abort)
                                break;
                        node++;
                }
                block = block->next;
        }
        return abort;
}

void jsonn_tree_free(jsonn_node root)
{
        if(root->type != JSONN_ROOT)
                return;

        node_block block = root_block(root);
        node_block next;
        while(block) {
                next = block->next;
                jsonn_dealloc(block);
                block = next;
        }
}

#ifdef JSONN_NODE_TESTING
#include <stdio.h>

static void print_block(node_block block)
{
        printf("{\n");
        printf("Block   : %p\n", block);
        printf("Next    : %p\n", block->next);
        printf("Current : %p\n", block->current);
        printf("Free    : %d\n", block->free_nodes);
        printf("}\n");
}

static void print_block_info(jsonn_node root)
{
        node_block block = root_block(root);
        while(block) {
                print_block(block);
                block = block->next;
        }
}



int main(int argc, char *argv[])
{
        printf("Size of block   : %ld\n", sizeof(struct node_block_s));
        printf("Size of node    : %ld\n", sizeof(struct jsonn_node_s));
        printf("Nodes per block : %d\n", nodes_per_block);

        jsonn_node root = jsonn_root_node();

        print_block_info(root);

        node_begin_array(root);

        print_block_info(root);

        node_boolean(root, 1);

        print_block_info(root);

        node_null(root);

        print_block_info(root);

        node_begin_object(root);

        node_key(root, "xxx", 3);

        node_integer(root, 5);

        node_end_object(root);

        node_end_array(root);

        print_block_info(root);

        jsonn_root_free(root);
}
#endif
