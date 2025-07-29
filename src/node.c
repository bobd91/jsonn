
#define JSONN_NODE_BLOCK_SIZE 4096

typdef union {
        int64_t integer;
        double real;
        void *ptr;
} jsonn_node_value;

typedef enum {
        JSONN_NODE_NULL,
        JSONN_NODE_BOOLEAN,
        JSONN_NODE_INTEGER,
        JSONN_NODE_REAL,
        JSONN_NODE_STRING,
        JSONN_NODE_ARRAY,
        JSONN_NODE_OBJECT,
        JSONN_NODE_OBJECT_MEMBER,
        JSONN_NONE_KEY,
        // internal use
        JSONN_NODE_TEE,
        JSONN_NODE_ROOT,
} jsonn_node_type;

typedef struct jsonn_node_s {
        jsonn_node_value value;
        jsonn_node_s *node;
        jsonn_node_s *next;
        json_node_type type;
        int extra;
};

static const nodes_per_block = 
                JSONN_NODE_BLOCK_SIZE / sizeof(struct jsonn_node_s);

typedef struct jsonn_node_s *jsonn_node;


int on_null(jsonn_node root)
{       
        if(!node_add(root, JSONN_NODE_NULL))
                return 1;
        return 0;
}

int on_boolean(jsonn_node root, int boolean)
{
        jsonn_node n = node_add(root, JSONN_NODE_BOOLEAN);
        if(!n) return 1;

        n->value.integer = boolean;
        return 0;
}

int on_integer(jsonn_node root, long integer)
{
        jsonn_node n = node_add(root, JSONN_NODE_INTEGER);
        if(!n) return 1;

        n->value.integer = integer;
        return 0;
}


int on_real(jsonn_node root, double real)
{
        jsonn_node n = node_add(root, JSONN_NODE_REAL);
        if(!n) return 1;

        n->value.real = real;
        return 0;
}

int on_string(jsonn_node root, jsonn_string string)
{
        jsonn_node n = node_add(root, JSONN_NODE_STRING);
        if(!n) return 1;

        n->value.integer = string.length;
        n->value.ptr = string.bytes;
        return 0;
}

int on_key(jsonn_node root, jsonn_string string)
{
        jsonn_node n = node_add(root, JSONN_NODE_KEY);
        if(!n) return 1;

        n->value.integer = string.length;
        n->value.ptr = string.bytes;
        return 0;
}

int on_begin_array(jsonn_node root)
{
        if(!node_push(root, JSONN_NODE_ARRAY))
                return 1;
        return 0;
}

int on_end_array(jsonn_node root)
{
        node_pop(root);
        return 0;
}

int on_begin_object(jsonn_node root)
{
        if(!node_push(root, JSONN_NODE_OBJECT))
                return 1;
        return 0;
}

int on_end_object(jsonn_node root)
{
        node_pop(root);
        return 0;
}

int on_error(jsonn_node root, jsonn_error error)
{
        node_reset(root);
        return 1;
}

jsonn_node node_add(jsonn_node root, jsonn_node_type type) 
{
        jsonn_node n = node_alloc(root, type);
        if(!n) return NULL;

        if(!root->node) {
                root->node = n;
                root->next = n;
                n->node = root;
        } else {
                root->next->next = n;
                n->node = root->next->node;
        }
        
        if(root->next->type == JSONN_NODE_KEY
                        && !is_container(type))
                // non-container value after key:
                // nothing should be written after value
                // so we must redirect root->next
                root->next = root->next->node->node;
        else
                root->next = n;

        return n;
}

int is_container(jsonn_node node)
{
        return node == JSONN_NODE_ARRAY || JSONN_NODE_OBJECT;
}

jsonn_node node_push(jsonn_node root, jsonn_node_type type) 
{
        json_node n;

        if(root->next->type == JSONN_NODE_ARRAY) {
                // Pushing array or object into 
                // Array element requires an extra, internal, element
                jsonn_node e = node_add(root, JSONN_NODE_TEE);
                if(!e) return NULL;

                n = node_alloc(root, type);
                if(!n) return NULL;

                n->node = e;
                e->value.ptr = n;
                root->next = n;
        } else {
                n = node_add(root, type);
        }
        return n;
}


void node_pop(jsonn_node root)
{
        root->next = root->next->node->node;
}

jsonn_node jsonn_node_new()
{
        jsonn_node alloc = jsonn_alloc(JSONN_NODE_BLOCK_SIZE);
        if(!alloc) return NULL;

        alloc->extra = nodes_per_block - 2;
        alloc->node = alloc;
        alloc->next = NULL;
        
        jsonn_node root = alloc + 1;
        root->type = JSONN_NODE_ROOT;
        root->node = NULL;
        root->next = NULL;

        return root;
}

jsonn_node node_add_block(jsonn_node root)
{
        jsonn_node new = jsonn_alloc(JSONN_NODE_BLOCK_SIZE);
        (!new) return NULL;

        jsonn_node alloc = root - 1;

        alloc->node->next = new;
        alloc->node = new;

        new->extra = nodes_per_block - 1;
        new->next = NULL;
}

jsonn_node node_alloc(jsonn_node root, jasonn_node_type type)
{
        jsonn_node alloc = root - 1;
        jsonn_node block = alloc->node;
        if(!block->extra) {
                block = node_add_block(root);
                if(!block) return NULL;
        }
        
        jsonn_node n = block + (bytes_per_block - block->extra);
        --block->extra;

        n.type = type;
        return n;
}







        
        






        a->next = NULL;
        a->value.integer = (JS
        alloc.type = JSONN_NODE_ALLOC;
        jsonn_node root = a + sizeof(struct jsonn_node_s);
        root->type = JSONN_NODE_ROOT;
        

        
}

jsonn_node jsonn_node_free(jsonn_node root)
{
}

jsonn_node node_alloc(jsonn_node root, jsonn_node_type type)
{

