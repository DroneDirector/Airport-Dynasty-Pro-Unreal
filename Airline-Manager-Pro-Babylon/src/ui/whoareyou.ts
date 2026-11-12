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
import { splashTakeMusic, splashGiveMusic } from './splash';
import { S, saveSettings } from '../core/settings';

const Wy: any = { root: null as HTMLElement | null, audio: null as HTMLAudioElement | null, stage: '', side: '', step: 0, hover: '', chosen: false, key$: null as any, raf: 0, padA: false, padB: false, padX: 0 };

function tell(action: string, arg = '') { if (action === 'cancel') musicEnd(true); const u = ue(); if (u && u.whoareyou) u.whoareyou(action, arg); }

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

function muteHtml(): string {
  return `<button class="ts-mute wy-mute ${S.music ? '' : 'off'}" data-mute="1" aria-label="Music on or off"><span class="ts-eq"><i></i><i></i><i></i><i></i></span><span class="ts-ml">${S.music ? 'Music on' : 'Music off'}</span></button>`;
}
function muteToggle() {
  S.music = !S.music; saveSettings('music');
  if (Wy.audio) { Wy.audio.muted = !S.music; if (S.music) Wy.audio.play().catch(() => { /* later */ }); }
  const b = Wy.root && Wy.root.querySelector('.wy-mute'); if (b) { b.classList.toggle('off', !S.music); const l = b.querySelector('.ts-ml'); if (l) l.textContent = S.music ? 'Music on' : 'Music off'; }
}
/** The music fades out (the loading screen next), or is handed back to the title (back out). */
function musicEnd(backToTitle: boolean) {
  const a = Wy.audio; Wy.audio = null; if (!a) return;
  if (backToTitle) { splashGiveMusic(a); return; }
  const v0 = a.volume, t0 = performance.now();
  const fade = () => { const k = Math.min(1, (performance.now() - t0) / 1200); a.volume = v0 * (1 - k); if (k < 1) requestAnimationFrame(fade); else { a.pause(); a.src = ''; } };
  fade();
}

function renderChoose() {
  const r = Wy.root; if (!r) return;
  r.innerHTML = `<div class="wy-head"><div class="wd-k">Airport Dynasty Pro  ·  a new career</div><div class="wy-t">Who are you?</div><div class="wd-k wy-sub">She, or he  ·  then make them yours</div></div>
    <div class="wy-half wy-l" data-who="she"></div><div class="wy-half wy-r" data-who="he"></div>
    <div class="wy-who wy-she" data-who="she">She</div><div class="wy-who wy-he" data-who="he">He</div>
    ${muteHtml()}
    <div class="wy-hint"><span class="kb">Point, or ← →</span><b class="kb">Click or Enter: choose</b><span class="kb">Esc: back</span><span class="pad">Stick or d-pad</span><b class="pad">A: choose</b><span class="pad">B: back</span></div>`;
}

// the steps after the choice, each a panel on the free side you cannot change yet (placeholders until the creator,
// the pets and the partner exist), with Back and Next; the last one's button is Start game
const STEPS: { id: string; k: string; t: string; parts: [string, string][] }[] = [
  { id: 'creator', k: 'Coming with the character creator', t: 'Your looks', parts: [['Face', 'star'], ['Skin', 'grid'], ['Body', 'plus'], ['Height', 'mountain'], ['Hair', 'save'], ['Clothes', 'folder']] },
  { id: 'pet', k: 'Coming: a companion at home', t: 'Your pet', parts: [['Dog', 'star'], ['Cat', 'star'], ['No pet', 'close']] },
  { id: 'partner', k: 'Coming: married, or not', t: 'Your partner', parts: [['Married', 'star'], ['Single', 'close'], ['Their looks', 'grid']] },
];
function renderStep(n: number, side: string) {
  const r = Wy.root; if (!r) return;
  const st = STEPS[n], last = n === STEPS.length - 1;
  r.innerHTML = `<div class="wy-head"><div class="wd-k">Airport Dynasty Pro  ·  a new career</div><div class="wy-t">Make them yours</div><div class="wd-k wy-sub">Step ${n + 2} of ${STEPS.length + 1}  ·  ${st.t.toLowerCase()}</div></div>
    ${muteHtml()}
    <div class="wy-panel wy-${side === 'left' ? 'left' : 'right'}">
      <div class="wd-k">${st.k}</div><div class="wd-t">${st.t}</div>
      <nav class="ts-nav">${st.parts.map(([t, ic], i) => `<button class="ts-btn ${i === 0 ? 'on' : ''}" data-part="${t}"><span class="ts-bi">${icon(ic)}</span><span class="ts-bt"><b>${t}</b><small>soon</small></span><i class="ts-sheen"></i></button>`).join('')}
        <div class="wy-row">
          <button class="ts-btn wy-back" data-back="1"><span class="ts-bi">${icon('back')}</span><span class="ts-bt"><b>Back</b><small class="pad">B</small></span><i class="ts-sheen"></i></button>
          <button class="ts-btn wy-next ${last ? 'hot' : ''}" data-next="1"><span class="ts-bi">${icon('play')}</span><span class="ts-bt"><b>${last ? 'Start game' : 'Next'}</b><small><span class="pad">A · </span>${last ? 'Load your airline' : STEPS[n + 1].t}</small></span><i class="ts-sheen"></i></button>
        </div>
      </nav>
    </div>`;
}
function stepTo(n: number) {
  if (n < 0) { tell('back'); Wy.stage = 'reverse'; if (Wy.root) Wy.root.innerHTML = ''; return; }   // the stage reverses; Unreal brings the choice back
  if (n >= STEPS.length) { musicEnd(false); tell('done'); return; }
  Wy.step = n; Wy.stage = 'step'; renderStep(n, Wy.side);
}

function padPoll() {
  if (!Wy.root) return;
  try {
    const pads = navigator.getGamepads ? Array.from(navigator.getGamepads()).filter(Boolean) as Gamepad[] : [];
    const p = pads[0];
    if (Wy.root) Wy.root.classList.toggle('has-pad', !!p);
    if (p) {
      const x = (p.axes[0] || 0) + (p.buttons[14] && p.buttons[14].pressed ? -1 : 0) + (p.buttons[15] && p.buttons[15].pressed ? 1 : 0);
      if (Wy.stage === 'choose') { if (x < -0.5) hover('she'); else if (x > 0.5) hover('he'); }
      const a = !!(p.buttons[0] && p.buttons[0].pressed), b = !!(p.buttons[1] && p.buttons[1].pressed);
      if (a && !Wy.padA) { if (Wy.stage === 'choose') choose(); else if (Wy.stage === 'step') stepTo(Wy.step + 1); }
      if (b && !Wy.padB) { if (Wy.stage === 'step') stepTo(Wy.step - 1); else if (Wy.stage === 'choose') tell('cancel'); }
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
      if (t.closest('[data-mute]')) { muteToggle(); return; }
      if (Wy.stage === 'choose') { const h = t.closest('[data-who]') as HTMLElement | null; if (h) { hover(h.getAttribute('data-who') || ''); choose(); } return; }
      if (Wy.stage === 'step') { if (t.closest('[data-next]')) stepTo(Wy.step + 1); else if (t.closest('[data-back]')) stepTo(Wy.step - 1); }
    });
    Wy.key$ = (e: KeyboardEvent) => {
      if (!Wy.root) return;
      if (e.key === 'Escape') { e.preventDefault(); e.stopImmediatePropagation(); if (Wy.stage === 'step') stepTo(Wy.step - 1); else if (Wy.stage === 'choose') tell('cancel'); return; }
      if (e.key === 'Backspace' && Wy.stage === 'step') { e.preventDefault(); stepTo(Wy.step - 1); return; }
      if (Wy.stage === 'choose') {
        if (e.key === 'ArrowLeft') { e.preventDefault(); hover('she'); }
        else if (e.key === 'ArrowRight') { e.preventDefault(); hover('he'); }
        else if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); choose(); }
      } else if (Wy.stage === 'step' && (e.key === 'Enter' || e.key === ' ')) { e.preventDefault(); stepTo(Wy.step + 1); }
    };
    window.addEventListener('keydown', Wy.key$, true);
    Wy.raf = requestAnimationFrame(padPoll);
    Wy.audio = splashTakeMusic();   // the title's music, playing on
    requestAnimationFrame(() => root.classList.add('in'));
  }
  Wy.side = stage === 'choose' ? Wy.side : side;
  if (stage === 'choose') { Wy.stage = 'choose'; Wy.hover = ''; Wy.chosen = false; Wy.root.classList.remove('chosen'); renderChoose(); if (side) hover(side); }
  else stepTo(0);
}

function close() {
  const root = Wy.root; if (!root) return;
  Wy.root = null; Wy.stage = ''; window.removeEventListener('keydown', Wy.key$, true); cancelAnimationFrame(Wy.raf);
  root.classList.remove('in'); root.classList.add('out');
  setTimeout(() => { root.remove(); document.documentElement.classList.remove('ue-stage'); }, 380);
}

(window as any).__ueWhoAreYou = open;
(window as any).__ueWhoAreYouClose = close;
