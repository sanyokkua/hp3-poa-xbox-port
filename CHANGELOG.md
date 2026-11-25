# Changelog

## Unreleased

Version 0.0.1: the port is started; nothing runs yet.

- Skip Cutscenes skips every cutscene again, and the story goes on after it (play test 3): Start or Back ends the ones
  the game lets you skip, and fast-forwards the others (and the levels' scripted scenes with the black bars) to their
  own end, about six times as fast and silent, so the level script sees them finish as on the console. Ending them
  instead (the first version) skipped their last events and once stuck the train's dementor scene; the version after
  it left almost every cutscene unskippable. `HPPOA_CUTSCENE_FF=0` turns the fast-forward off, `HPPOA_FF_STEP` sets
  its step (100 ms a frame). Dev aids: `HPPOA_CALL_SCRIPT` (run a level script function by name), `HPPOA_FF_AUTO`.
- Fixed: a mesh the camera is inside (a cutscene close-up of a face, a wall or painting at the camera) no longer fills the
  screen as a flat surface. As on the NV2A, pixels in front of the near plane are dropped when the game asks for it
  (`RECOMP_MTL_ZCULL=0` keeps the old clamp).
- The dark shapes on floors (luggage car, dormitory stairs) are the game's own character shadows from the nearest lamp;
  checked against xemu's NV2A rules: no change. Renderer details matched to the NV2A: stencil op 0 is ZERO
  (`RECOMP_MTL_STENCIL_ZERO=0`), the alpha test compares 8-bit alpha (`RECOMP_ATEST_ROUND=0`), no normal flip in
  fixed-function lighting (`RECOMP_FF_FLIP_NORMALS=1` brings it back).
- Fixed: the game stuck in the dementor scene on the train after Start or Back. Skip Cutscenes ended cutscenes the
  game marks unskippable, and those drive the level script; it now skips only skippable ones.
- Fixed: characters' skin, hair and robes blinking orange in cutscenes (Malfoy, Ron on the train). The cutscene models'
  second layer is their own texture, which squared the colour; that layer now passes the colour on
  (`RECOMP_SKIN_LAYER=1` keeps it).
- Decals drawn with a polygon offset (the library rug) get the console's slope offset when the picture is upscaled
  (`RECOMP_MTL_ZSLOPE=0` keeps the old one).
- Reflection, normal and sphere map texture coordinates are generated (the dormitory's sheen used one texel)
  (`RECOMP_MTL_TEXGEN_MAPS=0` off).
- Play test 1, round 2: the faint or missing floor shadow after the shadow fade-mask fix is what the game computes (mask,
  texture matrix and its builder checked; xemu shows no floor shadow either): no change. Dev aids for the draw log
  (`RECOMP_SURF_TRACE`, `RECOMP_DL_TEXGEN`, `RECOMP_DL_RT`, `RECOMP_DUMP_ALPHA`) and for the game's side (`HPPOA_TM_LOG`,
  `HPPOA_TEX_LOG`, `HPPOA_WATCH_FMT`, `HPPOA_DIFF_FN`/`_DIR`/`_AT`).
- Fixed: no stray "(" at the right of the Vibration and Subtitles rows in Options. The game sizes a text field's texture
  smaller than the field it draws (here 128 texels for a 139-pixel field, and the texture repeats); the texture is now made
  wide enough. `HPPOA_APT_TEXT_FIT=0` keeps the game's size. `tools/difftest.py` runs functions with SEH frames.
- Shaders compile with Metal Shading Language 3.0 instead of the OS default: a program seen for the first time stalled its frame
  by 350-600 ms (5.5 s at the first train corridor frame), now 50-90 ms. Later runs were already free of these stalls (the
  shader cache in the player folder). `RECOMP_MTL_LANG` picks another version.

- Started from the tree of the Chamber of Secrets port (sanyokkua/hpcos-xbox-port at `8342d21`): the engine, installer kit, tools
  and tests, renamed for *Harry Potter and the Prisoner of Azkaban* (Xbox, USA): app, binary `hppoa`, `HPPOA_` variables,
  player folder `~/Library/Application Support/Harry Potter Prisoner of Azkaban`, CMake target `hppoa_recomp`.
- The disc: `default.xbe` 3,190,784 bytes, SHA-1 `51b09e01169fda86f2cb7e5ce45ee80a10268918`, title `0x4541004C`, XDK 5849, entry
  `0x111DB`, 13 sections. `mac/xbe_check.py` checks it by section digests (all 13 verified against the disc: pristine).
- Removed everything that belonged to the Chamber of Secrets executable: the hand-written hooks in `engine/src/recomp_manual.c`
  (kept in a to-do list in `docs/PORTING.md`), its 177 seeds, its lifted-code patch, its settings rows (Frame Rate, Skip Logo
  Movies, Skip Cutscenes), its screenshots.
- The runtime's launch-data title id now comes from the XBE certificate (was a fixed `0x45410091`); the DSOUND range comes from
  `engine/src/region_profile.h`.
- References: HP-POA-Xbox-Recomp and HP3-Recompiled (see `docs/upstream-patches/README.md`).
- Play test 2: subtitles that kept the first line's text and were cut by the next line's box are traced to the renderer's
  texture change check (a P8 texture rewritten in place is not uploaded again); the renderer fix is prepared, not applied.
  Frame drops at 60 fps measured: loading screens (20 fps, the game waiting) and a few one-time compile or load frames; the
  frame loop itself holds 16.7 ms.
- Fixed: searching the trunk in the Gryffindor dormitory no longer gets stuck (8 functions referenced only from state-table
  initializers were never lifted; seeded, found with the new `tools/immscan.py`).
- Handling: the left stick keeps its whole travel on pads that reach full tilt (walking was squeezed into a third of the
  stick: a push to 65 % read as full); the range compensation is now learned per direction. `RECOMP_STICK_RANGE` still
  fixes it.
- `tools/play_dev.sh` no longer logs every controller change (`HPPOA_PAD_LOG=1` does); `HPPOA_MOVE_LOG=1` logs the player's
  movement a tick.
