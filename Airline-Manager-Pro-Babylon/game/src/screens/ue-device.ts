// The PC edition (Unreal Engine): your phone and the computers in the 3D terminal (owner, 2026-11-11). Unreal draws the
// room; picking up the phone or sitting down at a monitor opens the same screens as the game's own rooms do, over the
// room, with everything else of the game hidden:
//  - the phone: in its frame, with its apps (the laptop's apps in a phone layout, as in your office and Operations);
//  - the Operations monitor: the computer's desktop with the taskbar, the operations screens as apps of their own,
//    opening on the schedule;
//  - the office's computer: the same desktop.
// Esc, the phone's side button, a tap beside the phone or the desktop's power button put it away; the parent page
// tells Unreal (window.parent.__ueDeviceDone), which puts the phone back on the desk.
const UeDev: any = { root: null as HTMLElement | null, kind: '', key$: null as any };

function ueDeviceOpen(kind: string) {
  if (UeDev.root) ueDeviceClose(true);
  UeDev.kind = kind;
  document.documentElement.classList.add('ue-dev');
  const root = document.createElement('div'); root.className = 'office3d ue-dev-root'; document.body.appendChild(root); UeDev.root = root;
  if (kind === 'phone') {
    const ov = document.createElement('div') as any; ov.className = 'ph-screen';
    ov.innerHTML = `<div class="ph-frame"><div class="ph-glass"></div><span class="ph-glare"></span><span class="ph-cam"></span><button class="ph-side" data-pk="close" aria-label="Put the phone down" title="Put the phone down (Esc)"></button></div>`;
    root.appendChild(ov);
    ov.addEventListener('click', (e: Event) => { const t = e.target as HTMLElement; if (t === ov || t.closest('[data-pk=close]')) { e.stopPropagation(); ueDeviceClose(); } });
    try { lapMount(ov.querySelector('.ph-glass') as HTMLElement, true); } catch (e) { (ov.querySelector('.ph-glass') as HTMLElement).innerHTML = '<p class="muted">The phone could not start.</p>'; }
    phoneFitScreen(ov); ov._fit = () => phoneFitScreen(ov); window.addEventListener('resize', ov._fit);
    // up from below into your hand, the room dimming behind it
    const fr = ov.querySelector('.ph-frame') as HTMLElement; fr.style.transform = 'translateY(70vh) scale(.6)';
    requestAnimationFrame(() => { ov.classList.add('in'); fr.classList.add('go'); fr.style.transform = 'none'; });
  } else if (kind === 'tablet') {
    // your tablet (owner, 2026-11-18): its screen up close in its own frame, the computer's desktop inside (its apps,
    // the Overview, your wallpaper); it comes up from below as the phone does
    const ov = document.createElement('div') as any; ov.className = 'ph-screen tb-screen';
    ov.innerHTML = `<div class="tb-frame"><div class="tb-glass"></div><span class="tb-cam"></span></div>`;
    root.appendChild(ov);
    ov.addEventListener('click', (e: Event) => { if (e.target === ov) { e.stopPropagation(); ueDeviceClose(); } });
    (Lap as any).tablet = true;
    try { lapMount(ov.querySelector('.tb-glass') as HTMLElement); } catch (e) { (ov.querySelector('.tb-glass') as HTMLElement).innerHTML = '<p class="muted">The tablet could not start.</p>'; }
    const fr = ov.querySelector('.tb-frame') as HTMLElement; fr.style.transform = 'translateY(70vh) scale(.7)';
    requestAnimationFrame(() => { ov.classList.add('in'); fr.classList.add('go'); fr.style.transform = 'none'; });
    try { devWallSend(); } catch (e) { /* later */ }
  } else {
    const ov = document.createElement('div'); ov.className = 'of-planner lap-screen'; root.appendChild(ov);
    try { lapMount(ov); if (kind === 'ops') { (Lap as any).ops = true; Lap.app = 'scr:schedule'; lapRender(); } } catch (e) { ov.innerHTML = '<p class="muted">The computer could not start.</p>'; }
  }
  // Esc puts it away (an open window inside it first: the game's own way)
  UeDev.key$ = (e: KeyboardEvent) => { if (e.key !== 'Escape' || !UeDev.root) return; if (document.querySelector('.layer, .modal-back, .sheet-back')) return; e.preventDefault(); e.stopImmediatePropagation(); ueDeviceClose(); };
  window.addEventListener('keydown', UeDev.key$, true);
  try { Sound.tick(); } catch (e) { /* quiet */ }
}

function ueDeviceClose(quiet?: boolean) {
  const root = UeDev.root; if (!root) return;
  UeDev.root = null; window.removeEventListener('keydown', UeDev.key$, true);
  try { lapUnmount(); } catch (e) { /* closed */ }
  const ov = root.querySelector('.ph-screen') as any; if (ov && ov._fit) window.removeEventListener('resize', ov._fit);
  (Lap as any).tablet = false;
  const done = () => { root.remove(); document.documentElement.classList.remove('ue-dev'); if (!quiet) { try { (window.parent as any).__ueDeviceDone && (window.parent as any).__ueDeviceDone(UeDev.kind); } catch (e) { /* no parent */ } } };
  if (ov && !quiet) { ov.classList.add('out'); ov.classList.remove('in'); const fr = ov.querySelector('.ph-frame, .tb-frame') as HTMLElement; if (fr) fr.style.transform = 'translateY(70vh) scale(.6)'; setTimeout(done, 420); }
  else done();
  try { if (!quiet) Sound.tick(); } catch (e) { /* quiet */ }
}
/** Is a phone or computer open over Unreal's room? */
function ueDeviceOn(): boolean { return !!UeDev.root; }
