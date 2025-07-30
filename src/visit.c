#include "jsonn.h"

static int visit(jsonn_visitor *visitor, jsonn_type type, jsonn_value *value)
{
        void *ctx = visitor->ctx;
        jsonn_callbacks *callbacks = visitor->callbacks;
        int abort = 0;
        switch(type) { 
        case JSONN_NULL:
                abort = callbacks->null 
                        && callbacks->null(ctx);
                break;

        case JSONN_FALSE:
        case JSONN_TRUE:
                abort = callbacks->boolean 
                        && callbacks->boolean(ctx, JSONN_TRUE == type);
                break;
        case JSONN_INTEGER:
                abort = callbacks->integer
                        && callbacks->integer(ctx, value->number.integer);
                break;
        case JSONN_REAL:
                abort = callbacks->real 
                        && callbacks->real(ctx, value->number.real);
                break;
        case JSONN_STRING:
                abort = callbacks->string
                        && callbacks->string(ctx,
                                        value->string.bytes,
                                        value->string.length);
                break;
        case JSONN_KEY:
                abort = callbacks->key
                        && callbacks->key(ctx,
                                        value->string.bytes,
                                        value->string.length);
                break;
        case JSONN_BEGIN_ARRAY:
                abort = callbacks->begin_array 
                        && callbacks->begin_array(ctx);
                break;
        case JSONN_END_ARRAY:
                abort = callbacks->end_array 
                        && callbacks->end_array(ctx);
                break;
        case JSONN_BEGIN_OBJECT:
                abort = callbacks->begin_object 
                        && callbacks->begin_object(ctx);
                break;
        case JSONN_END_OBJECT:
                abort = callbacks->end_object 
                        && callbacks->end_object(ctx);
                break;
        case JSONN_ERROR:
                abort = callbacks->error 
                        && callbacks->error(ctx, 
                                        value->error.code,
                                        value->error.at);
                abort = 1; // always abort after error
                break;
        default:
                abort = 1;
        }

        return abort;
}
