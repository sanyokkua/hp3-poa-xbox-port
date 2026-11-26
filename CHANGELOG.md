# Changelog

## 0.1.1 (2026-11-26)

- Movies: the WB logo no longer freezes for up to a second and a half. The renderer checked only a sample of each movie
  frame for changes and missed many frames of the dark logo; it now checks the whole frame (found in the Goblet of Fire
  port).
- Full screen: the frame is laid out in the size of the screen it is drawn to, so the first frames after going full
  screen can no longer fill only the top-left corner (found in the Goblet of Fire port; not seen on this game, kept as a
  guard).

## 0.1.0 (2026-11-25)

First release: the Xbox USA disc of *Harry Potter and the Prisoner of Azkaban*, built on the player's Mac. Played from the
logos and the intro movie through New Game, the Hogwarts Express (the corridor, the luggage car, the Flipendo tutorial, the
dementor) and on into Hogwarts (the Gryffindor dormitory and common room, the sixth floor, the library, the Defence Against
the Dark Arts classroom, the Marauder's Map challenge); saving and loading work. The owner's fourth play test found no issues.

- Base: the engine of the Harry Potter and the Chamber of Secrets port (sanyokkua/hpcos-xbox-port), which is the NFS
  Underground 2 port's engine (xboxrecomp, nfsu2-sw, nfsu2-ios's Apple host and Metal renderer, NFSU2Deadware's Mac
  patches) by way of the Sorcerer's Stone port. This game's facts (hidden functions, the XInput functions, the game clock,
  the cutscene manager, the movie player) from HP-POA-Xbox-Recomp and HP3-Recompiled, each checked on this disc. The game
  runs on EA UK's own engine ("Tb"), not RenderWare.
- The game: the logos and the `.mad` movies, the title, New Game, Load Game, cutscenes with speech and subtitles, free play
  with spells, the HUD and the pause menu, the game's Options, music, speech and effects. 30 fps as on the console, or 60
  fps with the game clock at real speed.
- In-game settings menu (Back + Start, F1 or Cmd+,): Display (incl. **Frame Rate**: Original 30 fps or 60 fps), Graphics
  (anti-aliasing, anisotropic filtering, bloom and ambient occlusion, gamma), Audio, Input (vibration, keyboard), Tweaks and
  Advanced. Tweaks: **Skip Logo Movies**, and **Skip Cutscenes** (on by default): press Start or Back in a cutscene; the
  ones the game lets you skip end at once, the others (almost all) play to their own end about six times as fast and
  silent, so every event in them still happens and the story goes on.
- Controller with rumble and keyboard; a radial dead zone on the sticks, and the stick's range learned per direction, so a
  pad that reaches full tilt walks and runs over the whole travel.
- Renderer work found with this game (details in `docs/PORTING.md`): the shadow fade mask and the lock-on ring (inline
  vertex batches in fixed-function mode, 2-vertex lines), per-pixel depth clamp and near-plane cull as on the console's
  chip (no rug or floor patch cut along a straight line, no flat close-up in cutscenes), the polygon-offset slope at higher
  resolutions, reflection/normal/sphere map texture coordinates, subtitles (palette textures rewritten in place are
  uploaded again), the cutscene models' skin colour, Options text without stray "(" glyphs, shaders compiled as Metal 3.0
  (a first-seen effect no longer stalls the frame for half a second).
- Game fixes: the dormitory trunk search no longer hangs (8 functions reached only through state tables are lifted, found
  by the new `tools/immscan.py`); the console's NTSC video region is reported.
- Everything the port is built from is in the repository (`engine/`): no download at build time.
- The app keeps everything in `~/Library/Application Support/Harry Potter Prisoner of Azkaban`; logs in `logs/`.

Known limits: 4:3 only (no widescreen); the USA disc only (no PAL); at 60 fps HP-POA-Xbox-Recomp's lip-sync and ledge-step
fixes are not ported; specular lighting is not computed; character shadows' strength not compared with a console.
