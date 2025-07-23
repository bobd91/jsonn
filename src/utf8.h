/*
 * jsonn - a simple JSON parser
 * © 2025 Bob Davison (see also: LICENSE)
 */

#pragma once

#include <stdint.h>

int is_bom(const uint8_t *utf8);
void write_replacement_character(uint8_t *utf8_output);
int write_utf8(int cp, uint8_t* utf8_output);
int surrogate_pair_to_codepoint(int u1, int u2);
int is_first_surrogate(uint16_t utf16);
int is_second_surrogate(uint16_t byte);
int validate_utf8(const int8_t* utf8);
