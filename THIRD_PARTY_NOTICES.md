# Third-party notices

Harry Potter Prisoner of Azkaban for Mac (GPL-3.0-or-later, `LICENSE`) is built on the work below. The installer ships each one's
license text in `Contents/Resources/kit/licenses/`.

| Project | What it gives | Where | License |
|---|---|---|---|
| [nfsu2-sw](https://github.com/antoxa2584x/nfsu2-sw) by antoxa2584x | the engine around xboxrecomp: lifter fixes, kernel and D3D glue, the host the game runs in | `engine/` (by way of nfsu2-ios, the NFS Underground 2 port and the Sorcerer's Stone port) | no license file upstream; used here under this project's GPL-3.0-or-later, with credit (see `NOTICE`) |
| [nfsu2-ios](https://github.com/myUdav4iik/nfsu2-ios) by myUdav4iik | the Apple host and the Metal renderer (`nv2a_metal`) | `engine/` (from `85d2cbf`), the kit's objects | no license file upstream; as above |
| [HP-POA-Xbox-Recomp](https://github.com/MiddleDucky/HP-POA-Xbox-Recomp) by MiddleDucky (commit `2c2fb7b`, 2026-11-18; copyright line sp00nz) | this game's facts (seeds, addresses, quirks); a reference, no code copied yet | `docs/upstream-patches/README.md`; code that follows it will be marked | MIT |
| [HP3-Recompiled](https://github.com/Barthes-Recompiled/HP3-Recompiled) by Barthes-Recompiled and MiddleDucky (commit `ecd59ad`, 2026-11-01) | this game's bring-up notes, region data, seeds, patch mechanism; a reference, no code copied yet | as above | MIT |
| [HP-COS-Xbox-Recomp](https://github.com/MiddleDucky/HP-COS-Xbox-Recomp) by MiddleDucky (commit `f45cb33`) | runtime code inherited from the Chamber of Secrets port that follows it (stick range compensation, movie layer design, texture seam fix, handle table); the patch mechanism | `engine/xboxrecomp/` (marked where it follows it), `engine/tools/patch_gen.py` | MIT |
| [HP2-Recompiled](https://github.com/Barthes-Recompiled/HP2-Recompiled) by Barthes-Recompiled (a fork of HP-COS) | the fast-forward design behind the runtime's turbo hooks (`xbox_vblank_turbo`, `nv2a_mtl_turbo`) | `engine/xboxrecomp/` | MIT |
| [HP-PS-Xbox-Recomp](https://github.com/MiddleDucky/HP-PS-Xbox-Recomp) by MiddleDucky | the controller's direct XInput design and code | `engine/xboxrecomp/`, `engine/src/recomp_manual.c` (marked where it follows it) | MIT |
| [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) by sp00nz | the Xbox lifter (Python) and runtime: kernel, D3D8, NV2A, audio, input | the kit's `lift/` and libraries | MIT |
| [xemu](https://github.com/xemu-project/xemu) | the APU (MCPX audio) and NV2A parts inside xboxrecomp | the kit's libraries (relinked on the player's Mac) | LGPL-2.1-or-later |
| [NFSU2Deadware](https://github.com/LeoDeadware/NFSU2Deadware) by LeoDeadware | the Mac engine patches (render scale, Metal effects, mip chains, pacing), the kit builder, disc import, icon | `engine/` (via the NFS Underground 2 port), `tools/kit.py`, `mac/` | MIT |
| [reUndr-ps5](https://github.com/bystritskiy/reUndr-ps5) by bystritskiy | the idea of the disc check by section digests | `mac/xbe_check.py` (written for these ports) | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) 1.92.9 by Omar Cornut | the settings menu's drawing, Metal backend | `engine/third_party/imgui`, the kit's `libimgui.a` | MIT |
| [Roboto](https://fonts.google.com/specimen/Roboto) Medium | the settings menu's font, compiled in | `engine/third_party/fonts` | Apache-2.0 |
| [SpeedBreaker](https://github.com/SpeedBreakerProject) by project(u) | the settings menu's design (pages, rows, controller first, Quit Game) | `engine/src/overlay` (written for the NFS Underground 2 port) | GPL-3.0-or-later |
| [Capstone](https://github.com/capstone-engine/capstone) | the x86 disassembler the lifter uses | the kit's `py/capstone` | BSD-3-Clause |
| [SDL3](https://github.com/libsdl-org/SDL) 3.4.16 | window, input, gamepads, sound | `libSDL3.dylib` | Zlib |
| [sdl2-compat](https://github.com/libsdl-org/sdl2-compat) 2.32.74 | SDL2's API on SDL3 (the runtime is written against SDL2) | `libSDL2-2.0.0.dylib` | Zlib |
