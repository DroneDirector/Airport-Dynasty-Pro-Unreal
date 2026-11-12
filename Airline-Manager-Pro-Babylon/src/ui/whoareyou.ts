// The PC edition's new career (owner, 2026-11-12): New game on the title screen first shows Unreal's "who are you"
// stage (a dark room, two neon tubes, floor fog, she at the left and he at the right) with this overlay over it in the
// title screen's graphics. Hovering (mouse, arrow keys, the pad's stick or d-pad) puts one of them in the light; a click,
// Enter or A chooses: the other walks out of the picture, the tubes fade back into the fog, and the creator's panel
// comes up on the free side (she chosen: at the right; he chosen: at the left). Its Next goes on to the game's own
// new-game flow (the pilot and the airline), i.e. the Overview. Esc or B goes back to the title screen.
// Unreal drives the stages: window.__ueWhoAreYou('choose') / ('creator', 'right'|'left'); window.__ueWhoAreYouClose().
// The page tells Unreal: ue.adp.whoAreYou('hover', 'she'|'he'), ('choose', ...), ('cancel'), ('done').
import { ue } from '../core/uelink';
import { icon } from './icons';

const Wy: any = { root: null as HTMLElement | null, stage: '', side: '', hover: '', chosen: false, key$: null as any, raf: 0, padA: false, padB: false, padX: 0 };

function tell(action: string, arg = '') { const u = ue(); if (u && u.whoareyou) u.whoareyou(action, arg); }

function hover(who: string) {
  if (Wy.chosen || who === Wy.hover || !Wy.root) return;
  Wy.hover = who;
  Wy.root.querySelectorAll('.wy-who').forEach(el => el.classList.toggle('on', el.getAttribute('data-who') === who));
  tell('hover', who);
}
function choose() {
  if (Wy.chosen || !Wy.hover) return;
  Wy.chosen = true;
  if (Wy.root) Wy.root.classList.add('chosen');
  tell('choose', Wy.hover);
}

function renderChoose() {
  const r = Wy.root; if (!r) return;
  r.innerHTML = `<div class="wy-head"><div class="wd-k">Airport Dynasty Pro  ·  a new career</div><div class="wy-t">Who are you?</div><div class="wd-k wy-sub">She, or he  ·  then make them yours</div></div>
    <div class="wy-half wy-l" data-who="she"></div><div class="wy-half wy-r" data-who="he"></div>
    <div class="wy-who wy-she" data-who="she">She</div><div class="wy-who wy-he" data-who="he">He</div>
    <div class="wy-hint"><span>Point, or ← →</span><b>Click, Enter or A: choose</b><span>Esc or B: back</span></div>`;
}

function renderCreator(side: string) {
  const r = Wy.root; if (!r) return;
  const parts = [['Face', 'star'], ['Skin', 'grid'], ['Body', 'plus'], ['Height', 'mountain'], ['Hair', 'save'], ['Clothes', 'folder']];
  r.innerHTML = `<div class="wy-head"><div class="wd-k">Airport Dynasty Pro  ·  a new career</div><div class="wy-t">Make them yours</div><div class="wd-k wy-sub">Your looks  ·  then your pet, and your partner</div></div>
    <div class="wy-panel wy-${side === 'left' ? 'left' : 'right'}">
      <div class="wd-k">Coming with the character creator</div><div class="wd-t">Your looks</div>
      <nav class="ts-nav">${parts.map(([t, ic], i) => `<button class="ts-btn ${i === 0 ? 'on' : ''}" data-part="${t}"><span class="ts-bi">${icon(ic)}</span><span class="ts-bt"><b>${t}</b><small>soon</small></span><i class="ts-sheen"></i></button>`).join('')}
        <button class="ts-btn wy-next" data-done="1"><span class="ts-bi">${icon('play')}</span><span class="ts-bt"><b>Next</b><small>On to your airline</small></span><i class="ts-sheen"></i></button>
      </nav>
    </div>`;
}

function padPoll() {
  if (!Wy.root) return;
  try {
    const pads = navigator.getGamepads ? Array.from(navigator.getGamepads()).filter(Boolean) as Gamepad[] : [];
    const p = pads[0];
    if (p) {
      const x = (p.axes[0] || 0) + (p.buttons[14] && p.buttons[14].pressed ? -1 : 0) + (p.buttons[15] && p.buttons[15].pressed ? 1 : 0);
      if (Wy.stage === 'choose') { if (x < -0.5) hover('she'); else if (x > 0.5) hover('he'); }
      const a = !!(p.buttons[0] && p.buttons[0].pressed), b = !!(p.buttons[1] && p.buttons[1].pressed);
      if (a && !Wy.padA) { if (Wy.stage === 'choose') choose(); else if (Wy.stage === 'creator') tell('done'); }
      if (b && !Wy.padB) tell('cancel');
      Wy.padA = a; Wy.padB = b;
    }
  } catch (e) { /* no pad */ }
  Wy.raf = requestAnimationFrame(padPoll);
}

function open(stage: string, side = '') {
  if (!Wy.root) {
    document.documentElement.classList.add('ue-stage');
    const root = document.createElement('div'); root.className = 'wy'; document.body.appendChild(root); Wy.root = root;
    root.addEventListener('mousemove', (e: MouseEvent) => { if (Wy.stage !== 'choose') return; const h = (e.target as HTMLElement).closest('[data-who]') as HTMLElement | null; if (h) hover(h.getAttribute('data-who') || ''); });
    root.addEventListener('click', (e: Event) => {
      const t = e.target as HTMLElement;
      if (Wy.stage === 'choose') { const h = t.closest('[data-who]') as HTMLElement | null; if (h) { hover(h.getAttribute('data-who') || ''); choose(); } return; }
      if (t.closest('[data-done]')) tell('done');
    });
    Wy.key$ = (e: KeyboardEvent) => {
      if (!Wy.root) return;
      if (e.key === 'Escape') { e.preventDefault(); e.stopImmediatePropagation(); tell('cancel'); return; }
      if (Wy.stage === 'choose') {
        if (e.key === 'ArrowLeft') { e.preventDefault(); hover('she'); }
        else if (e.key === 'ArrowRight') { e.preventDefault(); hover('he'); }
        else if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); choose(); }
      } else if (Wy.stage === 'creator' && (e.key === 'Enter' || e.key === ' ')) { e.preventDefault(); tell('done'); }
    };
    window.addEventListener('keydown', Wy.key$, true);
    Wy.raf = requestAnimationFrame(padPoll);
    requestAnimationFrame(() => root.classList.add('in'));
  }
  Wy.stage = stage; Wy.side = side;
  if (stage === 'choose') { Wy.hover = ''; Wy.chosen = false; Wy.root.classList.remove('chosen'); renderChoose(); }
  else renderCreator(side);
}

function close() {
  const root = Wy.root; if (!root) return;
  Wy.root = null; Wy.stage = ''; window.removeEventListener('keydown', Wy.key$, true); cancelAnimationFrame(Wy.raf);
  root.classList.remove('in'); root.classList.add('out');
  setTimeout(() => { root.remove(); document.documentElement.classList.remove('ue-stage'); }, 380);
}

(window as any).__ueWhoAreYou = open;
(window as any).__ueWhoAreYouClose = close;
