#include "../App/address_adapter.h"
#include <assert.h>
#include <string.h>
static int increment(uint32_t value) { return (int)value+1; }
int main(void) {
    unsigned char ram[32]={0}, scratch[16]={0};
    GuestRegion regions[]={{0x80000000u,sizeof ram,ram},{0x9F800000u,sizeof scratch,scratch}};
    CallbackEntry entries[]={{0xC0000001u,increment}};
    GuestRecord record={0x80000004u,0xC0000001u};
    uint32_t value=41, got=0; int result=0;
    memcpy(guest_resolve(regions,2,record.data,4),&value,4);
    memcpy(&got,guest_resolve(regions,2,record.data,4),4);
    assert(got==41 && guest_dispatch(entries,1,record.callback,got,&result) && result==42);
    assert(guest_resolve(regions,2,0x8000001Fu,1)==ram+31);
    assert(!guest_resolve(regions,2,0x8000001Fu,2));
    assert(!guest_resolve(regions,2,0x80000020u,1));
    assert(!guest_resolve(regions,2,0x7FFFFFFFu,1));
    assert(!guest_resolve(regions,2,0xFFFFFFFFu,2));
    assert(!guest_resolve(regions,2,0x80000000u,SIZE_MAX));
    assert(!guest_resolve(regions,2,0x80000000u,0));
    assert(guest_resolve(regions,2,0x9F80000Fu,1)==scratch+15);
    assert(!guest_dispatch(entries,1,0xC0000002u,41,&result));
    assert(!guest_dispatch(entries,1,0xC0000001u,41,NULL));
    return 0;
}
