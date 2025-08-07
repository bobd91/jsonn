#include <assert.h>

static jsonn_visitor jsonn_visitor_new(jsonn_callbacks *callbacks, size_t ctx_size)
{
        jsonn_visitor v = jsonn_alloc(sizeof(struct jsonn_visitor_s) + ctx_size);
        if(!v)
                return NULL;
        v->callbacks = callbacks;
        v->ctx = ctx_size
                ? (((void *)v) + sizeof(struct jsonn_visitor_s))
                : NULL;

        return v;
}

static void jsonn_visitor_free(jsonn_visitor v)
{
        jsonn_dealloc(v);
}

static int jsonn_null(jsonn_visitor v)
{
        return v->callbacks->null 
                && v->callbacks->null(v->ctx);
}

static int jsonn_boolean(jsonn_visitor v, int is_true)
{
        return v->callbacks->boolean 
                && v->callbacks->boolean(v->ctx, is_true);
}

static int jsonn_integer(jsonn_visitor v, int64_t integer)
{
        return v->callbacks->integer
                && v->callbacks->integer(v->ctx, integer);
}

static int jsonn_real(jsonn_visitor v, double real)
{
        return v->callbacks->real 
                && v->callbacks->real(v->ctx, real);
}

static int jsonn_string(jsonn_visitor v, uint8_t *bytes, size_t count)
{
        return v->callbacks->string
                && v->callbacks->string(v->ctx, bytes, count);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int jsonn_cstr(jsonn_visitor v, char *cstr)
{
        return jsonn_string(v, (uint8_t *)cstr, strlen(cstr));
}                        
#pragma GCC diagnostic pop

static int jsonn_key(jsonn_visitor v, uint8_t *bytes, size_t count)
{
        return v->callbacks->key
                && v->callbacks->key(v->ctx, bytes, count);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int jsonn_ckey(jsonn_visitor v, char *ckey)
{
        return jsonn_string(v, (uint8_t *)ckey, strlen(ckey));
}
#pragma GCC diagnostic pop

static int jsonn_begin_array(jsonn_visitor v)
{
        return v->callbacks->begin_array 
                && v->callbacks->begin_array(v->ctx);
}

static int jsonn_end_array(jsonn_visitor v)
{
        return v->callbacks->end_array 
                && v->callbacks->end_array(v->ctx);
}

static int jsonn_begin_object(jsonn_visitor v)
{
        return v->callbacks->begin_object 
                && v->callbacks->begin_object(v->ctx);
}

static int jsonn_end_object(jsonn_visitor v)
{
        return v->callbacks->end_object 
                && v->callbacks->end_object(v->ctx);
}

static int jsonn_error(jsonn_visitor v, int code, int at)
{
        (void)(v->callbacks->error 
                && v->callbacks->error(v->ctx, code, at));
        return 1; // always abort after error
}

static int visit(jsonn_visitor visitor, jsonn_type type, jsonn_value *value)
{
        switch(type) { 
        case JSONN_NULL:
                return jsonn_null(visitor);     
        case JSONN_FALSE:
        case JSONN_TRUE:
                return jsonn_boolean(visitor, JSONN_TRUE == type);
        case JSONN_INTEGER:
                return jsonn_integer(visitor, value->number.integer);
        case JSONN_REAL:
                return jsonn_real(visitor, value->number.real);
        case JSONN_STRING:
                return jsonn_string(visitor, value->string.bytes, value->string.length);
        case JSONN_KEY:
                return jsonn_key(visitor, value->string.bytes, value->string.length);
        case JSONN_BEGIN_ARRAY:
                return jsonn_begin_array(visitor);
        case JSONN_END_ARRAY:
                return jsonn_end_array(visitor);
        case JSONN_BEGIN_OBJECT:
                return jsonn_begin_object(visitor);
        case JSONN_END_OBJECT:
                return jsonn_end_object(visitor);
        case JSONN_ERROR:
                return jsonn_error(visitor, value->error.code, value->error.at);
        default:
                assert(!type && 0);
                return 1;
        }
}
