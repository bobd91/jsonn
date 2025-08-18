
typedef struct jsonpg_generator_s *jsonpg_generator;

struct jsonpg_generator_s {
        jsonpg_callbacks *callbacks;
        void *ctx;
};

