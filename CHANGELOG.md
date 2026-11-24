# Changelog

## Unreleased

Version 0.0.1: the port is started; nothing runs yet.

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
- Fixed: searching the trunk in the Gryffindor dormitory no longer gets stuck (8 functions referenced only from state-table
  initializers were never lifted; seeded, found with the new `tools/immscan.py`).
