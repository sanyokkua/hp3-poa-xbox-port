# How the port was made

A log of the steps, the decisions and what went wrong. This port starts from the tree of the Harry Potter and the Chamber of Secrets
port ([hpcos-xbox-port](https://github.com/sanyokkua/hpcos-xbox-port) at `8342d21`, v0.1.0 plus later commits), which is itself the
Sorcerer's Stone port's tree ([hp1-xbox-port](https://github.com/sanyokkua/hp1-xbox-port)) on the NFS Underground 2 port's engine
([nfsu2-xbox-port](https://github.com/sanyokkua/nfsu2-xbox-port)). Only the platform stack (Xbox kernel, D3D8, NV2A, DSOUND) is
shared with those games. Per the HP3-Recompiled notes, this game is **EA UK's engine** (the same family as the PS2 Chamber of
Secrets), not Eurocom's (CoS) and not Warthog's (HP1).

Status: **step 1 done (lifted, 0 stubs on real code, builds, boots to the first XDK blocker: the AC'97 reset spin in DSOUND); no frame is drawn yet.**

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

Next (not done): the XDK glue of the CoS list for these addresses. Known now: `D3D_BlockOnTime 0x0026C540`, device pointer `0x00275FE8`,
fence `+0x2C`/`+0x30`, vblank ISR `0x00269C90`, XInput `0x002BFC0C..0x002C0C19`, DSOUND code `0x2B66E0` + 37,428 (already in `region_profile.h`),
AC'97 reset `0x2BE412`. All hooks of the references with their meaning for us: `docs/hp-poa-hooks.md`.

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

## 2. Tools

Dev scripts (inherited): `tools/stubcheck.py` (is any empty stub real code), `tools/run_test.sh <name> <secs> <shot times>`
(a scripted run with screenshots), `tools/sheet.sh`, `tools/seedcheck.py` (is every seed an instruction start), `tools/addseed.py`,
`tools/relift.sh`, `tools/play_dev.sh` (play in a window, own HOME), `tools/sigmatch.py`, `tools/xdis.py`. Configure
(as `tools/build_release.sh` does): `engine/tools/regen.sh`, then CMake with `-DNFSU2_GEN_DIR=engine/gen`, target `hppoa_recomp`.

## 3. Not verified

Everything about running the game. Nothing was lifted, compiled or run in phase 1; `engine/src/recomp_manual.c` and
`hppoa_options.c` were syntax-checked only, and `mac/xbe_check.py` was run on the disc's `default.xbe` (OK, pristine `.text`).

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
