#include "../App/game_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);
 for(uint32_t n=0;n<32;n++){
  memset(m->ram,0xCC,256);for(unsigned i=0;i<64;i++)m->ram[64+i]=i;
  assert(Lekak_CopyWords(m,0x80000000,0x80000040,n));
  unsigned span=(n+3)&~3u;for(unsigned i=0;i<span;i++)assert(m->ram[i]==i);
  assert(m->ram[span]==0xCC);
  assert(Lekak_FillMemory(m,0x80000000,0x182,n));for(unsigned i=0;i<span;i++)assert(m->ram[i]==0x82);
  assert(m->ram[span]==0xCC);
 }
 for(unsigned i=0;i<32;i++)m->ram[i]=i;
 assert(Lekak_CopyWords(m,0x80000004,0x80000000,16));for(unsigned i=0;i<16;i++)assert(m->ram[4+i]==i);
 assert(!Lekak_CopyWords(m,0x801ffffc,0x80000000,5));assert(!Lekak_FillMemory(m,0x80000001,1,4));
 assert(!Lekak_FillMemory(m,0x80000000,1,UINT32_MAX));
 int values[]={-32768,-1,0,1,32767};int got;
 for(unsigned i=0;i<5;i++)for(unsigned j=0;j<5;j++){
  m->ram[0]=values[i]&255;m->ram[1]=(uint16_t)values[i]>>8;
  m->ram[2]=values[j]&255;m->ram[3]=(uint16_t)values[j]>>8;
  assert(Lekak_CompareS16(m,0x80000000,0x80000002,&got));assert(got==((values[i]>values[j])-(values[i]<values[j])));
 }
 assert(!Lekak_CompareS16(m,0x801fffff,0x80000000,&got));free(m);
 puts("Game memory routines: rounded tails, backwards overlap, boundaries and signed comparisons passed");return 0;
}
