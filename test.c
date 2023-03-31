#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "gap_buffer.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

static size_t generateUnsignedIntegerBetween(size_t min, size_t max)
{
    assert(max >= min);
    return min + rand() % (max - min + 1);
}

static size_t generateString(char *dst, size_t max)
{
    size_t len = generateUnsignedIntegerBetween(0, max);
    for (size_t i = 0; i < len; i++)
        dst[i] = (char) generateUnsignedIntegerBetween(0, 255);
    return len;
}

static size_t generateUTF8String(char *dst, size_t max)
{
    if (max == 0)
        return 0;

    size_t max_len = generateUnsignedIntegerBetween(1, max);
    size_t len = 0;
    do {
        size_t num = generateUnsignedIntegerBetween(1, MIN(4, max_len-len));

        uint8_t temp[4];

        uint32_t rune;

        switch (num) {
            case 1: rune = generateUnsignedIntegerBetween(0x0000,  0x007f); break;
            case 2: rune = generateUnsignedIntegerBetween(0x0080,  0x07ff); break;
            case 3: rune = generateUnsignedIntegerBetween(0x0800,  0xffff); break;
            case 4: rune = generateUnsignedIntegerBetween(0x10000, 0x10ffff); break;
        }

        switch (num) {
            case 1: temp[0] = rune; 
                    break;
            case 2: temp[0] = ((rune >> 6) & 0x1f) | 0xC0; 
                    temp[1] = ((rune >> 0) & 0x3f) | 0x80; 
                    break;
            case 3: temp[0] = ((rune >> 12) & 0x0f) | 0xe0; 
                    temp[1] = ((rune >>  6) & 0x3f) | 0x80;
                    temp[2] = ((rune >>  0) & 0x3f) | 0x80; 
                    break;
            case 4: temp[0] = ((rune >> 18) & 0x07) | 0xf0; 
                    temp[1] = ((rune >> 12) & 0x3f) | 0x80;
                    temp[2] = ((rune >>  6) & 0x3f) | 0x80; 
                    temp[3] = ((rune >>  0) & 0x3f) | 0x80; 
                    break;
        }
        
        uint32_t rune2;
        int k = getSymbolRune((char*) temp, num, &rune2);
        assert(k >= 0);
        assert(k == num);
        assert(rune == rune2);

        memcpy(dst + len, temp, num);
        len += num;
    } while (len < max_len);
    return len;
}

void printStringAsHex(char *str, size_t len, FILE *stream)
{
    fprintf(stream, "[ ");
    for (size_t i = 0; i < len; i++) {
        static const char table[] = "0123456789abcdef";
        fprintf(stream, "%c%c ", table[(unsigned char) str[i] >> 4], table[(unsigned char) str[i] & 0xf]);
    }
    fprintf(stream, "]");
}

int main(void)
{
    srand(time(NULL));
    char buffer[32/*65536*/];
    GapBuffer *gap_buffer = GapBuffer_create(0);
    assert(gap_buffer != NULL);
    while (1) {
        switch (generateUnsignedIntegerBetween(0, 6)) {
            
            case 0:
            {
                size_t len = generateString(buffer, sizeof(buffer));
                bool done = GapBuffer_insertString(&gap_buffer, buffer, len); 
                fprintf(stderr, "INSERT %ld \"%.*s\" .. %s\n", len, (int) len, buffer, done ? "DONE" : "NOT DONE");
                //printStringAsHex(buffer, len, stderr);
                //fprintf(stderr, "\n");
                break;
            }

            case 1:
            {
                size_t len = generateUTF8String(buffer, sizeof(buffer));
                bool done = GapBuffer_insertString(&gap_buffer, buffer, len); 
                fprintf(stderr, "INSERT %ld \"%.*s\" .. %s\n", len, (int) len, buffer, done ? "DONE" : "NOT DONE");
                //printStringAsHex(buffer, len, stderr);
                //fprintf(stderr, "\n");
                break;
            }

            case 2:
            {
                size_t limit = 1.5 * GapBuffer_getSize(gap_buffer);
                size_t index = generateUnsignedIntegerBetween(0, limit);
                fprintf(stderr, "MOVE_ABSOLUTE %ld\n", index);
                GapBuffer_moveAbsolute(gap_buffer, index);
                break;
            }

            case 3:
            {
                size_t limit = 1.5 * GapBuffer_getSize(gap_buffer);
                size_t index = generateUnsignedIntegerBetween(0, limit);
                fprintf(stderr, "MOVE_RELATIVE %ld\n", index);
                GapBuffer_moveRelative(gap_buffer, index);
                break;
            }


            case 4:
            {
                size_t limit = 1.5 * GapBuffer_getSize(gap_buffer);
                size_t length = generateUnsignedIntegerBetween(0, limit);
                fprintf(stderr, "REMOVE_FORWARDS %ld\n", length);
                GapBuffer_removeForwards(gap_buffer, length);
                break;
            }

            case 5:
            {
                size_t limit = 1.5 * GapBuffer_getSize(gap_buffer);
                size_t length = generateUnsignedIntegerBetween(0, limit);
                fprintf(stderr, "REMOVE_BACKWARDS %ld\n", length);
                GapBuffer_removeBackwards(gap_buffer, length);
                break;
            }

            case 6:
            {
                fprintf(stderr, "PRINT\n");
                GapBufferIter iter;
                GapBufferLine line;
                GapBufferIter_init(&iter, gap_buffer);
                while (GapBufferIter_next(&iter, &line));
                GapBufferIter_free(&iter);
                break;
            }

        }
    }
    GapBuffer_destroy(gap_buffer);
    return 0;
}