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
