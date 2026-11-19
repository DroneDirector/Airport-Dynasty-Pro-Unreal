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
  const held = kind === 'phone-held'; if (held) kind = 'phone';   // (the phone in your hand: Unreal holds it, its screen laid on the 3D one)
  UeDev.kind = kind;
  if (kind === 'phone') {
    const ov = document.createElement('div') as any; ov.className = 'ph-screen';
    ov.innerHTML = `<div class="ph-frame"><div class="ph-glass"></div><span class="ph-glare"></span><span class="ph-cam"></span><button class="ph-side" data-pk="close" aria-label="Put the phone down" title="Put the phone down (Esc)"></button></div>`;
    root.appendChild(ov);
    ov.addEventListener('click', (e: Event) => { const t = e.target as HTMLElement; if (t === ov || t.closest('[data-pk=close]')) { e.stopPropagation(); ueDeviceClose(); } });
    try { lapMount(ov.querySelector('.ph-glass') as HTMLElement, true); } catch (e) { (ov.querySelector('.ph-glass') as HTMLElement).innerHTML = '<p class="muted">The phone could not start.</p>'; }
    phoneFitScreen(ov); ov._fit = () => phoneFitScreen(ov); window.addEventListener('resize', ov._fit);
    if (held) phHeldOpen(ov);
    else {
      // up from below into your hand, the room dimming behind it
      const fr = ov.querySelector('.ph-frame') as HTMLElement; fr.style.transform = 'translateY(70vh) scale(.6)';
      requestAnimationFrame(() => { ov.classList.add('in'); fr.classList.add('go'); fr.style.transform = 'none'; });
    }
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
  const wasHeld = PhHeld.on; phHeldClose(); if (wasHeld) quiet = quiet;   // (the 3D phone goes down in Unreal's hand: no slide here)
  const done = () => { root.remove(); document.documentElement.classList.remove('ue-dev'); if (!quiet) { try { (window.parent as any).__ueDeviceDone && (window.parent as any).__ueDeviceDone(UeDev.kind); } catch (e) { /* no parent */ } } };
  if (ov && !quiet && !wasHeld) { ov.classList.add('out'); ov.classList.remove('in'); const fr = ov.querySelector('.ph-frame, .tb-frame') as HTMLElement; if (fr) fr.style.transform = 'translateY(70vh) scale(.6)'; setTimeout(done, 420); }
  else done();
  try { if (!quiet) Sound.tick(); } catch (e) { /* quiet */ }
}
/** Is a phone or computer open over Unreal's room? */
function ueDeviceOn(): boolean { return !!UeDev.root; }

// ---- the phone in your hand (owner, 2026-11-19; docs/Device-Hold-Plan.md parts 3-4) ----------------------------------
// Unreal holds the 3D phone in your hand and tells this page, every frame it moves, where the phone's screen is on the
// window (its four corners, 0..1 of the window: top-left, top-right, bottom-right, bottom-left). The phone's own screen
// is laid exactly onto it (a perspective transform), without the page's frame - the hand and the phone's body are
// Unreal's. The "+" pointer (Unreal moves it with the mouse) is drawn on the screen; clicks land on what is under it,
// the mouse wheel scrolls like a finger on a touch screen (no click), and the side button steps a highlight box from
// item to item (Unreal moves the thumb and plays the click).
const PhHeld: any = { said: false, on: false, ov: null as any, fr: null as HTMLElement | null, M: null as DOMMatrix | null, u: 0.5, v: 0.5, plus: null as HTMLElement | null, hl: -1 };

/** The 3x3 projective map from the unit square to the quad p (4 points), as a CSS matrix3d for a w x h box. */
function phHomography(w: number, h: number, p: number[][]): DOMMatrix {
  const [[x0, y0], [x1, y1], [x2, y2], [x3, y3]] = p;
  const dx1 = x1 - x2, dx2 = x3 - x2, dx3 = x0 - x1 + x2 - x3, dy1 = y1 - y2, dy2 = y3 - y2, dy3 = y0 - y1 + y2 - y3;
  const den = dx1 * dy2 - dx2 * dy1 || 1e-9;
  const g = (dx3 * dy2 - dx2 * dy3) / den, hh = (dx1 * dy3 - dx3 * dy1) / den;
  const a = x1 - x0 + g * x1, b = x3 - x0 + hh * x3, c = x0, d = y1 - y0 + g * y1, e = y3 - y0 + hh * y3, f = y0;
  // unit square -> quad; the box (w x h) first scaled to the unit square
  return new DOMMatrix([a / w, d / w, 0, g / w, b / h, e / h, 0, hh / h, 0, 0, 1, 0, c, f, 0, 1]);
}

function phHeldOpen(ov: any) {
  PhHeld.on = true; PhHeld.ov = ov; PhHeld.fr = ov.querySelector('.ph-frame'); PhHeld.hl = -1;
  ov.classList.add('ph-held'); ov.style.setProperty('--phz', '1');
  if (ov._fit) { window.removeEventListener('resize', ov._fit); ov._fit = null; }
  const plus = document.createElement('div'); plus.className = 'ph-plus'; (ov.querySelector('.ph-glass') as HTMLElement).appendChild(plus); PhHeld.plus = plus;
  phHeldPtr(0.5, 0.5);
}

function phGlassBox(): { el: HTMLElement, w: number, h: number } | null {
  const fr = PhHeld.fr as HTMLElement; if (!fr) return null;
  return { el: fr, w: fr.offsetWidth, h: fr.offsetHeight };
}

/** Unreal: where the phone's screen is now (8 numbers, 0..1 of the window). */
(window as any).__uePhoneQuad = (q: number[]) => {
  const b = phGlassBox(); if (!PhHeld.on || !b) return;
  const W = window.innerWidth, H = window.innerHeight;
  const p = [[q[0] * W, q[1] * H], [q[2] * W, q[3] * H], [q[4] * W, q[5] * H], [q[6] * W, q[7] * H]];
  const M = phHomography(b.w, b.h, p); PhHeld.M = M;
  b.el.style.transform = M.toString();
  // (once: where it ended up, for checking)
  if (!PhHeld.said) { PhHeld.said = true; const r = b.el.getBoundingClientRect(), o = (PhHeld.ov as HTMLElement).getBoundingClientRect(), g = (b.el.querySelector('.ph-glass') as HTMLElement).getBoundingClientRect();
    return 'PHLOG frame ' + b.w + 'x' + b.h + ' at ' + [r.left, r.top, r.width, r.height].map(Math.round).join(',') + ' glass ' + [g.left, g.top, g.width, g.height].map(Math.round).join(',') + ' overlay ' + [o.left, o.top, o.width, o.height].map(Math.round).join(',') + ' window ' + W + 'x' + H + ' zoom ' + ((window as any).__uiZoom || 1) + ' frame css ' + getComputedStyle(b.el).position + ' ' + getComputedStyle(b.el).zoom; }
  return '';
};

/** Unreal: the "+" pointer on the screen (0..1 across and down the screen). */
function phHeldPtr(u: number, v: number) {
  PhHeld.u = Math.min(1, Math.max(0, u)); PhHeld.v = Math.min(1, Math.max(0, v));
  const b = phGlassBox(); if (!b || !PhHeld.plus) return;
  PhHeld.plus.style.left = (PhHeld.u * 100) + '%'; PhHeld.plus.style.top = (PhHeld.v * 100) + '%';
}
(window as any).__uePhonePtr = (u: number, v: number, show?: boolean) => { phHeldPtr(u, v); if (PhHeld.plus) PhHeld.plus.hidden = show === false; };

/** A point on the screen (0..1) where it is on the window now (the transform applied). */
function phToClient(u: number, v: number): { x: number, y: number } | null {
  const b = phGlassBox(); if (!b || !PhHeld.M) return null;
  const pt = PhHeld.M.transformPoint(new DOMPoint(u * b.w, v * b.h, 0, 1));
  return { x: pt.x / pt.w, y: pt.y / pt.w };
}

function phElementAt(u: number, v: number): HTMLElement | null {
  const c = phToClient(u, v); if (!c) return null;
  const plus = PhHeld.plus as HTMLElement; const was = plus ? plus.style.display : ''; if (plus) plus.style.display = 'none';
  const el = document.elementFromPoint(c.x, c.y) as HTMLElement | null;
  if (plus) plus.style.display = was;
  return el && PhHeld.fr && PhHeld.fr.contains(el) ? el : null;
}

/** Unreal: a click with the "+" (no side-button sound - that is only the side button). */
(window as any).__uePhoneClick = () => {
  const el = phElementAt(PhHeld.u, PhHeld.v); const c = phToClient(PhHeld.u, PhHeld.v); if (!el || !c) return;
  const o = { bubbles: true, cancelable: true, clientX: c.x, clientY: c.y, view: window };
  try { el.dispatchEvent(new PointerEvent('pointerdown', o)); el.dispatchEvent(new MouseEvent('mousedown', o)); el.dispatchEvent(new PointerEvent('pointerup', o)); el.dispatchEvent(new MouseEvent('mouseup', o)); } catch (e) { /* old */ }
  el.click();
};

/** Unreal: the mouse wheel over the screen - a finger's swipe (smooth, silent). */
(window as any).__uePhoneScroll = (dy: number) => {
  let el = phElementAt(PhHeld.u, PhHeld.v);
  while (el && el !== PhHeld.fr) { const s = getComputedStyle(el); if (/(auto|scroll)/.test(s.overflowY) && el.scrollHeight > el.clientHeight + 2) break; el = el.parentElement as HTMLElement; }
  const target = el && el !== PhHeld.fr ? el : (PhHeld.fr && PhHeld.fr.querySelector('.ph-glass *') as HTMLElement);
  if (target) target.scrollBy({ top: dy, behavior: 'smooth' });
};

/** The side button: the highlight box to the next (+1) or previous (-1) thing you can open on the screen. */
function phItems(): HTMLElement[] {
  const g = PhHeld.fr && PhHeld.fr.querySelector('.ph-glass') as HTMLElement; if (!g) return [];
  return Array.from(g.querySelectorAll('button, a[href], [data-act], [data-ap], [data-app], [role=button], input, select, .lap-app, .lap-ic')).filter((e: any) => {
    const r = e.getBoundingClientRect(); return r.width > 4 && r.height > 4 && !e.closest('[hidden]') && e !== PhHeld.plus;
  }) as HTMLElement[];
}
(window as any).__uePhoneStep = (dir: number) => {
  const it = phItems(); if (!it.length) return;
  document.querySelectorAll('.ph-hl').forEach(e => e.classList.remove('ph-hl'));
  PhHeld.hl = PhHeld.hl < 0 ? (dir > 0 ? 0 : it.length - 1) : (PhHeld.hl + dir + it.length) % it.length;
  const el = it[PhHeld.hl]; el.classList.add('ph-hl');
  try { el.scrollIntoView({ block: 'nearest', behavior: 'smooth' }); } catch (e) { /* old */ }
};
/** E / Cross: open the highlighted thing (true if there was one). */
(window as any).__uePhoneActivate = (): boolean => {
  const el = document.querySelector('.ph-held .ph-hl') as HTMLElement | null; if (!el) return false;
  el.click(); return true;
};

function phHeldClose() { PhHeld.on = false; PhHeld.M = null; PhHeld.plus = null; PhHeld.fr = null; PhHeld.ov = null; PhHeld.hl = -1; }
