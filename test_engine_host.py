#!/usr/bin/env python3
"""Verify the actual portable engine sources and the iOS bridge on a host."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
SOURCES=['App/guest_exec.c','App/startup_memory.c','App/disc_loader.c','App/game_memory.c','App/engine_callbacks.c','App/optional_textures.c','Engine/src/pc/memory.c',
 'Engine/src/pc/rng.c','Engine/src/pc/compat/libgs_ot.c','Engine/src/pc/compat/gte.c',
 'Engine/src/pc/compat/pgxp.c','Engine/src/pc/render/packets.c','Engine/src/pc/render/soft_gpu.c']
with tempfile.TemporaryDirectory(prefix='lekak-engine-') as tmp:
 for name,unit,extra in [
  ('bridge','tools/test_engine_bridge.c',['App/engine_bridge.c']),
  ('startup-memory','tools/test_startup_memory.c',[]),
  ('guest-execution','tools/test_guest_exec.c',[]),
  ('disc-loader','tools/test_disc_loader.c',[]),
  ('game-memory','tools/test_game_memory.c',[]),
  ('ordering-table','Engine/tests/libgs_ot_test.c',[]),
  ('packets','Engine/tests/packet_test.c',[]),
  ('software-gpu','Engine/tests/soft_gpu_test.c',[])]:
  binary=Path(tmp)/name
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O2','-Wall','-Wextra',
   '-I'+str(ROOT/'Engine/src'),str(ROOT/unit),*[str(ROOT/p) for p in extra+SOURCES],
   '-lm','-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
  print(name+': passed',flush=True)
