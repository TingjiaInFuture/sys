#include <stdio.h>
#include <stdlib.h> // For strtol
#include <string.h> // For strerror
#include <errno.h>  // For errno

// Function to print the 16x16 dot matrix font
void print_char_matrix(unsigned char dot_matrix[32]) {
    int i, j;
    unsigned char byte_val;
    printf("---------------------------------\n"); // Top border
    for (i = 0; i < 16; i++) { // 16 rows
        printf("|"); // Left border
        byte_val = dot_matrix[i * 2];     // Left 8 pixels of the row
        for (j = 0; j < 8; j++) {
            if ((byte_val << j) & 0x80) { // Check MSB first
                printf("* ");
            } else {
                printf("  ");
            }
        }
        byte_val = dot_matrix[i * 2 + 1]; // Right 8 pixels of the row
        for (j = 0; j < 8; j++) {
            if ((byte_val << j) & 0x80) { // Check MSB first
                printf("* ");
            } else {
                printf("  ");
            }
        }
        printf("|\n"); // Right border
    }
    printf("---------------------------------\n"); // Bottom border
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <GB2312_byte1_hex> <GB2312_byte2_hex>\n", argv[0]);
        fprintf(stderr, "Example (for '中'): %s D6 D0\n", argv[0]);
        fprintf(stderr, "Example (for '国'): %s B9 FA\n", argv[0]);
        fprintf(stderr, "Example (for '你'): %s C4 E3\n", argv[0]);
        return 1;
    }

    char *endptr;
    long val1 = strtol(argv[1], &endptr, 16);
    if (*endptr != '\0' || val1 < 0 || val1 > 0xFF) {
        fprintf(stderr, "Error: Invalid hex value for byte1: %s\n", argv[1]);
        return 1;
    }
    unsigned char b1 = (unsigned char)val1;

    long val2 = strtol(argv[2], &endptr, 16);
    if (*endptr != '\0' || val2 < 0 || val2 > 0xFF) {
        fprintf(stderr, "Error: Invalid hex value for byte2: %s\n", argv[2]);
        return 1;
    }
    unsigned char b2 = (unsigned char)val2;

    // Validate GB2312 range (A1A1 - F7FE)
    if (b1 < 0xA1 || b1 > 0xF7 || b2 < 0xA1 || b2 > 0xFE) {
        fprintf(stderr, "Warning: GB2312 code 0x%02X%02X is outside the typical character range (A1A1-F7FE).\n", b1, b2);
        // Allow proceeding for testing arbitrary offsets if needed, but warn.
    }

    // Calculate offset in HZK16 file
    // qm (区码) and wm (位码) are 1-indexed for the formula.
    // b1, b2 are the actual byte values (e.g., 0xD6, 0xD0 for '中')
    // 区码 = b1 - 0xA0 (e.g., 0xD6 - 0xA0 = 0x36 = 54 decimal)
    // 位码 = b2 - 0xA0 (e.g., 0xD0 - 0xA0 = 0x30 = 48 decimal)
    unsigned int qm = b1 - 0xA0;
    unsigned int wm = b2 - 0xA0;
    // The formula ((区码-1)*94 + (位码-1)) * 32 assumes 1-based 区码 and 位码 from 01-94.
    // Our qm and wm are derived by subtracting 0xA0, so they are effectively 1-based.
    // Example: 0xA1A1 -> qm=1, wm=1. Offset = ((1-1)*94 + (1-1))*32 = 0. Correct.
    unsigned long offset = ((qm - 1) * 94 + (wm - 1)) * 32;

    FILE *fp_hzk16;
    // HZK16 file is expected to be in the parent directory relative to where the executable runs.
    // If test_hzk16.exe is in hzk16_tester/, then ../HZK16 points to sys/HZK16.
    const char *hzk16_path = "../HZK16"; 
    unsigned char font_buffer[32];

    fp_hzk16 = fopen(hzk16_path, "rb");
    if (fp_hzk16 == NULL) {
        fprintf(stderr, "Error opening HZK16 file at %s: %s\n", hzk16_path, strerror(errno));
        fprintf(stderr, "Please ensure HZK16 file exists at the root of the 'sys' directory.\n");
        return 1;
    }

    if (fseek(fp_hzk16, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking to offset %lu (0x%lX) in HZK16 file: %s\n", offset, offset, strerror(errno));
        fclose(fp_hzk16);
        return 1;
    }

    size_t bytes_read = fread(font_buffer, 1, 32, fp_hzk16);
    if (bytes_read != 32) {
        if (feof(fp_hzk16)) {
            fprintf(stderr, "Error reading from HZK16 file: reached end of file prematurely at offset %lu (0x%lX). Expected 32 bytes, got %zu.\n", offset, offset, bytes_read);
        } else if (ferror(fp_hzk16)) {
            fprintf(stderr, "Error reading from HZK16 file at offset %lu (0x%lX): %s\n", offset, offset, strerror(errno));
        } else {
            fprintf(stderr, "Error reading from HZK16 file at offset %lu (0x%lX): unknown error. Expected 32 bytes, got %zu.\n", offset, offset, bytes_read);
        }
        fclose(fp_hzk16);
        return 1;
    }

    fclose(fp_hzk16);

    printf("Displaying character for GB2312 code: 0x%02X%02X (区码: %u, 位码: %u)\n", b1, b2, qm, wm);
    printf("Calculated offset in HZK16: %lu (0x%lX)\n", offset, offset);
    print_char_matrix(font_buffer);

    return 0;
}
