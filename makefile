all: test

test: test.c gap_buffer.c
	gcc $^ -o $@ -Wall -Wextra -DGAPBUFFER_DEBUG

clean:
	rm test