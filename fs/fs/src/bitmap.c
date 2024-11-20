#include "../include/fs.h"

/**
 * A Bitmap in memory is an array of bytes, each byte represents 8 bits.
 * We can directly use uint8_t to represent a byte.
 */

uint8_t *bitmap_init(uint32_t size) {
    uint32_t bitmap_size = (size + 7) / 8;
    uint8_t *bitmap = (uint8_t *)malloc(bitmap_size);
    if (bitmap == NULL) {
        return ERROR_NOSPACE;
    }
    memset(bitmap, 0, bitmap_size);
    return bitmap;
}

/**
 * Caller should ensure index is valid.
 */
void bitmap_set(uint8_t *bitmap, uint32_t index) {
    uint32_t byte_index = index / 8;
    uint32_t bit_index = index % 8;
    bitmap[byte_index] |= (1 << bit_index);
}

/**
 * Caller should ensure index is valid.
 */
void bitmap_clear(uint8_t *bitmap, uint32_t index) {
    uint32_t byte_index = index / 8;
    uint32_t bit_index = index % 8;
    bitmap[byte_index] &= ~(1 << bit_index);
}

/**
 * @param size: the size of bitmap
 */
int bitmap_find_first_zero(uint8_t *bitmap, uint32_t size) {
    int ret = -1;
    uint32_t bitmap_size = (size + 7) / 8;
    for (uint32_t i = 0; i < bitmap_size; i++) {
        if (bitmap[i] == 0xFF)
            continue;
        for (uint32_t j = 0; j < 8; j++) {
            if ((bitmap[i] & (1 << j)) == 0) {
                ret = i * 8 + j;
                if (ret >= size) {
                    return ERROR_NOSPACE;
                }
                return ret;
            }
        }
    }
    return ERROR_NOSPACE;
}

int bitmap_alloc(uint8_t *bitmap, uint32_t size) {
    int index = bitmap_find_first_zero(bitmap, size);
    if (index == ERROR_NOSPACE) {
        return ERROR_NOSPACE;
    }
    bitmap_set(bitmap, index);
    return index;
}