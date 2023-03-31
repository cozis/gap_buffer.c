#warning "temp"
#include <stdint.h>

#include <stddef.h>
#include <stdbool.h>

typedef struct GapBuffer GapBuffer;

typedef struct {
    GapBuffer *buff;
    bool crossed_gap;
    size_t cur;
    void *mem;
    char maybe[256];
} GapBufferIter;

typedef struct {
    const char *str;
    size_t len;
} GapBufferLine;

GapBuffer *GapBuffer_createUsingMemory(void *mem, size_t len);
GapBuffer *GapBuffer_create(size_t capacity);
void       GapBuffer_destroy(GapBuffer *buff);
bool       GapBuffer_insertString(GapBuffer **buff, const char *str, size_t len);
void       GapBuffer_moveRelative(GapBuffer *buff, int off);
void       GapBuffer_moveAbsolute(GapBuffer *buff, size_t num);
void       GapBuffer_removeForwards(GapBuffer *buff, size_t num);
void       GapBuffer_removeBackwards(GapBuffer *buff, size_t num);
size_t     GapBuffer_getByteCount(GapBuffer *buff);
void       GapBufferIter_init(GapBufferIter *iter, GapBuffer *buff);
void       GapBufferIter_free(GapBufferIter *iter);
bool       GapBufferIter_next(GapBufferIter *iter, GapBufferLine *line);

#warning "temporarily not static"
int getSymbolRune(const char *sym, size_t symlen, uint32_t *rune);