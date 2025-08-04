
static jsonn_type error(jsonn_parser p, jsonn_error_code code) 
{
        p->result.error.code = code;
        p->result.error.at = p->current - p->start;
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

static jsonn_type utf8_error(jsonn_parser p)
{
        return error(p, JSONN_ERROR_UTF8);
}

static jsonn_type alloc_error(jsonn_parser p)
{
        return error(p, JSONN_ERROR_ALLOC);
}

static jsonn_type file_read_error(jsonn_parser p)
{
        return error(p, JSONN_ERROR_FILE_READ);
}

