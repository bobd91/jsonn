#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

// TODO do we need real to be long double to preserve precision?

static jsonn_type set_integer_result(jsonn_parser p, long integer) 
{
        p->result.number.integer = integer;
        return JSONN_INTEGER;
}

static jsonn_type set_real_result(jsonn_parser p, double real) 
{
        p->result.number.real = real;
        return JSONN_REAL;
}

// Increment pos whilst parsing a number
// If we run into end of buffer then 
// allocate new buffer, copy across all seen bytes
// and setup to continue parsing at the same position
static uint8_t *next_number_pos(jsonn_parser p, uint8_t *pos)
{
        if(p->last > ++pos)
                return pos;
        int copy_count = p->last - p->current;
        if(copy_next(p, copy_count))
                return p->current + copy_count;
        // failed to allocate a new block
        // returning a pointer to last (which is pointer to null byte)
        // is safe as parse number will not match anything
        return pos;
}

/*
 * Parse JSON number as real or integer
 * Return JSONN_REAL or JSONN_INTEGER and set value into jsonn_result
 * Return JSONN_ERROR if string cannot be parsed as a number
 *
 * JSON is much tighter about what numbers it allows than the library functions
 * strtod or strtol that we use to convert text to numbers 
 * so we have to do a lot of sanity checking first.
 *
 * TODO: error reporting, where is <at> after failed number parse
 */
static jsonn_type parse_number(jsonn_parser p) 
{
        uint8_t *pos = ensure_current(p);
        long integer;
        double real;
        int has_minus = 0;
        int has_zero;
        int int_digits;
        int has_decimal = 0;
        int frac_digits = 0;
        int has_exp = 0;
        int has_exp_sign = 0;
        int exp_digits = 0;
        
        // sanity check
        if('-' == *p->current) {
                has_minus = 1;
                pos = next_number_pos(p, pos);
        }

        has_zero = ('0' == *pos);
        
        while(*pos >= '0' &&  *pos <= '9')
                pos = next_number_pos(p, pos);

        int_digits = pos - p->current - has_minus;

        if('.' == *pos) {
                has_decimal = 1;
                pos = next_number_pos(p, pos);

                while(*pos >='0' && *pos <= '9')
                        pos = next_number_pos(p, pos);

                frac_digits = pos
                        - p->current
                        - has_minus 
                        - int_digits
                        - 1;
        }

        if('e' == *pos || 'E' == *pos) {
                has_exp = 1;
                pos = next_number_pos(p, pos);
                if('-' == *pos || '+' == *pos) {
                        has_exp_sign = 1;
                        pos = next_number_pos(p, pos);
                }

                while(*pos >= '0' && *pos <= '9')
                        pos = next_number_pos(p, pos);

                exp_digits = pos
                        - p->current
                        - has_minus
                        - int_digits
                        - has_decimal
                        - frac_digits
                        - has_exp_sign
                        - 1;
        }
        
        if(int_digits == 0
                        || (has_zero && int_digits > 1)
                        || (has_decimal && frac_digits == 0)
                        || (has_exp && exp_digits == 0)) {
                return number_error(p);
        }

        if(has_decimal || has_exp) {
                errno = 0;
                real= strtod((char *)p->current, NULL);
                if(errno) return number_error(p);
                p->current = pos;
                return set_real_result(p, real);
        } else {
                errno = 0;
                integer = strtol((char *)p->current, NULL, 10);
                if(errno) return number_error(p);
                p->current = pos;
                return set_integer_result(p, integer);
        }
}

