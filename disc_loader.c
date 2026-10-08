#include "disc_loader.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
typedef struct Disc { FILE *file; unsigned stride, offset; } Disc;
static int fail(LekakDiscResult *r, const char *reason) {
    snprintf(r->error, sizeof(r->error), "%s", reason); return 0;
}
static int sector(Disc *d, uint32_t lba, uint8_t out[2048]) {
    uint64_t position = (uint64_t)lba * d->stride;
    uint8_t raw[2352];
    if (position > LONG_MAX || fseek(d->file, (long)position, SEEK_SET) ||
        fread(raw, 1, d->stride, d->file) != d->stride) return 0;
    if (d->stride == 2352 && (raw[15] != 2 || (raw[18] & 0x20) ||
        memcmp(raw + 16, raw + 20, 4))) return 0;
    memcpy(out, raw + d->offset, 2048); return 1;
}
static int record(const uint8_t *p, size_t available) {
    return available >= 34 && p[0] >= 34 && p[0] <= available &&
        p[32] && 33u + p[32] <= p[0] && !(p[25] & 0x80) &&
        p[1] == 0 && p[26] == 0 && p[27] == 0;
}
static int executable(Disc *d, uint32_t root, uint32_t size,
                      uint32_t *lba, uint32_t *bytes) {
    uint8_t buffer[2048];
    if (!size || size > 64u * 2048u) return 0;
    for (uint32_t offset = 0; offset < size; offset += 2048) {
        if ((uint64_t)root + offset / 2048 > UINT32_MAX ||
            !sector(d, root + offset / 2048, buffer)) return 0;
        unsigned limit = size-offset < 2048 ? size-offset : 2048;
        for (unsigned at = 0; at < limit && buffer[at]; at += buffer[at]) {
            const uint8_t *p = buffer + at;
            if (!record(p, limit-at)) return 0;
            if (!(p[25] & 2) && p[32] == 13 &&
                !memcmp(p+33, "SLUS_014.11;1", 13)) {
                *lba = Memories_ReadLE32(p+2);
                *bytes = Memories_ReadLE32(p+10); return 1;
            }
        }
    }
    return 0;
}
int LekakDisc_Load(FILE *file, MemoriesMemory *memory, LekakDiscResult *r) {
    if (!r) return 0;
    memset(r, 0, sizeof(*r));
    if (!file || !memory) return fail(r,"Missing file or guest RAM");
    Disc d = {file,2352,24}; uint8_t pvd[2048];
    if (!sector(&d,16,pvd) || pvd[0]!=1 || memcmp(pvd+1,"CD001",5)) {
        d.stride=2048; d.offset=0;
        if (!sector(&d,16,pvd) || pvd[0]!=1 || memcmp(pvd+1,"CD001",5))
            return fail(r,"Expected a MODE2/2352 BIN or ISO/2048 image");
    }
    r->sector_bytes=d.stride;
    if (pvd[6]!=1 || pvd[128]!=0 || pvd[129]!=8 ||
        !record(pvd+156,2048-156) || !(pvd[181]&2))
        return fail(r,"Invalid ISO9660 volume descriptor");
    uint32_t lba=0, bytes=0;
    if (!executable(&d, Memories_ReadLE32(pvd+158),
                    Memories_ReadLE32(pvd+166), &lba, &bytes))
        return fail(r,"USA executable SLUS_014.11;1 not found in root directory");
    if (bytes < 2048 || bytes > 4u*1024u*1024u)
        return fail(r,"Invalid executable size");
    uint8_t *exe=malloc(bytes);
    if (!exe) return fail(r,"Executable allocation failed");
    int ok=0;
    for (uint32_t offset=0; offset<bytes; offset+=2048) {
        uint8_t page[2048]; unsigned count=bytes-offset<2048?bytes-offset:2048;
        if ((uint64_t)lba+offset/2048>UINT32_MAX || !sector(&d,lba+offset/2048,page)) {
            fail(r,"Truncated or unsupported executable sector"); goto done;
        }
        memcpy(exe+offset,page,count);
    }
    if (memcmp(exe,"PS-X EXE",8)) {fail(r,"Missing PS-X EXE header"); goto done;}
    r->executable_bytes=bytes;
    r->entry=Memories_ReadLE32(exe+0x10); r->gp=Memories_ReadLE32(exe+0x14);
    r->load_address=Memories_ReadLE32(exe+0x18); r->load_bytes=Memories_ReadLE32(exe+0x1c);
    r->stack_base=Memories_ReadLE32(exe+0x30); r->stack_bytes=Memories_ReadLE32(exe+0x34);
    uint64_t end=(uint64_t)r->load_address+r->load_bytes;
    void *destination=Memories_Resolve(memory,r->load_address,r->load_bytes,4);
    if (!r->load_bytes || r->load_bytes>bytes-2048 || r->load_address<0x80010000u ||
        end>0x80200000u || !destination || (r->entry&3) ||
        r->entry<r->load_address || r->entry>=end ||
        (r->stack_base && ((r->stack_base&3) || r->stack_base<0x80000000u ||
            (uint64_t)r->stack_base+r->stack_bytes>0x80200000u))) {
        fail(r,"Executable guest memory range or entry point rejected"); goto done;
    }
    r->payload_hash=2166136261u;
    for(uint32_t i=0;i<r->load_bytes;i++) r->payload_hash=(r->payload_hash^exe[2048+i])*16777619u;
    memcpy(destination,exe+2048,r->load_bytes); ok=1;
done:
    free(exe); return ok;
}
