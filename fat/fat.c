#include "fat.h"

static void fat_UCS2ToUTF8(uint8_t* filename, const fat_LongFileName* lfn)
{
	memcpy(filename, lfn->unicode1, 0x0A);
	memcpy(filename + 0x05, lfn->unicode2, 0x0C);
	memcpy(filename + 0x0B, lfn->unicode3, 0x04);
}
