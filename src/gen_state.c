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
typedef struct action_builder_s *action_builder;

#define MAX_CHARS_IN_CLASS 22


#define render(X, Y)   (str_buf_append_chars(X, Y))
#define render_c(X, Y)  (str_buf_append(X, (uint8_t *)&Y, 1))

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

struct action_builder_s {
        uint8_t id;
        str_buf sbuf;
};

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
                fail("Expected type %d, got %d", t1, t2);
        }
}

void expect_next(jsonn_type t, jsonn_parser p)
{
        jsonn_type t2 = jsonn_parse_next(p);
        expect_type(t, t2);
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

void write_str_buf(str_buf sbuf, char *filename)
{
        FILE *f = fopen(filename, "w");
        if(!f)
                fail("Unable to create %s", filename);

        uint8_t *str;
        int len = str_buf_content(sbuf, &str);
        fprintf(f, "%.*s\n", len, (char *)str);

        fclose(f);
}

void render_x(str_buf sbuf, uint8_t x)
{
        render(sbuf, "0x");
        render_c(sbuf, gen_hex_chars[x >> 4]);
        render_c(sbuf, gen_hex_chars[x & 0xF]);
}        

void render_enum(rule r, str_buf enums, int first) 
{
        if(r->id < 0)
                // will miss first processing if first entry
                // if virtual, which it isn't at the moment (phew)
                return;

        if(!first)
                render(enums, ",");
        render(enums, "\n        state_");
        render(enums, r->name);
}

void render_map_values(rule r, uint8_t *bytes, str_buf map, int first_map)
{
        if(r->id < 0)
                return;

        int first = 1;
        if(!first_map)
                render(map, ", ");
        render(map, "\n{");
        for(int i = 0 ; i < 256 ; i++) {
                if(!first)
                        render(map, ", ");
                first = 0;
                render_x(map, bytes[i]);
        }
        render(map, "}");
}
        
void render_command(builtin command, str_buf code)
{
        int is_if =  (0 == strncmp("if", command->name, 2));
        if(is_if)
                render(code, "if(");
        render(code, command->name);
        if(strlen(command->arg))
                render(code, "_");
        render(code, command->arg);
        render(code, "()");
        if(is_if)
                render(code, ")\n");
        else
                render(code, ";\n");
}

void render_rule_action(rule r, str_buf code)
{
        render(code, "return state_");
        render(code, r->name);
        render(code, ";\n");
}

void render_actions(action_list, str_buf);

void render_action(action a, str_buf code)
{
        switch(a->type) {
        case ACTION_LIST:
                render_actions(a->action.actions, code);
                break;
        case ACTION_COMMAND:
                render_command(a->action.command, code);
                break;
        case ACTION_RULE:
                rule r = a->action.rule;
                if(r->id < 0) {
                       render_action(r->matches->this->action, code);
                } else {
                        render_rule_action(r, code);
                }
                break;
        case ACTION_RULE_NAME:
                fail("Rule %s not validated", a->action.rule_name);
        }
}

void render_actions(action_list al, str_buf code)
{
        render(code, "{\n");
        while(al) {
                render_action(al->this, code);
                al = al->next;
        }
        render(code, "}\n");
}


void render_code(rule r, str_buf code)
{
        match_list ml = r->matches;
        while(ml) {
                match m = ml->this;
                render_action(m->action, code);
                ml = ml->next;
        }
}

uint8_t render_new_action(str_buf code)
{
        uint8_t s = gen_action_id++;
        render(code, "case ");
        render_x(code, s);
        render(code, ":\n");
        return s;
}

void render_action_comment(rule r, match m, str_buf code)
{
        render(code, "// ");
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
        render(code, "\n");
}

uint8_t render_match_action(rule r, match m, str_buf code)
{
        if(m->id)
                return m->id;

        action a = m->action;
        switch(a->type) {
        case ACTION_LIST:
                m->id = render_new_action(code);
                render_action_comment(r, m, code);
                render_actions(a->action.actions, code);
                render(code, "break;\n");
                break;
        case ACTION_COMMAND:
                m->id = render_new_action(code);
                render_action_comment(r, m, code);
                render_command(a->action.command, code);
                render(code, "break;\n");
                break;
        case ACTION_RULE:
                rule ar = a->action.rule;
                if(ar->id < 0) {
                        render_action_comment(r, m, code);
                        m->id = render_match_action(ar, ar->matches->this, code);
                } else {
                        m->id = ar->id;
                }
                break;
        case ACTION_RULE_NAME:
                fail("Rule %s not validated", a->action.rule_name);
        }


        return m->id;
}

uint8_t render_rule_default_state(rule r, str_buf code)
{
        match_list ml = r->matches;
        while(ml) {
                match m = ml->this;
                if(m->type == MATCH_ANY || m->type == MATCH_VIRTUAL) {
                        return render_match_action(r, m, code);
                        break;
                }
                ml = ml->next;
        }

        return -1; //state_error;
}





void render_rule(rule r, str_buf enums, str_buf map, str_buf code, int first)
{
        static uint8_t rule_states[256];

        render_enum(r, enums, first);
        match_list ml = r->matches;
        uint8_t def_state = render_rule_default_state(r, code);
        memset(rule_states, def_state, 256);

        while(ml) {
                int len;
                match m = ml->this;
                uint8_t match_state = render_match_action(r, m, code);
                switch(m->type) {
                case MATCH_CLASS:
                        uint8_t *chars = m->match.class->chars;
                        len = strlen((char *)chars);
                        for(int i = 0 ; i < len ; i++)
                                rule_states[chars[i]] = match_state;
                        break;
                case MATCH_CHAR:
                        rule_states[m->match.character] = match_state;
                        break;
                case MATCH_RANGE:
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
                        // r->id == -1 so exit at top of fn
                }
                ml = ml->next;
        }
        render_map_values(r, rule_states, map, first);
}


void render_state(state states)
{
        rule_list rl = states->rules;
        str_buf enums = str_buf_new();
        str_buf map = str_buf_new();
        str_buf code = str_buf_new();

        render(enums, "typdef enum {");
        render(map, "uint8_t state_map[][256] = {");
        
        int first = 1;
        while(rl) {
                render_rule(rl->this, enums, map, code, first);
                first = 0;
                rl = rl->next;
        }

        render(enums, "\n} state;\n");
        render(map, "\n};\n");

        write_str_buf(enums, "state_enums.c");
        write_str_buf(map, "state_map.c");
        write_str_buf(code, "state_code.c");
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
