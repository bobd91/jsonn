#ifndef __JSN_MACRO_H__
#define __JSN_MACRO_H__

#ifndef JSN_CTX
#define JSN_CTX ctx
#endif

#define object(...)     begin_object, __VA_ARGS__, end_object
#define array(...)      begin_array, __VA_ARGS__, end_array

#define kv(S,...)       key(S), __VA_ARGS__
#define key(S)          jsn_key(JSN_CTX, S)
#define str(S)          jsn_str(JSN_CTX, S)
#define b_true          jsn_boolean(JSN_CTX, 1)
#define b_false         jsn_boolean(JSN_CTX, 0)
#define null            jsn_null(JSN_CTX)
#define integer(I)      jsn_integer(JSN_CTX, I)
#define real(R)         jsn_real(JSN_CTX, R)
#define bytes(B, C)     jsn_bytes(JSN_CTX, B, C)
#define begin_object    jsn_begin_object(JSN_CTX)
#define end_object      jsn_end_object(JSN_CTX)
#define begin_array     jsn_begin_array(JSN_CTX)
#define end_array       jsn_end_array(JSN_CTX)

#else // __JSN_MACRO_H__

#undef object
#undef array
#undef kv
#undef key
#undef str
#undef b_true
#undef b_false
#undef null
#undef integer
#undef real
#undef bytes
#undef begin_object
#undef end_object
#undef begin_array
#undef begin_array
#undef end_array

#undef __JSN_MACRO_H__

#endif // __JSN_MACRO_H__


