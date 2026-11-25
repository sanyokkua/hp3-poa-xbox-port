# Hooks in the two Windows references (inventory for the next phase)

Read from `upstream/hp-poa` (MiddleDucky/HP-POA-Xbox-Recomp, `2c2fb7b`) and `upstream/hp3-recompiled`
(Barthes-Recompiled/HP3-Recompiled, `ecd59ad`), both MIT. Facts only; their runtime is not copied. All addresses are
this disc's (USA, XDK 5849); both references use the same XBE, so no address needs mapping. A hook there is a function
`sub_XXXXXXXX` in `port/src/*.c` that wraps the lifted body `sub_XXXXXXXX_gen` (they rename the body by hand in
`gen/`; here the mechanism is `engine/src/recomp_manual.c` plus `--exclude-manual`).

"Ours?" answers whether this runtime (D3D8 through the NV2A push buffer to Metal, XInput and DSOUND in
`engine/xboxrecomp`) likely needs it: **yes** (needed to boot or play), **maybe** (depends on what boot shows),
**no** (their renderer or Windows layer only), **opt** (a feature or diagnostic, not needed to play).

## 1. What HP-POA's `recomp_manual.c` / `recomp_extra.c` contain

Only two entries in `recomp_lookup_manual`, both for functions the lifter missed (we seed them instead, see below):

| Address | File | What | Category | Ours? |
|---|---|---|---|---|
| `sub_000FCC70` | recomp_manual.c | 5-byte thunk `jmp sub_000FC200`, reached by a function pointer (enemy state table); without it a troll could not be hit | game fix (missed function) | seeded (already a detected start in our disasm) |
| `sub_000777F0` | recomp_extra.c | hand-lifted 631-byte thiscall (character interaction handler, `ret 12`), lives after a switch table | game fix (missed function) | seeded; the full lift gives it |

`0x000CBEE0` (method stored by a static initializer, table at `0x2FE200`) and `0x00243390` (after int3 padding) are
only in `icall_seeds.json`. All four are in our `engine/config/seed_functions.json`.

## 2. XDK glue and boot (the hooks the next phase should look at first)

| Address | File (ref) | What | Category | Ours? |
|---|---|---|---|---|
| `0x0026C540` (`D3D_BlockOnTime`) | main.c (both) | GPU fence: `xbox_Nv2aMirrorFence(0x00275FE8, 0x2C, 0x30)`: device pointer global `0x275FE8`, write sequence at device `+0x2C`, semaphore `+0x30`. This is CoS hook 2 for HP3 | XDK glue / timing | yes |
| `0x00275FE8` / `0x00275FF0` | bringup.md | `D3D__pDevice` / static CDevice (`D3D__DirtyFlags 0x275B48`, `TextureState 0x275B50`, `RenderState 0x275D50`) | XDK glue (data) | yes (CoS hook 2 and 3 offsets) |
| `0x00269C90` | bringup.md "Display timing" | D3D vblank ISR (increments device `+0x1DE8`); `BlockUntilVerticalBlank 0x00267470` waits on the KEVENT at `+0x1DBC` (clears `+0x1DC0`). `SetVerticalBlankCallback`, `GetDisplayFieldStatus` and `GetOverlayUpdateStatus` are not linked in PoA | XDK glue / timing | yes (CoS hook 3; PGRAPH handler address not named by them) |
| `0x0026A610` (`Swap`), `0x0026A2D0`, `0x0026A050`, `0x0026A0E0` | bringup.md "Boot fix 1" | the title swaps through `sub_000345B0`: BlockOnFence, Swap(BYPASSCOPY), copy quad `sub_00034110`, Swap(FINISH), InsertFence. Device `SwapEffect 3` (COPY), `MultiSampleType 0x11`, so CDevice::Init (`0x0026B380`) sets `+8` bit `0x4000` and Swap takes the copy path. Their HLE had to copy this; ours runs the original | D3D renderer / game flow | maybe (the push buffer path should do it itself; check white frames) |
| `0x002B874A` (`DirectSoundCreate`), `0x0014ECA5` / `sub_0014EC90` | bringup.md, hle_dsound.c | EA's audio init creates DirectSound and downloads the DSP image (`0x002DCD28`, `0x3360` bytes). On the emulated APU it dies at `[APU] MMIO decode fail offset=0x2000` (read of `0xFE802000`) from `0x002B8761` (DSOUND). They replaced all of DirectSound with a host mixer | audio | maybe (our APU emulation may carry it; else this is the first blocker) |
| `0x002B6962` `0x002B6978` `0x002B7508` `0x002B752F` `0x002B754F` `0x002B756B` `0x002B7587` `0x002B75A3` `0x002B75DF` `0x002B7603` `0x002B761F` `0x002B763F` `0x002B765B` `0x002B767B` `0x002B7A60` `0x002B7A7C` `0x002B7FAE` `0x002B8560` `0x002B8584` `0x002B874A` `0x002B8791` | hle_dsound.c (hp-poa) | the 21 DSOUND entry points the title calls (44 call sites, all from EA's audio layer): Release x2, DownloadEffectsImage, SetMixBinHeadroom, SetEG, SetFilter, SetMixBinVolumes (0x2B7587, not SetVolume), Play, PauseEx, Pause, SetLoopRegion, GetStatus, GetCurrentPosition, SetCurrentPosition, SetFrequency, SetMixBins, SetBufferData, CreateSoundBuffer, SetI3DL2Listener, DirectSoundCreate, DirectSoundCreateBuffer. PoA creates no streams (181 buffers: 180 mono SFX, one 6-channel movie buffer polled 8424 times via GetCurrentPosition). `0x002B9760` is CMcpxAPU's deleting destructor | audio | maybe (only if APU emulation fails; names useful for tracing) |
| `0x00247768` (`timeSetEvent`), `0x00222B90`, `0x00222B10`, `0x00222C40`, `0x00247580`, `0x00222570`, `0x0003C2C58` event | bringup.md "Boot fix 2" | the game clock tick thread: `sub_00222B90` re-arms itself through `timeSetEvent` (1 ms relative), XAPI timer thread `0x247580` waits on 63 KTIMERs. The first-movie hang was a lost KTIMER expiry in the runtime's `kernel_set_timer` (reset + arm must be one step under the lock; expiry signals before its DPC). Fixed in their toolkit copy `kernel_bridge.c` | kernel / timing | check `engine/xboxrecomp/src/kernel/kernel_bridge.c` has the same race (no PoA address involved) |
| `0x000117AB` (`XapiInitProcess`), `0x0001179A`, `0x000116E0`, `0x00012BCD`, `0x00012869`, `0x00011735` (`XapiBootToDash`) | hle_xapi.c, bringup.md | PoA's XDK 5849 has no disc authentication (HP1's `0x155251` hook is `#if 0` for PoA). It maps `\Device\Harddisk0\partition1\`, checks cache and utility drives and a kernel version, each failing to BootToDash | XDK glue | no hook, but watch these four checks if boot ends in a launch of the dashboard |
| `0x00195129` | hle_skip.c (hp3) | XAPI `QueryPerformanceCounter` (rdtsc, `ret 4`), the game clock source through `sub_001A7610`; scaled in their cutscene fast-forward | timing | opt (fast-forward only); plain rdtsc must work |
| `0x00195024` | bringup.md | `XGetVideoStandard`, read once at renderer init (`0x000349E5`); decides the 33 ms (NTSC) or 40 ms clock floor | timing | no hook (factory AV region answered by the kernel) |
| `0x002BFC0C` XInitDevices, `0x002C096C` XInputOpen, `0x002C09C2` XInputClose, `0x002C09CE` XInputGetCapabilities, `0x002C0BA6` XInputGetState, `0x002C0C19` XInputSetState, `0x002C08FF` XGetDeviceChanges, `0x002C08DD` XGetDevices | hle_input.c (both) | direct host pad instead of the emulated OHCI USB path (the USB route delivered sticks late); XGetDevices/XGetDeviceChanges keep XAPI's code and read the type record. Device type table `0x002BF924..0x002BF92C`, gamepad type `0x002BF9F4` | input | yes (CoS hook 7 with these addresses) |
| `0x001A7450` (this = `0x38BA6C`) | hle_debug.c / hle_clock.c (hp-poa), hle_video.c (hp3) | the frame timer: adds the real time, at least the floor at `+0x18` (33 ms NTSC), at most 100 ms. Not needed at 30 fps | timing | opt (60 fps) |

## 3. Everything else, by file

### hp-poa

| Address | File | What | Category | Ours? |
|---|---|---|---|---|
| `sub_00096520` | hle_clock.c | the +0x18 setter (game clock min step); halves it at 60 fps | timing | opt |
| `sub_00213E40`, `sub_001E7640` | hle_clock.c | the +0x14 setter and getter (fixed step a frame, set by cutscene scripts via `sub_000676A0`); halve it at 60 fps, getter answers the script's value | timing | opt |
| `sub_00225130`, `sub_0022EC00` | hle_loader.c | file interface slot `+0x40` (`0x2ED220`; one read per request slot `0x30D9F0..8`) traced; package stream task run again within a frame (1 MB level package took about 1 s, doorway into it showed nothing) | game fix / loading speed | maybe (check the CoS file-cache race analogue, hook 9) |
| `sub_001AC0A0` | hle_video.c | script `PlayMovie` (`<Movie>%s.mad`; names `eac_usa`, `wb`, `intro`, `attract`, `filmus`, `gallery`): logo skip by stopping the player with `sub_0005DC90` (player `[0x364120]`, made in `sub_0005DBE0`; `sub_0005DE00` asks if it is still going, `sub_001AC100` StopMovie) | game fix / movies | opt (logo skip); movies themselves are textures, no overlay |
| `sub_00034750` | hle_widescreen.c | renderer init (thiscall); writes 16:9 into the screen descriptor (`[0x30F948]+0x2E14`, aspect at `+0x34`) | D3D renderer | opt |
| `sub_000264F0`, `sub_00038E00`, `sub_000376E0`, `sub_000377B0`, `sub_000376B0`, `sub_000518D0`, `sub_00037000`(hp3) | hle_widescreen.c, hle_video.c | camera FOV and projection builders (widen, cutscene lens, shot turn); `0x2FAA78` is the portal walk's view x projection | D3D renderer / game fix | no (16:9 later) |
| `sub_0004E710`, `sub_00051A60`, `sub_00051810`, `sub_00034FF0`, `sub_0003A260`, `sub_0012F4C0` | hle_portals.c | portal walk view tests (draw every room), room-crossing load hold, stale material link skipped (crash in Night 2 lower dungeon), bad anim cache buffer left unfreed (crash on Hogsmeade loading) | game fix | maybe (the two crash guards, if the same crashes appear; they are original-game bugs hidden on the console) |
| `sub_00021A60` StartGame, `0001E980` EndGame, `0001FA80` GameFlow, `0001FBD0` MemCard, `00021E30` RequestSaveGameAvailability, `0001F6F0` PlayMovie, `0001F760` StopMovie, `00022020` updateoptions, `0001FF00` setoption, `0001E860` RenderGameWorld, `0001FB90` SkipCutscene, `0001E8F0` LoadTextIcons, `000204F0` ... (42 in all, 39 `apt_log` wrappers), `0001EE30`, `0001EE70`, `000A7020`, `0006D500`, `001AD3B0` | hle_quit.c | trace wrappers of the Apt (Flash) menu natives, plus the Quit-to-desktop and PC-Settings screen tracking | game / UI | no (`SkipCutscene 0x1FB90` is the likely cutscene-skip native, useful for CoS hook 12) |
| `sub_00020220`, `sub_00021C90` (PauseGame), `sub_00021D00` (UnpauseGame) | hle_quit.c, hle_pause.c (hp3) | FSCommands (table of 84 `{name, handler}` at `0x30EF28`, filled by `0x00256C50` via `sub_001AE6C0`); the pause flag | game / UI | opt |
| `sub_00020310` | hle_debug.c (hp3), hle_trainer.c | FSCommand `DebugMenu`: retail answers "false" (`0x2C8718`), "true" is `0x2C8730`, APT set-variable pointer `[0x38A2F8]`, name `0x2C9360`. Debug menu handlers write chapter/room/start point to `0x376B44/48/4C` | game fix / debug | opt (a warp: this is the HP3 equivalent of CoS hook 16) |
| `sub_001ABDD0`, `sub_002135F0` | hle_trainer.c | front-end pad poll (send the debug key); gait lookup (`+0x1090`) for responsive movement | input / game | no |
| `sub_000CDFB0`, `sub_00075F60`, `sub_000BA820`, `sub_00073C50`, `sub_000A2C00`, `sub_000FF330`, `sub_000FF420`, `sub_0013F8E5` | hle_debug.c | sneak step wraps and a guest-string helper; traces | game fix / debug | no |
| `sub_00070380`, `sub_00070230`, `sub_000A6E80`, `sub_000A7300`, `sub_0006FE40`, `sub_000A6B40`, `sub_00075EA0`, `sub_000AFC10` | hle_debug.c | speech channel start/cleanup, face, state trace (character state machine `+0xE78`), Carpe Retractum stick read; lip-sync follow | debug / game | no |
| `sub_00161E60`, `sub_001624A0`, `sub_001B2D50`, SFX hooks `0x162670`, `0x1629B0`, `0x162C10` | hle_dsound.c | Reparo spell used Glacius's sound category (disc data bug), sample made EA-XA; stream format check trace | game fix (audio) | no |
| `RECOMP_ICALL` and `recomp_icall_fail_log` | recomp_manual.c | the generic ICALL failure log (ours exists) | debug | no |

### hp3-recompiled (not in hp-poa)

| Address | File | What | Category | Ours? |
|---|---|---|---|---|
| `sub_00115500`, label `loc_00115678` | tools/patch_gen.py, region_check.c | `CGameChapter::Start`: after the label call `hppoa_RegionCheck(path)` (reads timer `0x38BA6C`, device `0x275FE8 == 0x275FF0`); read-only self-check | debug | no |
| `sub_00195129` | hle_skip.c | QPC scaled (see section 2) | timing | opt |
| `sub_001934C0`, `sub_0019D910` | hle_video.c | disabled: hp1 addresses (GetDisplayFieldStatus, GetOverlayUpdateStatus), not linked in PoA | n/a | no |
| `sub_001A5FD6`...`sub_001A87B8` (34 addresses) | hle_dsound.c | hp1's DirectSound stream and buffer wrappers; unbound leftovers, not PoA addresses | n/a | no |
| `sub_00155251` | hle_xapi.c | hp1's disc authentication; `#if 0`, PoA has none | n/a | no |

Everything in D3D (`hle_d3d8.cpp`, 3000+ lines, `renderer/`) is their HLE and does not apply: here the NV2A push buffer is
executed. Their D3D facts that still help, since the same push buffer arrives: `Begin 0x00269B20`, `End 0x00269B60`,
`SetVertexData2f 0x00269A80`, `SetVertexData4f 0x00269AC0` (32 callers) are used for immediate mode (an effect object
built at `sub_0004B190`, drawn by `sub_0004A3C0` and `sub_00049A70`; two quads at start-up); `SetVertexShader 0x00267090`
(20 sites), the embedded `xvs.1.1` source at `0x002CA8C0` assembled at renderer init by `sub_00030850` through XGRPH's
assembler `sub_0029D050` (declaration `0x002FA6B4`); `D3DDevice_Clear 0x00269050`; `D3DVertexBuffer_Lock2 0x00268E10`
(its NV097 `0x1710` push is the vertex cache invalidate, 16 per level load); pixel shaders are not called by the title.

## 4. Their PORT_CHANGES.md items with addresses

HP-POA (`port/PORT_CHANGES.md`), rows that carry an address:

| Item | Address(es) | Meaning for us |
|---|---|---|
| 60 fps: clock min step, cutscene fixed steps | `sub_00213E40`, `sub_001E7640`, tick `sub_001A7450` | later |
| True 16:9 | `sub_00034750` (screen descriptor aspect) | later |
| Quit prompt / PC settings | `sub_00021C90`, `sub_00021D00`, `sub_00020220`, 34 traced natives | our overlay covers settings |
| Trainer | `sub_00020310`, `sub_001ABDD0` | dev tool |
| FOV / cutscene lens | `sub_000376E0`, `sub_000377B0`, `sub_000376B0`, `sub_000518D0` | later |
| Every room beyond a doorway | `sub_00051DC0`, `sub_0004E710`, `sub_00051A60`, `sub_00051810` | later |
| Level packages streamed within a frame | `sub_0022EC00` | maybe |
| Quick step back through a doorway held | `sub_00034FF0` | game bug guard |
| Stale material entries skipped | `sub_0003A260` (link `0xB7F2E800`) | crash guard |
| Bad anim cache buffers unfreed | `sub_0012F4C0` (+0x1C buffer read `0xB33BBD2E`) | crash guard |
| Gait / responsive movement | `sub_002135F0`, player table `+0x1090` | no |
| Reparo sound | `sub_001B2D50`, `sub_00161E60` | no |
| Heap reclaim, Z: cache partition kept, factory game region from the XBE certificate, factory AV region (NTSC min step 33 ms) | toolkit `xbox_memory_layout.c`, `kernel_path.c`, `kernel_bridge.c`, `kernel_xbox.c` | check our kernel does the same |
| DirectSound host mixer | `hle_dsound.c` | see section 2 |

HP3-Recompiled (`port/PORT_CHANGES.md`): QPC `sub_00195129`; frame rates `sub_001A7450` (floor to 1 ms above 30 fps);
host vblank counter (unbound); console time zone from the PC (toolkit `kernel_xbox.c`, `ExQueryNonVolatileSetting`, whole
user block index 0xFF read by XAPI `GetTimeZoneInformation`); DirectSound off the emulated APU ("PoA's XDK DirectSound
crashes on the emulated APU").

## 5. Facts for booting, from their notes

- Their M2 baseline on the emulated APU: boots, opens `runtime.cfg`, `filesys.dat`, `data/packages/plfs/frontend-startup.pkg`,
  creates the D3D device (640x480, format 6, interval `0x80000002`), then crashes about 2 s in at DirectSoundCreate.
- With DSOUND replaced: `eac_usa.mad`, `wb.mad`, `poa_legal_eng.big`, `intro.mad` (about 65 s in the front end loads, title
  screen "Press START", attract `attract.mad` on idle). The first-movie hang was the lost KTIMER expiry (section 2).
  Movie loop `sub_0005E6D0` (at `0x5E6E4`) spins on the timer-callback dispatcher `sub_00147BC0`, which only leaves when a
  callback produces work: a stopped game clock (`0x3836C0` / `0x3836C4`) is a silent endless spin.
- The movies are `.mad` frames converted from planar YUV to YUY2 (MMX, `sub_0005D3C0`) and drawn as a textured quad; there is no overlay, so CoS hook 8
  (overlay and field status) does not apply.
- Threads: main (start `0x00011167`, main `0x0001A5F0`), callback dispatcher `0x00147CE0` (archive worker `0x0014A650` "BF
  Header", timing worker `0x00222B10`, memory card worker `0x00223A60`), audio worker `0x0014E9C0`, XAPI timer `0x00247580`.
- Files the console misses too (path joined without separator): `fontsfonts.ini`, `texturessymbols.xsh`,
  `variablesthingvars.txt`: not our fault.
- The unresolved call targets from their lift (47) are 40 from gap-prologue guesses and 7 from D3DX, XGRPH, XPP.

## 6. Cutscenes (ours, play test 3; neither reference skips PoA's cutscenes)

HP3-Recompiled's `hle_skip.c` has a fast-forward (QPC `sub_00195129` scaled 16x, mixer silent) whose `cutscene()` is
always false for PoA ("not found yet"). HP-POA has none. Ours (`engine/src/recomp_manual.c`, "Cutscenes"):

| Address | What |
|---|---|
| `0x365EA8` | cutscene manager singleton (`sub_000616A0`): `+0x8C` the cutscene playing (two objects used in turn), `+0x99` skippable |
| `sub_00061F10` | PlayScene's set-up: script functions `done` -> cutscene `+0x1C`, `skip` -> `+0x20` (looked up by name, `sub_0006A300` on the level's script module) |
| `sub_00062100` | per-frame update: cutscene time `+4` advanced, orchestrator `[0x3BA438]` stepped (`sub_001A53B0`); past its length -> `sub_00062090` |
| `sub_00062090` | EndScene (`Orchestrator::Runtime::EndScene(%s)`): calls `done` (`+0x1C`, then cleared) through the script VM (`sub_001B8C70` -> `0x3BA73C`, `sub_001B88D0(handle, out)`), starts the next queued scene (`sub_000614C0`) |
| `sub_00062170` | skip: subtitles off (`sub_001AF3D0(0,0)`), orchestrator skip mode (`+0x34`, `sub_001A50D0(1)` on every track), EndScene, then `skip` (`+0x20`) |
| `sub_00061520` | `(this, force)`: skip if skippable or forced, posts `PostSkipScene`; called by the Apt native SkipCutscene `sub_0001FB90` |
| `sub_00020220` | Apt native SetTraceControlMode(name): `widescreen` while a level's scripted scene with the bars runs (no orchestrator cutscene) |
| `sub_001A7450` clock `0x38BA6C` `+0x14` | fixed step a tick, not clamped (real-time steps are clamped to 100 ms): the fast-forward's lever |

Level script functions (`data/misc/intro_hogwartsexpress.hpb`): `Cutscene_EnterTheDementor` plays `InHogEx_07` with
`done = Cutscene_EnterTheDementor_Over_1`, `skip = Cutscene_EnterTheDementor_Skip` (only `MUS_HE_DEM1`), then queues
`InHogExCam_01`; `_Over_1` sets up the dementor (`Setup_Dementor`, `Dem_ResetForLevelRestart`), `_Over_2` starts the chase.
