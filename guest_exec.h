#ifndef LEKAK_GUEST_EXEC_H
#define LEKAK_GUEST_EXEC_H
#include "pc/memory.h"
#define LEKAK_EXEC_TRACE 24
#define LEKAK_EXEC_RETURN 0xFFFFFFFCu
typedef struct LekakExecResult {
    uint32_t steps, pc, instruction, detail, registers[32];
    uint32_t trace_pc[LEKAK_EXEC_TRACE], trace_ins[LEKAK_EXEC_TRACE], trace_count;
    int returned;
    char reason[128];
} LekakExecResult;
/* Bounded functional MIPS-I diagnostic. No BIOS, MMIO, CD, SPU, scheduler or
 * cycle-accurate PS1 emulation. Unsupported operations halt with a report.
 * RAM may change: run on a fresh private copy of the loaded executable. */
int LekakExec_Run(MemoriesMemory *,uint32_t entry,uint32_t gp,uint32_t sp,
                  uint32_t budget,LekakExecResult *);
#endif
