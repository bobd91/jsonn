#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "jsonn.h"


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

#define MAX_CHARS_IN_CLASS 22

typedef struct define_config {
        char *name;
        char *define;
} config_flag;

define_config stack_object = { .name = "object", .define = "STACK_OBJECT" };
define_config stack_array = { .name = "array", .define = "STACK_ARRAY" };

define_config[] config_stacks[] {
        stack_object,
        stack_array
};

define_config state_null = { .name = "null", .define = "STATE_NULL" };
define_config state_true = { .name = "true", .define = "STATE_TRUE" };
define_config state_false = { .name = "false", .define = "STATE_FALSE" };
define_config state_number = { .name = "number", .define = "STATE_NUMBER" };
define_config state_key = { .name = "key", .define = "STATE_KEY" };
define_config state_string = { .name = "string", .define = "STATE_STRING" };
define_config state_double_quote = { .name = "double-quote", .define = "STATE_DOUBLE_QUOTE" };
define_config state_single_quote = { .name = "single-quote", .define = "STATE_SINGLE_QUOTE" };
define_config state_escape = { .name = "escape", .define = "STATE_ESCAPE" };
define_config state_utf8 = { .name = "utf8", .define = "STATE_UTF8" };
define_config state_surrogate = { .name = "surrogate", .define = "STATE_SURROGATE" };

define_config config_states[] {
        state_null,
        state_true,
        state_false,
        state_number,
        state_key,
        state_string,
        state_double_quote,
        state_single_quote,
        state_escape,
        state_utf8,
        state_surrgate
};


define_config config_comments = { .name = "comments", .define = "JSONN_FLAG_COMMENTS" };
define_config config_trailing_commas = { .name = "trailing_commas", .define = "JSONN_FLAG_TRAILING_COMMAS" };
define_config config_single_quotes = { .name = "single_quotes", .define = "JSONN_FLAG_SINGLE_QUOTES" };
define_config config_unquoted_keys = { .name = "unquoted_keys", .define = "JSONN_FLAG_UNQUOTED_KEYS" };
define_config config_unquoted_strings = { .name = "unquoted_strings", .define = "JSONN_FLAG_UNQUOTED_STRINGS" };
define_config config_escape_characters = { .name = "escape_characters", .define = "JSONN_FLAG_ESCAPE_CHARACTERS" };
define_config config_optional_commas = { .name = "optional_commas", .define = "JSONN_FLAG_OPTIONAL_COMMAS" };

define_config config_flags[] = {
        config_comments,
        config_trailing_commas,
        config_single_quotes,
        config_unquoted_keys,
        config_unquoted_strings,
        config_escape_characters,
        config_optional_commas
};

struct state_s {
        class_list classes;
        rule_list rules;
};

struct class_s {
        char *name;
        char chars[MAX_CHARS_IN_CLASS];
};

typedef enum {
        RULE_WHITESPACE,
        RULE_TOKEN,
        RULE_OTHER
} rule_type;

struct rule_s {
        char *name;
        rule_type type;
        match_list matches;
};

typedef enum {
        MATCH_CLASS,
        MATCH_CHAR,
        MATCH_RANGE,
        MATCH_SPECIAL
} match_type;

typedef enum {
        DO_CONSUME,
        DONT_CONSUME
} special;

struct range_s {
        uint8_t start;
        uint8_t end;
};

struct match_s {
        match_type type;
        union {
                char *match_class_name;
                class match_class;
                char match_char;
                range match_range;
                special match_special;
        } match;
        action action;
};

typedef enum {
        ACTION_LIST,
        ACTION_COMMAND,
        ACTION_RULE
} action_type;

struct action_s {
        action_type type;
        union {
                action_list actions;
                builtin action_command;
                char *action_rule_name;
                rule action_rule;
        } action;
};

typedef enum {
        PUSH,
        POP,
        POPX,
        IFPOP,
        IFPEEK,
        IFNPEEK,
        IFCONFIG,
        NEXT_TOKEN
} builtin_id;

char *builtin_names[] = {
        "push",
        "pop",
        "popx",
        "ifpop",
        "ifpeek",
        "ifnpeek",
        "ifconfig",
        "next_token"
};

struct builtin_s {
        builtin_id id;
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

builtin_id builtin_lookup(char *name)
{
        for(int i = 0 ; i < sizeof(builtin_names) ; i++) {
                if(0 == strcmp(name, builtin_names[i]))
                        return i;
        }
        fail("Unknown builtin %s", name);
        return -1;
}
        
builtin parse_builtin(jsonn_parser p)
{
        builtin b = fmalloc(sizeof(struct builtin_s));

        jsonn_type type = jsonn_parse_next(p);
        expect_type(type, JSONN_KEY);
        char *key = result_str(p);
        int id = builtin_lookup(key);

        type = jsonn_parse_next(p);
        expect_type(type, JSONN_STRING);


        b->id = id;
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
                act->type = ACTION_RULE;
                act->action.action_rule_name = result_str(p);
                break;
        case JSONN_BEGIN_OBJECT:
                act->type = ACTION_COMMAND;
                act->action.action_command = parse_builtin(p);
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
        int n = strlen(chars);
        switch(n) {
        case 0:
                fail("Rule match cannot be empty");
                // never get here
        case 1:
                m->type = MATCH_CHAR;
                m->match.match_char = *chars;
                break;
        case 9:
                unsigned int r1, r2;
                if(2 == sscanf(chars, "0x%2X-0x%2X", &r1, &r2)) {
                        if(r1 >= r2)
                                fail("Invalid range: %s", chars);
                        m->type = MATCH_RANGE;
                        m->match.match_range.start = r1;
                        m->match.match_range.end = r2;
                        break;
                }
                // fallthrough
        default:
                if('$' == *chars) {
                        m->type = MATCH_CLASS;
                        m->match.match_class_name = chars + 1;
                        break;
                }

                if(0 == strcmp("...", chars)) {
                        m->type = MATCH_SPECIAL;
                        m->match.match_special = 1;
                        break;
                } if(0 == strcmp("???", chars)) {
                        m->type = MATCH_SPECIAL;
                        m->match.match_special = 0;
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

rule parse_rule(char *name, rule_type type, jsonn_parser p)
{
        rule r = fmalloc(sizeof(struct rule_s));
        r->name = name;
        r->type = type;
        r->matches = parse_match_list(p);
        return r;
}

void add_rule(state states, char *name, rule_type type, jsonn_parser p)
{
        append_rule(states, parse_rule(name, type, p));
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
                if(0 == strcmp(name, rl->this->name))
                        return rl->this;
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
        case ACTION_RULE:
                char *name = a->action.action_rule_name;
                a->action.action_rule = find_rule(states, name);
                if(!a->action.action_rule) {
                        warn("Cannot find rule %s", name);
                        res = 0;
                }
                break;
        }
        return res;
}

int validate_match(state states, match m)
{
        int res = 1;
        if(m->type == MATCH_CLASS && !m->match.match_class) {
                char *name = m->match.match_class_name;
                m->match.match_class = find_class(states, name);
                if(!m->match.match_class) {
                        warn("Cannot find class %s", name);
                        res = 0;
                }
                res &= validate_action(states, m->action);
        }
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
                        add_rule(states, key + 1, RULE_WHITESPACE, p);
                } else if('*' == *key) {
                        add_rule(states, key + 1, RULE_TOKEN, p);
                } else {
                        add_rule(states, key, RULE_OTHER, p);
                }
                type = jsonn_parse_next(p);
        }
        expect_type(JSONN_END_OBJECT, type);
        
        return states;
}

void render_states(state states, char *file_prefix)
{
        // Over all rules
        //
        // whitespace rule => state 0
        //
        // token rules, follow whitespace and a note
        // made of max state so it can be tested at runtime
        // to call whitespace until "next_token" builtin allows
        // the requested state transition.
        //
        // Determine default state for non_matches
        //   -1 = error
        //   ... = result of action
        //   ??? = 0x40 | result of action (don't consume)
        //
        // Set state for each match character (class, char, range)
        // to result of action
        //
        // Determining result of action:
        //
        // if rule action then set state to rule state
        //
        // if builtin then:
        //    call required function:
        //      all empty fns atm, then we can determine what they should be
        //    and then set the next state
        //    if/ifn options, next state will not be known
        //    until runntime, must be set in code
        //
        // if action list then as above because
        // at some point there will be a builtin, possibly multiple,
        // called until a state can be derived
        //
        // Make sure #defines are also set in code
        // automated or done by me?
        //
        // Sound jolly complicated to me :(

        (void)states;
        (void)file_prefix;
}

int main(int argc, char *argv[])
{
        if(argc != 3) {
                printf("Usage: state <json file> <output file prefix>\n");
                exit(1);
        }
        
        FILE *stream = fopen(argv[1], "rb");
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
                        if(validate_states(s))
                                render_states(s, argv[2]);
                        else
                                printf("Invalid states\n");
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
