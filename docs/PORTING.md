# How the port was made

A log of the steps, the decisions and what went wrong. This port starts from the tree of the Harry Potter and the Chamber of Secrets
port ([hpcos-xbox-port](https://github.com/sanyokkua/hpcos-xbox-port) at `8342d21`, v0.1.0 plus later commits), which is itself the
Sorcerer's Stone port's tree ([hp1-xbox-port](https://github.com/sanyokkua/hp1-xbox-port)) on the NFS Underground 2 port's engine
([nfsu2-xbox-port](https://github.com/sanyokkua/nfsu2-xbox-port)). Only the platform stack (Xbox kernel, D3D8, NV2A, DSOUND) is
shared with those games. Per the HP3-Recompiled notes, this game is **EA UK's engine** (the same family as the PS2 Chamber of
Secrets), not Eurocom's (CoS) and not Warthog's (HP1).

Status: **0.1.0 (2026-11-25), the first release.** Plays from the logos through the train into Hogwarts (dormitory, sixth floor, library, Defence Against the Dark Arts, the Marauder's Map challenge) at 30 and 60 fps with sound, saves and loads; the owner's play test 4 found no issues. Release checks: section 4.

## Sources (pinned)

| What | Where | Pin |
|---|---|---|
| Engine, overlay menu, installer | [sanyokkua/hpcos-xbox-port](https://github.com/sanyokkua/hpcos-xbox-port) (vendored `engine/`) | `8342d21` |
| This game's facts, Windows recomp (D3D12) | [MiddleDucky/HP-POA-Xbox-Recomp](https://github.com/MiddleDucky/HP-POA-Xbox-Recomp) (MIT) | `2c2fb7b` (2026-11-18) |
| This game's facts, bring-up notes, region data, Windows recomp (SCDA D3D8 HLE) | [Barthes-Recompiled/HP3-Recompiled](https://github.com/Barthes-Recompiled/HP3-Recompiled) (MIT) | `ecd59ad` (2026-11-01) |
| Disc | Xbox USA, plain XISO (partition offset 0), `default.xbe` 3,190,784 bytes, SHA-1 `51b09e01169fda86f2cb7e5ce45ee80a10268918`, title `0x4541004C`, XDK 5849, entry `0x111DB`, `.text` 2,440,292 bytes | section digests in `mac/xbe_check.py` |

Both references are cloned (shallow) into the gitignored `upstream/` and never committed. What is taken from each:
`docs/upstream-patches/README.md`.

## 0. Phase 1: the new tree (done)

1. `git archive` of hpcos-xbox-port at `8342d21` into this folder, new `git init` (history is not carried).
2. The two references cloned; their LICENSE texts are in `LICENSES/`.
3. The disc extracted with `mac/xiso.py` into `game/extracted/` (3,125 files, 1.9 GB): `default.xbe` SHA-1 matches. Disc formats:
   `.pkg` 1,327 and `.pkgA` 768 (packages: levels, orchestrator), `.sdt` 491 (sound banks), `.hpb` 384 (compiled room scripts),
   `.big` 65 (BIGF archives: menus), `.ids` 28 (speech), `.loc` 16, `.mus`/`.mpf` 12 each (music), `.mad` 6 (movies), `.pfr` 3,
   `.xfn`, `.btf`, `.ttf`. No `.xmv`.
4. Section table of the XBE (13 sections, all stored digests verified against the data, so the image is pristine): `.text`
   `0x11000` (2,440,292), `D3D` `0x264C80`, `D3DX` `0x278A20`, `XGRPH` `0x295480`, `DSOUND` `0x2B66E0` (36,812 raw, 37,428 virtual),
   `XPP` `0x2BF920`, `.rdata` `0x2C77C0`, `.data` `0x2F8F00`, `DOLBY` `0x3CBA80`, `$$XTINFO`, `$$XTIMAGE`, `$$XSIMAGE`, `.XTLID`
   (image end `0x3D6B78`, as HP3-Recompiled's `poa-usa.json`). There is no `XMV` section (CoS had one); `--mmio-sections DSOUND,XPP` still applies.
5. Renamed: app and player folder `Harry Potter Prisoner of Azkaban`, binary `hppoa`, CMake target `hppoa_recomp`, `HPPOA_*` for the
   game and installer layers (the engine's `NFSU2_*` and `RECOMP_*` stay), `mac/xbe_check.py` (title `0x4541004C`, new digest
   table), `engine/src/region_profile.h` (size, SHA-1, entry, `HPPOA_DSOUND_LO/SIZE`), the DMG test's disc patterns.
6. `engine/src/main.c` had CoS's DSOUND range `0x1444E0` (+40,740) hard-coded: now `HPPOA_DSOUND_LO` / `HPPOA_DSOUND_SIZE`.
7. `engine/xboxrecomp/src/kernel/kernel_bridge.c`: the launch-data page's title id (written when `RECOMP_CMDLINE` is set) was a fixed
   `0x45410091`, from the toolkit's Half-Life 2 work. It now reads the XBE certificate's title id (the header is mapped at
   `0x10000`, certificate pointer at `+0x118`, title id at `+8`), `0x4541004C` as a fallback.
8. `engine/src/recomp_manual.c`: all CoS hooks removed (list below); `engine/tools/patch_gen.py`: the CoS fix removed, mechanism
   kept; `engine/config/seed_functions.json`: `[]`. The overlay's rows for CoS-only hooks (Frame Rate, Skip Logo Movies, Skip
   Cutscenes) are removed from `engine/src/overlay/hppoa_options.c`.
9. Comments in `engine/xboxrecomp/` still say `hpcos-xbox-port` or `CoS` where they record where a fix came from; they are history,
   not game code, and were left.

Icon: `mac/make_icon.sh` builds it from the disc's title image as for CoS; for this disc it is Harry, Ron and Hermione on the
menu's green background (checked in the 0.1.0 clean install).

## 1a. Step 1: seeds, lift, build, first boot (done)

1. **Seeds.** `upstream/hp-poa/port/icall_seeds.json` (`0x777F0`, `0xCBEE0`, `0xFCC70`) and `upstream/hp3-recompiled/port/icall_seeds.json`
   (`0x777F0`, `0xCBEE0`, `0x243390`) are 4 addresses together. Neither repo ships another function list: `region/*.json` hold only the
   disc identity. `tools/seedcheck.py` on a first disassembly (21,555 functions): `0x777F0` and `0xCBEE0` free after padding, `0xFCC70`
   already a detected start, `0x243390` a real function after `int3` padding that the detector had merged into `0x243370`'s span. All four
   seeded.
2. **Lift** (`engine/tools/regen.sh`, venv with `capstone<6`): disasm 21 s, whole script 105 s on this Mac. 21,558 functions with the
   four seeds, 0 failed, 2.08 M lines of C in 87 files. `tools/stubcheck.py` had `TEXT` still at CoS's `.text` end (`0x130620`): now
   `0x11000..0x2C77C0` (the code sections up to `.rdata`), and bytes past a section's raw data count as data (XGRPH `0x2B5A67`..`0x2B5A83`
   are zero fill: `read` returns nothing).
3. **Stub audit** (seed every `BOUNDARY` and `code` stub, relift, repeat):

   | Round | Functions | Stubs | Real code | Seeds added |
   |---|---|---|---|---|
   | 0 (4 icall seeds) | 21,558 | 83 | 54 (38 boundary, 16 code; 5 of the code ones are zero fill) | 49 |
   | 1 | 21,693 | 57 | 28 | 23 |
   | 2 | 21,737 | 37 | 8 (3 + the 5 zero-fill) | 3 |
   | 3 | 21,740 | 34 | 5 before the zero-fill fix, 0 after | 0 |

   79 seeds in `engine/config/seed_functions.json`; all are function starts in the final disassembly. Notes name the stub's caller. Seeding
   cuts a few CRT functions (the printf formatter `0x14427E`, the float converters at `0x142xxx`, the XPP ring code `0x2BFF50`) at shared exits,
   as in CoS. `engine/tools/patch_gen.py` with its empty list: rc 0, nothing to do. 48 unresolved call targets remain stubbed in the first lift's count (the last lift: 48, 34 listed by `stubcheck`, none real code); 935 instructions are lifted as no-ops (38 mnemonics).
4. **Build** (`cmake` as `tools/build_release.sh`, `build/deps` copied from hp1-xbox-port's, same SDL3 pin): configure and build in 19 s wall (158 s CPU),
   165 objects, no error, no change to the engine needed. The binary is 23.7 MB.
5. **First boot** (`tools/run_test.sh hppoa_boot 60 20,40`, own HOME): the game starts at `0x111DB`, XAPI init passes (partition1 `TDATA`/`UDATA`
   `4541004c`, `TitleMeta.xbx`, the drive checks), the three worker threads start, `runtime.cfg`, `filesys.dat` and
   `data/packages/plfs/frontend-startup.pkg` are read, the D3D device is created (NV2A `DMA_PUT` moves to `0x02320280`, `vblank -> ISR claimed`),
   `fontsfonts.ini`, `data/misc/debugfont.xfn` and `texturessymbols.xsh` are opened (the first and last do not exist on the console either).
   Then no more file or kernel calls: the game thread spins. 0 `[ICALL] Failed to resolve` lines, no crash, 0 draws, no frame.
   `sample` of the process: game thread `sub_0001A5F0 -> ... -> sub_0014C570 -> sub_0014E100 -> sub_0014EC90` (EA's audio init) `-> sub_002B874A`
   (`DirectSoundCreate`) `-> sub_002B86DD -> sub_002B85A4 -> sub_002B9940 -> sub_002BE0B3 -> sub_002BD8AC -> sub_002BE52C -> sub_002BE412`,
   100 % of the samples in `sub_002BE412`: the AC'97 channel reset. It writes `2` to the bus-master control byte (table of NABM bases at
   `0x2BF060`, byte at `base - 0x13FFEF5`) and waits for the bit to clear with `mov cl,[..]; and cl,2; test cl,cl; jne` where the load is **before** the
   loop (`0x2BE448`), so it loops forever on plain memory. This is CoS hook 5 (`0x14CBAD`) at the new address: lock/unlock helpers `0x2B6710` /
   `0x2B6732`, re-arm `0x2BE21C`. Also in the log: `[UNIMPL]` `out dx,al` `0x26EE90`, `wbinvd` `0x26BC74`, `in al,dx` `0x26DF7D` (all D3D).
   The HP3-Recompiled notes had the same stop (their emulated APU died in `DirectSoundCreate`, see `docs/hp-poa-hooks.md`).
   `tools/smoke.sh` fails for the same reason (no frame dump).

## 1b. Step 2: XDK glue and boot to the Hogwarts Express (done)

PoA is XDK 5849, the same as NFSU2, so the glue was found from NFSU2's hooks: `tools/sigmatch.py` on NFSU2's address gave one hit
each, and `tools/xdis.py` of both bodies differs only in branch targets (100-140 instructions compared).

| CoS # | What | PoA address | NFSU2 address |
|---|---|---|---|
| 1 | CRT memmove (two copies) | `0x13E2F0`, `0x13FF20` | `0x2A7EE0`, `0x2A9450` |
| 2 | D3D_BlockOnTime, device pointer | `0x26C540`, `0x275FE8` (reads `+0x2C`/`+0x30`) | `0x2E8F20` |
| 3 | vblank handler, PGRAPH handler, D3D DPC (not hooked) | `0x26DF00`, `0x26E470`, `0x26E6B0` | `0x2F1D80`, `0x2F22F0`, `0x2F2530` |
| 4 | DSOUND DSP command post | `0x2B8B0D` | `0x32EB65` |
| 5 | AC'97 channel reset (NABM table `0x2BF060`, lock `0x2B6710`/`0x2B6732`, re-arm `0x2BE21C`) | `0x2BE412` | `0x33518D` |
| 7 | XInitDevices, XInputOpen, Close, GetCapabilities, GetState, SetState; gamepad type `0x2BF9F4`, type records `0x2BF924..0x2BF92C` | `0x2BFC0C`, `0x2C096C`, `0x2C09C2`, `0x2C09CE`, `0x2C0BA6`, `0x2C0C19` | (CoS: `0x176729`...; four match CoS by signature, SetState is the same code) |

HP3-Recompiled's "vblank ISR `0x269C90`" is not the vblank handler: it is a swap-side routine that calls `BlockUntilVerticalBlank 0x267470`.
The flip-interval rewrite (CoS `HPCOS_FPS=60`) was left out: 60 fps is later work.

Blockers, in the order they showed (`tools/run_test.sh`, scripted pad):

1. **DirectSoundCreate spins.** Symptom: game thread 100 % in `sub_002BE412`. Cause: the AC'97 reset's NABM load is hoisted out of its wait
   loop (CoS hook 5). Fix: the hook above. Evidence: the next run got past it (commit `7b59edc`).
2. **DirectSoundCreate crashes** (`SIGSEGV` in `sub_002B94B1`, a null `call [eax+8]` at `0x2B8740` first, `E_OUTOFMEMORY` in `ebx`).
   A trace of DSOUND's allocator `sub_002B88AE` showed objects at `0x34`, `0xC4`, `0x554`: the block **sizes**. Cause: the lifted pool
   allocator `sub_002B87E8` ended after its `ExQueryPoolBlockSize` call, because the disassembler had a function start at `0x2B87FF`, the
   middle of that `call [0x2C7954]`. Its only caller (`0x2B5A30`) was inside `sub_002B48AE`, XGRPH's tables (`0x2B2C78..0x2B5A54`) decoded as
   code, itself "called" only from those tables (`0x2B3831`). The toolkit's `_prune_data_call_targets` dropped split targets called from
   outside every function, but this caller was inside a (fake) one. Fix (`engine/xboxrecomp/tools/disasm/functions.py`): a function known
   only as a call target whose callers are all outside every function is data too; its calls no longer count as real callers of a split
   target. Two unit tests; all 62 disasm tests pass. The relift dropped exactly `0x2B87FF` (21,740 -> 21,739 functions; a mid-instruction scan
   of `functions.json` finds one other case, `0x1F45F0` inside `0x1F45DA`: both are a jump table decoded as code, never called, harmless). Evidence: real pool addresses
   (`0x01204324`...), no crash, the EA/WB logos, `intro.mad` and "Press START" at 58 s (commit `7b59edc`).
3. **XPP callbacks unresolved.** `[ICALL] Failed to resolve VA 0x002C0507` (from XInitDevices' driver-init loop, return `0x2BFC44`) and
   `0x002C223D` (return `0x2C3D1F`). Both free (after `ret`/padding). A scan of data dwords pointing into code found five more XPP
   functions only referenced from XPP data: `0x2C0E11`, `0x2C17D8`, `0x2C182A`, `0x2C1C1E`, `0x2C5BEA`. All 7 seeded (86 seeds); stubcheck
   32 stubs, 0 real code. Evidence: 0 ICALL failures (commit `23aa5b1`).
4. **Input.** Start at "Press START" reaches the main menu, both through the direct XInput hooks and through the emulated USB pad
   (`HPPOA_XINPUT_USB=1`). The direct path is the default as in CoS (XInputSetState's OUT pipe never answers on the emulated USB). An early
   "Start does nothing" was the zsh trap again: `$t:start` in the script builder is a history modifier; write `${t}:start`.
5. **New Game.** Start, then A every 4 s: New Game -> the Hogwarts Express cutscene (Hermione, subtitles) -> Harry in the train corridor
   with the HUD, about 75 s after boot (`build/t/ng/sheet.png`). With the left stick after that (`build/t/final/sheet.png`, final build):
   Harry walks down the corridor, through the door into the luggage car, the Ron/Scabbers cutscene plays and the Flipendo tutorial prompt
   ("Pull and hold right trigger...") appears. 0 crashes, 0 ICALL failures. Loaded: `data/packages/orchestrator/inhogex_01.pkga`, `poa_ig.big`.
6. **AV region.** `XGetVideoStandard` (`0x195024`) reads `XC_FACTORY_AV_REGION` (`0x103`), which the kernel did not answer (0). The renderer
   init (`0x349E5`) then keeps its PAL flag and sets a 40 ms minimum game step (`sub_00096520`) instead of NTSC's 33 ms. Fix: the kernel
   answers NTSC-M `0x00400100` (`kernel_xbox.c`). Game region `0x104` already answers 1 (North America), which is this disc's certificate region.

Checked and fine: the KTIMER lost-expiry race of HP3-Recompiled's toolkit (movies did not hang; the game clock runs), no white frames in the
shots. Remaining `[UNIMPL]` no-ops: `in al,dx 0x26DF7D` (vblank handler, port `0x80C0`, as in NFSU2), `pushfd/popfd 0x159676/0x159680`
(around a call, flags unused after), `wbinvd 0x26BC74`, `out dx,al 0x26EE90`, `cli/sti 0x2C6401/0x2C6412`.

## 1c. Step 3: audio, game speed, 60 fps, logo and cutscene skips (done)

Test route (`build/t/train_script.txt`): Start at 55/58 s, A every 4 s from 62 to 118 s, left stick up from 125 s: title,
New Game, the Hogwarts Express cutscene, the corridor, the luggage car and the Flipendo prompt by 160 s.

1. **Audio plays (measured, no fix needed).** `RECOMP_AUDIO_DUMP=<file>,0,165` (the PCM handed to the Mac, 48 kHz s16 stereo)
   over the route, RMS a second: logos 0-8 s (RMS 400-1900), legal screen silent, `intro.mad` 15-55 s (RMS 2300-5900, peak
   11,400-11,900 from the output limiter), menu 58-70 s (RMS 180-1700, harmonic lines in the spectrogram: music), load
   silent 71-74 s, cutscene 75-95 s (RMS 600-1300), corridor 96-136 s (RMS 400-1000), luggage car 137-165 s (RMS 700-2300).
   Zero-crossing rate 0.02-0.08 everywhere (noise would be about 0.5). A 0-4 kHz spectrogram of the cutscene shows voiced
   speech (harmonic stacks with moving pitch, 4-5 syllables a second). `fe.mus`, `hogex.mus` and the `english/speech/*.sdt`
   banks are read. 0 underruns, 0 dropped blocks, APU at 1500 frames/s (real time). None of the CoS audio faults (DSOUND heap
   VA, bit-26 tag, streaming hold) shows here.
2. **Game speed is the console's (measured, no fix needed).** Hook on the frame timer `sub_001A7450` (this = `0x38BA6C`;
   its layout, checked in the disassembly: `+0` ticks, `+4` game ms, `+8` last step, `+0x14` fixed step, `+0x18` minimum
   step, at most 100 ms a tick). `HPPOA_CLOCK_LOG=1` prints game ms against wall ms every 5 s (`=2`: every second). Over the
   route at the default rate: 29.8-29.9 fps (`RECOMP_FPS_LOG=1`), game clock x0.999-x1.001 in every window of menus, the
   cutscene and play; the floor is 33 ms (NTSC). One exception, the New Game load (below).
3. **The load before the train cutscene runs the game clock 22-150x for about a second** (open, not changed). The main
   loop (`sub_001161A0`, tick from `0x115CF9`) spins without presenting while the level loads: 1,300-2,400 ticks in a
   second, each at least the 33 ms floor, so the clock gets ahead of real time and the next tick's step goes negative,
   which the timer reads as a 100 ms hitch (half the ticks are 100 ms). Game time jumps 80-150 s; the next window is x1.000
   again (the timer re-bases its origin). The cutscene manager has the scene set at that moment, but it plays its 25 s at
   x1.000 afterwards with speech in step (spectrogram), so nothing visible was found. On the single-core console the same
   loop also spins while the loader waits on the DVD, so this may be the console's behaviour too. HP-POA-Xbox-Recomp's fix
   (`hle_clock.c`: a tick with no frame since the last one sleeps 1 ms with a 1 ms floor) is the candidate if a symptom shows.
4. **60 fps** (`HPPOA_FPS=60`, overlay Display > Frame Rate, from the next start). Present's NOP trap word is
   `(interval << 5) | 1` as in CoS/NFSU (`49980141`: 2 vblanks): the PGRAPH handler hook rewrites it to 1 vblank.
   Alone that would run the game at double speed (33 ms floor a 16.7 ms frame). The clock: the minimum step becomes 1 ms
   each tick (HP3-Recompiled's measured choice: a 10-16 ms floor puts the clock ahead after a short frame and the next tick
   steps 100 ms); a cutscene's fixed step (setter `sub_00213E40`, getter `sub_001E7640`, both checked: `mov [ecx+0x14],v;
   ret 4` / `mov eax,[ecx+0x14]; ret`) is split over two frames with the odd ms carried, and the getter answers the
   script's own value (HP-POA-Xbox-Recomp). Evidence over the route: `[fps] flip interval 2 -> 1 vblank`, 59.6-60.0 fps,
   game clock x1.000 in every window but the load, screenshots identical to 30 fps (`build/t/c60/sheet.png`). No cutscene
   on the route sets a fixed step (no `[CLOCK] fixed step` line), so the split is untested here. Not ported:
   HP-POA-Xbox-Recomp's lip-sync follow (needs their host mixer's voice levels) and the Carpe Retractum ledge step at 60 fps
   (`sub_000CDFB0`, a level far from the train): both listed as open.
5. **Skip Logo Movies** (`HPPOA_SKIP_LOGOS=1`, overlay Tweaks). PlayMovie `sub_001AC0A0` (thiscall, `ret 8`, name then a
   flag) runs as always and for `eac*` and `wb` the player `[0x364120]` is stopped at once with `sub_0005DC90` (the stop
   StopMovie uses), so the next frame ends the movie the game's way. Evidence: `[movie] logo EAC_USA skipped`, `logo wb
   skipped`, `intro.mad` on screen at 20 s instead of 30 s.
6. **Skip Cutscenes** (`HPPOA_SKIP_CUTSCENES`, on unless 0, overlay Tweaks). The cutscene manager is the singleton
   `0x365EA8` (`sub_000616A0`): `+0x8C` the cutscene playing, `+0x99` skippable. The Apt native SkipCutscene (`0x1FB90`,
   called when A is pressed in a cutscene) runs `sub_00061520(this, force)`, which ends a skippable or forced one with
   `sub_00062170` and sets the menu's `CutsceneSkipped`. Almost no cutscene is skippable (`+0x99` stays 0). Start or
   Back ends a skippable one that way and fast-forwards the others: see 1h (play test 3) for why and how.

7. **Saves work and land in the player folder (no fix needed).** Saving is off in the train (pause menu: Save Game greyed);
   it is on at Hogwarts. Dev helper `HPPOA_START_CHAPTER=<chapter>[:<room>]`: the chapter start `sub_00115390` (cdecl:
   chapter, room or -1, start point, flag) gets that chapter from New Game (StartGame `0x21A60`, return `0x21C6B`; New
   Game passes chapter 0 room 42, the train; chapter 1 is Night 0, `n0start_01.pkg`, the dormitory). `HPPOA_CUTSCENE_LOG=1`
   logs each chapter start. Test (`build/t/sv2`, own HOME and game clone): chapter 1, Start skips the cutscene, pause >
   Save Game > slot 1 > "Saving Harry Potter POA. Please don't turn off your Xbox console." > slot "The Marauder's Map";
   `game/UDATA/4541004c/71370821DB7F/SaveGame-0` (16,384 bytes) and `SaveMeta.xbx`. Restart, title, Load Game (the menus
   move with the left stick, not the scripted D-pad), slot 1: `SaveGame-0` read, chapter start `1 room 40` from the load
   path (`0x21B7E`), Harry in the dormitory with the HUD, the save slot listed again. `mac/import_disc.py` keeps `UDATA`
   and `TDATA` on a new import (as CoS).
8. **Renderer (screenshots: `build/t/a1`, `c60`, `cs3`, `sv2`).** Movies, title, menus, the pause and save screens,
   cutscenes, the train corridor and luggage car (lamp light, Harry's projected shadow on the floor and wall), the
   dormitory and common room: no missing geometry, no white frames, transparency and subtitles right. NV2A lines: the
   Metal path maps `LINES`/`LINE_STRIP`/`LINE_LOOP` to Metal lines (`nv2a_metal.m`, `prim_indices`), so none are dropped,
   but they are one host pixel wide, a third of a console pixel at the default 3x scale (HP3-Recompiled draws each segment
   as a quad one console pixel wide because PoA's shadow fade mask needs it). Not changed: `RECOMP_DRAW_LOG` over frames
   2500-4800 of the train route (860,489 draws) and 1,100 frames of the dormitory (498,938 draws) has only triangle strips
   (prim 6); no line draw was found to test a fix against. (Later, 1d item 1: the mask's line was dropped by the executor before
   the back end, which is why none showed.)

## 1d. Play test 1: rendering

Owner's play test 1 (`.wt/PLAYTEST1.md`): issues 1 (shadows on walls and ceilings), 6 (lock-on marker), the dormitory light
smears and blobs: one root cause, fixed. Issues 2 (Malfoy's face) and 4 (Options "(" glyphs): open, evidence below.
Dev aids added: overlay script action `capture` (`NFSU2_OVERLAY_SCRIPT="102:capture"`: the F12 `[dl]` draw log of the next
frame, for scripted runs), and `[dl]` prints stage 0's texture coordinates (`uv0..`) for draws of 6 vertices or fewer.

1. **Shadows on walls, ceilings, bed canopies; lock-on marker missing; dormitory light smears (fixed).**
   - Symptom: Harry's projected shadow far from him on corridor walls, the dining car ceiling, the dormitory bed canopy and the
     luggage car wall; vertical light smears and glow spots on the dormitory walls and floor; no ring around a Flipendo target.
   - How the shadow works (draw log, `capture`): the character silhouettes go into a 256x256 A8R8G8B8 target (`0x8277F000`,
     alpha = silhouette); each receiver is drawn again with stage 0 = that map (eye-linear texgen, texture matrix: an
     orthographic projection along the light) and stage 1 = a 16x32 **fade mask** (`0x827C0000`, clamp), whose v is the
     receiver's depth along the light (texture matrix row 1: `0.0019 x + 0.0023 y - 0.0016 z + 1.507` in eye space). Combiner:
     alpha = map.a x mask.a, blended over the scene. The mask is drawn once per level: a triangle strip from black (top) to
     white (row 30) and a one-segment `LINE_STRIP` that writes the last row black, so a receiver beyond the range (clamped
     to the last row) gets no shadow.
   - NV2A root cause, three executor bugs (`kernel/nv2a_pb_exec.c`), all in how the mask is submitted (`SET_VERTEX4F`,
     fixed-function mode, positions 0..1 through the composite matrix `16 0 0 0 / 0 -32 0 32`):
     (a) `batch_is_ffp()` excluded inline and `SET_VERTEX` batches, so they were drawn as screen positions: the strip
     covered one pixel and the mask kept alpha 1 everywhere: every receiver in the map's projection got the full shadow, at
     any depth. On the NV2A the transform mode alone decides (D3D's pre-transformed vertices use its pass-through vertex
     program, mode 2). (b) Diffuse sent as `SET_VERTEX_DATA4F(3)` (D3D `SetVertexData4f`) was ignored; only the 4UB form set
     the immediate colour, so the strip and line came out white. (c) Inline and `SET_VERTEX` batches of fewer than three
     vertices were dropped, and with them the closing line (2 vertices). That is why step 3's draw log found no lines:
     they never reached the back end.
   - The same rule (a) drew the game's other inline world-space quads at pixel coordinates: the dormitory's light shafts and
     glows (`[dl]` draws 223-225: 100-unit tall quads at x -107..383, y 600..841, blob texture) became the vertical smears,
     and the spell target ring and dust sprites (world x ~2000) went off screen.
   - Fix (commit `006d8a3`): inline/immediate batches are transformed in fixed-function mode (`RECOMP_INLINE_FF=0` restores
     the old rule), `SET_VERTEX_DATA4F(3)` sets the immediate diffuse, a batch needs 1 vertex for points, 2 for lines.
   - Evidence: the mask dump (`RECOMP_GL_DUMP_SURF=827C0000`) went from alpha `F` in all 32 rows to the ramp `0 0 1 1 2 ... e
     f f 0` (last row black). Before/after (before = `RECOMP_INLINE_FF=0`, same binary, same script):
     `build/t/evidence/luggage_wall_shadow_and_lockon_before_after.png` (wall silhouette gone, green target ring back),
     `dorm_canopy_shadow_blobs_before_after.png` (canopy shadow and wall glows gone), `dorm_light_smears_before_after.png`.
     Route run `build/t/route1/sheet.png` (logos, menu, New Game, cutscene, corridor, luggage car, Flipendo prompt) matches
     main's `build/t/final/sheet.png`; dormitory and pause/Options `build/t/dorm4/sheet.png`; no crash.
   - Not verified against a console: how strong the remaining shadows should be. Receivers under the silhouette now get
     t = 0.2-0.8 in the luggage car (a faint shadow) and about 1.9 on the dormitory floor (none). The mask's content (ramp
     plus closing row) matches HP3-Recompiled's note that the console's mask has the line in its last row.
   - Line width: the mask target is stored at 1x vertically (`offscreen_scale` keeps targets under 512x384 at 1x), so the
     one-pixel Metal line fills the console row here. Lines into upscaled targets are still one host pixel wide.
2. **Malfoy's face and hair blink orange in the dining car cutscene (fixed narrowly in play test 2, item 5).** Not reproduced: the cutscene follows the
   Malfoy fight, which the scripted route does not reach (`HPPOA_START_CHAPTER=0:41/43/44` hang on the loading screen). The
   train's first cutscene (`capture` at 80/86/92 s) has no character draw whose stage 1 texture is stage 0's (the pattern
   HP-POA-Xbox-Recomp's `NoSecondSkinPass` removes); its characters use stage 1 for the window light (`0x819FB400`,
   eye-linear texgen) only. HP-POA-Xbox-Recomp works at the D3D level; their cause (a stage 1 MODULATE2X left over from
   the previous object, sampling the face's own texture) is state the NV2A would also see, so the console should square
   the skin too unless something in our path differs (stale texture contents at a reused address, draw merging). Next
   step: a save or warp into the dining car, then `capture` on a blinking and a normal frame and diff stage 1's texture.
3. **Options screen: "(" glyphs at the right of the Vibration and Subtitles rows (open).** Root of the glyph found, not
   why: the "Off" text quad (`[dl]`, title Options and pause Options) spans x 466..605 with u 0..**1.086** on a 128x64 P8
   text texture (format `06710B29`) with WRAP addressing (`00010101`); u 1.0-1.086 wraps to texels 0-11, the left half of
   the "O" of "Off": the "(". Every other text quad on the screen has u <= 1 on a texture at least the field's width
   (292 px on 512, 175 on 256, 103 on 128); only this 139 px field got a 128 texture. The textures are not made through
   `CreateTexture2` (`0x26AB00`) or `XGSetTextureHeader` (`0x29D2A8`) (logged); the Apt text code writes the header
   itself, not found yet. Suspect a lifted-code difference in the size chosen for that field (it would be 256 on the
   console). "Frame cut off on the right": the frame's quads span x -0.5..639.5 and the parchment band is whole at both
   edges (`build/t/evidence/options_glyphs_unfixed.png`); the dark band on the left is the menu art, not a clip.

4. **Round 2 (after the merge of fix/render, fix/game, fix/perf; 2026-11-24).**
   - Merged main checked: relift 21,754 functions, stubcheck 32 stubs / 0 real code; route at `HPPOA_FPS=60`
     (`build/t/A_route/sheet.png`: logos, menu, cutscene, corridor, luggage car), dormitory runs (`build/t/A_trunk60`,
     `build/t/A_wander`, 250 s): 0 `[ICALL]`, `shader language 3.0`, no crash. The scripted walk no longer reaches the
     trunk (the learned stick range and shorter loads move Harry elsewhere); `sub_000B6170` and the other 7 seeds are
     in the lift.
   - **Floor shadow "missing" after the fade-mask fix: correct, no change.** Measured: (a) the mask as drawn (`RECOMP_DL_RT=827C0000`,
     per-vertex diffuse): strip rows 0 -> 30.4 from (0,0,0,0) to white, white to row 32, `LINE_STRIP` at y 31 with
     (0,0,0,0): row 0 black, row 31 black, as HP-POA-Xbox-Recomp describes the console's. (b) The receivers' v (stage 1,
     texture matrix row 1 on the eye position, `RECOMP_DL_TEXGEN=1` prints `inmap N v1 a..b` = v over the vertices
     inside the shadow map): 0.19-0.33 in the luggage car, so mask alpha 0.2-0.3 x the caster's 0.5 (combiner: map alpha
     = c0.a 0.5; receiver alpha = map.a x mask.a): a 10-15 % darkening. (c) The matrix comes from the game:
     D3D `SetTransform` (`0x2672C0`, `HPPOA_TM_LOG=1`) from `sub_0004A3C0` (`0x4AD63`) = view inverse x the shadow
     record's matrix built by `sub_00048DC0`; difftest of `sub_00048DC0` (`HPPOA_DIFF_DIR`/`HPPOA_DIFF_AT`, 3 calls):
     no difference beyond float rounding in the record (the other differences are other threads' stacks). (d) xemu
     reference frames (`.wt/xemu_ref/xemu_96s.png`, `xemu_104s.png`, `xemu_192s.png`): no clear floor shadow under Harry,
     none on walls or ceilings, the green lock-on ring as ours (`xemu_88s.png`). The "before" floor shadow was the
     old mask's alpha 1. Frame structure seen with `RECOMP_SURF_TRACE=1` (surface methods in order with `[dl]`):
     each caster group clears the 256x256 map, draws its casters, switches to the screen, draws its receivers.
   - Dev aids added: `RECOMP_SURF_TRACE=1`, `RECOMP_DL_TEXGEN=1` (generated coordinates, combiner registers),
     `RECOMP_DL_RT=<va>` (every draw into one target, any frame), `RECOMP_DUMP_ALPHA=1` (dumps write alpha as grey),
     `HPPOA_TM_LOG=1`, `HPPOA_DIFF_DIR`/`HPPOA_DIFF_AT` (difftest capture around `sub_00048DC0`; run
     `tools/difftest.py` with a venv that has unicorn, e.g. hpcos-xbox-port's).

   - **Options "(" glyph: narrowed, not fixed.** The text textures are made by D3DX `D3DXCreateTexture` (`0x279064`,
     from `sub_0004F280`; `HPPOA_TEX_LOG=1`), so `CreateTexture2` is used after all (through D3DX). The size is already
     a power of two when asked: the Apt text field builder `sub_001ADFC0` (return `0x1AE5DB`) passes its record's
     `+0x1C`/`+0x20` (floats through the CRT `_ftol` `0x13E9F8`); `sub_0004EDA0` (round up to a power of two, 16 bit) was
     read in the lifted C: correct. The "Off" field: record `+4` x0 -6, `+C` x1 133 (139 px) gets `+1C` = 128; a field
     with `+C` 169 (175 px) gets 256, one with 97 gets 128. So the width chosen in `sub_001ADFC0` is not the field's
     width rounded up; whether that differs from the console is not known. Difftest of `sub_001ADFC0` (field builder
     call 40 = the "Off" field, `HPPOA_DIFF_FN=1ADFC0 HPPOA_DIFF_AT=40`) stops after 1,596 instructions at a read of
     address 0 in `sub_001AAA00` (`0x1AAA03`; the capture starts at 0x10000), so it is not conclusive. Next: map page 0
     in `tools/difftest.py` (or capture `sub_001AAA00` itself) and compare `+0x1C`; the record is the caller's `ebx`.
     Watch tool `HPPOA_WATCH_FMT=<dword>,<s>` (`HPPOA_WATCH_ADDR`): write-protects the pages holding a value and logs
     the lifted function that writes next to it (dladdr of the host pc).
   - **Options "(" glyph: fixed (round 3).** Symptom: a "(" at the right end of the Vibration and Subtitles rows (title and
     pause Options). Difftest finished: `tools/difftest.py` now maps the low 64 KB as zeros (Unicorn's 32-bit mode keeps
     fs's base at 0, so SEH prologues' `fs:[0]` reach address 0; it stopped there before) and takes `DIFF_BREAK=<eip>,...`
     (registers and `[ebp-0x20]`/`[ebp-0x1c]` at those addresses). Field builder call 33 of the title Options route
     (`build/t/diffO`, the "Off" field) runs 102,647 x86 instructions to the first kernel call: at `0x1AE31C` the original
     x86 also has `_ftol` = 6, so `+0x1C` = 128 on both sides (no difference in the record). **Root cause: the game's own
     arithmetic, not the lift.** `sub_001ADFC0` sizes the text texture as `2 << _ftol(log2(W + 7))` (`0x1AE2E9..0x1AE323`;
     W = the field width `[ebp-0x18]`) but draws the field's quad W + 24 wide (W + 13..14 for fields of mode 2, field
     `+0x18`; measured over 38 fields with `HPPOA_TEX_LOG=1`), with u = quad width / texture width and WRAP addressing (every
     Apt draw has `addr 00010101`; the game never sets clamp). The "Off" field: W 115, texture 128, quad 139, u 1.086: texels
     0-11, the left of its "O", repeat at the right end. Any field with W in [2^k - 24, 2^k - 8] can do it; on the routes
     checked only "Off" does (title menu, Load Game, Bonus Activities, the intro text, pause, pause Options). The console
     should show it too (same code, same wrap); not checked on xemu (needs an owner screenshot of Options there).
   - Fix (hook, `engine/src/recomp_manual.c`, `sub_0013E9F8` = the CRT `_ftol`, only for the call returning to `0x1AE31C`):
     the exponent is raised until the texture is at least the quad's width (at most 512). `HPPOA_APT_TEXT_FIT=0` keeps the
     game's size. Evidence (same binary, fit off / on): the "Off" texture 128 -> 256, its u 1.086 -> 0.543
     (`build/t/O5_off`, `O5_fix` `[dl]`), no "(" on the title and pause Options (`build/t/evidence/options_glyph_before_after.png`,
     `options_title_and_pause_fixed.png`); the other 37-45 fields of each route keep their size (`[d3dx] text field` lines:
     only `W 115 mode 3: texture 128 -> 256`); main menu, Load Game and Bonus Activities shots differ only in the animated
     selection sparkle (`menus_fit_off_vs_on.png`, `pause_fit_off_vs_on.png`).
   - **Malfoy's orange face (open, round 3: cutscene not reached).** Route work (30 fps, `tools/run_test.sh`):
     the monster book is beaten by script (`build/t/book_script2.txt`: the train route, then from 138 s cycles of a
     stick push, `rt` held 600 ms, `rt+x`; the book's defeat cutscene `8220CF14`/`8220D064` at about 167 s, `build/t/B2`,
     `L1`); walking on (`lsu`) reaches the luggage car's end wall at x 2616 z -308 (`build/t/L1`), short of the trunks
     and door of the box puzzle (xemu: pick up a box, drop it, Open the door). New dev aids: `HPPOA_POKE8=<va>=<v>,...`
     (bytes written every tick; NoClip `0x36A93C` alone does not let Harry through walls) and
     `HPPOA_SETPOS=<game ms>,x,y,z` (puts the player there, writing every copy of the position in the object:
     `+4EC`, `+B7C`, `+B8C`, `+E28`, `+174C`, `+1F98`). A jump past the end wall (x 3200 z -150) lands outside the train
     on the track (`build/t/D1`): the dining car is not laid out after the luggage car, so its position is still
     unknown. Draw check on the reachable cutscene (luggage car, Ron and the book, `build/t/F1`, 3 frames `capture`):
     no draw has stage 1 = stage 0's texture except the bloom chain's 4-stage downsample; the characters' stage 1 is a
     separate texture (`comb 00000002`). Next: the box puzzle by script (or the dining car's coordinates, e.g. from
     `[move]` on a real play), then `capture` on the "Smiling on the other side of your face" shot.


5. **Play test 2 (60 fps, `build/t/pt2/play2_partial.log`, owner screenshots 4-6, 13-19, recording 10.00.03).**
   - **Skin blinking orange in cutscenes (Malfoy's face and hair, Ron in "You asked for it, Malfoy!"): fixed narrowly.**
     Symptom: faces, hands, hair and robes of some characters flip between a normal and an orange, over-saturated look
     (robes darker). The recording measured at 60 fps on Malfoy's face (141-144 s): orange (RGB ~210,134,86) for 8-9
     frames, normal (~186,126,88) for 2-3, repeating, so orange is the usual state and normal the blink.
     Captures (28 F12 frames in inhogex_04): the characters in question are drawn with attributes `060D` (a second UV
     set) and stage 1 = stage 0's own image (`s1` = `s0`, same format, no texgen, no matrix); combiner (from the owner's
     `progcache.bin`): stage 0 `t0 x v0 x2`, stage 1 `t1 x r0 x2` (`color_icw 090C0000`, `ocw 00010C00`): base x base x
     light x 4. Game side: the two-layer material `sub_0004C110` sets stage 0 = layer `+0xC` (or the override's), stage 1 =
     layer `+8`, `COLOROP MODULATE2X` (`0x275C00` = 5), `TEXCOORDINDEX` 0 (`0x4C23D`); its multipass twin `sub_0004BFE0`
     draws layer `+8` with blend `DST_COLOR, ZERO`: same product. The cutscene models carry the base texture in both
     layers, so the square is in the title's data and the XDK D3D: the push buffer the NV2A gets is the one we execute.
     Not proven what the console shows; HP-POA-Xbox-Recomp saw the same at the D3D level and takes the pass out
     (`NoSecondSkinPass`, "characters as in play"). Root cause of the alternation not found (which frames take that
     material; not reproduced: the dining car cutscene still needs the box puzzle, a `HPPOA_SETPOS` jump into the car,
     `build/t/DC1`, does not trigger it).
   - Fix (`nv2a_metal.m`, `skin_second_layer`): a later combiner stage whose RGB is `t_s x r0 x 2` (C, D zero, sum to
     r0), stage s sampling stage 0's image through the same program from the same coordinates (attribute 9+s equal to 9,
     no texgen, no texture matrix) passes r0 on (A = r0, B = 1, no scale; alpha `t_s.a x r0.a` likewise). Logs `[skin]`
     (first 4). `RECOMP_SKIN_LAYER=1` keeps the title's colour. Evidence: train route with the book fight
     (`build/t/SK1`): no `[skin]` line (no other draw matches). Owner check: replay the dining car cutscene; the log
     should show `[skin] stage 1 ...` and the faces should stay as in the 2-3 "normal" frames.
   - **Library rug gone near the camera (screenshots 16/17): likely fixed, not reproduced.** No capture of it; the draw
     log of the dormitory shows the title's polygon offset (`SET_POLYGON_OFFSET` scale -0.25, bias -1, fill enable) does
     reach `setDepthBias`, no W-buffer in PoA. Found: Metal's slope term is per pixel of the target, the NV2A's per pixel
     of the title's surface; at render scale 4 the slope part was a quarter of the console's, the part that matters on
     a floor near the camera. The cut on the rug is a straight line, as two coplanar meshes' depth difference is. Fix:
     the factor times target pixels / title pixels (`apply_dynamic`, `slope_k`); `RECOMP_MTL_ZSLOPE=0` off. The log line
     now reads `polygon offset in use (scale -1.000, bias -1.0; target scale 4.00)`. Dormitory A/B (`build/t/TG0` off,
     `TG1` on): no visible change. Owner check: the library fight; if the rug still drops, F12 there.
   - **`[mtl] texgen 8512 on stage 0 coord 0 not generated`: done.** 0x8512 is `GL_REFLECTION_MAP` (D3D
     `TCI_CAMERASPACEREFLECTIONVECTOR`); 0x8511 normal map, 0x2402 sphere map. In the dormitory a 15-vertex blended draw
     (`816E4980`, attributes position + normal only, texture matrix on: an env sheen) used it on S, T, R; the Metal
     fixed-function shader left its coordinates as the constant texcoord (one texel). Now generated in the vertex shader
     (eye-space normal from the model-view 3x3, r = u - 2(n.u)n, sphere map from r), then the texture matrix;
     `RECOMP_MTL_TEXGEN_MAPS=0` off. Shader test (`tests/nv2a_metal_msl`, with the owner's `progcache.bin`): 459
     program pairs, 0 failures. Dormitory shots off/on (`build/t/evidence/dorm_texgen_reflection_off_on_diff.png`):
     a few pixels differ, nothing else.

## 1f. Play test 1: gameplay fixes (2026-11-24, `.wt/PLAYTEST1.md`)

1. **#7 Dormitory trunk: Harry searches it forever.** Symptom (recording 20.21.09 at 5:15-6:00, and again after Load Game):
   A at the red "H.P." trunk, Harry opens it and stays bent over it; no card, no next step. The owner's `build/play/last.log`:
   2,464 `[ICALL] Failed to resolve VA 0x000B6170`, the first in the frame A was pressed, then one per frame (call site
   `sub_000BB670`, return `0xBB635`), plus one `0x000B97B0` on the train (from `sub_000B7870`). Root cause: both are member
   functions the static pass never found. Their only reference is a static initializer that fills a state table
   (`0x260764`: `mov eax, 0xB6170; mov [0x2FE5A0], eax`; state `0x1F`'s slots at `0x2FE590`), an immediate in code, not a
   data dword, so neither the call graph nor the data-pointer scan saw them. The ICALL miss returns without running the
   state's update, so the search state never ends. Fix: `tools/immscan.py` (new: `push imm32`/`mov eax, imm32` pointing at
   code after `int3`/`ret` padding that is no function start) listed 8; all real code by `seedcheck.py`, all seeded
   (94 seeds): `0xB3AC0`, `0xB6170`, `0xB67A0`, `0xB97B0`, `0xF9AE0` (state-table member functions), `0xB9390` (after
   padding inside `0xB9306`'s span), `0x1915C0` (callback pushed at `0x1922AF`), `0x26E140` (D3D callback stored at
   `0x26F02A`). Relift: 21,754 functions, stubcheck 32 stubs, 0 real code; `immscan` now lists only 4 non-code hits
   (`0x4003D`, `0x8C25D`: int3 runs inside data immediates; `0x292638`: a string; `0x2A80BA`: an SEH label). Evidence: the same
   scripted run (`build/t/trunk_script.txt`, `HPPOA_START_CHAPTER=1`, walk to the trunk spamming A) on d6c6489: Harry stuck
   bent over the open trunk from 106 s to the end at 130 s, 763 `0xB6170` misses; with the seeds: he takes a collector's card
   out, the hint "Complete collector's card sets for extra rewards" shows, play goes on, 0 ICALL misses
   (`build/t/t7old`, `build/t/t7new`). `tools/run_test.sh` takes `HPPOA_BIN=<binary>` for such A/B runs.
   Backport candidate: run `tools/immscan.py` on every Xbox port (NFSU1, NFSU2, HP1, CoS): any "free" hit is a silent
   `[ICALL]` miss waiting for its state.

2. **#5 Handling "not smooth enough".** Measured (`HPPOA_MOVE_LOG=1`, new: a `[move]` line a tick with game ms, step, the
   stick as the game sees it and Harry's position at player `+0x4EC`; `=scan` lists the player's changing floats; the
   player is player 0's handle in the character table `[0x3660C8]+0xE6`, object `[0x3BDCF0 + handle*4]`):
   - Game side: the poll `sub_00229F90` turns each axis into `(v + 0.5) / 32767.5` and zeroes it under 0.25 per axis
     (`0x2ED468`); the rest drift in the owner's log (LX -594, LY 2174 = 0.02/0.07) never reaches the game. Triggers: an
     analog button counts as held above 1 (`cmp dl,1; seta`), ours are 0..255 (SDL >> 7): fine.
   - 30 vs 60 fps, same scripted walk from the dormitory start (`build/t/mv30`, `mv60`, `build/t/move_script.txt`): the
     steps are 33/34 ms and 16/17 ms with no stray ticks while walking; Harry moves a fixed 19.55 units a tick at 30 and
     9.78 at 60, i.e. the same 575-611 units per game second, starts moving 0.3-0.4 s after the push at both, and the
     motion per displayed frame is even. No frame-rate dependence found in walking speed or start-up. Input is fresh:
     XInputGetState reads SDL's state each call, and the main thread's event wait pumps the pads (SDL 3's periodic
     poll while joysticks are open).
   - **Defect: the stick range compensation.** `stick_range` (from CoS) made a push to 65 % read as full on every pad.
     In the owner's play log 92 % of the 7,212 moving stick readings were at full magnitude: the game's walk band
     (its 0.35 dead zone up to the run at 0.675, per HP-POA-Xbox-Recomp) sat in 23-44 % of the travel, so Harry jumped
     from a stop to a run and could hardly walk. Fix (`stick_deadzone.h`, after HP-POA-Xbox-Recomp's learned
     compensation, MIT, facts only): each of 8 directions learns the furthest the stick has reached (at least 0.60);
     a reading is scaled by its direction's reach. A full-range pad keeps its whole travel, a short pad (0.67 straight
     back) still reaches full there. `RECOMP_STICK_RANGE=<fraction>` keeps a fixed range, `1` turns it off. Tests:
     `engine/xboxrecomp/tests/stick_deadzone` (7 new checks; written first, failed, then pass). Not verified with the
     owner's pad (scripted sticks are full pushes, unchanged by either rule).
   - Not changed: upstream's "Responsive movement" (turn speeds x2, no shuffle/pivot wait, instant run) is a
     deliberate change from the console's feel, not a port defect; a candidate setting if the owner still finds it slow.
   Backport candidate: the learned range in every port that has CoS's fixed 0.65 (`stick_deadzone.h`: HP1, CoS, NFSU1, NFSU2
   if they took it).
3. **`[pad]` lines off by default in `tools/play_dev.sh`** (11,340 of the 58,301 lines in the owner's log); `HPPOA_PAD_LOG=1`
   brings them back.

4. **"Can get stuck": scripted runs after the fix** (60 fps, `HPPOA_CLOCK_LOG=1`). `build/t/st1` (250 s,
   `build/t/wander_script.txt`: the trunk, then 15 pushes with A): the trunk search ends with the card, Harry goes through
   the door to the stairs and back (`portal_boysdorm_to_boysdormstairs.pkg` and back); `build/t/st2` (175 s,
   `build/t/stairs_script.txt`: straight out of the door): stairs, common room portal loaded, then a cutscene and Harry back
   in the dormitory with the "show the Folio Bruti to Ron" hint (`n0start_01.pkga`: the story sends him back until the task
   is done; not a stall). Both: 0 `[ICALL]` misses, the clock at x1.000 (300 ticks per 5 s; one window x0.979), no stub hit.
   `[UNIMPL]` seen: the known D3D `in`/`out`/`wbinvd`, and `pushfd`/`popfd` at `0x159676`/`0x159680` (6 each), inside a
   `pushal ... popal` around a call whose flags nothing reads after: harmless. The common room's own steps (after the
   Folio) were not reached by script.

5. **Trunk recheck (round 3, current main + the Options hook).** `build/t/trunk_script.txt` (`HPPOA_START_CHAPTER=1`)
   reached the trunk in 1 of 3 runs (`build/t/T1`: Harry takes the collector's card out at 111 s, 0 `[ICALL]`;
   `T2`, `T4`: the camera-relative walk ended at a bed). The walk depends on when the level finishes loading (game ms
   at the first push 200,927 vs 203,624) and the camera swing. A fresh script with `HPPOA_SETPOS=206000,1881.5,451,-1650`
   (next to the trunk, `build/t/trunk_script3.txt`) puts Harry there every time (`T8`-`T10`) and the "Open" prompt
   shows, but plain `a` presses (100 ms) did not open it in `T3`/`T10`, while `T1`'s `lsl+a` (stick held into the
   trunk) did. Open: why the prompt does not take a bare A (facing?), not checked on the owner's pad.

## 1g. Play test 2 (60 fps, 2026-11-25): subtitles and frame drops

1. **Subtitles cut on the right and in half (root cause found; fix is in the renderer, not applied here).**
   - Symptom (owner's screenshot 18, recording 10.00.03): the train cutscene's "Blimey! Twelve years ago Sirius Black
     mur" cut on the right, its second line cut through the letters. Scripted (`build/t/sub2`, `capture` + shots at
     78-92 s): the same "Blimey!" text stays on screen for 15 s, centred in some shots and cut in others.
   - Not the Apt text field size (1d item 4): the subtitles do not go through `sub_001ADFC0`. They have their own
     system: `sub_001AEFB0` lays the line out with `sub_001B08C0` into a 512x256 byte buffer `0x39A2D0`, writing its
     size to `0x30C2A8`/`0x30C2AC`; `sub_001AEF40` swizzles the buffer into the 512x256 P8 texture `[0x3BA2E0]`
     (`0x831DD000`); each frame `sub_001AF290` draws it in a fixed box x 100-540, y 386-438 (`sub_001AEE20`), the
     text centred: `sub_001AF050` x0 = 100 + (440 - W) / 2, u = 0..(540 - x0) / 512; `sub_001AF0B0` the same for the
     height in 52 (taller text scrolls, `sub_001AF110`). So the box follows the size of the current line.
   - Measured (dev aid, `build/t/evidence/subtitle_meas_aid.diff`, not committed: the size and a hash of the buffer
     and texture at each vblank, dumps): each new line ("Imagine it - a single curse...", W 215; "Crookshanks! No!",
     W 133; "Get out of it!", W 98) is laid out, the buffer and the guest texture hold it
     (`build/t/evidence/subtitle_canvas_guest_memory.png`), but the screen keeps "Blimey!" in the new, smaller box:
     the renderer never uploads the texture again (`RECOMP_TEX_CHANGES=1`: no `831DD000 changed` line in the run).
   - **Root cause: the Metal renderer's texture change check misses it** (`nv2a_metal.m` `tex_get`): it hashes one
     dword every `bytes / 256` bytes, and `tex_bytes()` counts 4 bytes a texel for every uncompressed format, so for
     this P8 texture `bytes` is 512 KB: 64 dwords inside the texture (every 2048 bytes = texels at u multiple of 64,
     v multiple of 32 in swizzled order), plus 192 outside it. A line of thin glyphs changes none of them.
   - Check: bumping `nv2a_tex_replace_gen` (every texture asked again) whenever the guest texture changes (same aid,
     `MEAS_BUMP=1`, `build/t/sub7`): every line shows, whole and centred
     (`build/t/evidence/subtitles_stale_before.png` vs `subtitles_forced_refresh.png`).
   - Fix (not applied: `nv2a_metal.m` is being changed on main by the renderer work):
     `build/t/evidence/p8_texture_hash.patch` hashes P8 textures over their real size (w x h bytes), every byte
     (8 bytes a step; 128 KB for the canvas). The patch is not built or run yet; the check above shows what it must
     give. The same check is in every port's renderer (NFSU1, NFSU2, HP1,
     CoS: same `bytes_hash`/`tex_bytes`), so any text drawn into a reused P8 texture can stay stale there too.

2. **Frame drops at 60 fps: measured, nothing in the frame loop to fix.**
   - Recording (OCR of the fps counter, 2 samples a second over 672 s): 621 seconds show 60 fps, and in 611 of them the
     worst frame of the second is 17 ms (the rest 18-27 ms). The drops are (a) 7 loading screens, 3-5 s each at 20 fps (frames 50-60 ms,
     the `[hitch] ... 13 draws` / `17 draws` lines), and (b) single frames: two first-time shader compiles (310 ms,
     6 programs, at 342 s; 201 ms, 2 programs, at 532 s; the cache keeps them for the next run) and four frames
     where the game loads a package on its own thread (cutscene or portal `.pkg`): 61 ms (571 draws), 128 ms
     (1093 draws), 89 ms, 73 ms. Log time = recording time + 60274.3 s.
   - Loading screens: `sample` during the New Game load (`build/t/prof1`): every thread waits. The game thread spins
     in the cutscene manager (`sub_000611F0`: yield until the cutscene's `+0xA6` flag clears), the pushbuffer thread,
     audio and loader threads sleep. Same frame times at 30 fps (`B1`, `T1` on main: 31 frames over 50 ms, mean
     55 ms, as at 60 fps): not a 60 fps or host cost.
   - Owner's settings (scale 400, SSAO, `build/t/perf1`, the train route, `RECOMP_FPS_LOG=1`): flip p99 16.8-17.6 ms
     in every 10 s window outside the load, vblank phase error 0.01 ms; GPU p50 3.4-3.8 ms in menus and 11-13 ms in
     the cutscene, corridor and luggage car, p99 15-20 ms, 0-13 frames a window over 16.7 ms (the next frame
     absorbs it). That margin is the risk in heavier scenes. SSAO is not the cost: the same route with bloom
     (`build/t/perf2`) has GPU p50 10-13 ms, p99 15.6-17.8 ms; the 400 % scale is.
   - Not changed: the renderer (compiles, GPU load) is outside this branch, and the remaining single-frame hitches are
     the game's own synchronous loads. Before/after numbers: none (no change made).

## 1h. Play test 3: Skip Cutscenes skips every cutscene (fast-forward)

Owner's report (`.wt/PLAYTEST3.md` issue 1): after `d3b878c` Start/Back did nothing in almost every cutscene (only
`+0x99` skippable ones were ended); before it, forcing every cutscene ended `inhogex_07` (the dementor) and
`inhogexcam_01` then looped forever.

1. **How a cutscene ends (disassembly, `tools/xdis.py`; script constants from `data/misc/intro_hogwartsexpress.hpb`).**
   The level script plays a cutscene with PlayScene(orc, done, skip): `sub_00061F10` looks both script functions up by
   name on the level's script module and keeps their handles at `+0x1C` / `+0x20` of the cutscene object. Each frame
   `sub_00062100` advances the cutscene's time and steps the orchestrator (`sub_001A53B0`), which fires the timeline's
   events in order; past its length it calls EndScene `sub_00062090`, which runs `done` through the script VM
   (`sub_001B8C70` -> `sub_001B88D0`) and starts the next queued scene. A skip, `sub_00062170`, clears the subtitle,
   puts the orchestrator in skip mode (`+0x34 = 1`, `sub_001A50D0(1)` on every track), runs EndScene (so `done`), then
   runs `skip`, and posts `PostSkipScene`. So a forced end drops every timeline event after the press and runs a `skip`
   function the title never runs for an unskippable scene. The dementor's `Cutscene_EnterTheDementor` plays `InHogEx_07`
   with `done = _Over_1` (sets up the dementor) and `skip = _Skip` (only starts the music `MUS_HE_DEM1`), then queues
   `InHogExCam_01`. Not proven: which lost event or call made `inhogexcam_01` loop for the owner. A staged forced end
   (below, `forceA`, `forceC`) went on to the chase; the loop needs something of the owner's run that the staging lacks.
2. **Fix: fast-forward, not end** (`engine/src/recomp_manual.c`, "Cutscenes"). Start or Back in a cutscene that is not
   skippable sets the game clock's fixed step (`0x38BA6C +0x14`, not clamped, unlike real-time steps) to
   `HPPOA_FF_STEP` ms a tick (100: the step the timer itself takes after a hitch) and flips every frame on the next
   vblank (the PGRAPH handler's trap rewrite, as 60 fps), with the sound muted (`xa2_set_mute`), until `+0x8C` changes;
   then the title's own fixed step goes back. The getter `sub_001E7640` answers the script's own value meanwhile, so a
   script that saves and restores the step keeps its value. About 6x at 30 and 60 fps. The scene plays out: every
   event fires and `done` runs, as on the console. Skippable ones still end the game's way. The level scripts' scenes
   with the bars that are not orchestrator cutscenes (`+0x8C` 0, Apt screen `widescreen` from SetTraceControlMode
   `sub_00020220`, e.g. the Marauder's Map challenge's opening) are fast-forwarded the same way until the screen
   changes. HP3-Recompiled's idea (scaled QPC, `hle_skip.c`), applied through the clock's own fixed step instead.
   `HPPOA_CUTSCENE_FF=0` off (unskippable ones play, as `d3b878c`), `=force` the old forced end (A/B only).
   Speech: the timeline does not wait on it (game ms against wall ms in the logs is the full 6x).
3. **Dev aids.** `HPPOA_CALL_SCRIPT="<s>:<function>,..."` runs a level script function by name `<s>` seconds after the
   first clock tick (the module and lookup PlayScene uses). The train's dementor scene without the luggage-car puzzle
   and the fights: `112:Cutscene_MBOM_Defeated,125:CutScene_DinerCarDoTauntRon,140:Cutscene_DinerCarMalfoyAttacks,
   155:Cutscene_DinerCarDuelWon,170:Cutscene_EnterTheDementor` (New Game, walk up the corridor 92-104 s): the dark
   dining car, Ron frozen, then inhogex_07 -> inhogexcam_01 -> the chase, as in the owner's recording.
   `HPPOA_FF_AUTO=<n>` fast-forwards the next n unskippable cutscenes without a press. `HPPOA_CUTSCENE_LOG=1` also
   logs the Apt screen and, on a forced end, the `done`/`skip` handles.
4. **Evidence** (30 fps unless named; shots `build/t/<run>/shotN.png`, sheets `sheet.png`):
   - (a) train opening, Start (`ff_train`): `[cutscene] 8220CFBC fast-forward (not skippable), 100 ms a tick`,
     `fast-forward done: 11400 game ms in 3812 wall ms` (before the vblank flips: 3x); shot2 its last line "I think
     Scabbers went that way...", shot3/4 Harry in the corridor with the HUD, shot5 the luggage-car cutscene next. At
     60 fps with Back (`train60`): `fast-forward done: 19400 game ms in 3235 wall ms`, then `[CLOCK] ... x1.000`.
   - (b) dementor, pad Start in each scene (`ffpad`): `8220B384 fast-forward` (inhogex_07, shot1 "Harry? Harry!"),
     `fast-forward done: 13800 game ms in 2302 wall ms (next 8220B2DC)`, `8220B2DC fast-forward` (inhogexcam_01, shot2),
     `done: 5800 game ms in 977 wall ms (next 00000000)`; shots 3-15 the chase in the dark car with the HUD, the
     dementor, the kiss, then the game's own restart (inhogexcam_01 again, natural, at game ms 537260). Without presses
     (`nat`): the same chain at 1x. Forced end (`forceA`, `forceC`, `HPPOA_CUTSCENE_FF=force`): `8220EABC script
     functions: done 810B43EC skip 810B4908`, `skipped (skippable 0): ended`, then inhogexcam_01 (`done 810B472C skip
     00000000`) and the chase.
   - (c) Hogwarts, the owner's `SaveGame-2` (Load Game, slot 3): the Marauder's Map challenge's opening (scripted, Back,
     `hw2`): `00000001 fast-forward (scripted scene, screen widescreen)`, `done: 6600 game ms in 1102 wall ms (next
     00000000, screen pre_ig)`, shot3+ Harry and Ron with the HUD. Its next cutscene N0Maraud_04 (called with
     `HPPOA_CALL_SCRIPT=97:N0MARAUD_04_CutScene`, Start, `hw3`): `8220EA14 fast-forward (not skippable)`, `done: 10300
     game ms in 1721 wall ms`; shots 6-11 the pixie fight with the HUD. `SaveGame-1` loads (`hw1`, sixth floor); no
     cutscene was reached from it by script.
   - Not verified: the exact play-test-3 loop; a skippable (`+0x99`) cutscene; the owner's own pad and timing.
## 1i. Play test 3: floor blotches (issue 3) and the flat close-up (issue 4)

Owner's play test 3 (`.wt/PLAYTEST3.md`, recording 13.49.25). Places found by matching the images to the recording
(1 fps frames, grey 64x41, least absolute difference): images 20, 28, 29 = 5:31-5:32, the frozen luggage car **before**
Effects was set to AO (about 6:45); images 26, 27 = 7:37, the boys' dormitory stairs in Gryffindor Tower (not the sixth
floor); image 30 = 11:38, the library; image 31 = 14:07, the Fred and George cutscene.

1. **The blotches are the game's projected character shadows (no change).**
   - The owner's frames around 7:37 (`build/t/evidence/pt3_owner_dormstairs_shadow_is_harry.png`) show the blotch as
     Harry's own silhouette (head, arms, robe) thrown sideways from the wall lamp; it moves with him and is cut where the
     game stops drawing a receiver mesh. In the luggage car (5:32) the same shadows lie under the mist.
   - A/B, same route and times, `RECOMP_MTL_SKIP_TEX=8277F000` (new dev aid: drop every draw that samples that texture,
     here the receiver passes that sample the 256x256 shadow map): the dark shapes go and nothing else changes
     (`pt3_shadow_receivers_on_off_train.png`: train corridor, luggage car entry, Flipendo; `pt3_hall_receivers_on_off_aodebug.png`:
     SaveGame-0's hall).
   - The pipeline was checked against xemu (`hw/xbox/nv2a/pgraph/glsl/vsh-ff.c`, `psh.c`): eye-linear texgen dots the
     model-view position, the texture matrix is applied row by row as here, the receiver combiner is `r0.a = map.a x mask.a`,
     colour = map colour, blended SRC_ALPHA; map and mask addressing clamp to edge. The mask's closing line lands on row 31
     (1d), as HP-POA-Xbox-Recomp checked against the console's mask. Casters draw without the depth test into the map
     whole (dump `pt3`-time `RECOMP_GL_DUMP_AT=58,1 RECOMP_GL_DUMP_SURF=8277F000`: a full silhouette, alpha 0.5).
   - Not the stencil (xemu audit 5): `[dl]` now prints the stencil state when the test is on (`| st func ref mask op ...`);
     0 of 1,002 draws in the hall and the train captures have it on. The op-0 mapping was fixed anyway (below).
   - **AO is not the cause** (the luggage car images predate it). Measured on SaveGame-1's landing with
     `NFSU2_POST_DEBUG=1`: the occlusion term is a faint crease shade plus one grey patch beside Ron; colour shots with and
     without AO differ by a few percent. Found on the way, not changed: the AO linearises depth with NFS's near 0.5 / far
     10000, while PoA projects with near 10 / far ~10010 game units (from the composite matrix: depth 0 at eye z 10) and
     about 62 units to a metre. `NFSU2_AO_NEAR=0.16 NFSU2_AO_FAR=160` gives metre-true AO: real crease shading, the grey patch
     lighter, but the picture darker overall (`pt3_ao_term_near05_vs_near016.png`, `pt3_ao_colour_near05_vs_near016.png`).
     A taste call for the owner, so left as is. Also seen: the scene effects run at the first draw without the depth test,
     which in PoA is the shadow-map caster pass, so they apply before the transparent effects of the frame.
   - Not verified against a console: the shadows' strength. xemu's reference frames show no floor shadow at all in the
     luggage car, while HP-POA-Xbox-Recomp fixed the mask because Harry's shadow showed on walls on PC, which implies the
     console draws it on floors. If the owner finds them too strong, a "Character Shadows" setting is the honest knob.

2. **Issue 4: geometry in front of the near plane drawn flat over the screen (fixed: clip-range cull).**
   - The game sets `SET_ZMIN_MAX_CONTROL` = 1 (ZCLAMP_EN = CULL, `[dl]` now prints `zmm`, the clip range and stencil) on
     its 3D draws, clip range 0..zmax. On the NV2A (xemu `glsl/psh.c` "Depth clipping") a pixel whose depth is outside the
     clip range is dropped. Here every vertex's z was clamped to [0, w] and Metal clamps depth (MTLDepthClipModeClamp), so
     a mesh the camera is inside was drawn at depth 0 in front of everything: the cutscene close-up of image 31 (a twin's
     face filling the screen, 14:07) and, reproduced here, Ron's legs and a wall painting at the camera on the sixth-floor
     stairs (`build/t/zc_on` vs `zc_off`, `pt3_zcull_off_on_stairs.png`: the near painting and Ron's legs cut away).
   - Fix (`nv2a_metal_msl.c`, `nv2a_metal.m`): the vertex shader passes the unclamped z and w (`vZw`), the fragment shader
     drops the pixel when z / w is outside the clip range and the draw's ZCLAMP_EN is CULL (`FsU.u_zcull`; not with the
     W-buffer). `RECOMP_MTL_ZCULL=0` turns it off. Independent of the per-vertex z clamp (the vis branch removes that).
   - Checked: train route (title, New Game, corridor, luggage car, Flipendo; `build/t/tr_fin` vs `tr_off`), SaveGame-0/1/2
     loads, the sixth- and seventh-floor stairs: no missing geometry, HUD and text whole. 133 of the 277 draws of the luggage-car capture use CLAMP
     (`zmm 10`) and are untouched. The image-31 cutscene itself was not reached by script (14 min into the owner's run).

3. **Other xemu audit items taken here (each behind a flag, A/B on the train route: no visible change, nothing broken):**
   - Stencil op 0 is ZERO, not KEEP (`stencil_op`, audit 5; `RECOMP_MTL_STENCIL_ZERO=0` off). PoA never enables stencil in
     the scenes captured.
   - Alpha test compares the rounded 8-bit alpha (`gl_psh.c`, audit 8; `RECOMP_ATEST_ROUND=0` off).
   - Fixed-function lighting no longer flips normals that face away from the eye (`lit_color` in `nv2a_pb_exec.c`, audit 3;
     an X-Men Legends hack the NV2A does not have). `RECOMP_FF_FLIP_NORMALS=1` brings it back; the hall shot changes by a few
     pixels at Harry's silhouette.
   - Not done: specular lighting (audit 2: every lit draw has SPECULAR_ENABLE, no specular term is computed), LOD bias
     (audit 7).

Dev aids added: `RECOMP_MTL_SKIP_TEX=<va>`; `[dl]` prints `zmm`/clip range, the stencil state, and with `RECOMP_DL_TEXGEN`
the composite's screen position per vertex (`scr`); `RECOMP_DL_TEXGEN=2` prints the texture matrices, composite and
model-view of every draw (`[dlt]`); `RECOMP_GL_DUMP_SURF` also works with `RECOMP_GL_DUMP_AT`. Test aids (not committed,
`build/t`): `setup_save.sh <name> [effects]` (the owner's save copies and settings in a test dir), load scripts
`20000:start,23000:start,40000:lsd,43000:a[,46500:lsd],48500:a` (slot 1 = SaveGame-0, the hall; SaveGame-1/2 = the
sixth-floor landing), `train2_script.txt` (New Game to Flipendo, skip-logo settings, warm shader cache).

## 1j. Play test 3: the library rug and the plank patch (issue 2)

Images 21-25 and 30 (library, recording 13.49.25 about 11:20-11:40, right after the Peeves cutscene, camera high above
the rug's near end) and 32-35 (the gargoyle door at the start of the Marauder's challenge, playing as Ron).

1. **Not the game's culling.** The rug is three meshes drawn one after the other, last in the room pass: `[da]` lines
   (`RECOMP_DL_ATTRS=0605`, new) show the pieces with 208, 40 and 126 indices (local z 0..-1883, attributes 0605: two
   texture coordinate sets) in every library frame together: 899 frames each over a 25 s walk, never one without the others.
   A cull (room/portal walk, HP-POA-Xbox-Recomp's `hle_portals.c`) would drop a piece; the owner's cut runs through the
   middle of the near piece. So the draws are there and something hides part of them.
2. **Root cause: the per-vertex z clamp in `nv2a_clip` (renderer; xemu audit finding 1).** Each vertex's z was clamped to
   [0, w] in the vertex shader. PoA projects with near 10 and far ~10010 game units: clip z = 16,794,000 w - 167,940,000
   (composite rows, `[dl]` now prints `mz`/`mw`), so a vertex between the eye and the near plane (0 < w < 10) has z down
   to -1.68e8, ten times the whole depth range; the clamp moves it to 0 and tilts the depth of every triangle using it.
   The rug's triangles run end to end (up to 1,883 units), so when the camera hangs over a rug end, the triangle's far
   part is pushed back behind the floor and disappears along the triangle's edge: a straight line, near half gone, gone
   or back as the camera moves a few units. xemu leaves z alone and clamps or culls per pixel (`gl/draw.c` depth clamp,
   `glsl/psh.c` clipRange); Metal already clamps per pixel (`MTLDepthClipModeClamp`), and main's clip-range cull (1i)
   drops the pixels in front of the near plane on the CULL draws.
   - Measured (`RECOMP_MTL_ZCLAMP_LOG=1`, new: `[zc]` lists fixed-function draws with a vertex at 0 < w and z < 0 or
     z > zmax w): on a library walk, floor and wall draws have such vertices in nearly every frame, and the rug pieces in
     long runs of frames (wmin 0.1-9 units, `[da]` `wmin`) whenever the camera is over a rug end.
3. **Fix** (`nv2a_metal_msl.c` `nv2a_clip`): z is passed unclamped; depth is clamped (CLAMP draws) or culled (CULL draws,
   1i) per pixel only. `RECOMP_MTL_ZCLAMP_PIXEL=0` brings the vertex clamp back (`u_fog.w`, a runtime uniform, so no
   shader rebuild); `RECOMP_MTL_ZCLAMP_PIXEL=0ab` alternates the two every 2 s of wall time and logs `[mtl] depth clamp now
   per ...`, for A/B shots of a still scene in one run.
4. **Verification, and what is missing.**
   - Shader test: 461 program pairs (61 from the owner's `progcache.bin`), 0 failures.
   - Library, SaveGame-1 (`build/t/load1.txt` then the door; `HPPOA_POKE8` CameraTargetYOffset `0x36AACC` = 150.0 for a high
     camera in `AB_hi`): static A/B in one run with `0ab` while a rug vertex sat in the near slab (w 2.7 and 8.5,
     `build/t/LE_z1`, `AB_1200`): pixel and vertex shots identical except Harry's idle animation
     (`build/t/evidence/pt3_rug_static_ab_pixel_vs_vertex.png`, `_diff.png`, `_2.png`, `_2_diff.png`). No regression.
   - **Reproduced and fixed in one frame.** The owner's recording (696-700 s) shows the rug's corner flickering frame
     to frame with an almost still camera: a rug vertex in the near slab flips with the camera's sway. A debug build
     (`.wt/hppoa-render/build/t/evidence/pt3_zsplit_debug.patch`, not committed) drew the left half of each frame with
     the old per-vertex clamp's depth and the right half with the per-pixel clamp, and took a shot whenever a rug
     triangle with a vertex in the slab was on screen (SaveGame-1, library walk, 4x). In
     `pt3_rug_cut_split_f3032.png` and `_f3038.png` the left half cuts the rug off along a straight edge (bare floor and
     Harry's shadow), the right half draws it whole; `pt3_rug_split_control_f3156.png` shows no seam at the split.
     `[zv] f3038 #102 verts 61 attrs 0605: 6 visible bent tris, slab w min 1.28`. The DADA plank patch was not reached.
   - The plank patch at the gargoyle door (images 32-35) was not reached: SaveGame-2 starts in the challenge hall, and
     the scripted walk fell into the collapsing floor. Same mechanism expected (a large floor quad near the camera,
     playing as Ron with a low camera).
   - Owner check: the library rug after the Peeves cutscene and the gargoyle door; if a cut still shows, F12 there and
     `RECOMP_MTL_ZCLAMP_PIXEL=0` for the A/B.

Dev aids added: `RECOMP_MTL_ZCLAMP_PIXEL=0`/`0ab`, `RECOMP_MTL_ZCLAMP_LOG=1` (`[zc]`), `RECOMP_DL_ATTRS=<hex>` (`[da]`: one
line per draw with exactly those attributes, with the smallest positive and largest negative w), `[dl]` `zb`/`zmm`/`mz`/`mw`
(composite z and w rows), `HPPOA_POKE8_AT=<ms>` (pokes only after that much wall time, e.g. TimeFactor `0x3676D8` = 0 for a
still scene; the camera keeps moving).

## 1e. Performance: shader compile stalls (play test 1, issue 3)

The owner's play test (60 fps) showed drops to 51 fps with single frames of 170-600 ms, each logged as
`[hitch] ... 1 programs compiled (172 ms)`: a program (vertex program + combiner state) seen for the first time is compiled on
the render thread while the frame waits.

1. **Measured.** Route: `build/t/train_script.txt` plus the stick up again at 150 s and A at 175/180 s (title, New Game, train
   cutscene, corridor, Ron cutscene, luggage car), `HPPOA_FPS=60 RECOMP_FPS_LOG=1`, 200 s, fresh HOME (`build/t/base1`); then the
   same HOME again. First run: 26 programs compiled in play, **12.7 s of compiling**, 13 frames over 50 ms from compiles (worst
   5.5 s at the first corridor frame, 11 programs; then 350-600 ms each). Second run: `prewarm: 27 programs, 63 pipelines in 6 ms`,
   **0 compile hitches**: the existing cache (`progcache.bin` + `mtlpipes.bin` in the working directory, the player folder
   `.../player/` in the app; Metal's own cache keeps the compiled code) already works across runs. What is left in a warm run:
   the New Game load (about 100 frames over 25 ms while the level loads, documented in step 3) and two boot frames.
2. **Why a compile took 500 ms.** Timing each `newLibraryWithSource` (and an offline harness compiling the dumped MSL): vertex
   programs 15 ms, combiner (fragment) shaders 340-590 ms, even a trivial 5 KB one. The cost is the standard library's texture
   `sample()` templates at the newest MSL version, which Metal picks by default (4.1 on this macOS). Per fragment shader:
   4.1 540 ms, 4.0 190, 3.2 170, 3.1 63, **3.0 43 ms**, 2.4 33 ms. All dumped shaders compile at 3.0 and 2.4.
3. **Fix (`nv2a_metal.m`): compile with MSL 3.0** (`MTLCompileOptions.languageVersion`; `RECOMP_MTL_LANG=<major>.<minor>`
   picks another, `0` Metal's default; the log says `shader language 3.0`). Same math mode (safe) and invariance options; the
   generated shaders need nothing newer. The hitch line now also counts pipeline creations (`N pipelines (ms)`), which the
   compile time did not include.
4. **Result (first run, fresh HOME, same route):** compiles 45-60 ms a program + 8-30 ms a pipeline; frames over 50 ms from
   compiles 11, worst **176 ms** (was 5,553), the rest 57-90 ms (were 350-600); 12 compile frames over 50 ms add up to 603 ms
   (were 12,733). Second run: 0 compile hitches, prewarm 6 ms. Dormitory (`HPPOA_START_CHAPTER=1`, fresh HOME): 1 compile
   frame (61 ms + 25 ms pipeline). Every 10 s window of play outside the load: flip p50 16.7 ms, p99 16.9-17.9 ms.
   Image check: the pause screen over the dormitory at MSL 3.0 and 4.1 look the same; the remaining pixel differences (mostly
   ±1) are no larger than between two runs at the same version (the scene's lights move).
5. **CPU (`sample`, 10 s in the luggage car, warm, 60 fps, 530 draws a frame):** game thread (lifted code) ~66 % busy, the rest
   waiting on the GPU fence; pushbuffer thread ~53 % (`lit_color`, the CPU vertex lighting, ~12 % of it; `mtl_draw_raw` ~10 %);
   GPU p50 7 ms, p99 12-17 ms. No thread is the limit at 60 fps here, so nothing else was changed.

Not done: compiling a first-seen program off the render thread (draws would have to be skipped or wait; no such pattern in this
renderer). A first-seen program still costs one 50-90 ms frame, once per player.

## 1. Phase 2: the to-do (items 1 done above)

1. Lift: `engine/tools/regen.sh` with this XBE (`LIFT_ONLY=1` to skip the disassembly). Seeds are empty; start from the facts in
   `docs/upstream-patches/README.md` (3 seeds both references agree on: `0x000777F0`, `0x000CBEE0`, plus `0x00243390` from
   HP3-Recompiled, `0x000FCC70` from HP-POA).
2. XDK 5849 glue by signature (`tools/sigmatch.py`, `tools/xdis.py`): see the checklist. HP3-Recompiled's `docs/bringup.md` lists
   PoA USA addresses found by matching HP1's XDK 5659 library into this one (`D3D__pDevice` `0x00275FE8`, ...), to be
   verified here, not trusted.
3. Boot blockers one at a time, then renderer gaps, then input, movies (`.mad`, EA's own player), then the user's play tests.
4. Per the repository rule: an engine fix found here that the other Xbox ports lack is backported (NFSU1, NFSU2, HP1, CoS).

## CoS hooks to re-find for HP3

PoA status (2026-11-24): **1-5 and 7 installed** (step 2, table in 1b); 6 done in step 1; 8 not needed so far (`.mad` movies play as
textures, no overlay call); 9 and 10 not seen (no DVD-race or loader stall up to the train); 11, 12, 15, 16 not started (features/dev tools);
13, 14 CoS-only (PoA's crash guards are other functions, `docs/hp-poa-hooks.md`); 17 not needed (no such patch found yet); 18 replaced by
PoA's 86 seeds. The CoS rows below keep CoS's addresses.


Everything below was in `engine/src/recomp_manual.c` for Chamber of Secrets (XDK 5028, NTSC); the addresses are **CoS's and mean
nothing in this executable**. The old file is in hpcos-xbox-port at `8342d21`. A hook is a strong `void sub_XXXXXXXX(void)` in
`recomp_manual.c` that calls the lifted original as `sub_XXXXXXXX_gen`. Order of need: boot (1-6), input (7), movies (8),
the rest.

| # | Name | Purpose | CoS address(es) |
|---|---|---|---|
| 1 | CRT `memmove` (memcpy shares the body) | the MSVC CRT's backward-copy and tail paths use jump tables indexed by a negative register that the lifter cannot follow: copies lost their tail. Replaced by the host `memmove` | `0x127A00` (a second copy at `0x2A9450` was not hooked) |
| 2 | `D3D_BlockOnTime` (fence wait) | with the pushbuffer executor, hands the real GPU semaphore to it once and runs the original wait; without it, reports the GPU caught up. Device pointer global, semaphore at device `+0x30`, write sequence at `+0x2C` | `0x1359D0`; device pointer `0x140888` |
| 3 | D3D vblank ISR and PGRAPH handler | run at DISPATCH_LEVEL (raise the IRQL, release the guest lock around it) so the DPC and a busy-wait never interleave (a flip stayed pending forever); report flips retired (`+0x1BC`); the vblank taken; the PGRAPH trap lock; **frame-rate uncap** (`HPPOA_FPS=60` rewrites the flip interval in the trap word) | ISR `0x139080`, PGRAPH `0x139690`; D3D DPC `0x1398D0` (seeded, not hooked) |
| 4 | DSOUND DSP command post | registers the DSP's completion word (`scratch + 0x810`) with the runtime's ack list, then runs the original | `0x1465D2` |
| 5 | AC'97 channel reset | the compiler hoisted the NABM register load out of the wait loop: reimplemented with the reset completing at once; uses the NABM table, the lock helpers and the re-arm | `0x14CBAD`; table `0x14DB74`; helpers `0x144510`, `0x144543`, `0x14C9B7` |
| 6 | DSOUND tag range | MmGetPhysicalAddress callers inside DSOUND get tagged addresses (now `HPPOA_DSOUND_LO/SIZE` in `region_profile.h`) | `0x1444E0` + 40,740 (done for HP3: `0x2B66E0` + 37,428) |
| 7 | XInput replacements (direct host pad, `HPPOA_XINPUT_USB=1` keeps XAPI's) | XInitDevices (+ 250 ms poller reporting insertions), XInputOpen, Close, GetCapabilities, GetState (packet number on change, optional pad replay), SetState (rumble to the host pad, completes at once). Device-type record: gamepad type, begin/end of the linked types | `0x176729`, `0x1771FB`, `0x177251`, `0x17725D`, `0x17743B`, `0x1774A7`; types `0x176294`, `0x176264`..`0x17626C` |
| 8 | Movie overlay (UpdateOverlay, EnableOverlay) and GetDisplayFieldStatus | YUY2 frames converted to RGB and drawn by the Metal movie layer; the XMV player polled the field status in a busy loop (waits for the next vblank when called from the decoder). **For HP3: movies are `.mad` through EA's own player, no XMV section**: re-find the overlay calls and the player | `0x136520`, `0x1366A0`, `0x133790` (return address `0x14F063`) |
| 9 | File-cache hold (the DVD latency race) | worker thread waits while the main thread is between allocating a request and hooking it (200 ms at most); the worker's wait for work becomes 50 ms waits; the status polls end the hold | queue `0x1133F0`, alloc `0x113090`, hook `0x112E20`, worker loop lock `0x114EB0` (return `0x1125DE`), wait `0x114A10` (return `0x11263B`, calls `0x10E5D2`), take `0x114BF0`, push `0x114B60`, polls `0x1129D0`, `0x112BA0`, `0x1138B0`; worker thread `0x1146C0`, loop `0x1125A0`, chained reader `0x1136C0`. Check first whether HP3 has the race |
| 10 | Loader callbacks' order | the boot and level loaders' callbacks published their state before their result; same work, state last. 12 callbacks | `0x163E0`, `0x16410`, `0x16440`, `0x16460`, `0x16490`, `0x164C0`, `0x164E0`, `0x16510`, `0x16530`, `0x16550`, `0x16570`, `0x165A0`; states `0x1BE8C8`, `0x436F2C`; results `0x436F40`..`0x436F60`. Check first whether HP3 stalls at boot |
| 11 | Logo movie skip | the movie player returns at once for the logo names (Settings > Tweaks) | `0x18D40` |
| 12 | Cutscene fast-forward (Start/Back in a cutscene) | vblank and APU clock run 6x (`xbox_vblank_turbo`, `nv2a_mtl_turbo`), muted, render and Swap only every 250 ms; the press is hidden from the game; stops when the camera mode leaves 7 | camera mode `0x50C2E4`; render `0x35190` (prep `0x38C80`), Swap `0x134D70`; pad read in GetState. Needs the cutscene state of HP3 (HP3-Recompiled has not found it either) |
| 13 | Spawner clean-up guard | removing a live entity twice corrupted the entity list | `0xFC830` (entity list `0x5242FC`, pool `0x524304`) |
| 14 | Diagnostics | entity list check before the world's walk, with the last removals (`0x42D60`, `0x42FF0`); the wild-call context printed the entity pool (the ICALL log is now generic); save-check logging (`0x16D40`, `0x18AD0`, `0x18570`); DS `GetCurrentPosition` trace (`0x145337`) | |
| 15 | Save slot kept in play | the game systems' init reset the save slot to -1 after End Day: saving did nothing | `0x4ABF0`; slot `0x51DF64` |
| 16 | Dev tools | warp to a level (`HPPOA_WARP`: `0x4AD50`, level table `0x1E3DE8`, start `0x527DFC`, `0x1EF704`), save now (`0x4DD30`), player position log and teleport (`0x53710`, player entity `0x530408`), differential capture around it (`tools/difftest.py`), pause-menu debug rows (`0x527F26`), pad replay, `.data` snapshots (`0x18E680`..`0x54EDB0`), `HPPOA_PEEK` in `xbox_after_vblank_dpc` | as listed |
| 17 | Lifted-code patch | `sub_000C0001` starts with a branch on its caller's flags (the lifter gave `_flags = 0`): `engine/tools/patch_gen.py` | `0xC0001` (the null check after `0xBFFFF`) |
| 18 | Seeds | 177 hidden-function seeds from HP-COS in `engine/config/seed_functions.json` | now `[]` |

## Backport candidates (runtime and toolkit fixes found here)

Not applied to the other repos (rule: an engine fix found in one Xbox port goes to NFSU1, NFSU2, HP1, CoS):

1. **Disassembler: split call targets called from data-decoded bodies** (`engine/xboxrecomp/tools/disasm/functions.py`,
   `_prune_data_call_targets`, with two tests in `test_prune_data_call_targets.py`). Any title whose library sections end in tables
   (XGRPH here) can have a fake function cut a real one at a mid-instruction "call". Check each port: relift and diff `functions.json`
   (PoA: exactly one function start removed). The mid-instruction scan used here: start addresses inside a decoded instruction of the
   preceding function.
2. **Kernel: `XC_FACTORY_AV_REGION` (0x103) answers NTSC-M `0x00400100`** (`kernel_xbox.c`). It was 0 in every port; any title that
   calls `XGetVideoStandard` saw "no standard" (PoA: PAL frame floor). Harmless for titles that never read it.
3. **Check, not a fix yet: NV2A lines are one host pixel wide in the Metal path** (`nv2a_metal.m`, `prim_indices` maps them to
   `MTLPrimitiveTypeLine`). Upscaled, a console line is 3-4 host pixels; a title that masks with lines into an upscaled target gets
   gaps. PoA's one line (the shadow fade mask, 1d item 1) goes into a 16x32 target kept at 1x vertically, so it is whole.
4. **Method: game clock against wall time** (`HPPOA_CLOCK_LOG`, the frame timer hook in `recomp_manual.c`). Each port's 60 fps
   option should be checked the same way (game ms / wall ms per window, ticks without a frame).
5. **Metal: compile with MSL 3.0, not the OS default** (`nv2a_metal.m`, `s_copts.languageVersion`; section 1e). On macOS 27 a
   combiner shader takes ~540 ms at the default 4.1 and ~43 ms at 3.0; every port shares this renderer and the same cold-compile
   stall (NFSU1, NFSU2, HP1, CoS). Copy the block and the hitch line's pipeline count; check that `shader language 3.0` is
   logged and no `shader failed` lines appear over each port's routes.

6. **Missed functions referenced only by an immediate** (`tools/immscan.py <repo>`, read-only on another port): PoA had 8
   (#7, the trunk). Run on the other ports' current `functions.json` (2026-11-24): HP1 0 and CoS 0 hits; NFSU2 3
   (`0x4C25D`, `0x10C2C9`, `0x119144`) and NFSU1 about 30 at `0x1BDxxx..0x1BE8xx` (pushed from `0x30xxx..0x4Cxxx`, likely
   string or table pointers in `.text`): to be checked with `seedcheck.py` there before seeding.
7. **Learned stick range** (`stick_deadzone.h`, `stick_range_at`; 1f item 2) for every port with CoS's fixed 0.65.
8. **Executor: inline/`SET_VERTEX` batches in fixed-function mode are transformed; `SET_VERTEX_DATA4F` diffuse; 2-vertex
   lines and 1-vertex points kept** (`kernel/nv2a_pb_exec.c`: `batch_is_ffp`, `imm_vertex_method`, `prim_min_vertices`;
   commit `006d8a3`, 1d item 1). Every port's executor has the old `!s_gpu.inline_active` rule (NFSU2, CoS checked). Any title that
   draws world-space quads, particles or masks with D3D's immediate mode (`Begin`/`SetVertexData4f`/`End`) in fixed-function mode
   lost them (drawn as pixel coordinates, mostly off screen). Before backporting, run each port's route with `RECOMP_INLINE_FF=0`
   and without and compare: 2D drawn that way in fixed-function mode with a stale composite matrix would move.
9. **`tools/difftest.py`: low 64 KB mapped, `DIFF_BREAK`** (1d item 4). The copy in hpcos-xbox-port stops at the first SEH
   prologue (`mov eax, fs:[0]` reads address 0 in Unicorn's 32-bit mode); copy the mapping there and to any port that has
   the tool.
10. **Renderer: P8 textures rewritten in place stay stale** (`nv2a_metal.m` `tex_get`, 1g item 1;
   `build/t/evidence/p8_texture_hash.patch`, not applied yet). `tex_bytes()` counts 4 bytes a texel for P8, and the
   sampled hash reads 256 dwords over that size, so a P8 texture gets 64 samples (every 2048 bytes) and a new line of
   text in a reused texture is missed. Every port has the same `bytes_hash`/`tex_bytes` (NFSU1, NFSU2, HP1, CoS): check
   their text and subtitle textures with `RECOMP_TEX_CHANGES=1` after applying it.

10. **Metal: polygon offset slope scaled by the render scale** (`nv2a_metal.m`, `apply_dynamic`'s `slope_k`; 1d item 5).
    Every port renders upscaled with the same backend, so every decal with a slope-scaled offset got 1/scale of it.
11. **Metal: sphere, normal and reflection map texgen** (`nv2a_metal_msl.c` fixed-function vertex shader; 1d item 5).
    Any port that logs `texgen 8512/8511/2402 ... not generated` (NFSU2 car reflections?) gets them.
12. **Not to backport as is: the skin second-layer pass-through** (`skin_second_layer`) is PoA's data; the matcher is narrow
    but other titles may draw a texture over itself by modulate 2x on purpose.

## 2. Tools

Dev scripts (inherited): `tools/stubcheck.py` (is any empty stub real code), `tools/run_test.sh <name> <secs> <shot times>`
(a scripted run with screenshots; `HPPOA_BIN=` runs another binary), `tools/sheet.sh`, `tools/seedcheck.py` (is every seed an instruction start), `tools/addseed.py`,
`tools/relift.sh`, `tools/immscan.py` (functions referenced only by an immediate), `tools/play_dev.sh` (play in a window, own HOME), `tools/sigmatch.py`, `tools/xdis.py`. Configure
(as `tools/build_release.sh` does): `engine/tools/regen.sh`, then CMake with `-DNFSU2_GEN_DIR=engine/gen`, target `hppoa_recomp`.

## 3. Not verified / open issues (after step 3)

- **Play beyond Night 0's dormitory**: spells, fights, the Dementor scene, later chapters not tried.
- **Load spike of the game clock** (step 3, item 3): 80-150 s of game time in about a second while the train level loads, at 30
  and 60 fps; no visible effect found. HP-POA-Xbox-Recomp's loading-loop pacing is the candidate fix if one shows.
- **60 fps**: a cutscene with a fixed clock step was not met on the route (the split is untested); lip-sync follow and the Carpe
  Retractum ledge step (HP-POA-Xbox-Recomp) not ported.
- **Skip Cutscenes** (1h): the forced end's exact loop in play test 3 (inhogexcam_01 replaying) was not reproduced in a
  staged run; the fast-forward does not use that path. A skippable cutscene (`+0x99` set) was not met in the runs, so
  the game's own skip path is unchanged but untested this round.
- **Load Game screen**: the slot names showed a moment after the screen (one shot had an empty list); not checked further.
- **NV2A lines** one host pixel wide (step 3, item 8); PoA's one line (the shadow fade mask) lands whole (1d item 1).
- **Play test 1 open items** (1d): Malfoy's orange/blinking face (not reproduced; the dining car not reached by script). The Options "(" glyphs are fixed by a
  hook (1d item 4); whether the console shows them too is not checked.
- **Renderer** not checked: fog, W-buffer bit of CONTROL0, post-effect thresholds (CoS's).
- **Play test 3** (1j): the rug cut is shown fixed in a same-frame split; the DADA plank patch is not reached (same cause expected).
- **Play test 3** (1h): character shadow strength vs the console; AO units (near 10 / far 10010) left for the owner; specular lighting missing.
- **Host pad in scripted tests**: a controller connected to the Mac also drives the overlay during `tools/run_test.sh` runs (one run's
  settings were changed that way); check `[overlay] open` in a test log.
- **Rumble**: XInputSetState goes to the host pad; not tested with a real pad.
- Run it: `tools/play_dev.sh` (own HOME in `build/play`). Scripted: `RECOMP_PAD_SCRIPT="10000:start:200,..." tools/run_test.sh <name> <secs> <shots>`
  then `tools/sheet.sh build/t/<name>`.

## 4. Release 0.1.0 (2026-11-25)

After the owner's play test 4 (no issues), at `0961689` plus the release commits. Evidence in the gitignored
`build/rel_evidence/`.

- Kit audit: every installer/app name, `HPPOA_*` variable, player folder, bundle id (`io.github.sanyokkua.hppoa`,
  `.installer`), DMG name and disc pattern is this game's; no `hpcos`/`HPCOS`/`xmv` left in `installer/`, `mac/`, `tools/`,
  `tests/`. `tools/publish_check.sh` now also knows `.mus .big .mad .ids .loc .pfr .xfn .btf`.
- `tools/smoke.sh`: OK, 75 s, last frame mean 75, overlay opened and closed (`smoke.txt`).
- `tools/make_dmg.sh`: `Harry-Potter-Prisoner-of-Azkaban-0.1.0.dmg`, 5,286,649 bytes; `tests/dmg_no_game_code.sh` PASS
  (2,899 disc files, 64 code windows, 3,069 KB of executable sections). Negative check: a DMG with 256 KB from the middle of
  `.text` fails (`6 windows of the program's code`, exit 1; `dmg_negative.txt`).
- Clean install from the mounted DMG, throwaway HOME, `HPPOA_NO_UI=1 HPPOA_IMPORT_ISO=<the USA iso> HPPOA_INSTALL_DIR=<tmp>`:
  **3 min 13 s** on the M1 Pro (import 3,228 entries, 1,947.7 MiB, "pristine .text"; lift; build; icon; app). The
  installed app run from its player folder with a real controller connected (`[apple] controller added`, `controller ports
  now 1`): title, Load Game, the owner's `SaveGame-1` copy (sixth floor), the library walk, spells, 165 s, clean exit, no
  ICALL miss (`installed_play.log`, `installed_play_sheet.png`).
- Screenshots (`docs/screenshots/`, 33-101 KB): scripted runs with `tools/run_test.sh` and the overlay's shot script. The
  shot is the stretched drawable (2560x1440 in a window) whatever Picture says, so the game shots are resampled to 1280x960
  (4:3) and the settings shot kept at 1280x720. The library route (`lib_route5`) only works at 30 fps and with the logos: at
  60 fps the same pad times walk Harry up to the seventh floor.

## Pitfalls (each cost a run)

- **A seed inside an instruction** cuts a function in two; its switch cases become fake functions without their tables. Run
  `tools/seedcheck.py`.
- **The console's timing is a hidden contract.** The file-cache race never showed on the Xbox because the DVD was slow.
- **XDK versions differ** in what the driver writes: XDK 5028 leaves transform constants to the chip. Do not assume a newer XDK's
  behaviour.
- **Sampling a video at 1 fps** hides which part of a scene is slow; use 3 fps around the moment.
- **zsh** reads `$t:shot` as a variable modifier: write `${t}` in generated scripts.
- **Frame dumps at 4x are 11-20 MB each:** dump sparsely and delete them.
- **An empty stub is silent.** The lifter's "not detected" stubs return at once. On a shared exit that another function jumps to,
  the caller's registers and stack come back wrong and the game breaks much later, somewhere else. Run `tools/stubcheck.py`
  after every lift, and repeat after seeding (a seed can split a function).
- **A warp without a start point** (`HPPOA_WARP=30`) can leave Harry outside every room: only objects are drawn. Give one
  (`HPPOA_WARP=30:0`).
- **SDL from two threads.** sdl2-compat's pad list (`SDL_NumJoysticks`) has no lock. Only the thread that pumps SDL's events
  may open, close or enumerate pads; other threads read open ones.

### 1g addendum: P8 hash applied (b010ad5)
The `p8_texture_hash.patch` from 1g is applied on main as b010ad5 (`bytes_hash_all` for P8 textures, w*h bytes). Verified:
`build/t/subfix` (train script, 60 fps, shots 76-96 s) shows every line whole and centred: "Blimey!...", "Imagine it...",
"Crookshanks! No!", "Can't you keep that monster...", "You don't want to wake up Professor Lupin...", "I'll help you
find Scabbers". 0 ICALL misses.
