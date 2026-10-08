/* Adapted from fade_init.c, Main_ClearFrameServiceCallbacks in main_services.c
 * and Movie_ResetPlaybackState in movie_playback_control.c. Source provenance
 * and license scope: Engine/game-provenance.json. */
#include "startup_memory.h"
int LekakStartup_Apply(MemoriesMemory *memory,unsigned routine){
 if(routine==0){
  uint8_t *fade=Memories_Resolve(memory,0x800E9EC8u,0x28,1);
  uint8_t *white=Memories_Resolve(memory,0x8009B145u,1,1);
  if(!fade||!white)return 0;
  fade[6]=0;fade[4]=0;fade[5]=0;fade[7]=8;*white=0;return 1;
 }
 if(routine==1){
  uint8_t *slots=Memories_Resolve(memory,0x800E9DB0u,16,4);
  uint8_t *callback=Memories_Resolve(memory,0x8009B0B8u,4,4);
  if(!slots||!callback)return 0;
  for(int i=3;i>=0;i--)Memories_WriteLE32(slots+4*i,0);
  Memories_WriteLE32(callback,0);return 1;
 }
 if(routine==2){
  uint8_t *state=Memories_Resolve(memory,0x8009B318u,1,1);
  if(!state)return 0;
  *state=0;return 1;
 }
 return 0;
}
