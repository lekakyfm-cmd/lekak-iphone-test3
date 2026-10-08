#ifndef LEKAK_GAME_MEMORY_H
#define LEKAK_GAME_MEMORY_H
#include "pc/memory.h"
int Lekak_CopyWords(MemoriesMemory *, uint32_t destination, uint32_t source, uint32_t length);
int Lekak_FillMemory(MemoriesMemory *, uint32_t destination, int32_t value, uint32_t length);
int Lekak_CompareS16(MemoriesMemory *, uint32_t left, uint32_t right, int *result);
#endif
