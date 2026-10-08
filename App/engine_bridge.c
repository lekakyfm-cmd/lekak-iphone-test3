#include "engine_bridge.h"
#include "engine_callbacks.h"
#include "pc/memory.h"
#include "pc/compat/libgs_ot.h"
#include "pc/compat/gte.h"
#include "pc/rng.h"
#include "pc/render/soft_gpu.h"
#include <stdlib.h>
#include <string.h>
static int put(MemoriesMemory *m,uint32_t address,uint32_t value) {
    uint8_t *p=Memories_Resolve(m,address,4,4);
    if(!p)return 0;
    Memories_WriteLE32(p,value);return 1;
}
static int callback_one(uint32_t argument) { return (int)argument+1; }
static int callback_two(uint32_t argument) { return (int)argument*2; }
int LekakEngine_Run(LekakEngineResult *r) {
    if(!r)return 0;
    memset(r,0,sizeof(*r));
    MemoriesMemory *m=calloc(1,sizeof(*m));
    r->allocation=m!=NULL;if(!m)return 0;
    r->memory_aliases=Memories_Resolve(m,0x80001000u,4,4)==Memories_Resolve(m,0x1000u,4,4) &&
        Memories_Resolve(m,0xA0001000u,4,4)==Memories_Resolve(m,0x1000u,4,4);
    r->scratchpad_aliases=Memories_Resolve(m,0x1F800000u,4,4)==Memories_Resolve(m,0x9F800000u,4,4) &&
        Memories_Resolve(m,0x1F800000u,4,4)==m->scratchpad;
    r->rejected_spans=!Memories_Resolve(m,0x801FFFFCu,8,4) &&
        !Memories_Resolve(m,0x80001001u,4,4) && !Memories_Resolve(m,0x40001000u,4,4) &&
        !Memories_Resolve(m,0x80000000u,SIZE_MAX,4);
    /* A real Psy-Q ordering table, linked packet, GP0 stream and rasterizer. */
    const uint32_t descriptor=0x80100000u,table=0x80101000u,packet=0x80102000u;
    put(m,descriptor,2);put(m,descriptor+4,table);
    r->ordering_table=Memories_OtInstallTail(m)==MEMORIES_GS_OK &&
        Memories_GsClearOt(m,0,0,descriptor)==MEMORIES_GS_OK;
    const uint32_t drawing[]={
        0xE3000000u,0xE4000000u|319u|(239u<<10),0xE5000000u,
        0x02080808u,0,(240u<<16)|320u,
        0x600000F8u,(70u<<16)|25u,(100u<<16)|70u,
        0x60F80000u,(70u<<16)|125u,(100u<<16)|70u,
        0x60F800F8u,(70u<<16)|225u,(100u<<16)|70u};
    const size_t n=sizeof(drawing)/sizeof(*drawing);
    uint8_t *entry=Memories_Resolve(m,table+12,4,4);
    uint32_t old=Memories_ReadLE32(entry);
    put(m,packet,((uint32_t)n<<24)|(old&0xFFFFFFu));
    for(size_t i=0;i<n;i++)put(m,packet+4+(uint32_t)i*4,drawing[i]);
    put(m,table+12,packet&0xFFFFFFu);
    uint32_t words[64];size_t count=0;
    r->packet_collection=Memories_GsCollectOt(m,descriptor,words,64,64,&count)==MEMORIES_GPU_OK && count==n+8;
    r->snapshot_words=(unsigned)count;
    r->packet_validation=Memories_GpuValidate(words,count)==MEMORIES_GPU_OK;
    SoftGpu_Reset();
    size_t consumed=0;
    if(r->packet_collection && r->packet_validation)consumed=SoftGpu_Gp0(words,count);
    const uint16_t *vram=SoftGpu_Vram();
    r->red_pixel=vram[100*SOFT_GPU_WIDTH+50];
    r->blue_pixel=vram[100*SOFT_GPU_WIDTH+150];
    r->violet_pixel=vram[100*SOFT_GPU_WIDTH+250];
    r->rendered_pixels=consumed==count && r->red_pixel==0x001Fu && r->blue_pixel==0x7C00u && r->violet_pixel==0x7C1Fu;
    uint32_t hash=2166136261u;
    for(int y=0;y<240;y++)for(int x=0;x<320;x++) {
        uint16_t p=vram[y*SOFT_GPU_WIDTH+x];hash=(hash^(p&255u))*16777619u;hash=(hash^(p>>8))*16777619u;
    }
    r->framebuffer_hash=hash;
    const uint32_t bad[]={0xFFFFFFFFu},cut[]={0x280000F8u,0};
    r->gpu_invalid_rejection=Memories_GpuValidate(bad,1)==MEMORIES_GPU_UNSUPPORTED_COMMAND &&
        Memories_GpuValidate(cut,2)==MEMORIES_GPU_TRUNCATED_COMMAND;
    Memories_Srand(1);int a=Memories_Rand(),b=Memories_Rand(),c=Memories_Rand();
    r->rng=a==16838 && b==5758 && c==10113;
    Memories_GteReset();Memories_GteWriteData(0,0x00020001u);
    uint8_t stored[4]={0};Memories_GteStore(0,stored);
    r->gte=Memories_GteReadData(0)==0x00020001u && Memories_ReadLE32(stored)==0x00020001u;
    const IOSGuestFunction map[]={
        {0x80180000u,(IOSNativeFunction)callback_one,0x80170000u,1},
        {0x80180000u,(IOSNativeFunction)callback_two,0x80170000u,2}};
    uint32_t encoded=0;
    put(m,0x80170000u,1);
    IOSNativeFunction f=IOSGuest_FindFunction(m,map,2,0x80180000u);
    int firstOK=f && ((int(*)(uint32_t))f)(41)==42;
    int encodeOK=IOSGuest_EncodeFunction(m,map,2,(IOSNativeFunction)callback_one,&encoded) && encoded==0x80180000u;
    put(m,0x80170000u,2);
    f=IOSGuest_FindFunction(m,map,2,encoded);
    int secondOK=f && ((int(*)(uint32_t))f)(41)==82;
    put(m,0x80170000u,3);
    r->callback_bank_dispatch=firstOK&&encodeOK&&secondOK&&
        !IOSGuest_FindFunction(m,map,2,encoded)&&!IOSGuest_FindFunction(m,map,2,0x80180004u);
    free(m);
    return r->allocation&&r->memory_aliases&&r->scratchpad_aliases&&r->rejected_spans&&r->ordering_table&&
        r->packet_collection&&r->packet_validation&&r->rendered_pixels&&r->gpu_invalid_rejection&&r->rng&&r->gte&&r->callback_bank_dispatch;
}
void LekakEngine_CopyRGBA(uint8_t *destination) {
    if(!destination)return;
    const uint16_t *v=SoftGpu_Vram();
    for(int y=0;y<240;y++)for(int x=0;x<320;x++) {
        uint16_t p=v[y*SOFT_GPU_WIDTH+x];uint8_t *d=destination+4*(y*320+x);
        d[0]=(uint8_t)(((p&31u)*255u)/31u);d[1]=(uint8_t)((((p>>5)&31u)*255u)/31u);
        d[2]=(uint8_t)((((p>>10)&31u)*255u)/31u);d[3]=255;
    }
}
