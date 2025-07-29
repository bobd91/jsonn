#include "jsonn.h"
#include "parse.h"

static jsonn_type error(jsonn_parser p, jsonn_error_code code) 
{
        p->result.is.error.code = code;
        p->result.is.error.at = p->current - p->start;
        return JSONN_ERROR;
}

static jsonn_type parse_error(jsonn_parser p)
{
        return error(p, JSONN_ERROR_PARSE);
}

static jsonn_type number_error(jsonn_parser p)
{
        return error(p, JSONN_ERROR_NUMBER);
}
