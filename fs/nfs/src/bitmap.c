/**
 * Encapsulates bitmap operations
 */
#include "../include/nfs.h"

int bitmap_get_bit(uint8_t *bitmap, int index) {
    int byte_index = index / 8;
    int bit_index = index % 8;
    return bitmap[byte_index] & (1 << bit_index);
}

int bitmap_reverse_bit(uint8_t *bitmap, int index) {
    int byte_index = index / 8;
    int bit_index = index % 8;
    bitmap[byte_index] ^= (1 << bit_index);
}

int bitmap_find_first_zero(uint8_t *bitmap) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (!bitmap_get_bit(bitmap, i)) {
            return i;
        }
    }
}
