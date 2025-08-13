#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "unity.c"


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
        uint8_t states[256];
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

struct builtin_s {
        char *name;
        char *arg;
};

struct class_list_s {
        class this;
        class_list next;
};

struct rule_list_s {
        rule this;
        rule_list next;
};

struct match_list_s {
        match this;
        match_list next;
};

struct action_list_s {
        action this;
        action_list next;
};

struct renderer_s {
        int level;
        str_buf sbuf;
};

typedef enum {
        CMD_PUSH_STATE,
        CMD_POP_STATE,
        CMD_PUSH_STACK,
        CMD_IF_POP_STACK,
        CMD_IF_PEEK_STACK,
        CMD_PUSH_TOKEN,
        CMD_IF_PEEK_TOKEN,
        CMD_IFN_PEEK_TOKEN,
        CMD_IF_POP_TOKEN,
        CMD_POP_TOKEN,
        CMD_IF_CONFIG
} command_type;

static uint8_t gen_rule_id = 0;
static uint8_t gen_action_id = 0x80;
static char *gen_hex_chars = "0123456789ABCDEF";


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
                fail("Failed to allocate %d", size);
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
        if(t1 != t2) {
                fail("Expected type %d, got %d", t2, t1);
        }
}

void expect_next(jsonn_type t, jsonn_parser p)
{
        jsonn_type t1 = jsonn_parse_next(p);
        expect_type(t1, t);
}

void append_class(state states, class c)
{
        class_list cl = fmalloc(sizeof(struct class_list_s));
        cl->this = c;
        cl->next = NULL;

        if(!states->classes) {
                states->classes = cl;
                return;
        }

        class_list next = states->classes;
        class_list last = next;
        while(next) {
                if(0 == strcmp(next->this->name, c->name))
                        fail("Duplicate class name: %s", c->name);
                last = next;
                next = next->next;
        }
        last->next = cl;
}

void append_rule(state states, rule r)
{
        rule_list rl = fmalloc(sizeof(struct rule_list_s));
        rl->this = r;
        rl->next = NULL;

        if(!states->rules) {
                states->rules = rl;
                return;
        }

        rule_list next = states->rules;
        rule_list last = next;
        while(next) {
                if(0 == strcmp(next->this->name, r->name))
                        fail("Duplicate rule name: %s", r->name);
                last = next;
                next = next->next;
        }
        last->next = rl;
}       

builtin parse_builtin(jsonn_parser p)
{
        builtin b = fmalloc(sizeof(struct builtin_s));

        jsonn_type type = jsonn_parse_next(p);
        expect_type(type, JSONN_KEY);
        char *key = result_str(p);

        type = jsonn_parse_next(p);
        expect_type(type, JSONN_STRING);

        b->name = key;
        b->arg = result_str(p);

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
                current->this = act;
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
        m->id = 0;
        int n = strlen(chars);
        switch(n) {
                case 0:
                        fail("Rule match cannot be empty");
                        // never get here
                case 1:
                        m->type = MATCH_CHAR;
                        m->match.character = *chars;
                        break;
                case 9:
                        unsigned int r1, r2;
                        if(2 == sscanf(chars, "0x%02X-0x%02X", &r1, &r2)) {
                                if(r1 >= r2)
                                        fail("Invalid range: %s", chars);
                                m->type = MATCH_RANGE;
                                m->match.range.start = r1;
                                m->match.range.end = r2;
                                break;
                        }
                        // fallthrough
                default:
                        if('$' == *chars) {
                                m->type = MATCH_CLASS_NAME;
                                m->match.class_name = chars + 1;
                                break;
                        } else if(0 == strcmp("...", chars)) {
                                m->type = MATCH_ANY;
                                break;
                        } else if(0 == strcmp("???", chars)) {
                                m->type = MATCH_VIRTUAL;
                                break;
                        }

                        fail("Invalid match specification: %s", chars);

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
                current->this = m;
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
                        fail("Too many characters in character class: %s", c->name);
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
        return r->matches->this->type == MATCH_VIRTUAL
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
                if(0 == strcmp(name, cl->this->name))
                        return cl->this;
                cl = cl->next;
        }
        return NULL;
}

rule find_rule(state states, char *name)
{
        rule_list rl = states->rules;
        while(rl) {
                if(0 == strcmp(name, rl->this->name)) {
                        return rl->this;
                }
                rl = rl->next;
        }
        return NULL;
}

int validate_action(state states, action a)
{
        int res = 1;
        switch(a->type) {
                case ACTION_LIST:
                        action_list al = a->action.actions;
                        while(al) {
                                res &= validate_action(states, al->this);
                                al = al->next;
                        }
                        break;
                case ACTION_COMMAND:
                        // validated at parse time
                        break;
                case ACTION_RULE_NAME:
                        char *name = a->action.rule_name;
                        rule r = find_rule(states, name);
                        if(!r) {
                                warn("Cannot find rule %s", name);
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
                        warn("Cannot find class %s", name);
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
                res &= validate_match(states, ml->this);
                ml = ml->next;
        }
        return res;
}

int validate_states(state states) 
{
        int res = 1;
        rule_list rl = states->rules;
        while(rl) {
                res &= validate_rule(states, rl->this);
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
                } else if('_' == *key) {
                        add_rule(states, key + 1, p);
                } else if('*' == *key) {
                        add_rule(states, key + 1, p);
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
                                dump_action(al->this);
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
                dump_match(ml->this);
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
                dump_class(cl->this);
                cl = cl->next;
        }
        rule_list rl = states->rules;
        while(rl) {
                dump_rule(rl->this);
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

void write_renderer(renderer r, FILE *stream)
{
        uint8_t *str;
        int len = str_buf_content(r->sbuf, &str);
        fprintf(stream, "%.*s\n", len, (char *)str);
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
                // will miss first processing if first entry
                // if virtual, which it isn't at the moment (phew)
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

void render_if(char *prefix, char *arg, renderer code)
{
        render_indent(code, "if(");
        render(code, prefix);
        render(code, arg);
        render(code, ")) {");
        render_level(code, 1);
}

int is_str(char *s1, char *s2)
{
        return 0 == strcmp(s1, s2);
}

int is_stack_arg(char *arg)
{
        return is_str(arg, "object") || is_str(arg, "array");
}

command_type determine_command_type(builtin command)
{
        char *name = command->name;
        char *arg = command->arg;

        if(is_stack_arg(arg)) {
                if(is_str(name, "push"))
                        return CMD_PUSH_STACK;
                else if(is_str(name, "ifpop"))
                        return CMD_IF_POP_STACK;
                else if(is_str(name, "ifpeek"))
                        return CMD_IF_PEEK_STACK;
        }

        if(is_str(name, "popstate"))
                return CMD_POP_STATE;
        if(is_str(name, "pushstate"))
                return CMD_PUSH_STATE;
        if(is_str(name, "push"))
                return CMD_PUSH_TOKEN;
        if(is_str(name, "ifpeek"))
                return CMD_IF_PEEK_TOKEN;
        if(is_str(name, "ifnpeek"))
                return CMD_IFN_PEEK_TOKEN;
        if(is_str(name, "ifpop"))
                return CMD_IF_POP_TOKEN;
        if(is_str(name, "pop"))
                return CMD_POP_TOKEN;
        if(is_str(name, "ifconfig"))
                return CMD_IF_CONFIG;

        fail("Command %s not recognized", name);
        // never get here
        return -1;
}


void render_command(int is_virtual, builtin command, renderer code)
{
        char *arg = command->arg;
        switch(determine_command_type(command)) {
                case CMD_PUSH_STATE:
                        render_call("push_state(state_", arg, code);
                        break;
                case CMD_POP_STATE:
                        if(is_virtual)
                                render_indent(code, "repeat");
                        else
                                render_indent(code, "next");
                        render(code, "_byte(pop_state());");
                        break;
                case CMD_PUSH_STACK:
                        render_call("push_stack(stack_", arg, code);
                        break;
                case CMD_IF_POP_STACK:
                        render_if("ifpop_stack(stack_", arg, code);
                        break;
                case CMD_IF_PEEK_STACK:
                        render_if("ifpeek_stack(stack_", arg, code);
                        break;
                case CMD_IF_CONFIG:
                        render_if("ifconfig(config_", arg, code);
                        break;
                case CMD_PUSH_TOKEN:
                        render_call("push_token(token_", arg, code);
                        break;
                case CMD_IF_PEEK_TOKEN:
                        render_if("ifpeek_token(token_", arg, code);
                        break;
                case CMD_IFN_PEEK_TOKEN:
                        render_if("!ifpeek_token(token_", arg, code);
                        break;
                case CMD_IF_POP_TOKEN:
                        render_if("ifpeek_token(token_", arg, code);
                        // fallthrough
                case CMD_POP_TOKEN:
                        render_indent(code, "result = accept_");
                        render(code, arg);
                        render(code, "(pop_token());");
                        break;
        }
}

void render_rule_match(int is_virtual, rule r, renderer code)
{
        if(is_virtual)
                render_indent(code, "repeat");
        else
                render_indent(code, "next");
        render(code, "_byte(state_");
        render(code, r->name);
        render(code, ");");
}

void render_actions(int, action_list, renderer);

void render_action(int is_virtual, action a, renderer code)
{
        switch(a->type) {
                case ACTION_LIST:
                        render_actions(is_virtual, a->action.actions, code);
                        break;
                case ACTION_COMMAND:
                        render_command(is_virtual, a->action.command, code);
                        break;
                case ACTION_RULE:
                        rule r = a->action.rule;
                        if(r->id < 0) {
                                render_action(is_virtual, r->matches->this->action, code);
                        } else {
                                render_rule_match(is_virtual, r, code);
                        }
                        break;
                case ACTION_RULE_NAME:
                        fail("Rule %s not validated", a->action.rule_name);
        }
}

void render_if_block(int is_virtual, action_list al, renderer code)
{
        render_action(is_virtual, al->this, code);
        al = al->next;
        if(!al)
                fail("If without then");
        render_action(is_virtual, al->this, code);
        render_level(code, -1);
        render_indent(code, "}");
        al = al->next;
        if(al) {
                render(code, " else {");

                render_level(code, 1);
                render_action(is_virtual, al->this, code);
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
        if(is_if_action(al->this)) {
                render_if_block(is_virtual, al, code);
        } else {
                while(al) {
                        render_action(is_virtual, al->this, code);
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
                        fail("Class %s not resolved", m->match.class_name);

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
                        render_command(is_virtual, a->action.command, code);
                        render_indent(code, "break;");
                        render_level(code, -1);
                        break;
                case ACTION_RULE:
                        rule ar = a->action.rule;
                        if(ar->id < 0) {
                                m->id = render_match_action(is_virtual, ar, ar->matches->this, code);
                        } else {
                                m->id = ar->id;
                        }
                        break;
                case ACTION_RULE_NAME:
                        fail("Rule %s not validated", a->action.rule_name);
        }


        return m->id;
}

uint8_t render_rule_default_state(rule r, renderer code)
{
        match_list ml = r->matches;
        while(ml) {
                match m = ml->this;
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





void render_rule(rule r, renderer enums, renderer map, renderer code, int first)
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
                match m = ml->this;
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

renderer renderer_new()
{
        renderer r = fmalloc(sizeof(struct renderer_s));
        r->level = 0;
        r->sbuf = str_buf_new();
        return r;
}

void render_state(state states)
{
        rule_list rl = states->rules;
        renderer enums = renderer_new();
        renderer map = renderer_new();
        renderer code = renderer_new();

        render(enums, "typdef enum {");
        render_level(enums, 1);

        render(map, "uint8_t state_map[][256] = {");
        render_level(map, 1);

        render(code, "jsonn_type jsonn_parse_next(jsonn_parser parser) {");
        render_level(code, 1);
        render_indent(code, "uint8_t *current = parser->current;");
        render_indent(code, "uint8_t *last = parser->last;");
        render_indent(code, "state current_state = parser->state");
        render_indent(code, "jsonn_type result = JSONN_EOB");
        render_indent(code, "while(current < last) {");
        render_level(code, 1);
        render_indent(code, "state new_state = state_map[current_state][*current];");
        render_indent(code, "if(new_state < 0x80) {");
        render_level(code, 1);
        render_indent(code, "current_state = new_state;");
        render_indent(code, "current++;");
        render_indent(code, "continue;");
        render_level(code, -1);
        render_indent(code, "}");
        render_indent(code, "current_state = state_error;");
        render_indent(code, "switch(new_state) {");

        int first = 1;
        while(rl) {
                render_rule(rl->this, enums, map, code, first);
                first = 0;
                rl = rl->next;
        }

        render(enums, ",");
        render_indent(enums, "state_error = 0xFF");
        render_level(enums, -1);
        render_indent(enums, "} state;");

        render_level(map, -1);
        render_indent(map, "};");

        render_indent(code, "}");
        render_indent(code, "if(current_state == state_error)");
        render_level(code, 1);
        render_indent(code, "result = parse_error(parser);");
        render_level(code, -1);
        render_indent(code, "if(result != JSONN_EOB)");
        render_level(code, 1);
        render_indent(code, "break;");
        render_level(code, -1);
        render_level(code, -1);
        render_indent(code, "}");
        render_indent(code, "parser->state = current_state;");
        render_indent(code, "parser->current = current;");
        render_indent(code, "return result;");
        render_level(code, -1);
        render_indent(code, "}");

        renderer header = renderer_new();
        render(header, "// Auto generated by gen_state");
        render_indent(header, "// Manual edits will be discarded");
        render_indent(header, "");
        render_indent(header, "#include <stdint.h>");
        render_indent(header, "#include \"state_code.c\"");


        FILE *stream = fopen("state.c", "w");
        if(!stream)
                fail("Failed to create state.c");

        render(header, "\n\n");
        render(map, "\n\n");
        render(enums, "\n\n");

        write_renderer(header, stream);
        write_renderer(map, stream);
        write_renderer(enums, stream);
        write_renderer(code, stream);

        fclose(stream);
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
