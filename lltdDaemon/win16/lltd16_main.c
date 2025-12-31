/*
 * LLTD Win16 diagnostic utility stub.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char tlv_bytes[] = {
    0x01, 0x04, 0xde, 0xad, 0xbe, 0xef,
    0x02, 0x02, 0xca, 0xfe
};

static void write_binary(const char *path) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return;
    }
    fwrite(tlv_bytes, 1, sizeof(tlv_bytes), file);
    fclose(file);
}

static void write_text(const char *path) {
    FILE *file = fopen(path, "w");
    if (!file) {
        return;
    }
    fprintf(file, "LLTD Win16 diagnostic stub\n");
    fprintf(file, "TLV bytes (%u):", (unsigned)sizeof(tlv_bytes));
    for (size_t i = 0; i < sizeof(tlv_bytes); i++) {
        fprintf(file, " %02X", tlv_bytes[i]);
    }
    fprintf(file, "\n");
    fclose(file);
}

int main(void) {
    printf("LLTD Win16 diagnostic stub\n");
    write_binary("OUTPUT.BIN");
    write_text("OUTPUT.TXT");
    return 0;
}
