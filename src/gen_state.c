#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "unity.c"

#define SKELETON_FILE "jsonpg_state.skel.c"
#define OUTPUT_FILE   "jsonpg_state.c"

typedef struct state_s *state;
typedef struct class_s *class;
typedef struct rule_s *rule;
typedef struct match_s *match;
typedef struct action_s *action;
typedef struct builtin_s *builtin;
typedef struct range_s range;
typedef struct class_list_s *class_list;
typedef struct rule_list_s *rule_list;
typedef struct match_list_s *match_list;
typedef struct action_list_s *action_list;
typedef struct renderer_s *renderer;

#define MAX_CHARS_IN_CLASS 22
#define CODE_START_LEVEL 3

struct state_s {
        class_list classes;
        rule_list rules;
};

struct class_s {
        char *name;
        uint8_t chars[MAX_CHARS_IN_CLASS];
};

struct rule_s {
        char *name;
        int id;
        match_list matches;
};

typedef enum {
        MATCH_CLASS_NAME,
        MATCH_CLASS,
        MATCH_CHAR,
        MATCH_RANGE,
        MATCH_ANY,
        MATCH_VIRTUAL
} match_type;

struct range_s {
        uint8_t start;
        uint8_t end;
};

struct match_s {
        match_type type;
        union {
                char *class_name;
                class class;
                uint8_t character;
                range range;
        } match;
        action action;
        uint8_t id;
};

typedef enum {
        ACTION_LIST,
        ACTION_COMMAND,
        ACTION_RULE_NAME,
        ACTION_RULE
} action_type;

struct action_s {
        action_type type;
        union {
                action_list actions;
                builtin command;
                char *rule_name;
                rule rule;
        } action;
};

typedef struct {
        char *name;
        int value;
} map;

#define MAPPING(X) map X[] = {
#define MAP(X, Y) { X, Y },
#define MAPPING_END() { NULL, -1 } };

typedef enum {
        TOKEN_FINAL,
        TOKEN_MULTI,
        TOKEN_PARTIAL,
        TOKEN_MARKER
} token_type;

MAPPING(tokens)
MAP("null", TOKEN_FINAL)
MAP("true", TOKEN_FINAL)
MAP("null", TOKEN_FINAL)
MAP("true", TOKEN_FINAL)
MAP("false", TOKEN_FINAL)
MAP("integer", TOKEN_FINAL)
MAP("real", TOKEN_FINAL)
MAP("string", TOKEN_FINAL)
MAP("key", TOKEN_FINAL)
MAP("sq_string", TOKEN_FINAL)
MAP("sq_key", TOKEN_FINAL)
MAP("nq_string", TOKEN_FINAL)
MAP("nq_key", TOKEN_FINAL)
MAP("object", TOKEN_MULTI)
MAP("array", TOKEN_MULTI)
MAP("escape", TOKEN_PARTIAL)
MAP("escape_u", TOKEN_PARTIAL)
MAP("escape_chars", TOKEN_PARTIAL)
MAP("surrogate", TOKEN_MARKER)
MAPPING_END()

// don't need the value but want to validate config names
MAPPING(configs)
MAP("comments", JSONN_FLAG_COMMENTS)
MAP("single_quotes", JSONN_FLAG_SINGLE_QUOTES)
MAP("unquoted_strings", JSONN_FLAG_UNQUOTED_STRINGS)
MAP("unquoted_keys", JSONN_FLAG_UNQUOTED_KEYS)
MAP("trailing_commas", JSONN_FLAG_TRAILING_COMMAS)
MAP("optional_commas", JSONN_FLAG_OPTIONAL_COMMAS)
MAP("escape_characters", JSONN_FLAG_ESCAPE_CHARACTERS)
MAPPING_END()

typedef enum {
        CMD_PUSH_STATE,
        CMD_POP_STATE,
        CMD_PUSH,
        CMD_IF_PEEK,
        CMD_IFN_PEEK,
        CMD_IF_POP,
        CMD_POP,
        CMD_SWAP,
        CMD_IF_CONFIG
} builtin_type;

MAPPING(builtins)
MAP("pushstate", CMD_PUSH_STATE)
MAP("popstate", CMD_POP_STATE)
MAP("push", CMD_PUSH)
MAP("ifpeek", CMD_IF_PEEK)
MAP("ifnpeek", CMD_IFN_PEEK)
MAP("ifpop", CMD_IF_POP)
MAP("pop", CMD_POP)
MAP("swap", CMD_SWAP)
MAP("ifconfig", CMD_IF_CONFIG)
MAPPING_END()

struct builtin_s {
        builtin_type type;
        char *name;
        char *arg;
        union {
                int config;
                token_type token_type;
        } arg_info;
};

struct class_list_s {
        class class;
        class_list next;
};

struct rule_list_s {
        rule rule;
        rule_list next;
};

struct match_list_s {
        match match;
        match_list next;
};

struct action_list_s {
        action action;
        action_list next;
};

struct renderer_s {
        int level;
        int startlevel;
        str_buf sbuf;
};

static uint8_t gen_rule_id = 0;
static uint8_t gen_action_id = 0x80;
static char *gen_hex_chars = "0123456789ABCDEF";

int str_equal(char *s1, char *s2)
{
        return 0 == strcmp(s1, s2);
}

int map_lookup(map *mp, char *name)
{
        map m = *mp++;
        while(m.name) {
                if(str_equal(name, m.name))
                        return m.value;
                m = *mp++;
        }
        return m.value;
}

void warn(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
}

void fail(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
        exit(1);
}


void *fmalloc(size_t size) {
        void *p = malloc(size);
        if(!p)
                fail("Failed to allocate %d bytes", size);
        return p;
}

char result_char(jsonn_parser p)
{
        return (char)*jsonn_parse_result(p).string.bytes;
}

char *result_str(jsonn_parser p)
{
        jsonn_string_val s = jsonn_parse_result(p).string;
        char *ptr = fmalloc(1 + s.length);
        memcpy(ptr, s.bytes, s.length);
        ptr[s.length] = '\0';
        return ptr;
}

void expect_type(jsonn_type t1, jsonn_type t2)
{
        if(t1 != t2)
                fail("Expected type %d, got %d", t2, t1);
}

void expect_next(jsonn_type t, jsonn_parser p)
{
        jsonn_type t1 = jsonn_parse_next(p);
        expect_type(t1, t);
}

void append_class(state states, class c)
{
        class_list cl = fmalloc(sizeof(struct class_list_s));
        cl->class = c;
        cl->next = NULL;

        if(!states->classes) {
                states->classes = cl;
                return;
        }

        class_list next = states->classes;
        class_list last = next;
        while(next) {
                if(str_equal(next->class->name, c->name))
                        fail("Duplicate class name '%s'", c->name);
                last = next;
                next = next->next;
        }
        last->next = cl;
}

void append_rule(state states, rule r)
{
        rule_list rl = fmalloc(sizeof(struct rule_list_s));
        rl->rule = r;
        rl->next = NULL;

        if(!states->rules) {
                states->rules = rl;
                return;
        }

        rule_list next = states->rules;
        rule_list last = next;
        while(next) {
                if(str_equal(next->rule->name, r->name))
                        fail("Duplicate rule name '%s'", r->name);
                last = next;
                next = next->next;
        }
        last->next = rl;
}       

builtin parse_builtin(jsonn_parser p)
{
        // validate here as best to combine multiple ifconfig flags early
        // we have nowhere to put multiple config flag strings
        // but we can or all of the flags into one value

        builtin b = fmalloc(sizeof(struct builtin_s));

        jsonn_type type = jsonn_parse_next(p);
        expect_type(type, JSONN_KEY);
        char *key = result_str(p);

        b->name = key;
        b->type = map_lookup(builtins, key);

        type = jsonn_parse_next(p);
        if(type == JSONN_BEGIN_ARRAY && b->type == CMD_IF_CONFIG) {
                // config can take multiple ored together
                str_buf sbuf = str_buf_new();
                int flag = 0;
                while(JSONN_STRING == (type = jsonn_parse_next(p))) {
                        char *r = result_str(p);
                        if(0 != flag)
                                // a bit ugly but early rendering
                                // makes life much easier
                                str_buf_append_chars(sbuf, " | config_");
                        str_buf_append_chars(sbuf, r); 
                        flag |= map_lookup(configs, r);
                }
                expect_type(type, JSONN_END_ARRAY);
                b->arg_info.config = flag;
                str_buf_append(sbuf, (uint8_t *)"", 1);
                str_buf_content(sbuf, (uint8_t **)&b->arg);
        } else {
                expect_type(type, JSONN_STRING);
                b->arg = result_str(p);
                switch(b->type) {
                case CMD_IF_CONFIG:
                        b->arg_info.config = map_lookup(configs, b->arg);
                        break;
                case CMD_POP_STATE:
                case CMD_PUSH_STATE:
                        break;
                default:
                        b->arg_info.token_type = map_lookup(tokens, b->arg);
                }
        }

        expect_next(JSONN_END_OBJECT, p);

        return b;
}

action_list parse_action_list(jsonn_parser);

action parse_action(jsonn_parser p)
{
        action act = fmalloc(sizeof(struct action_s));
        jsonn_type type = jsonn_parse_next(p);
        switch(type) {
                case JSONN_STRING:
                        act->type = ACTION_RULE_NAME;
                        act->action.rule_name = result_str(p);
                        break;
                case JSONN_BEGIN_OBJECT:
                        act->type = ACTION_COMMAND;
                        act->action.command = parse_builtin(p);
                        break;
                case JSONN_BEGIN_ARRAY:
                        act->type = ACTION_LIST;
                        act->action.actions = parse_action_list(p);
                        break;
                default:
                        return NULL;
        }
        return act;
}

action_list parse_action_list(jsonn_parser p)
{
        action_list current = NULL;
        action_list head = NULL;
        action_list prev = NULL;
        action act;
        while((act = parse_action(p))) {
                current = fmalloc(sizeof(struct action_list_s));
                if(!head)
                        head = current;
                current->action = act;
                if(prev)
                        prev->next = current;
                prev = current;
        }
        return head;
}

match parse_match(jsonn_parser p)
{
        jsonn_type type = jsonn_parse_next(p);
        if(type == JSONN_END_OBJECT)
                return NULL;

        char *chars = result_str(p);
        match m = fmalloc(sizeof(struct match_s));
        m->type = -1;
        m->id = 0;
        int n = strlen(chars);
        unsigned r1, r2;
        switch(n) {
        case 0:
                fail("Rule match cannot be empty");
                // never get here
        case 1:
                m->type = MATCH_CHAR;
                m->match.character = *chars;
                break;
        case 4:
                if(1 == sscanf(chars, "0x%02X", &r1)) {
                        m->type = MATCH_CHAR;
                        m->match.character = (uint8_t)r1;
                }
                break;
        case 9:
                if(2 == sscanf(chars, "0x%02X-0x%02X", &r1, &r2)) {
                        if(r1 >= r2)
                                fail("Invalid range '%s'", chars);
                        m->type = MATCH_RANGE;
                        m->match.range.start = r1;
                        m->match.range.end = r2;
                }
                break;
        } 
        if(m->type == -1) {
                if('$' == *chars) {
                        m->type = MATCH_CLASS_NAME;
                        m->match.class_name = chars + 1;
                } else if(str_equal("...", chars)) {
                        m->type = MATCH_ANY;
                } else if(str_equal("???", chars)) {
                        m->type = MATCH_VIRTUAL;
                } else {
                        fail("Invalid match specification '%s'", chars);
                }
        }
        m->action = parse_action(p);
        return m;
}


match_list parse_match_list(jsonn_parser p) {
        expect_next(JSONN_BEGIN_OBJECT, p);

        match_list current = NULL;
        match_list head = NULL;
        match_list prev = NULL;
        match m;
        while((m = parse_match(p))) {
                current = fmalloc(sizeof(struct match_list_s));
                if(!head)
                        head = current;
                current->match = m;
                if(prev)
                        prev->next = current;
                prev = current;
        }
        return head;
}




state create_states()
{
        return malloc(sizeof(struct state_s));
}

void parse_class_chars(class c, jsonn_parser p)
{
        expect_next(JSONN_BEGIN_ARRAY, p);
        jsonn_type type = jsonn_parse_next(p);
        int i = 0;
        while(type == JSONN_STRING) {
                if(i >= MAX_CHARS_IN_CLASS)
                        fail("Too many characters in character class '%s'", c->name);
                c->chars[i++] = result_char(p);
                type = jsonn_parse_next(p);
        }
        c->chars[i] = '\0';
        expect_type(type, JSONN_END_ARRAY);
}

class parse_class(char *name, jsonn_parser p)
{
        class c = fmalloc(sizeof(struct class_s));
        c->name = name;
        parse_class_chars(c, p);
        return c;
}

void add_class(state states, char *name, jsonn_parser p)
{
        append_class(states, parse_class(name, p));
}

int rule_is_virtual(rule r)
{
        // rule with single "???" match clause
        return r->matches->match->type == MATCH_VIRTUAL
                && !(r->matches->next);
}

rule parse_rule(char *name, jsonn_parser p)
{
        rule r = fmalloc(sizeof(struct rule_s));
        r->name = name;
        r->matches = parse_match_list(p);
        r->id = rule_is_virtual(r) ? -1 : gen_rule_id++;

        return r;
}

void add_rule(state states, char *name, jsonn_parser p)
{
        append_rule(states, parse_rule(name, p));
}





class find_class(state states, char *name)
{
        class_list cl = states->classes;
        while(cl) {
                if(str_equal(name, cl->class->name))
                        return cl->class;
                cl = cl->next;
        }
        return NULL;
}

rule find_rule(state states, char *name)
{
        rule_list rl = states->rules;
        while(rl) {
                if(str_equal(name, rl->rule->name)) {
                        return rl->rule;
                }
                rl = rl->next;
        }
        return NULL;
}

int validate_builtin(builtin b)
{
        if(b->type == -1) {
                warn("Unknown builtin '%s'", b->name);
                return 0;
        }
        switch(b->type) {
        case CMD_PUSH_STATE:
        case CMD_POP_STATE:
                // no validation required
                break;
        case CMD_IF_CONFIG:
                if(b->arg_info.config < 0) {
                        warn("Unknown config '%s'", b->arg);
                        return 0;
                }
                break;
        default:
                if(b->arg_info.token_type == -1) {
                        warn("Unknown token '%s'", b->arg);
                        return 0;
                }
                break;
        }
        return 1;
}

int validate_action(state states, action a)
{
        int res = 1;
        switch(a->type) {
                case ACTION_LIST:
                        action_list al = a->action.actions;
                        while(al) {
                                res &= validate_action(states, al->action);
                                al = al->next;
                        }
                        break;
                case ACTION_COMMAND:
                        res &= validate_builtin(a->action.command);
                        break;
                case ACTION_RULE_NAME:
                        char *name = a->action.rule_name;
                        rule r = find_rule(states, name);
                        if(!r) {
                                warn("Unknown rule '%s'", name);
                                res = 0;
                        }
                        a->type = ACTION_RULE;
                        a->action.rule = r;
                        break;
                case ACTION_RULE:
                        // will only exist post validation
        }
        return res;
}

int validate_match(state states, match m)
{
        int res = 1;
        if(m->type == MATCH_CLASS_NAME) {
                char *name = m->match.class_name;
                class c = find_class(states, name);
                if(!c) {
                        warn("Unknown class '%s'", name);
                        res = 0;
                }
                m->type = MATCH_CLASS;
                m->match.class = c;
        }
        res &= validate_action(states, m->action);
        return res;
}

int validate_rule(state states, rule r)
{
        int res = 1;
        match_list ml = r->matches;
        while(ml) {
                res &= validate_match(states, ml->match);
                ml = ml->next;
        }
        return res;
}

int validate_states(state states) 
{
        int res = 1;
        rule_list rl = states->rules;
        while(rl) {
                res &= validate_rule(states, rl->rule);
                rl = rl->next;
        }
        return res;
}


state parse_state(jsonn_parser p)
{
        state states = create_states();

        jsonn_type type = jsonn_parse_next(p);
        while(JSONN_KEY == type) {
                char *key = result_str(p);
                if('$' == *key) {
                        add_class(states, key + 1, p);
                } else {
                        add_rule(states, key, p);
                }
                type = jsonn_parse_next(p);
        }
        expect_type(JSONN_END_OBJECT, type);

        return states;
}

void dump_command(builtin command)
{
        printf("  Command: %s\n", command->name);
        printf("           %s\n", command->arg);
}

void dump_action_rule(rule r)
{
        printf("  Action Rule: %s\n", r->name);
}

void dump_action(action a)
{
        switch(a->type) {
                case ACTION_LIST:
                        action_list al = a->action.actions;
                        while(al) {
                                dump_action(al->action);
                                al = al->next;
                        }
                        break;
                case ACTION_COMMAND:
                        dump_command(a->action.command);
                        break;
                case ACTION_RULE:
                        dump_action_rule(a->action.rule);
                        break;
                case ACTION_RULE_NAME:
                        fail("Rule %s not validated", a->action.rule_name);
        }

}

void dump_match(match m)
{
        switch(m->type) {
                case MATCH_CLASS:
                        printf(" Match class %s\n", m->match.class->name);
                        break;
                case MATCH_CHAR:
                        uint8_t c = m->match.character;
                        if(c < 0x20)
                                printf(" Match char 0x%02X\n", (int)c);
                        else
                                printf(" Match char '%c'\n", (int)c);
                        break;
                case MATCH_RANGE:
                        range *r = &m->match.range;
                        printf(" Match range 0x%02X-0x%02X\n", r->start, r->end);
                        break;
                case MATCH_ANY:
                        printf( "Match any\n");
                        break;
                case MATCH_VIRTUAL:
                        printf(" Match virtual\n");
                        break;
                case MATCH_CLASS_NAME:
                        fail("Class %s not validated", m->match.class_name);
        }

        dump_action(m->action);

}

void dump_matches(match_list ml)
{
        while(ml) {
                dump_match(ml->match);
                ml = ml->next;
        }
}

void dump_rule(rule r)
{
        printf("Rule: %s [%d]\n", r->name, (unsigned)r->id);
        dump_matches(r->matches);
        printf("\n");
}

void dump_class(class c)
{
        printf("Class: %s\n", c->name);
        uint8_t *chars = c->chars;
        int len = strlen((char *)chars);
        int first = 1;
        for(int i = 0 ; i < len ; i++) {
                if(first) {
                        printf("Chars: ");
                        first = 0;
                } else {
                        printf(", ");
                }
                if(chars[i] < 0x20)
                        printf("0x%02X", (int)chars[i]);
                else
                        printf("'%c'", (int)chars[i]);
        }
        printf("\n\n");
}

void dump_state(state states) {
        class_list cl = states->classes;
        while(cl) {
                dump_class(cl->class);
                cl = cl->next;
        }
        rule_list rl = states->rules;
        while(rl) {
                dump_rule(rl->rule);
                rl = rl->next;
        }
}

void render(renderer r, char *chars)
{
        str_buf_append_chars(r->sbuf, chars);
}

void render_c(renderer r, uint8_t c)
{
        str_buf_append(r->sbuf, &c, 1);
}

void render_indent(renderer r, char *chars)
{
        static char indent[] = "        ";
        render(r, "\n");
        for(int i = 0 ; i < r->level ; i++)
                render(r, indent);
        render(r, chars);
}

void render_level(renderer r, int inc)
{
        r->level += inc;
}

void render_startlevel(renderer r, int l)
{
        r->level = l;
        r->startlevel = l;
}

void write_renderer(FILE *stream, renderer r)
{
        uint8_t *str;
        int len = str_buf_content(r->sbuf, &str);
        // drop leading newline
        fprintf(stream, "%.*s", len - 1, 1 + (char *)str);
}

void render_x(renderer r, uint8_t x)
{
        render(r, "0x");
        render_c(r, gen_hex_chars[x >> 4]);
        render_c(r, gen_hex_chars[x & 0xF]);
}        

void render_enum(rule r, renderer enums, int first) 
{
        if(r->id < 0)
                // will mess up first processing if first entry is virtual,
                // which it isn't at the moment (phew)
                return;

        if(!first)
                render(enums, ",");
        render_indent(enums, "state_");
        render(enums, r->name);
}

void render_map_values(rule r, uint8_t *bytes, renderer map, int first_map)
{
        if(r->id < 0)
                return;

        int first = 1;
        if(!first_map)
                render(map, ", ");
        render_indent(map, "{");
        for(int i = 0 ; i < 256 ; i++) {
                if(!first)
                        render(map, ", ");
                first = 0;
                render_x(map, bytes[i]);
        }
        render(map, "}");
}

int is_if_command(builtin command)
{
        return 0 == strncmp("if", command->name, 2);
}

void render_call(char *prefix, char *arg, renderer code)
{
        render_indent(code, prefix);
        render(code, arg);
        render(code, ");");
}

void render_if(int not, char *prefix, char *arg, char *close, renderer code)
{
        render_indent(code, "if(");
        if(not)
                render(code, "!");
        render(code, prefix);
        render(code, arg);
        render(code, close);
        render(code, ") {");
        render_level(code, 1);
}

void render_ifpeek(int not, builtin command, renderer code)
{
        if(command->arg_info.token_type == TOKEN_MULTI) {
                render_if(not, "in_", command->arg, "()", code);
        } else {
                render_if(not, "ifpeek_token(token_", command->arg, ")", code);
        }
}

void render_alloc_error(renderer code)
{
        render(code, " {");
        render_level(code, 1);
        render_indent(code, "result = alloc_error(p);");
        render_indent(code, "break;");
        render_level(code, -1);
        render_indent(code, "}");
}

void render_builtin(int is_virtual, builtin command, renderer code)
{
        char *arg = command->arg;
        switch(command->type) {
        case CMD_PUSH_STATE:
                render_indent(code, "push_state(state_");
                render(code, arg);
                render(code, ");");
                break;
        case CMD_POP_STATE:
                if(is_virtual)
                        render_indent(code, "incr = 0;");
                render_indent(code, "new_state = pop_state();");
                break;
        case CMD_IF_CONFIG:
                render_if(0, "if_config(config_", arg, ")", code);
                break;
        case CMD_PUSH:
                switch(command->arg_info.token_type) {
                case(TOKEN_MULTI):
                        render_indent(code, "result = begin_");
                        render(code, arg);
                        render(code, "();");
                        break;
                case(TOKEN_PARTIAL):
                        render_indent(code, "if(push_token(token_");
                        render(code, arg);
                        render(code, "))");
                        render_alloc_error(code);
                        break;
                default:
                        render_indent(code, "push_token(token_");
                        render(code, arg);
                        render(code, ");");
                }
                break;
        case CMD_IF_PEEK:
                render_ifpeek(0, command, code);
                break;
        case CMD_IFN_PEEK:
                render_ifpeek(1, command, code);
                break;
        case CMD_IF_POP:
                render_ifpeek(0, command, code);
                // fallthrough
        case CMD_POP:
                switch(command->arg_info.token_type) {
                case TOKEN_FINAL:
                        render_indent(code, "result = accept_");
                        render(code, arg);
                        render(code, "(pop_token());");
                        break;
                case TOKEN_MULTI:
                        render_indent(code, "result = end_");
                        render(code, arg);
                        render(code, "();");
                        break;
                case TOKEN_PARTIAL:
                        render_indent(code, "if(process_");
                        render(code, arg);
                        render(code, "(pop_token()))");
                        render_alloc_error(code);
                        break;
                case TOKEN_MARKER:
                        render_indent(code, "pop_token();");
                        break;
                }
                break;
        case CMD_SWAP:
                render_indent(code, "swap_token(token_");
                render(code, arg);
                render(code, ");");
                break;
        default:
                fail("Unexpected builtin command '%s' [%d]", command->name, command->type);
 
        }
}

void render_rule_match(int is_virtual, rule r, renderer code)
{
        if(is_virtual)
                render_indent(code, "incr = 0;");
        render_indent(code, "new_state = state_");
        render(code, r->name);
        render(code, ";");

        if(code->level > 1 + code->startlevel)
                render_indent(code, "break;");
}

void render_actions(int, action_list, renderer);

void render_action(int is_virtual, action a, renderer code)
{
        switch(a->type) {
                case ACTION_LIST:
                        render_actions(is_virtual, a->action.actions, code);
                        break;
                case ACTION_COMMAND:
                        render_builtin(is_virtual, a->action.command, code);
                        break;
                case ACTION_RULE:
                        rule r = a->action.rule;
                        if(r->id < 0) {
                                render_action(is_virtual, r->matches->match->action, code);
                        } else {
                                render_rule_match(is_virtual, r, code);
                        }
                        break;
                case ACTION_RULE_NAME:
                        fail("Rule '%s' not validated", a->action.rule_name);
        }
}

void render_if_block(int is_virtual, action_list al, renderer code)
{
        render_action(is_virtual, al->action, code);
        al = al->next;
        if(!al)
                fail("If without then");
        render_action(is_virtual, al->action, code);
        render_level(code, -1);
        render_indent(code, "}");
        al = al->next;
        if(al) {
                render(code, " else {");

                render_level(code, 1);
                render_action(is_virtual, al->action, code);
                render_level(code, -1);

                render_indent(code, "}");

                al = al->next;
                if(al)
                        fail("If with too many clauses");
        }
}

int is_if_action(action a)
{
        if(a->type == ACTION_COMMAND)
                return is_if_command(a->action.command);
        else
                return 0;
}

void render_actions(int is_virtual, action_list al, renderer code)
{
        if(is_if_action(al->action)) {
                render_if_block(is_virtual, al, code);
        } else {
                while(al) {
                        render_action(is_virtual, al->action, code);
                        al = al->next;
                }
        }
}

uint8_t render_new_action(renderer code)
{
        uint8_t s = gen_action_id++;
        render_indent(code, "case ");
        render_x(code, s);
        render(code, ":");
        render_level(code, 1);
        return s;
}

void render_action_comment(int is_virtual, rule r, match m, renderer code)
{
        render_indent(code, "// ");
        if(is_virtual)
                render(code, "[virtual] ");
        render(code, r->name);
        render(code, "/");
        switch(m->type) {
                case MATCH_CLASS:
                        render(code, "$");
                        render(code, m->match.class->name);
                        break;
                case MATCH_CHAR:
                        render(code, "'");
                        render_c(code, m->match.character);
                        render(code, "'");
                        break;
                case MATCH_RANGE:
                        render_x(code, m->match.range.start);
                        render(code, "-");
                        render_x(code, m->match.range.end);
                        break;
                case MATCH_ANY:
                        render(code, "...");
                        break;
                case MATCH_VIRTUAL:
                        render(code, "???");
                        break;
                case MATCH_CLASS_NAME:
                        fail("Class '%s' not resolved", m->match.class_name);

        }
}

uint8_t render_match_action(int is_virtual, rule r, match m, renderer code)
{
        if(m->id)
                return m->id;

        action a = m->action;
        switch(a->type) {
                case ACTION_LIST:
                        m->id = render_new_action(code);
                        render_action_comment(is_virtual, r, m, code);
                        render_actions(is_virtual, a->action.actions, code);
                        render_indent(code, "break;");
                        render_level(code, -1);
                        break;
                case ACTION_COMMAND:
                        m->id = render_new_action(code);
                        render_action_comment(is_virtual, r, m, code);
                        render_builtin(is_virtual, a->action.command, code);
                        render_indent(code, "break;");
                        render_level(code, -1);
                        break;
                case ACTION_RULE:
                        rule ar = a->action.rule;
                        if(ar->id < 0) {
                                m->id = render_match_action(is_virtual, ar, ar->matches->match, code);
                        } else {
                                m->id = ar->id;
                        }
                        break;
                case ACTION_RULE_NAME:
                        fail("Rule '%s' not validated", a->action.rule_name);
        }


        return m->id;
}

uint8_t render_rule_default_state(rule r, renderer code)
{
        match_list ml = r->matches;
        while(ml) {
                match m = ml->match;
                if(m->type == MATCH_ANY) {
                        return render_match_action(false, r, m, code);
                        break;
                } else if(m->type == MATCH_VIRTUAL) {
                        return render_match_action(true, r, m, code);
                        break;
                }
                ml = ml->next;
        }

        return -1; //state_error;
}





void render_rule(rule r, renderer map, renderer enums, renderer code, int first)
{
        if(r->id < 0)
                return;

        static uint8_t rule_states[256];

        render_enum(r, enums, first);
        match_list ml = r->matches;
        uint8_t def_state = render_rule_default_state(r, code);
        memset(rule_states, def_state, 256);

        while(ml) {
                int len;
                match m = ml->match;
                uint8_t match_state;
                switch(m->type) {
                        case MATCH_CLASS:
                                match_state = render_match_action(false, r, m, code);
                                uint8_t *chars = m->match.class->chars;
                                len = strlen((char *)chars);
                                for(int i = 0 ; i < len ; i++)
                                        rule_states[chars[i]] = match_state;
                                break;
                        case MATCH_CHAR:
                                match_state = render_match_action(false, r, m, code);
                                rule_states[m->match.character] = match_state;
                                break;
                        case MATCH_RANGE:
                                match_state = render_match_action(false, r, m, code);
                                range *rg = &m->match.range;
                                len = rg->end - rg->start;
                                for(int i = 0 ; i <= len ; i++)
                                        rule_states[rg->start + i] = match_state;
                                break;
                        case MATCH_CLASS_NAME:
                                // validation will have changed this to CLASS or failed
                        case MATCH_ANY:
                                // handled as default set above
                        case MATCH_VIRTUAL:
                                // handled as default set above
                }
                ml = ml->next;
        }
        render_map_values(r, rule_states, map, first);
}

renderer renderer_new(int level)
{
        renderer r = fmalloc(sizeof(struct renderer_s));
        r->level = level;
        r->startlevel = level;
        r->sbuf = str_buf_new();
        return r;
}

void copy_until(FILE *in, FILE *out, char *tag)
{
        size_t tlen = tag ? strlen(tag) : 0;
        char *line = NULL;
        size_t len = 0;
        ssize_t read;

        while ((read = getline(&line, &len, in)) != -1) {
                if(tag && 0 == strncmp(tag, line, tlen))
                        return;
                fprintf(out, "%s", line);
        }
        if(tag)
                fail("Tag %s not found", tag);
}

void merge_renderer(FILE *in, FILE *out, renderer r, char *tag)
{
        copy_until(in, out, tag);
        write_renderer(out, r);
}

void render_state(state states)
{
        rule_list rl = states->rules;
        renderer map = renderer_new(1);
        renderer enums = renderer_new(1);
        renderer code = renderer_new(CODE_START_LEVEL);

        int first = 1;
        while(rl) {
                render_rule(rl->rule, map, enums, code, first);
                first = 0;
                rl = rl->next;
        }

        FILE *skelfile = fopen(SKELETON_FILE, "r");
        if(!skelfile)
                fail("Failed to open %s", SKELETON_FILE);

        FILE *cfile = fopen(OUTPUT_FILE, "w");
        if(!cfile)
                fail("Failed to create %s", OUTPUT_FILE);

        merge_renderer(skelfile, cfile, map, "<= map");
        merge_renderer(skelfile, cfile, enums, "<= enums");
        merge_renderer(skelfile, cfile, code, "<= code");
        copy_until(skelfile, cfile, NULL);

        fclose(cfile);
        fclose(skelfile);
}

int main(int argc, char *argv[])
{
        if((argc != 2 && argc != 3) 
                        || (argc == 3 && 0 != strcmp("-d", argv[1]))) {
                printf("Usage: state [-d] <json file>");
                exit(1);
        }

        int dump = (argc == 3);

        FILE *stream = fopen(argv[argc - 1], "rb");
        if(!stream) {
                perror("Failed to open input");
                exit(1);
        }

        jsonn_parser p = jsonn_new(NULL);
        jsonn_type type = jsonn_parse_stream(p, stream, NULL);
        if(type != JSONN_EOF && type != JSONN_ERROR) {
                type = jsonn_parse_next(p);
                if(JSONN_BEGIN_OBJECT == type) {
                        state s = parse_state(p);
                        if(validate_states(s)) {
                                if(dump) {
                                        dump_state(s);
                                } else {
                                        render_state(s);
                                }
                        } else {
                                printf("Invalid states\n");
                        }
                } else {
                        printf("Expecting begin object, got: %d\n", type);
                        exit(1);
                }
                type = jsonn_parse_next(p);
        }
        if(type != JSONN_EOF) {
                if(type == JSONN_ERROR) 
                        printf("Error %d at %ld\n", 
                                        jsonn_parse_result(p).error.code, 
                                        jsonn_parse_result(p).error.at);
                else
                        printf("Expecting EOF, got: %d\n", type);
                exit(1);
        }
}
