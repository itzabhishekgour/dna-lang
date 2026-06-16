#pragma once
#include <stdint.h>

/**
 * DNAString Ownership Rules:
 * - dna_string_literal() returns a non-owning string view.
 * - dna_string_copy() returns an owning string.
 * - dna_string_concat() returns an owning string.
 * - dna_string_free() only frees owning strings.
 */
struct DNAString {
    char* data;
    int32_t length;
    int32_t capacity;
};

extern "C" {
    DNAString dna_string_literal(const char* data, int32_t len);
    DNAString dna_string_concat(DNAString a, DNAString b);
    DNAString dna_string_copy(DNAString src);
    void dna_print_string(DNAString str);
    void dna_string_free(DNAString str);
}
