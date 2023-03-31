# gap_buffer.c
This repository is a self-contained gap buffer implementation in C which supports unicode (utf-8). The aim of the implementation is to be simple, robust and easy to use.

# Table of contents
* [What is a gap buffer?](#what-is-a-gap-buffer)
    * [Insertion](#insertion)
    * [Motion](#motion)
    * [Deletion](#deletion)
    * [Considerations](#considerations)
* [License](#license)
* [Usage](#usage)
    * [Install](#install)
    * [Instanciate](#instanciate)
    * [Text insertion](#text-insertion)
    * [Cursor position](#cursor-position)
    * [Text deletion](#text-deletion)
* [Testing](#testing)

## What is a gap buffer?
A gap buffer is a data structure that stores strings of text in a way that's optimized for operations based on a cursor, which makes it useful for text editors. The way it works is by dividing the memory is three regions: the text preceding the cursor, the unused memory and then the text following the cursor. The unused memory region is often referred to as "gap", hence the name "gap buffer".

### Insertion
Imagine storing the string "Hello, world!" in a buffer with a capacity of 20 bytes and placing the cursor just before the "w" of "world". What the gap buffer would look like is this:

```
Hello, |world!
+---+---+---+---+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---+
| H | e | l | l | o | , | _ |   |   |   |   |   |   |   | w | o | r | l | d | ! |
+---+---+---+---+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---+
                            ^ cursor

(the "_" represents a space)
```
In the diagram the cursor position was emphasized, but it's just a way to refer to the first unused byte, so it's actually redundant to specify it. 

When text is inserted in the buffer, it's placed before the cursor and the free space is decreased. Imagine inserting the string "my " in the previous example:

```
Hello, |world!
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
| H | e | l | l | o | , | _ |   |   |   |   |   |   |   | w | o | r | l | d | ! |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

Hello, my |world!
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
| H | e | l | l | o | , | _ | m | y | _ |   |   |   |   | w | o | r | l | d | ! |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
```

this operation is very fast because it does not depend on how much text is stored in the buffer. If we were to store the text in a basic string, we would have needed to move all of the text after the cursor before being able to insert the new text.

### Motion
To insert text in positions different than the cursor, you can change the cursor. This is done by moving text from the tail of the first region to the head of the second one, or viceversa. Let's move the cursor at the start of the string and then just before the "r" of "world"

```
Hello, my |world!
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
| H | e | l | l | o | , | _ | m | y | _ |   |   |   |   | w | o | r | l | d | ! |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

|Hello, my world!
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
|   |   |   |   | H | e | l | l | o | , | _ | m | y | _ | w | o | r | l | d | ! |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

Hello, my wo|rld!
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
| H | e | l | l | o | , | _ | m | y | _ | w | o |   |   |   |   | r | l | d | ! |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

(now we put it back)

Hello, my |world!
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
| H | e | l | l | o | , | _ | m | y | _ |   |   |   |   | w | o | r | l | d | ! |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
```

### Deletion
To delete text, you need to make sure that the text comes just after the cursor, then increase the gap size. Lets say we want to delete the " my " text in the previous example:
```
Hello, my |world!
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
| H | e | l | l | o | , | _ | m | y | _ |   |   |   |   | w | o | r | l | d | ! |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

Hello,| my world!
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
| H | e | l | l | o | , |   |   |   |   | _ | m | y | _ | w | o | r | l | d | ! |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

Hello,|world!
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
| H | e | l | l | o | , |   |   |   |   |   |   |   |   | w | o | r | l | d | ! |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
```

### Considerations
Gap buffers are great because of their simplicity and speed for basic operations, but don't scale very well and more sophisticated operations are relatively slow (like moving the cursor to a line given its number). Alternative solutions are piece tables and ropes.

## License
This code is MIT licensed

> Copyright 2023 Francesco Cozzuto
> 
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
> 
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Usage
An overview of how to use this code follows, though you can find more information in the documentation that comes with the code.

### Install
To use it, first you need to drop `gap_buffer.c` and `gap_buffer.h` in your project directory and link them during compilation
like they were your own files.

### Instanciate
You can instanciate a gap buffer in one of two ways:

```c
GapBuffer *GapBuffer_create(size_t capacity);
GapBuffer *GapBuffer_createUsingMemory(void *mem, size_t len);
```

The basic option is using `GapBuffer_create`, which instanciates a buffer with a given initial capacity allocating space through the libc allocator. If you fill the buffer up and insert more text, the buffer will grow by moving to a bigger memory region (also allocated through `malloc`). Conversely, `GapBuffer_createUsingMemory` doesn't use dynamic memory and does its job using only memory provided by the caller. Either way, if it wasn't possible to instanciate the gap buffer (either because the dynamic allocation failed or the provided memory isn't big enough), NULL is returned.
Once you're done with the buffer, you'll need to deallocate it using

```c
void GapBuffer_destroy(GapBuffer *buff);
```

### Text insertion
To insert text, you must use the function
```c
bool GapBuffer_insertString(GapBuffer **buff, const char *str, size_t len);
```
which expects a UTF-8 string `str` as input and inserts it into the buffer at the cursor's position (the `len` argument refers to the number of bytes of the string, not the number of characters). After insertion, the cursor is moved after the inserted text, just like a cursor of a text editor! 

The validity of the string is checked before insertion to make sure the buffer only contains valid UTF-8. If the string is inserted then `true` is returned, else if the string is invalid UTF-8 or relocation fails, false is returned.

An important thing to note is that the caller's handle of the buffer is passed by reference because the function may need to change the pointer if relocation occurres. 

### Cursor position
To move the cursor position you can use the functions
```c
void GapBuffer_moveRelative(GapBuffer *buff, int off);
void GapBuffer_moveAbsolute(GapBuffer *buff, size_t num);
```
which move the cursor position relative to the start of the buffer or the current position of the cursor. Both the `off` and `num` quantities refer tu number of unicode characters, not raw bytes.

### Text deletion
To delete text, you need to do so relative to the cursor's position. You can either remove text before or after the cursor using these functions
```c
void GapBuffer_removeForwards(GapBuffer *buff, size_t num);
void GapBuffer_removeBackwards(GapBuffer *buff, size_t num);
```
Where `num` is the number of unicode characters to be removed. If less than `num` characters are available, then they are all removed.

## Testing