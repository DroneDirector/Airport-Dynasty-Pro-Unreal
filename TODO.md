# To-do

- [ ] **Chapters in the code** (both versions, Android and PC): comment-only chapter markers and a short description at
  the top of every file, as described in CLAUDE.md. From now on every new feature automatically gets its chapters and its line in CODE-MAP.md. Reminders set for Monday 2026-11-09 at 10:00, 13:00 and 16:00.
- [x] A clearer "✕ Exit cinema" button in the top-left corner while walking in the world (with the next release).
- [ ] **Your own 3D body while walking** (later, when the people get their 3D bodies): you see your own body in walking
  mode (your legs and arms when you look down, your shadow, your reflection in glass), everywhere you walk: in the
  airport and anywhere on the 3D globe.
- [x] **Fix: double-tap doesn't bring the UI back** after Hide UI (walking in the world). Likely cause: on touch screens
  the stick and look layer lies over the 3D view, so the view itself never gets the taps. Fix: listen for the double
  tap on the whole page while the UI is hidden.
- [x] **Fix first on Monday: the 3D view of a flight is black** (fixed in 0.4.5: the map overwrote the sky) (since 0.4.1, still in 0.4.4; worked in 0.4.0). Changed in 0.4.1: skymap.ts, the clean layout CSS (.v3d.live), Hide UI listeners, groundAt and the terrain awareness (aware/safeAware in viewer3d/index.ts). Also black in the cruise, after take-off, so not the ground or mountains. Check the console first, then undo one change at a time.
