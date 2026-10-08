/* Adapted from src/game/util_memory.c and util_compare_s16.c, revision
 * 0d78202729967a5bbdbc886aa128e48e82bef7ac. These game routines are outside
 * the upstream MIT license's PC-port scope. See Engine/game-provenance.json.
 * Preserve backwards word copying and the game's rounded-up tail writes.
 * Explicit guest addresses replace dereferences of fixed host addresses. */
#include "game_memory.h"
#include <string.h>
int Lekak_CopyWords(MemoriesMemory *m,uint32_t dst,uint32_t src,uint32_t n) {
    if (!n) return 1;
    size_t span=((uint64_t)n+3)&~UINT64_C(3);
    uint8_t *d=Memories_Resolve(m,dst,span,4), *s=Memories_Resolve(m,src,span,4);
    if (!d || !s) return 0;
    for (uint32_t i=n/4;i;i--) {uint8_t word[4];memcpy(word,s+4*(i-1),4);memcpy(d+4*(i-1),word,4);}
    if (n&3) {uint8_t word[4];memcpy(word,s+4*(n/4),4);memcpy(d+4*(n/4),word,4);}
    if ((n&3)==3) d[n-1]=s[n-1];
    return 1;
}
int Lekak_FillMemory(MemoriesMemory *m,uint32_t dst,int32_t value,uint32_t n) {
    if (!n) return 1;
    size_t span=((uint64_t)n+3)&~UINT64_C(3);
    uint8_t *d=Memories_Resolve(m,dst,span,4);
    if (!d) return 0;
    memset(d,(uint8_t)value,span);return 1;
}
int Lekak_CompareS16(MemoriesMemory *m,uint32_t left,uint32_t right,int *result) {
    const uint8_t *l=Memories_Resolve(m,left,2,2), *r=Memories_Resolve(m,right,2,2);
    if (!l || !r || !result) return 0;
    int32_t a=l[0]|((uint32_t)l[1]<<8), b=r[0]|((uint32_t)r[1]<<8);
    if(a&0x8000)a-=65536;
    if(b&0x8000)b-=65536;
    *result=(a>b)-(a<b);return 1;
}
