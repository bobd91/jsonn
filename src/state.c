#include <stdlib.h>
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
typedef struct match_list_s match_list;
typedef struct action_list_s action_list;

struct state_s {
        class_list classes;
        rule_list rules;
};

#define MAX_CHARS_IN_CLASS 16

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
                char *match_classname;
                class match_class;
                char match_char;
                range match_range;
                special match_special;
        } match;
        action_list actions;
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
        } actions;
}

typedef enum {
        PUSH,
        POP,
        POPX,
        IFPOP,
        IFPEEK,
        IFNPEEK,
        IFCONFIG,
        AFTER_WHITESPACE
} builtin_id;

struct builtin_s {
        builtin_id id;
        char *arg;
}

struct class_list_s {
        class this;
        class_list next;
}

struct rule_list_s {
        rule this;
        rule_list next;
}

struct match_list_s {
        match this;
        match_list next;
}

struct action_list_s {
        action this;
        action_list next;
}


state states;



class create_class(state states, char *name)
{
        class next = states->classes;
        class last = next;
        while(next) {
                if(0 == strcmp(next->this->name, name))
                        fail("Duplicate class name: %s", name);
                last = next;
                next = next->next;
        }
        class_list *ptr;
        if(!last)
                ptr = &states->classes;
        else
                ptr = &last->next;
        *ptr = fmalloc(sizeof(struct class_list_s));
        class c = fmalloc(sizeof(struct class_s));
        c->name = name;
        c->chars[0] = '\0';
        *ptr->this = c;
        *ptr->next = NULL;
        return c;
}

void class_add(class cls, char c)
{
        int p = strlen(c->chars);
        if(p >= MAX_CHARS_IN_CLASS)
                fail("Too many characters in character class: %s", c->name);
        c->chars[p] = c;
        c->chars[p + 1] = '\0';
}

rule create_rule(states, char *name, rule_type type)
{
        rule next = states->rules;
        rule last = next;
        while(next) {
                if(0 == strcmp(next->this->name, name))
                        fail("Duplicate rule name: %s", name);
                last = next;
                next = next->next;
        }
        rule_list *ptr;
        if(!last) 
                ptr = &states->rules;
        else
                ptr = &last->next;
        *ptr = fmalloc(sizeof(struct rule_list_s));
        rule r = fmalloc(sizeof(struct rule_s));
        r->name = name;
        r->type = type;
        r->matches = NULL;
        *ptr->this = r;
        *ptr->next = NULL;
        return r;
}

void match_add(rule r, char *chars, jsonn_parser p)
{
        match m = fmalloc(sizeof(struct match_s));
        int n = strlen(chars);
        switch(n) {
        case 0:
                fail("Rule match cannot be empty");
                // never get here
        case 1:
                m->match_type = MATCH_CHAR;
                m->match_char = *chars;
                break;
        case 9:
                unisigned int r1, r2;
                if(2 == sscanf(chars, "0x2X-0x2X", &r1, &r2)) {
                        if(r1 >= r2)
                                fail("Invalid range: %s", chars);
                        m->match_type = MATCH_RANGE;
                        m->match_range.start = r1;
                        m->match_range.end = r2;
                        break;
                }
                // fallthrough
        default:
                if('$' == *chars) {
                        m->match_type = MATCH_CLASS;
                        m->match_classname = chars + 1;
                        break;
                }

                if(0 == strcmp("...", chars)) {
                        m->match_type = MATCH_SPECIAL;
                        m->match_special = 1;
                        break;
                } if(0 == strcmp("???", chars)) {
                        m->match_type = MATCH_SPECIAL;
                        m->match_special = 0;
                        break;
                }
                
                fail("Invalid match specification: %s", chars);

        }
        match_list ml = fmalloc(sizeof(struct match_list_s));
        ml->this = m;
        ml->next = NULL;
        if(!r->matches) {
                r->matches = ml;
        } else {
                match_list next = r->matches;
                while(next->next)
                        next = next->next;
                next->next = ml;
        }

        add_actions(m, p);
}


void parse_rule(rule r, jsonn_parse p) {
        expect_next(JSONN_BEGIN_OBJECT, p);
        jsonn_type type = jsonn_parse_next(p);
        while(type == JSONN_KEY) {
                match_add(r, (char *)(jsonn_parse_result(p).string.bytes), p);
                type = jsonn_parse_next(p);
        }
        expect_type(type, JSONN_END_OBJECT);
}

                                


state create_states()
{
        return malloc(sizeof(struct state_s));
}

void add_class(char *name, jsonn_parser p)
{
        class c = create_class(states, name);
        expect_next(JSONN_BEGIN_ARRAY, p);
        jsonn_type type = jsonn_parse_next(p);
        while(type == JSONN_STRING) {
                class_add(c, (char)*jsonn_parse_result(p).string.bytes);
                type = jsonn_parse_next(p);
        }
        expect_type(type, JSONN_END_ARRAY);
}

void add_rule(char *name, rule_type type, jsonn_parser p)
{
        rule r = create_rule(states, name, type);
        parse_rule(r, p);
}





jsonn_type parse_state(jsonn_parser p)
{
        states = create_states();

        jsonn_type type = jsonn_parse_next(p);
        while(JSONN_KEY == type) {
                char *key = (char *)(jsonn_parse_result(p).string.bytes);
                if('$' == *key) {
                        add_class(key + 1, p);
                } else if('_' == *key) {
                        add_rule(key + 1, RULE_WHITESPACE, p);
                } else if('*' == *key) {
                        add_rule(key + 1, RULE_TOKEN, p);
                } else {
                        add_rule(key, RULE_OTHER, p);
                }
                type = jsonn_parse_next(p);
        }

        validate_state(states);

        return type;
}




int main(int argc, char *argv[])
{
        if(argc != 2) {
                printf("Usage: state <file>\n");
                exit(1);
        }
        
        FILE *stream = fopen(argv[1], "rb");
        if(!stream) {
                perror("Failed to open input");
                exit(1);
        }

        jsonn_parser p = jsonn_new(NULL);
        jsonn_type type = jsonn_parse_stream(stream, NULL, p);
        if(type != JSONN_EOF && type != JSONN_ERROR) {
                type = jsonn_parse_next(p);
                if(JSONN_BEGIN_OBJECT == type) {
                        type = parse_state(p);
                } else {
                        printf("Expecting begin object, got: %d\n", type);
                        exit(1);
                }
                if(JSONN_END_OBJECT != type) {
                        printf("Expecting end object, got: %d\n", type);
                        exit(1);
                }
                type = jsonn_parse_next(p);
        }
        if(type != JSONN_EOF) {
                if(type == JSONN_ERROR) 
                        printf("Error %d at %d\n", 
                                        jsonn_parse_result(p).error.code, 
                                        jsonn_parse_result(p).error.at);
                else
                        printf("Expecting EOF, got: %d\n", type);
                exit(1);
        }
}
