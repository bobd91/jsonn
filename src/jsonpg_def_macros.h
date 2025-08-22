
#ifndef JSONPG_GEN
#define JSONPG_GEN      gen
#endif

#ifndef JSONPG_MACROS
#define JSONPG_MACROS

#define object(...)     begin_object(), __VA_ARGS__, end_object()
#define array(...)      begin_array(), __VA_ARGS__, end_array()

#define kv(S,...)       key(S), __VA_ARGS__
#define key(S)          jsonpg_key((JSONPG_GEN), (S))
#define string(S)       jsonpg_str((JSONPG_GEN), (S))
#define true()          jsonpg_boolean((JSONPG_GEN), 1)
#define false()         jsonpg_boolean((JSONPG_GEN), 0)
#define null()          jsonpg_null((JSONPG_GEN))
#define integer(I)      jsonpg_integer((JSONPG_GEN), (I))
#define real(R)         jsonpg_real((JSONPG_GEN), (R))
#define bytes(B, C)     jsonpg_bytes((JSONPG_GEN), (B), (C))
#define begin_object()  jsonpg_begin_object((JSONPG_GEN))
#define end_object()    jsonpg_end_object((JSONPG_GEN))
#define begin_array()   jsonpg_begin_array((JSONPG_GEN))
#define end_array()     jsonpg_end_array((JSONPG_GEN))

#endif

