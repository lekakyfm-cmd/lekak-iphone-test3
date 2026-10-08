#include "../App/startup_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
 MemoriesMemory *m=malloc(sizeof(*m));assert(m);memset(m,0xA5,sizeof(*m));
 assert(LekakStartup_Apply(m,0));
 for(unsigned i=0;i<0x28;i++)assert(m->ram[0xE9EC8+i]==(i==7?8:((i==4||i==5||i==6)?0:0xA5)));
 assert(m->ram[0x9B145]==0&&m->ram[0x9B144]==0xA5&&m->ram[0x9B146]==0xA5);
 assert(LekakStartup_Apply(m,1));for(unsigned i=0;i<16;i++)assert(m->ram[0xE9DB0+i]==0);
 assert(m->ram[0xE9DAF]==0xA5&&m->ram[0xE9DC0]==0xA5);
 assert(Memories_ReadLE32(m->ram+0x9B0B8)==0&&m->ram[0x9B0B7]==0xA5&&m->ram[0x9B0BC]==0xA5);
 assert(LekakStartup_Apply(m,2)&&m->ram[0x9B318]==0&&m->ram[0x9B317]==0xA5&&m->ram[0x9B319]==0xA5);
 assert(!LekakStartup_Apply(m,3));assert(!LekakStartup_Apply(NULL,0));free(m);
 puts("Native startup memory: fade fields, four 32-bit callbacks, movie flag and unchanged neighbours passed");return 0;
}
