#include "../App/guest_exec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define I(op,rs,rt,imm) (((uint32_t)(op)<<26)|((rs)<<21)|((rt)<<16)|((imm)&0xffffu))
#define R(rs,rt,rd,fn) (((rs)<<21)|((rt)<<16)|((rd)<<11)|(fn))
#define RET R(31,0,0,8)
static void program(MemoriesMemory *m,const uint32_t *words,size_t count){memset(m,0,sizeof(*m));for(size_t i=0;i<count;i++)Memories_WriteLE32(m->ram+0x1000+i*4,words[i]);}
int main(void){
 MemoriesMemory *m=malloc(sizeof(*m));assert(m);LekakExecResult r;
 const uint32_t basic[]={I(15,0,8,0x8000),I(13,8,8,0x200),I(9,0,9,1234),I(0x2b,8,9,0),I(0x23,8,10,0),I(9,10,11,1),I(9,10,12,1),RET,0};
 program(m,basic,sizeof(basic)/4);assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r));
 assert(r.registers[10]==1234&&r.registers[11]==1&&r.registers[12]==1235&&r.steps==9);assert(Memories_ReadLE32(m->ram+0x200)==1234);
 const uint32_t branch[]={I(9,0,8,1),I(4,8,8,2),I(9,0,9,5),I(9,0,9,99),RET,0};
 program(m,branch,sizeof(branch)/4);assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.registers[9]==5);
 const uint32_t merge[]={I(15,0,8,0x8000),I(13,8,8,0x201),I(0x22,8,9,3),I(0x26,8,9,0),0,RET,0};
 program(m,merge,sizeof(merge)/4);for(unsigned i=0;i<8;i++)m->ram[0x200+i]=(uint8_t)(0x11*(i+1));
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.registers[9]==0x55443322);
 const uint32_t divide[]={I(15,0,8,0x8000),I(9,0,9,0xffff),R(8,9,0,0x1a),R(0,0,10,0x12),RET,0};
 program(m,divide,sizeof(divide)/4);assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.registers[10]==0x80000000);
 const uint32_t overflow[]={I(15,0,8,0x7fff),I(13,8,8,0xffff),I(8,8,9,1)};
 program(m,overflow,sizeof(overflow)/4);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"overflow"));
 const uint32_t mmio[]={I(15,0,8,0x1f80),I(0x23,8,9,0x1814)};
 program(m,mmio,sizeof(mmio)/4);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.detail==0x1f801814&&r.pc==0x80001004);
 const uint32_t bios[]={I(9,0,9,0x44),I(9,0,8,0xa0),R(8,0,0,8),0};
 program(m,bios,sizeof(bios)/4);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"BIOS")&&r.detail==0x44);
 const uint32_t loop[]={I(4,0,0,0xffff),0};program(m,loop,2);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,7,&r)&&r.steps==7&&strstr(r.reason,"budget"));
 const uint32_t bad_delay[]={I(4,0,0,1),RET};program(m,bad_delay,2);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"delay slot"));
 const uint32_t cop0[]={0x40086000};program(m,cop0,1);assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"unsupported"));
 free(m);puts("Bounded guest execution: load/branch delays, merge pair, signed division, overflow, MMIO/BIOS rejection, budget and delay-slot rejection passed");return 0;
}
