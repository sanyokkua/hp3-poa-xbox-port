# How engine/ was made, and what this port takes from the HP3 references

## The engine

`engine/` is the `engine/` of the [Harry Potter and the Chamber of Secrets port](https://github.com/sanyokkua/hpcos-xbox-port) at
`8342d21` (v0.1.0 plus later commits), imported so that this port builds from this repository alone. That tree is the `engine/` of
the [Harry Potter and the Sorcerer's Stone port](https://github.com/sanyokkua/hp1-xbox-port) at v0.1.0 plus CoS's runtime and
renderer fixes (marked `hpcos-xbox-port` in the code), which is itself the `engine/` of the
[NFS Underground 2 port](https://github.com/sanyokkua/nfsu2-xbox-port) with NFSU2's own parts removed (the Switch and iOS hosts, the
VP6 movie player, the HUD mover, the network card, the text patch, the texture and prompt packs) and HP1's work on top.
What this game adds on top is in this repository's git history. The vendored engine stays from our tree: the references below are
read, never merged.

## References for this game (gitignored `upstream/`, shallow clones)

| Folder | Repository | Commit | Date |
|---|---|---|---|
| `upstream/hp-poa` | [MiddleDucky/HP-POA-Xbox-Recomp](https://github.com/MiddleDucky/HP-POA-Xbox-Recomp) (MIT, `LICENSES/MIT-HP-POA-Xbox-Recomp.txt`) | `2c2fb7b514e13d30cf265001ee8b8c4310bd260d` | 2026-11-18 |
| `upstream/hp3-recompiled` | [Barthes-Recompiled/HP3-Recompiled](https://github.com/Barthes-Recompiled/HP3-Recompiled) (MIT, `LICENSES/MIT-HP3-Recompiled.txt`) | `ecd59ade77c33b865de577346e8715db10857cd7` | 2026-11-01 |

Both are Windows recompiles of this same disc (the PC runtime is xboxrecomp's, with a Direct3D 12 renderer; HP3-Recompiled is a
fork of the Sorcerer's Stone one with SCDA-XBERecomp's D3D8 layer). Their code is Windows-only and none of it is copied; what is taken is
**facts**, each to be confirmed on this disc before it is used:

| Fact | From | Where it lives there |
|---|---|---|
| Hidden-function seeds (indirect-call targets the static pass misses): `0x000777F0`, `0x000CBEE0`, `0x00243390` | HP3-Recompiled | `port/icall_seeds.json` |
| Hidden-function seeds: `0x000CBEE0`, `0x000FCC70`, `0x000777F0` | HP-POA | `port/icall_seeds.json`; the lifted `sub_000777F0` (a character's interaction handler, reached only by a function pointer after a switch table) is in `port/src/recomp_extra.c` |
| The XBE identity: SHA-1 `51b09e01...`, image end `0x003D6B78`, entry `0x111DB`; the EU disc (SHA-1 `c8841e3c...`) has identical addresses | HP3-Recompiled | `port/region/poa-usa.json`, `docs/region-eur.md` |
| XDK 5849 addresses found by matching HP1's XDK 5659 library (D3D device pointer `0x00275FE8`, ...), the audit of weak bindings | HP3-Recompiled | `docs/bringup.md` |
| The game clock tick `sub_001A7450` (frame count, time, last step, min step 33 ms on NTSC): frame rates above 30 | both | `port/PORT_CHANGES.md` |
| One lifted-code hook (a region check after `loc_00115678` of `sub_00115500`): our port does not need one yet | HP3-Recompiled | `port/tools/patch_gen.py` |
| Engine family (EA UK, like the PS2 Chamber of Secrets), disc layout, `.big` (BIGF), `.hpb` scripts, debug menu | HP3-Recompiled | `docs/re-notes.md` |
| No XMV section: movies are `.mad` through EA's own player | HP3-Recompiled | `docs/bringup.md` ("Title facts") |
| DirectSound crashes on the emulated APU in PoA, so the PCs replace it by a host mixer (our port emulates the APU: check) | HP3-Recompiled | `port/PORT_CHANGES.md`, `port/src/hle_dsound.c` |

All four seeds are in `engine/config/seed_functions.json` (checked by `tools/seedcheck.py` on a lift of this disc, see
`docs/PORTING.md` step 1a), together with 75 more from the stub audit.

The NFSU2 `engine/` was itself made from [myUdav4iik/nfsu2-ios](https://github.com/myUdav4iik/nfsu2-ios) at `85d2cbf` (branch
`ios`; nfsu2-sw v0.5.13 by antoxa2584x with the Apple host and the Metal renderer) with the patches in this folder, applied in
order (`git am`). They are kept here as the record of where the engine's code comes from:

- `0001`-`0024`: NFSU2Deadware's patches by LeoDeadware (MIT), without its HD texture loader (0005's `apple_textures.m` part,
  0010's `apple_textures.m` hunks) and its Ukrainian text and fonts (its 0024, 0025). Their numbers here differ from
  NFSU2Deadware's from 0024 on.
- `0025`-`0027`: the NFS Underground 2 port's (Settings window, player folder, F1).

The upstream agent notes (`CLAUDE.md` of nfsu2-ios) are in `engine/docs/NFSU2_IOS_AGENT_NOTES.md`.
