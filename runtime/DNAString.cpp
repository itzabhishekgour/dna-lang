#include "DNAString.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
    DNAString dna_string_literal(const char* data, int32_t len) {
        DNAString str;
        str.data = const_cast<char*>(data);
        str.length = len;
        str.capacity = len;
        return str;
    }

    DNAString dna_string_concat(DNAString a, DNAString b) {
        // Stub for future use
        (void)a; (void)b;
        DNAString str = { nullptr, 0, 0 };
        return str;
    }

    DNAString dna_string_copy(DNAString src) {
        // Stub for future use
        (void)src;
        DNAString str = { nullptr, 0, 0 };
        return str;
    }

    void dna_print_string(DNAString str) {
        if (str.data && str.length > 0) {
            printf("%.*s\n", str.length, str.data);
        } else {
            printf("\n");
        }
        fflush(stdout);
    }

    void dna_string_free(DNAString str) {
        // Stub for future use
        (void)str;
    }
}
