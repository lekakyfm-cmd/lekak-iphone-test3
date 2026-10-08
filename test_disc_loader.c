#include "../App/disc_loader.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static void rec(uint8_t *p,uint32_t lba,uint32_t size,const char *name,int dir) {
    size_t n=strlen(name);p[0]=(uint8_t)(33+n+(n%2==0));p[25]=dir?2:0;p[32]=(uint8_t)n;
    Memories_WriteLE32(p+2,lba);Memories_WriteLE32(p+10,size);memcpy(p+33,name,n);
}
static FILE *image(int raw,int bad) {
    FILE *f=tmpfile();assert(f);uint8_t data[2048],sector[2352];
    for(unsigned lba=0;lba<23;lba++) {
        memset(data,0,sizeof(data));
        if(lba==16){data[0]=1;memcpy(data+1,"CD001",5);data[6]=1;data[129]=8;rec(data+156,20,2048,"R",1);}
        if(lba==20){rec(data,21,4096,bad==1?"SLES_000.00;1":"SLUS_014.11;1",0);if(bad==2)data[0]=12;}
        if(lba==21){memcpy(data,bad==3?"NOT EXE!":"PS-X EXE",8);Memories_WriteLE32(data+0x10,0x80010000);
            Memories_WriteLE32(data+0x18,bad==4?0x801ffffc:0x80010000);Memories_WriteLE32(data+0x1c,16);}
        if(lba==22){for(int j=0;j<16;j++)data[j]=j+1;if(bad==5)break;}
        if(raw){memset(sector,0,sizeof(sector));sector[15]=2;if(bad==6 && lba==22)sector[18]=sector[22]=0x20;
            memcpy(sector+24,data,2048);assert(fwrite(sector,1,2352,f)==2352);}
        else assert(fwrite(data,1,2048,f)==2048);
    } rewind(f);return f;
}
int main(void) {
    MemoriesMemory *m=malloc(sizeof(*m));assert(m);LekakDiscResult r;
    for(int raw=0;raw<=1;raw++)for(int bad=0;bad<=6;bad++) {
        if(!raw && bad==6)continue;
        memset(m,0xA5,sizeof(*m));FILE *f=image(raw,bad);int ok=LekakDisc_Load(f,m,&r);fclose(f);
        if(!bad){assert(ok);assert(r.entry==0x80010000);assert(r.load_bytes==16);assert(r.sector_bytes==(raw?2352:2048));
            for(int j=0;j<16;j++)assert(m->ram[0x10000+j]==j+1);
            assert(m->ram[0xffff]==0xA5 && m->ram[0x10010]==0xA5);}
        else{assert(!ok && r.error[0]);for(size_t i=0;i<sizeof(*m);i++)assert(((uint8_t*)m)[i]==0xA5);}
    }
    free(m);puts("Disc loader: ISO and MODE2, bad directory, region, header, span, truncation and form rejection passed");return 0;
}
