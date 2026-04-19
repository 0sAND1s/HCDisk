#include "Compression.h"
extern "C" {
#include "zx0.h"
#include <stdlib.h>
}

#define MAX_OFFSET_ZX0    32640
#define MAX_OFFSET_ZX7     2176

Compression::Compression()
{

}

void reverse(unsigned char* first, unsigned char* last) {
    unsigned char c;

    while (first < last) {
        c = *first;
        *first++ = *last;
        *last-- = c;
    }
}

word Compression::Compress(byte* bufSrc, dword lenSrc, byte** bufDst, bool backwardsCompress, bool quickMode, int* delta)
{
	int outLen = 0;
    int deltaOut = 0;

    if (backwardsCompress)
	    reverse(bufSrc, bufSrc + lenSrc - 1);

    void* allocations[300] = { nullptr };    
    int allocationCount = 0;    

    init();
    byte* output_data = compress(optimize(bufSrc, lenSrc, 0, quickMode ? MAX_OFFSET_ZX7 : MAX_OFFSET_ZX0, allocations, &allocationCount),
        bufSrc, lenSrc, 0, backwardsCompress ? TRUE : FALSE, backwardsCompress ? FALSE : TRUE, &outLen, &deltaOut);

    if (backwardsCompress)
        reverse(output_data, output_data + outLen - 1);

    *bufDst = output_data;
    if (delta != nullptr)
        *delta = deltaOut;

    for (word allocIdx = 0; allocIdx < allocationCount; allocIdx++)
        free(allocations[allocIdx]);

	return outLen;
}


