#include "drivers/memory.h"

// Copies n bytes from src to dest. The regions must not overlap - overlapping
// regions produce undefined results because the byte-by-byte forward loop may
// read bytes that have already been overwritten.
void* memcpy(void* dest, const void* src, size_t n) {
	unsigned char* d = (unsigned char*)dest;
	const unsigned char* s = (const unsigned char*)src;

	for (size_t i = 0; i < n; i++) {
		d[i] = s[i];
	}

	return dest;
}

// Fills the first n bytes of dest with the low 8 bits of c.
// c is int (matching the C standard) but only the low byte is written,
// so values outside 0-255 are silently truncated.
void* memset(void* dest, int c, size_t n) {
	unsigned char* d = (unsigned char*)dest;
	unsigned char value = (unsigned char)c;

	for (size_t i = 0; i < n; i++) {
		d[i] = value;
	}

	return dest;
}

// Compares the first n bytes of s1 and s2 as unsigned chars.
// Returns negative, zero, or positive if s1 is less than, equal to,
// or greater than s2. Comparison stops at the first differing byte.
int memcmp(const void* s1, const void* s2, size_t n) {
	const unsigned char* a = (const unsigned char*)s1;
	const unsigned char* b = (const unsigned char*)s2;

	for (size_t i = 0; i < n; i++) {
		if (a[i] != b[i]) {
			return (int)a[i] - (int)b[i];
		}
	}

	return 0;
}
