#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "gap_buffer.h"

#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

typedef struct {
    const uint8_t *data;
    size_t size;
} String;

struct GapBuffer {
    bool is_static;
    bool no_resize;
    size_t gap_offset;
    size_t gap_length;
    size_t total;
    uint8_t data[];
};

/* Symbol: GapBuffer_getByteCount
**   Return the length in bytes of the string stored
**   in the buffer.
**
** Notes:
**   - This information isn't very useful to the owner
**     of the gap instance. It's only really used while
**     testing. It may be good to make it private to the
**     testing routine somehow.
*/
size_t GapBuffer_getByteCount(GapBuffer *buff)
{
    return buff->total - buff->gap_length;
}

/* Symbol: GapBuffer_createUsingMemory
**   Instanciate a gap buffer by providing it the memory
**   it should use. The gap buffer won't be able to relocate
**   to a bigger memory region if needed.
**
** Arguments:
**   - mem: Address of the memory region used by the buffer
**   - len: Length in bytes of the region referred to by [mem]
**
** Returns:
**   The pointer to a buffer instance or NULL if the
**   provided memory wasn't enough.
**
** Notes:
**   - If you only instanciate buffers this way, you basically
**     can drop the dependency on the libc allocator and potentially
**     libc entirely.
*/
GapBuffer *GapBuffer_createUsingMemory(void *mem, size_t len)
{
    if (mem == NULL || len < sizeof(GapBuffer))
        return NULL;
    
    size_t capacity = len - sizeof(GapBuffer);

    GapBuffer *buff = mem;
    buff->gap_offset = 0;
    buff->gap_length = capacity;
    buff->total = capacity;
    buff->no_resize = true;
    buff->is_static = true;
    return buff;
}

/* Symbol: GapBuffer_create
**   Instanciate a gap buffer by allocating memory through
**   the libc allocator.
**   
**   Gap buffers instanciated this way will allocate and
**   relocate to bigger memory regions if necessary.
**
** Arguments:
**   - capacity: The maximum string length (in bytes) that
**               will be possible to store in the buffer
**               without the need of allocating more memory.
**
** Returns:
**   The pointer to a buffer instance or NULL if the
**   dynamic memory allocation failed.
*/
GapBuffer *GapBuffer_create(size_t capacity)
{
    GapBuffer *buff = malloc(sizeof(GapBuffer) + capacity);
    if (buff == NULL)
        return NULL;
    buff->gap_offset = 0;
    buff->gap_length = capacity;
    buff->total = capacity;
    buff->no_resize = false;
    buff->is_static = false;
    return buff;
}

/* Symbol: GapBuffer_destroy
**   Delete an instanciated gap buffer. 
*/
void GapBuffer_destroy(GapBuffer *buff)
{
    if (!buff->is_static)
        free(buff);
}

/* Symbol: getStringBeforeGap
**   Returns a slice to the memory region before the gap
**   in the form of a (pointer, length) pair.
*/
static String getStringBeforeGap(GapBuffer *buff)
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
static String getStringAfterGap(GapBuffer *buff)
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

/* Forward declaration of [growGap] because [growGap]
** and [ensureSpace] refer to each other recursively,
** even though isn't not possible to have recursive
** calls during execution.
*/
static GapBuffer *growGap(GapBuffer *buff, size_t min);

/* Symbol: ensureSpace
**   Make sure that the buffer has at least [min]
**   unused bytes by relocating the buffer to a
**   bigger memory region if necessary.
**
** Arguments:
**   - buff: Reference to the buffer's reference. If a relocation
**           occurres, the caller's reference will be changed.
** I
**   - min: Minimum number of free bytes the call must ensure.
**          If the buffer has a gap smaller than [min], relocation
**          will occur.
**
** Returns:
**   True if the buffer has more than [min] free 
**   space or if relocation occurred succesfully.
**   If relocation failed or the buffer isn't allowed
**   to grow, false is returned.
**
** Notes:
**   - Only gaps instanciated with [GapBuffer_create]
**     are allowed to be relocated.
*/
static bool ensureSpace(GapBuffer **buff, size_t min)
{
    if ((*buff)->gap_length < min) {

        if ((*buff)->no_resize)
            return false;

        GapBuffer *new_buff = growGap(*buff, min);
        if (new_buff == NULL)
            return false;

        // Free up the old buffer and swap the old 
        // parent's reference with the new one.
        GapBuffer_destroy(*buff);
        *buff = new_buff;
    }
    return true;
}

static bool insertBytesBeforeCursor(GapBuffer **buff, String str)
{
    if (!ensureSpace(buff, str.size))
        return false;
    memcpy((*buff)->data + (*buff)->gap_offset, str.data, str.size);
    (*buff)->gap_offset += str.size;
    (*buff)->gap_length -= str.size;
    return true;
}

static bool insertBytesAfterCursor(GapBuffer **buff, String str)
{
    if (!ensureSpace(buff, str.size))
        return false;
    memcpy((*buff)->data + (*buff)->gap_offset + (*buff)->gap_length - str.size, str.data, str.size);
    (*buff)->gap_length -= str.size;
    return true;
}

static GapBuffer *growGap(GapBuffer *buff, size_t min)
{
    size_t capacity = MAX(2 * buff->total, buff->total + min);
    GapBuffer *new_buff = GapBuffer_create(capacity);
    if (new_buff == NULL)
        return NULL;

    if(!insertBytesBeforeCursor(&new_buff, getStringBeforeGap(buff)) || 
       !insertBytesAfterCursor(&new_buff, getStringAfterGap(buff))) {
        GapBuffer_destroy(new_buff);
        return NULL;
    }
    return new_buff;
}

// Returns true if and only if the [byte] is in the form 10xxxxxx
static bool isSymbolAuxiliaryByte(uint8_t byte)
{
    /*
    0000  0
    0001  1
    0010  2
    0011  3
    0100  4
    0101  5
    0110  6
    0111  7
    1000  8
    1001  9
    1010  A  10
    1011  B  11
    1100  C  12
    1101  D  13
    1110  E  14
    1111  F  15
    */

    //   Hex    Binary
    // +-----+----------+
    // | C0  | 11000000 |
    // +-----+----------+
    // | 80  | 10000000 |
    // +-----+----------+

    return (byte & 0xC0) == 0x80;
}

/* Symbol: getSymbolRune
**   Converts a UTF-8 symbol to UTF-32.
**
** Arguments:
**   - sym: string of bytes containing the UTF-8 symbol.
**
**   - symlen: Length (in bytes) of the string [sym].
** I
**   - rune: The location where the UTF-32 result will
**           be stored. This argument isn't optional
**           (you can pass NULL).
**
** Returns:
**   The number of bytes of [sym] considered to
**   decode the unicode symbol, 0 if the source
**   string was empty or -1 if the source contained 
**   invalid UTF-8.
**
** Notes:
**   - The (sym, symlen) pair can refer to a string of
**     more than one unicode symbol.
*/
#warning "temporarily not static"
int getSymbolRune(const char *sym, size_t symlen, uint32_t *rune)
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

/* Symbol: GapBuffer_insertString
**   Insert a UTF-8 encoded string in the gap buffer,
**   right before the cursor. If there's not enough
**   space to store the string and the gap buffer
**   was't instanciated with [GapBuffer_createUsingMemory],
**   relocation to a bigger memory region occurres.
**
** Arguments:
**   - buff: Reference to the caller's reference to the
**           buffer. The caller's handle is changed if
**           relocation occurres.
**
**   - str: Pointer to the UTF-8 string to be inserted.
** I
**   - len: Length (in bytes) of the input string.
**
** Returns:
**   True if the text was inserted and false if either:
**
**     - The buffer couldn't be relocated because
**       instanciated with a fixed memory pool.
**
**     - The buffer couldn't be relocated because
**       a new memory region couldn't be allocated.
**
**     - The input string wasn't valid UTF-8.
*/
bool GapBuffer_insertString(GapBuffer **buff, const char *str, size_t len)
{
    size_t i = 0;
    while (i < len) {
        uint32_t rune; // Unused
        int n = getSymbolRune(str + i, len - i, &rune);
        if (n < 0)
            return false;
        i += n;
    }
    return insertBytesBeforeCursor(buff, (String) {.data=(const uint8_t*) str, .size=len});
}

static size_t getPrecedingSymbol(GapBuffer *buff, size_t num)
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

static size_t getSymbolLengthFromFirstByte(uint8_t first)
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

static size_t getFollowingSymbol(GapBuffer *buff, size_t num)
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

static void moveBytesAfterGap(GapBuffer *buff, size_t num)
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

static void moveBytesBeforeGap(GapBuffer *buff, size_t num)
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
    if (iter->mem != NULL) {
        free(iter->mem);
        iter->mem = NULL;
    }
}

bool GapBufferIter_next(GapBufferIter *iter, GapBufferLine *line)
{
    if (iter->mem != NULL) {
        free(iter->mem);
        iter->mem = NULL;
    }

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
                char *str = malloc(line_length + line_length_2);
                if (str == NULL) {
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
                    iter->mem = str;
                    memcpy(str,               data + line_offset,   line_length);
                    memcpy(str + line_length, data + line_offset_2, line_length_2);
                    line->str = str;
                    line->len = line_length + line_length_2;
                }
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