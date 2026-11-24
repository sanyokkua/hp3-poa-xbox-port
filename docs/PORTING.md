# How the port was made

A log of the steps, the decisions and what went wrong. This port starts from the tree of the Harry Potter and the Chamber of Secrets
port ([hpcos-xbox-port](https://github.com/sanyokkua/hpcos-xbox-port) at `8342d21`, v0.1.0 plus later commits), which is itself the
Sorcerer's Stone port's tree ([hp1-xbox-port](https://github.com/sanyokkua/hp1-xbox-port)) on the NFS Underground 2 port's engine
([nfsu2-xbox-port](https://github.com/sanyokkua/nfsu2-xbox-port)). Only the platform stack (Xbox kernel, D3D8, NV2A, DSOUND) is
shared with those games. Per the HP3-Recompiled notes, this game is **EA UK's engine** (the same family as the PS2 Chamber of
Secrets), not Eurocom's (CoS) and not Warthog's (HP1).

Status: **step 3 done: boots to the train level (Flipendo tutorial) with sound (music, speech, effects measured), the game clock at console speed (30 fps), a 60 fps option, Skip Logo Movies and Skip Cutscenes (overlay rows). Saves and later levels not tried yet.**

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

TODO HP3 icon: `mac/make_icon.sh` and the installer still build the icon from the disc's title image as for CoS; check that
this game's `$$XTIMAGE` / title image gives a good one, or draw one.

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
   `sub_00062170` and sets the menu's `CutsceneSkipped`. The train's first cutscene is not skippable (A: native called,
   nothing happens). Start or Back while `+0x8C` is set now asks for a forced skip, run on the game thread at the next
   clock tick; the press is hidden from the game while the cutscene plays (Start would pause). Evidence (`build/t/cs3`):
   `[cutscene] 8220EA14 skipped (skippable 0): ended` at the Start press, Harry in the corridor with the HUD 2 s later,
   play and the pause menu work after it. `HPPOA_CUTSCENE_LOG=1` logs the manager's state and the native.

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
   (prim 6); no line draw was found to test a fix against.

## 1d. Play test 1 fixes (2026-11-24, `.wt/PLAYTEST1.md`)

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
   `MTLPrimitiveTypeLine`). Upscaled, a console line is 3-4 host pixels; a title that masks with lines (HP3-Recompiled: PoA's shadow
   fade mask) gets gaps. Count line draws with `RECOMP_DRAW_LOG` in each port before widening them (PoA's tested scenes have none).
4. **Method: game clock against wall time** (`HPPOA_CLOCK_LOG`, the frame timer hook in `recomp_manual.c`). Each port's 60 fps
   option should be checked the same way (game ms / wall ms per window, ticks without a frame).

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
- **Skip Cutscenes** forces the skip on cutscenes the title marks unskippable (the train's first); only that one was tested.
  Its last subtitle line stays on screen for about a second after the skip.
- **Load Game screen**: the slot names showed a moment after the screen (one shot had an empty list); not checked further.
- **NV2A lines** one host pixel wide (step 3, item 8); no line draws in the tested scenes.
- **Renderer** not checked: fog, W-buffer bit of CONTROL0, post-effect thresholds (CoS's).
- **Host pad in scripted tests**: a controller connected to the Mac also drives the overlay during `tools/run_test.sh` runs (one run's
  settings were changed that way); check `[overlay] open` in a test log.
- **Rumble**: XInputSetState goes to the host pad; not tested with a real pad.
- Run it: `tools/play_dev.sh` (own HOME in `build/play`). Scripted: `RECOMP_PAD_SCRIPT="10000:start:200,..." tools/run_test.sh <name> <secs> <shots>`
  then `tools/sheet.sh build/t/<name>`.

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
