#ifndef LEKAK_STARTUP_MEMORY_H
#define LEKAK_STARTUP_MEMORY_H
#include "pc/memory.h"
/* Native adaptations of three leaf startup routines. Guest layouts stay 32-bit. */
int LekakStartup_Apply(MemoriesMemory *, unsigned routine);
#endif
