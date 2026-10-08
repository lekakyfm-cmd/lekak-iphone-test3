/* Adapted from the upstream MIT-licensed PC overlay interpreter, revision
 * 0d78202729967a5bbdbc886aa128e48e82bef7ac. See Engine/LICENSE and exec-provenance.json.
 * Fixed host dereferences and native function casts are removed. This is a
 * bounded execution/debugging aid, not a full PS1 emulator or playable game. */
#include "guest_exec.h"
#include "pc/compat/gte.h"
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
typedef struct State {
 uint32_t r[32],hi,lo,pc,steps,budget,pending_reg,pending_value;
 int pending; MemoriesMemory *memory; LekakExecResult *out; jmp_buf escape;
} State;
static void fail(State *s,uint32_t pc,const char *reason,uint32_t detail) {
 s->out->pc=pc;s->out->detail=detail;
 snprintf(s->out->reason,sizeof(s->out->reason),"%s",reason);longjmp(s->escape,1);
}
static void *span(State *s,uint32_t a,size_t n,size_t align) {
 void *p=Memories_Resolve(s->memory,a,n,align);
 if(!p)fail(s,s->pc,"Unimplemented memory or invalid alignment",a);
 return p;
}
static uint32_t l32(State *s,uint32_t a){return Memories_ReadLE32(span(s,a,4,4));}
static uint16_t l16(State *s,uint32_t a){uint8_t *p=span(s,a,2,2);return p[0]|((uint16_t)p[1]<<8);}
static uint8_t l8(State *s,uint32_t a){return *(uint8_t*)span(s,a,1,1);}
static void s32(State *s,uint32_t a,uint32_t v){Memories_WriteLE32(span(s,a,4,4),v);}
static void s16(State *s,uint32_t a,uint16_t v){uint8_t *p=span(s,a,2,2);p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void s8(State *s,uint32_t a,uint8_t v){*(uint8_t*)span(s,a,1,1)=v;}
static uint32_t checked_add(State *s,uint32_t pc,uint32_t a,uint32_t b){
 int64_t n=(int64_t)(int32_t)a+(int32_t)b;if(n<INT32_MIN||n>INT32_MAX)fail(s,pc,"Signed arithmetic overflow",0);return (uint32_t)n;
}
static uint32_t checked_sub(State *s,uint32_t pc,uint32_t a,uint32_t b){
 int64_t n=(int64_t)(int32_t)a-(int32_t)b;if(n<INT32_MIN||n>INT32_MAX)fail(s,pc,"Signed arithmetic overflow",0);return (uint32_t)n;
}
static int control(uint32_t ins){unsigned op=ins>>26,fn=ins&63;return (op==0&&(fn==8||fn==9))||(op>=1&&op<=7);}
static void trace(State *s,uint32_t pc,uint32_t ins){
 if(s->steps>=s->budget)fail(s,pc,"Instruction budget reached",s->steps);
 s->pc=pc;s->steps++;s->out->instruction=ins;
 unsigned index=s->out->trace_count++%LEKAK_EXEC_TRACE;
 s->out->trace_pc[index]=pc;s->out->trace_ins[index]=ins;
}
static void plain(State *s, uint32_t pc)
{
    uint32_t ins = l32(s, pc), a, word;
    unsigned op = ins >> 26, rs = ins >> 21 & 31, rt = ins >> 16 & 31, rd = ins >> 11 & 31, sa = ins >> 6 & 31,
             fn = ins & 63;
    int32_t im = (int16_t)ins;
    uint64_t up;
    int64_t sp;
    if (!ins) return;
    switch (op) {
    case 0:
        switch (fn) {
        case 0: s->r[rd] = s->r[rt] << sa; break;
        case 2: s->r[rd] = s->r[rt] >> sa; break;
        case 3: s->r[rd] = (uint32_t)((int32_t)s->r[rt] >> sa); break;
        case 4: s->r[rd] = s->r[rt] << (s->r[rs] & 31); break;
        case 6: s->r[rd] = s->r[rt] >> (s->r[rs] & 31); break;
        case 7: s->r[rd] = (uint32_t)((int32_t)s->r[rt] >> (s->r[rs] & 31)); break;
        case 0x0c: fail(s, pc, "syscall", ins); break;
        case 0x0d: fail(s, pc, "break instruction", ins); break;
        case 0x10: s->r[rd] = s->hi; break;
        case 0x11: s->hi = s->r[rs]; break;
        case 0x12: s->r[rd] = s->lo; break;
        case 0x13: s->lo = s->r[rs]; break;
        case 0x18: sp = (int64_t)(int32_t)s->r[rs] * (int32_t)s->r[rt]; s->lo = (uint32_t)sp; s->hi = (uint32_t)(sp >> 32); break;
        case 0x19: up = (uint64_t)s->r[rs] * s->r[rt]; s->lo = (uint32_t)up; s->hi = (uint32_t)(up >> 32); break;
        case 0x1a:
            if (s->r[rt]) {
                if (s->r[rt] == 0xffffffffu) { s->lo = 0u - s->r[rs]; s->hi = 0; }
                else { s->lo = (uint32_t)((int32_t)s->r[rs] / (int32_t)s->r[rt]); s->hi = (uint32_t)((int32_t)s->r[rs] % (int32_t)s->r[rt]); }
            } else { s->lo = (int32_t)s->r[rs] < 0 ? 1u : 0xffffffffu; s->hi = s->r[rs]; }
            break;
        case 0x1b:
            if (s->r[rt]) { s->lo = s->r[rs] / s->r[rt]; s->hi = s->r[rs] % s->r[rt]; }
            else { s->lo = 0xffffffffu; s->hi = s->r[rs]; }
            break;
        case 0x20: s->r[rd] = checked_add(s, pc, s->r[rs], s->r[rt]); break;
        case 0x21: s->r[rd] = s->r[rs] + s->r[rt]; break;
        case 0x22: s->r[rd] = checked_sub(s, pc, s->r[rs], s->r[rt]); break;
        case 0x23: s->r[rd] = s->r[rs] - s->r[rt]; break;
        case 0x24: s->r[rd] = s->r[rs] & s->r[rt]; break;
        case 0x25: s->r[rd] = s->r[rs] | s->r[rt]; break;
        case 0x26: s->r[rd] = s->r[rs] ^ s->r[rt]; break;
        case 0x27: s->r[rd] = ~(s->r[rs] | s->r[rt]); break;
        case 0x2a: s->r[rd] = (int32_t)s->r[rs] < (int32_t)s->r[rt]; break;
        case 0x2b: s->r[rd] = s->r[rs] < s->r[rt]; break;
        default: fail(s, pc, "unsupported instruction", ins);
        }
        break;
    case 8: s->r[rt] = checked_add(s, pc, s->r[rs], (uint32_t)im); break;
    case 9: s->r[rt] = s->r[rs] + (uint32_t)im; break;
    case 0xa: s->r[rt] = (int32_t)s->r[rs] < im; break;
    case 0xb: s->r[rt] = s->r[rs] < (uint32_t)im; break;
    case 0xc: s->r[rt] = s->r[rs] & (ins & 0xffff); break;
    case 0xd: s->r[rt] = s->r[rs] | (ins & 0xffff); break;
    case 0xe: s->r[rt] = s->r[rs] ^ (ins & 0xffff); break;
    case 0xf: s->r[rt] = ins << 16; break;
    case 0x12: /* COP2 */
        if (ins & (1u << 25)) { if(!Memories_GteCommand(ins))fail(s,pc,"Unsupported GTE command",ins); break; }
        switch (rs) {
        case 0: s->r[rt] = Memories_GteReadData(rd); break;
        case 2: s->r[rt] = Memories_GteReadControl(rd); break;
        case 4: Memories_GteWriteData(rd, s->r[rt]); break;
        case 6: Memories_GteWriteControl(rd, s->r[rt]); break;
        default: fail(s, pc, "unsupported COP2 form", ins);
        }
        break;
    case 0x20: s->r[rt] = (uint32_t)(int32_t)(int8_t)l8(s, s->r[rs] + (uint32_t)im); break;
    case 0x21: s->r[rt] = (uint32_t)(int32_t)(int16_t)l16(s, s->r[rs] + (uint32_t)im); break;
    case 0x22: a = s->r[rs] + (uint32_t)im; s->r[rt] = (s->r[rt] & (0x00ffffffu >> ((a & 3) * 8))) | (l32(s, a & ~3u) << ((3 - (a & 3)) * 8)); break;
    case 0x23: s->r[rt] = l32(s, s->r[rs] + (uint32_t)im); break;
    case 0x24: s->r[rt] = l8(s, s->r[rs] + (uint32_t)im); break;
    case 0x25: s->r[rt] = l16(s, s->r[rs] + (uint32_t)im); break;
    case 0x26: a = s->r[rs] + (uint32_t)im; s->r[rt] = (s->r[rt] & (0xffffff00u << ((3 - (a & 3)) * 8))) | (l32(s, a & ~3u) >> ((a & 3) * 8)); break;
    case 0x28: s8(s, s->r[rs] + (uint32_t)im, (uint8_t)s->r[rt]); break;
    case 0x29: s16(s, s->r[rs] + (uint32_t)im, (uint16_t)s->r[rt]); break;
    case 0x2a: a = s->r[rs] + (uint32_t)im; word = l32(s, a & ~3u); s32(s, a & ~3u, (word & (0xffffff00u << ((a & 3) * 8))) | (s->r[rt] >> ((3 - (a & 3)) * 8))); break;
    case 0x2b: s32(s, s->r[rs] + (uint32_t)im, s->r[rt]); break;
    case 0x2e: a = s->r[rs] + (uint32_t)im; word = l32(s, a & ~3u); s32(s, a & ~3u, (word & (0x00ffffffu >> ((3 - (a & 3)) * 8))) | (s->r[rt] << ((a & 3) * 8))); break;
    case 0x32: Memories_GteLoad(rt, span(s, s->r[rs] + (uint32_t)im, 4, 4)); break;
    case 0x3a: Memories_GteStore(rt, span(s, s->r[rs] + (uint32_t)im, 4, 4)); break;
    default: fail(s, pc, "unsupported instruction", ins);
    }
    s->r[0] = 0;
}


/* MIPS-I load delay, including the LWL/LWR merge forwarding pair. No branch-likely:
 * those instructions belong to later ISAs and are rejected for this PS1 harness. */
static int loaded_register(uint32_t i){
 unsigned op=i>>26,rs=(i>>21)&31;
 if((op>=0x20&&op<=0x26)||(op==0x12&&!(i&(1u<<25))&&(rs==0||rs==2)))return (i>>16)&31;
 return -1;
}
static int written_register(uint32_t i){
 unsigned op=i>>26,rs=(i>>21)&31,fn=i&63;
 if(op==0){if(fn<=7||fn==9||fn==0x10||fn==0x12||(fn>=0x20&&fn<=0x2b))return (i>>11)&31;return -1;}
 if(op>=8&&op<=15)return (i>>16)&31;
 if(op==0x12&&!(i&(1u<<25))&&(rs==0||rs==2))return (i>>16)&31;
 return -1;
}
static void finish_pending(State *s,int written,int new_reg,uint32_t value){
 if(s->pending&&s->pending_reg!=(uint32_t)written)s->r[s->pending_reg]=s->pending_value;
 s->pending=new_reg>0;s->pending_reg=(uint32_t)new_reg;s->pending_value=value;s->r[0]=0;
}
static void execute_plain(State *s,uint32_t pc,int delay_slot){
 s->pc=pc;
 uint32_t ins=l32(s,pc),op=ins>>26;trace(s,pc,ins);
 if(control(ins))fail(s,pc,delay_slot?"Branch in delay slot":"Unexpected branch",ins);
 int load=loaded_register(ins);uint32_t before=load>=0?s->r[load]:0;
 if((op==0x22||op==0x26)&&s->pending&&s->pending_reg==(uint32_t)load){
  if(((ins>>21)&31)==(unsigned)load)fail(s,pc,"Unaligned load base aliases merge register",ins);
  s->r[load]=s->pending_value;
 }
 plain(s,pc);
 uint32_t value=load>=0?s->r[load]:0;
 if(load>=0)s->r[load]=before;
 finish_pending(s,load>=0?-1:written_register(ins),load,value);
}
int LekakExec_Run(MemoriesMemory *memory,uint32_t entry,uint32_t gp,uint32_t sp,
                  uint32_t budget,LekakExecResult *out){
 if(!out)return 0;
 memset(out,0,sizeof(*out));
 if(!memory||!budget){snprintf(out->reason,sizeof(out->reason),"Missing RAM or instruction budget");return 0;}
 State *s=calloc(1,sizeof(*s));if(!s){snprintf(out->reason,sizeof(out->reason),"Execution state allocation failed");return 0;}
 s->memory=memory;s->out=out;s->pc=entry;s->budget=budget;
 s->r[28]=gp;s->r[29]=sp;s->r[31]=LEKAK_EXEC_RETURN;
 if(!setjmp(s->escape))while(s->pc!=LEKAK_EXEC_RETURN){
  uint32_t pc=s->pc,physical=pc&0x1fffffffu;
  if(physical==0xA0||physical==0xB0||physical==0xC0)fail(s,pc,"BIOS service not implemented",s->r[9]);
  uint32_t ins=l32(s,pc),op=ins>>26,rs=(ins>>21)&31,rt=(ins>>16)&31,fn=ins&63;
  uint32_t target=0,next=pc+8;int written=-1;
  if(!control(ins)){execute_plain(s,pc,0);s->pc=pc+4;continue;}
  trace(s,pc,ins);
  if(op==0){target=s->r[rs];if(fn==9){written=(ins>>11)&31;s->r[written]=next;}}
  else if(op==2||op==3){target=((pc+4)&0xF0000000u)|((ins&0x3FFFFFFu)<<2);if(op==3){written=31;s->r[31]=next;}}
  else{
   int take=0;int32_t im=(int16_t)ins;
   if(op==1){
    if(rt==0||rt==16)take=(int32_t)s->r[rs]<0;
    else if(rt==1||rt==17)take=(int32_t)s->r[rs]>=0;
    else fail(s,pc,"Unsupported REGIMM instruction",ins);
    if(rt==16||rt==17){written=31;s->r[31]=next;}
   }else if(op==4)take=s->r[rs]==s->r[rt];
   else if(op==5)take=s->r[rs]!=s->r[rt];
   else if(op==6){if(rt)fail(s,pc,"Invalid BLEZ encoding",ins);take=(int32_t)s->r[rs]<=0;}
   else if(op==7){if(rt)fail(s,pc,"Invalid BGTZ encoding",ins);take=(int32_t)s->r[rs]>0;}
   target=take?pc+4+((uint32_t)im<<2):next;
  }
  finish_pending(s,written,-1,0);execute_plain(s,pc+4,1);s->pc=target;
 }
 out->steps=s->steps;memcpy(out->registers,s->r,sizeof(s->r));
 if(s->pc==LEKAK_EXEC_RETURN){out->returned=1;out->pc=s->pc;snprintf(out->reason,sizeof(out->reason),"Routine returned");}
 free(s);return out->returned;
}
