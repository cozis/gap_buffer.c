#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "gap_buffer.h"

#ifdef GAPBUFFER_DEBUG
#define PRIVATE
#else
#define PRIVATE static
#endif

#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

typedef struct {
    const char *data;
    size_t      size;
} String;

struct GapBuffer {
    void (*free)(void*);
    size_t gap_offset;
    size_t gap_length;
    size_t total;
    char   data[];
};

PRIVATE size_t getByteCount(GapBuffer *buff)
{
    return buff->total - buff->gap_length;
}

GapBuffer *GapBuffer_createUsingMemory(void *mem, size_t len, void (*free)(void*))
{
    if (mem == NULL || len < sizeof(GapBuffer)) {
        if (free) free(mem);
        return NULL;
    }
    
    size_t capacity = len - sizeof(GapBuffer);

    GapBuffer *buff = mem;
    buff->gap_offset = 0;
    buff->gap_length = capacity;
    buff->total = capacity;
    buff->free = free;
    return buff;
}

/* Symbol: GapBuffer_destroy
**   Delete an instanciated gap buffer. 
*/
void GapBuffer_destroy(GapBuffer *buff)
{
    if (buff->free)
        buff->free(buff);
}

/* Symbol: getStringBeforeGap
**   Returns a slice to the memory region before the gap
**   in the form of a (pointer, length) pair.
*/
PRIVATE String getStringBeforeGap(const GapBuffer *buff)
{
    return (String) {

        .data=buff->data, // The start of the buffer is also the
                          // start of the region before the gap.

        .size=buff->gap_offset, // The offset of the gap is the the length
                                // of the region that comes before it.
    };
}

/* Symbol: getStringAfterGap
**   Returns a slice to the memory region after the gap
**   in the form of a (pointer, length) pair.
*/
PRIVATE String getStringAfterGap(const GapBuffer *buff)
{
    // The first byte after the gap is the offset
    // of the text that comes after the gap and
    // the length of the region before the gap plus
    // the length of the gap.
    size_t first_byte_after_gap = buff->gap_offset + buff->gap_length;

    return (String) {
        .data = buff->data  + first_byte_after_gap,
        .size = buff->total - first_byte_after_gap, // The length of the region following the
                                                    // gap is the total number of bytes minus
                                                    // the offset of the first byte after the gap.
    };
}

PRIVATE bool insertBytesBeforeCursor(GapBuffer *buff, String str)
{
    if (buff->gap_length < str.size)
        return false;
    
    memcpy(buff->data + buff->gap_offset, str.data, str.size);
    buff->gap_offset += str.size;
    buff->gap_length -= str.size;
    return true;
}

PRIVATE bool insertBytesAfterCursor(GapBuffer *buff, String str)
{
    if (buff->gap_length < str.size)
        return false;

    memcpy(buff->data + buff->gap_offset + buff->gap_length - str.size, str.data, str.size);
    buff->gap_length -= str.size;
    return true;
}

GapBuffer *GapBuffer_cloneUsingMemory(void *mem, size_t len, 
                                      void (*free)(void*),
                                      const GapBuffer *src)
{
    GapBuffer *clone = GapBuffer_createUsingMemory(mem, len, free);
    if (!clone)
        return NULL;

    String before = getStringBeforeGap(src);
    if (!insertBytesBeforeCursor(clone, before))
        goto oopsie;

    String after = getStringAfterGap(src);
    if (!insertBytesAfterCursor(clone, after))
        goto oopsie;
        
    return clone;

oopsie:
    GapBuffer_destroy(clone);
    return NULL;
}

// Returns true if and only if the [byte] is in the form 10xxxxxx
PRIVATE bool isSymbolAuxiliaryByte(uint8_t byte)
{
    //   Hex    Binary
    // +-----+----------+
    // | C0  | 11000000 |
    // +-----+----------+
    // | 80  | 10000000 |
    // +-----+----------+

    return (byte & 0xC0) == 0x80;
}

PRIVATE int getSymbolRune(const char *sym, size_t symlen, uint32_t *rune)
{
    if(symlen == 0)
        return 0;
    
    if(sym[0] & 0x80) {

        // May be UTF-8.
            
        if((unsigned char) sym[0] >= 0xF0) {

            // 4 bytes.
            // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

            if(symlen < 4)
                return -1;

            if (!isSymbolAuxiliaryByte(sym[1]) ||
                !isSymbolAuxiliaryByte(sym[2]) ||
                !isSymbolAuxiliaryByte(sym[3]))
                return -1;
                    
            uint32_t temp 
                = (((uint32_t) sym[0] & 0x07) << 18) 
                | (((uint32_t) sym[1] & 0x3f) << 12)
                | (((uint32_t) sym[2] & 0x3f) <<  6)
                | (((uint32_t) sym[3] & 0x3f));

            if(temp < 0x010000 || temp > 0x10ffff)
                return -1;

            *rune = temp;
            return 4;
        }
            
        if((unsigned char) sym[0] >= 0xE0) {

            // 3 bytes.
            // 1110xxxx 10xxxxxx 10xxxxxx

            if(symlen < 3)
                return -1;

            if (!isSymbolAuxiliaryByte(sym[1]) ||
                !isSymbolAuxiliaryByte(sym[2]))
                return -1;

            uint32_t temp
                = (((uint32_t) sym[0] & 0x0f) << 12)
                | (((uint32_t) sym[1] & 0x3f) <<  6)
                | (((uint32_t) sym[2] & 0x3f));
            
            if (temp < 0x0800 || temp > 0xffff)
                return -1;

            *rune = temp;
            return 3;
        }
            
        if((unsigned char) sym[0] >= 0xC0) {

            // 2 bytes.
            // 110xxxxx 10xxxxxx

            if(symlen < 2)
                return -1;

            if (!isSymbolAuxiliaryByte(sym[1]))
                return -1;

            *rune 
                = (((uint32_t) sym[0] & 0x1f) << 6)
                | (((uint32_t) sym[1] & 0x3f));

            if (*rune < 0x80 || *rune > 0x07ff)
                return -1;

            assert(*rune <= 0x10ffff);
            return 2;
        }
            
        return -1;
    }

    // It's ASCII
    // 0xxxxxxx

    *rune = (uint32_t) sym[0];
    return 1;
}

PRIVATE bool isValidUTF8(const char *str, size_t len)
{
    size_t i = 0;
    while (i < len) {
        uint32_t rune; // Unused
        int n = getSymbolRune(str + i, len - i, &rune);
        if (n < 0)
            return false;
        i += n;
    }
    return true;
}

bool GapBuffer_insertString(GapBuffer *buff, const char *str, size_t len)
{
    if (!isValidUTF8(str, len))
        return false;
    return insertBytesBeforeCursor(buff, (String) {.data=str, .size=len});
}

/* Symbol: getPrecedingSymbol
**
**   Calculate the absolute byte offset of the 
**   [num]-th unicode symbol preceding the cursor.
**
**   If less than [num] symbols precede the
**   cursor, 0 is returned.
**
** Arguments:
**   - buff: Reference to the gap buffer
**
**   - num: Position of the unicode symbol preceding
**          the cursor of which the offset should be
**          returned, relative to the cursor.
**
** Notes:
**   - It's analogous to getFollowingSymbol.
*/
PRIVATE size_t getPrecedingSymbol(GapBuffer *buff, size_t num)
{
    size_t i = buff->gap_offset;

    while (num > 0 && i > 0) {

        // Consume the auxiliary bytes of the
        // UTF-8 sequence (those in the form
        // 10xxxxxx) preceding the cursor
        do {
            assert(i > 0); // FIXME: This triggers sometimes
            i--;
            // The index can never be negative because
            // this loop only iterates over the auxiliary
            // bytes of a UTF-8 byte sequence. If the
            // buffer only contains valid UTF-8, it will
            // not underflow.
        } while (isSymbolAuxiliaryByte(buff->data[i]));

        // A character was consumed.
        num--;
    }

    return i;
}

PRIVATE size_t getSymbolLengthFromFirstByte(uint8_t first)
{
    // NOTE: It's assumed a valid first byte
    if (first >= 0xf0)
        return 4;
    if (first >= 0xe0)
        return 3;
    if (first >= 0xc0)
        return 2;
    return 1;
}

/* Symbol: getFollowingSymbol
**
**   Calculate the absolute byte offset of the 
**   [num]-th unicode symbol following the cursor.
**
**   If less than [num] symbols follow the cursor, 
**   0 is returned.
**
** Arguments:
**   - buff: Reference to the gap buffer
**
**   - num: Position of the unicode symbol following
**          the cursor of which the offset should be
**          returned, relative to the cursor.
**
** Notes:
**   - It's analogous to getPrecedingSymbol.
*/
PRIVATE size_t getFollowingSymbol(GapBuffer *buff, size_t num)
{
    size_t i = buff->gap_offset + buff->gap_length;
    while (num > 0 && i < buff->total) {
        i += getSymbolLengthFromFirstByte(buff->data[i]);
        num--;
    }
    return i;
}

void GapBuffer_removeForwards(GapBuffer *buff, size_t num)
{
    size_t i = getFollowingSymbol(buff, num);
    buff->gap_length = i - buff->gap_offset;
}

void GapBuffer_removeBackwards(GapBuffer *buff, size_t num)
{
    size_t i = getPrecedingSymbol(buff, num);
    buff->gap_length += buff->gap_offset - i;
    buff->gap_offset = i;
}

PRIVATE void moveBytesAfterGap(GapBuffer *buff, size_t num)
{
    assert(buff->gap_offset >= num);

    assert(buff->gap_offset <= buff->total);
    assert(buff->gap_offset <= buff->total);
    assert(buff->gap_offset + buff->gap_length <= buff->total);

    memmove(buff->data + buff->gap_offset + buff->gap_length - num,
            buff->data + buff->gap_offset - num,
            num);
    buff->gap_offset -= num;
}

PRIVATE void moveBytesBeforeGap(GapBuffer *buff, size_t num)
{
    assert(buff->total - buff->gap_offset - buff->gap_length >= num); // FIXME: This triggers sometimes

    assert(buff->gap_offset <= buff->total);
    assert(buff->gap_offset <= buff->total);
    assert(buff->gap_offset + buff->gap_length <= buff->total);

    memmove(buff->data + buff->gap_offset, 
            buff->data + buff->gap_offset + buff->gap_length,
            num);
    buff->gap_offset += num;
}

void GapBuffer_moveRelative(GapBuffer *buff, int off)
{
    if (off < 0) {
        size_t i = getPrecedingSymbol(buff, -off);
        moveBytesAfterGap(buff, buff->gap_offset - i);
    } else {
        size_t i = getFollowingSymbol(buff, off);
        moveBytesBeforeGap(buff, i - buff->gap_offset - buff->gap_length);
    }
}

void GapBuffer_moveAbsolute(GapBuffer *buff, size_t num)
{
    size_t i;
    if (buff->gap_offset > 0)
        i = 0;
    else
        i = buff->gap_length;

    while (num > 0 && i < buff->total) {

        i += getSymbolLengthFromFirstByte(buff->data[i]);

        // If the cursor reached the gap, jump over it.
        if (i == buff->gap_offset)
            i += buff->gap_length;

        num--;
    }
    
    if (i <= buff->gap_offset)
        moveBytesAfterGap(buff, buff->gap_offset - i);
    else
        moveBytesBeforeGap(buff, i - buff->gap_offset - buff->gap_length);
}

void GapBufferIter_init(GapBufferIter *iter, GapBuffer *buff)
{
    iter->crossed_gap = false;
    iter->buff = buff;
    iter->cur = 0;
    iter->mem = NULL;
}

void GapBufferIter_free(GapBufferIter *iter)
{
    iter->mem = NULL;
}

bool GapBufferIter_next(GapBufferIter *iter, GapBufferLine *line)
{
    iter->mem = NULL;

    size_t i = iter->cur;
    size_t total = iter->buff->total;
    size_t gap_offset = iter->buff->gap_offset;
    char *data = iter->buff->data;

    if (iter->crossed_gap) {
        
        size_t line_offset = iter->cur;
        while (i < total && data[i] != '\n')
            i++;
        size_t line_length = i - line_offset;

        if (i < total)
            i++;
        else {
            if (line_length == 0)
                return false;
        }

        line->str = data + line_offset;
        line->len = line_length;
    
    } else {

        size_t line_offset = i;
        while (i < gap_offset && data[i] != '\n')
            i++;
        size_t line_length = i - line_offset;

        if (i == gap_offset) {
            
            i += iter->buff->gap_length;

            size_t line_offset_2 = i;
            while (i < total && data[i] != '\n')
                i++;
            size_t line_length_2 = i - line_offset_2;

            if (i < total)
                i++; // Consume "\n"
            else {
                if (line_length + line_length_2 == 0)
                    return false;
            }

            iter->crossed_gap = true;

            if (line_length + line_length_2 > sizeof(iter->maybe)) {
                // Line will be truncated
                if (line_length > sizeof(iter->maybe))
                    memcpy(iter->maybe, data + line_offset, sizeof(iter->maybe));
                else {
                    memcpy(iter->maybe,               data + line_offset,   line_length);
                    memcpy(iter->maybe + line_offset, data + line_offset_2, sizeof(iter->maybe) - line_length);
                }
                line->str = iter->maybe;
                line->len = line_length + line_length_2;
            } else {
                memcpy(iter->maybe,               data + line_offset,   line_length);
                memcpy(iter->maybe + line_length, data + line_offset_2, line_length_2);
                line->str = iter->maybe;
                line->len = line_length + line_length_2;
            }

        } else {
            i++; // Consume "\n"

            line->str = data + line_offset;
            line->len = line_length;
        }
    }
    iter->cur = i;
    return true;
}

#ifndef GAPBUFFER_NOMALLOC
#include <stdlib.h>
GapBuffer *GapBuffer_create(size_t capacity)
{
    size_t len = sizeof(GapBuffer) + capacity;
    void  *mem = malloc(len);
    return GapBuffer_createUsingMemory(mem, len, free);
}
bool GapBuffer_insertStringMaybeRelocate(GapBuffer **buff, const char *str, size_t len)
{
    if (!GapBuffer_insertString(*buff, str, len)) {
        // Need to relocate
        GapBuffer *buff2 = GapBuffer_create(getByteCount(*buff) + len);
        if (buff2 == NULL)
            return false; // Failed to create new location

        if (!GapBuffer_insertString(buff2, str, len)) {
            // Insertion failed unexpectedly. The gap was created
            // with enough free memory to hold the new text..
            GapBuffer_destroy(buff2);
            return false;
        }

        // Swap the parent buffer with the new one
        GapBuffer_destroy(*buff);
        *buff = buff2;
    }

    return true;
}
#endif