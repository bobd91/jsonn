

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

        block_root_free(block_root(root));
}




