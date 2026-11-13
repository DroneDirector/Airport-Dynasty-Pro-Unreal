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

const Wy: any = { root: null as HTMLElement | null, audio: null as HTMLAudioElement | null, stage: '', side: '', step: 0, diff: '', pet: 'none', coat: 0, partner: 'single', padY: 0, hover: '', chosen: false, key$: null as any, raf: 0, padA: false, padB: false, padX: 0 };

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
  { id: 'pet', k: 'A companion at home', t: 'Your pet', parts: [['Dog', 'star'], ['Cat', 'star'], ['No pet', 'close']] },
  { id: 'partner', k: 'Married, or not', t: 'Your partner', parts: [['Married', 'star'], ['Single', 'close'], ['Their looks', 'grid']] },
  { id: 'difficulty', k: 'How hard your career is', t: 'Difficulty', parts: [['Cruising along', 'star'], ['Easy', 'star'], ['Normal', 'star'], ['Hard', 'star'], ['Extreme', 'star']] },
];
// the difficulty (the game's own levels): what aircraft cost, what flights earn, how markets swing, and the money you
// start with: on Hard one used A320-class aircraft, its crew and a little spare; more on Normal; a lot on Easy
// the pet's coats (the game's own: its office pet takes the same), the first one on
const COATS: { [k: string]: [string, string][] } = {
  dog: [['#C8913E', 'Golden'], ['#2A2420', 'Black'], ['#6B4226', 'Brown'], ['#E6D3AE', 'Cream'], ['#F2EFE8', 'White'], ['#8A8783', 'Grey'], ['#A0522D', 'Red'], ['#4A3B30', 'Dark brown']],
  cat: [['#E08A3C', 'Ginger'], ['#2A2624', 'Black'], ['#9A9590', 'Grey'], ['#F1EEE8', 'White'], ['#7A6450', 'Tabby'], ['#D9C3A0', 'Cream'], ['#5E6670', 'Blue-grey'], ['#B07A4A', 'Cinnamon']],
};
/** The choices so far, for the game when the career begins (main.ts reads window.__ngChoices). */
function choicesOut() { (window as any).__ngChoices = { pet: Wy.pet || 'none', coat: Wy.coat || 0, married: Wy.partner === 'married' }; }
function petTell() { const k = Wy.pet; if (k === 'dog' || k === 'cat') tell('pet', k + ':' + COATS[k][Wy.coat || 0][0]); else tell('pet', 'none'); choicesOut(); }
function partnerTell() { tell('partner', Wy.partner === 'married' ? 'married' : 'single'); choicesOut(); }
const DIFFS: [string, string, string][] = [
  ['cruise', 'Cruising along', 'Cheap aircraft, generous passengers, calm markets. Just enjoy flying.'],
  ['easy', 'Easy', 'A forgiving start with room for mistakes, and plenty of money.'],
  ['normal', 'Normal', 'Real-world prices and margins. One aircraft and a little room.'],
  ['hard', 'Hard', 'Thin margins, pricier aircraft. One used aircraft, its crew, little spare.'],
  ['extreme', 'Extreme', 'Every dollar counts. Markets swing hard. For experts.'],
];
function renderStep(n: number, side: string) {
  const r = Wy.root; if (!r) return;
  const st = STEPS[n], last = n === STEPS.length - 1, isDiff = st.id === 'difficulty';
  if (isDiff && !Wy.diff) Wy.diff = 'normal';
  r.innerHTML = `<div class="wy-head"><div class="wd-k">Airport Dynasty Pro  ·  a new career</div><div class="wy-t">Make them yours</div><div class="wd-k wy-sub">Step ${n + 2} of ${STEPS.length + 1}  ·  ${st.t.toLowerCase()}</div></div>
    ${muteHtml()}
    <div class="wy-panel wy-${side === 'left' ? 'left' : 'right'}">
      <div class="wd-k">${st.k}</div><div class="wd-t">${st.t}</div>
      <nav class="ts-nav">${st.id === 'pet'
        ? [['dog', 'Dog', 'Walks in and sits beside you'], ['cat', 'Cat', 'Walks in and sits beside you'], ['none', 'No pet', 'Just you']].map(([id, t, hint]) => `<button class="ts-btn ${id === (Wy.pet || 'none') ? 'on' : ''}" data-pet="${id}"><span class="ts-bi">${icon(id === 'none' ? 'close' : 'star')}</span><span class="ts-bt"><b>${t}</b><small>${hint}</small></span><i class="ts-sheen"></i></button>`).join('')
          + (Wy.pet === 'dog' || Wy.pet === 'cat' ? `<div class="wy-coats">${COATS[Wy.pet].map(([hex, name], i) => `<button class="wy-coat ${i === (Wy.coat || 0) ? 'on' : ''}" data-coat="${i}" title="${name}" style="--c:${hex}"><i></i><small>${name}</small></button>`).join('')}</div>` : '')
        : st.id === 'partner'
        ? [['married', 'Married', 'Your partner walks in beside you'], ['single', 'Single', 'On your own']].map(([id, t, hint]) => `<button class="ts-btn ${id === (Wy.partner || 'single') ? 'on' : ''}" data-partner="${id}"><span class="ts-bi">${icon(id === 'single' ? 'close' : 'star')}</span><span class="ts-bt"><b>${t}</b><small>${hint}</small></span><i class="ts-sheen"></i></button>`).join('')
          + `<button class="ts-btn" disabled><span class="ts-bi">${icon('grid')}</span><span class="ts-bt"><b>Their looks</b><small>with the character creator</small></span><i class="ts-sheen"></i></button>`
        : isDiff
        ? DIFFS.map(([id, t, hint]) => `<button class="ts-btn ${id === Wy.diff ? 'on' : ''}" data-diff="${id}"><span class="ts-bi">${icon('star')}</span><span class="ts-bt"><b>${t}</b><small>${hint}</small></span><i class="ts-sheen"></i></button>`).join('')
        : st.parts.map(([t, ic], i) => `<button class="ts-btn ${i === 0 ? 'on' : ''}" data-part="${t}"><span class="ts-bi">${icon(ic)}</span><span class="ts-bt"><b>${t}</b><small>soon</small></span><i class="ts-sheen"></i></button>`).join('')}
        <div class="wy-row">
          <button class="ts-btn wy-back" data-back="1"><span class="ts-bi">${icon('back')}</span><span class="ts-bt"><b>Back</b><small class="pad">B</small></span><i class="ts-sheen"></i></button>
          <button class="ts-btn wy-next ${last ? 'hot' : ''}" data-next="1"><span class="ts-bi">${icon('play')}</span><span class="ts-bt"><b>${last ? 'Start game' : 'Next'}</b><small><span class="pad">A · </span>${last ? 'Load your airline' : STEPS[n + 1].t}</small></span><i class="ts-sheen"></i></button>
        </div>
      </nav>
    </div>`;
}
function diffMove(d: number) {
  const i = Math.max(0, DIFFS.findIndex(x => x[0] === (Wy.diff || 'normal'))), j = Math.min(DIFFS.length - 1, Math.max(0, i + d));
  Wy.diff = DIFFS[j][0];
  if (Wy.root) Wy.root.querySelectorAll('[data-diff]').forEach((b: Element) => b.classList.toggle('on', b.getAttribute('data-diff') === Wy.diff));
}
/** Up and down through the step's options: the difficulty, the pet, married or single. */
function optMove(d: number) {
  const id = STEPS[Wy.step].id;
  if (id === 'difficulty') { diffMove(d); return; }
  if (id === 'pet') { const L = ['dog', 'cat', 'none'], i = Math.max(0, L.indexOf(Wy.pet || 'none')); Wy.pet = L[Math.min(L.length - 1, Math.max(0, i + d))]; Wy.coat = 0; renderStep(Wy.step, Wy.side); petTell(); return; }
  if (id === 'partner') { const L = ['married', 'single'], i = Math.max(0, L.indexOf(Wy.partner || 'single')); Wy.partner = L[Math.min(L.length - 1, Math.max(0, i + d))]; renderStep(Wy.step, Wy.side); partnerTell(); }
}
function stepTo(n: number) {
  if (n < 0) { tell('back'); Wy.stage = 'reverse'; if (Wy.root) Wy.root.innerHTML = ''; return; }   // the stage reverses; Unreal brings the choice back
  if (n >= STEPS.length) { musicEnd(false); tell('done', Wy.diff || 'normal'); return; }
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
      const y = (p.axes[1] || 0) + (p.buttons[12] && p.buttons[12].pressed ? -1 : 0) + (p.buttons[13] && p.buttons[13].pressed ? 1 : 0);
      if (Wy.stage === 'choose') { if (x < -0.5) hover('she'); else if (x > 0.5) hover('he'); }
      if (Wy.stage === 'step') { const dy = y < -0.5 ? -1 : y > 0.5 ? 1 : 0; if (dy && dy !== Wy.padY) optMove(dy); Wy.padY = dy; }
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
      if (Wy.stage === 'step') {
        const pe = t.closest('[data-pet]') as HTMLElement | null;
        if (pe) { Wy.pet = pe.getAttribute('data-pet') || 'none'; Wy.coat = 0; renderStep(Wy.step, Wy.side); petTell(); return; }
        const co = t.closest('[data-coat]') as HTMLElement | null;
        if (co) { Wy.coat = +(co.getAttribute('data-coat') || 0); renderStep(Wy.step, Wy.side); petTell(); return; }
        const pa = t.closest('[data-partner]') as HTMLElement | null;
        if (pa) { Wy.partner = pa.getAttribute('data-partner') || 'single'; renderStep(Wy.step, Wy.side); partnerTell(); return; }
        const d = t.closest('[data-diff]') as HTMLElement | null;
        if (d) { Wy.diff = d.getAttribute('data-diff') || 'normal'; Wy.root.querySelectorAll('[data-diff]').forEach((b: Element) => b.classList.toggle('on', b === d)); return; }
        if (t.closest('[data-next]')) stepTo(Wy.step + 1); else if (t.closest('[data-back]')) stepTo(Wy.step - 1);
      }
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
      else if (Wy.stage === 'step' && (e.key === 'ArrowUp' || e.key === 'ArrowDown')) { e.preventDefault(); optMove(e.key === 'ArrowUp' ? -1 : 1); }
      else if (Wy.stage === 'step' && STEPS[Wy.step].id === 'pet' && (e.key === 'ArrowLeft' || e.key === 'ArrowRight') && (Wy.pet === 'dog' || Wy.pet === 'cat')) { e.preventDefault(); const n = COATS[Wy.pet].length; Wy.coat = ((Wy.coat || 0) + (e.key === 'ArrowLeft' ? -1 : 1) + n) % n; renderStep(Wy.step, Wy.side); petTell(); }
    };
    window.addEventListener('keydown', Wy.key$, true);
    Wy.raf = requestAnimationFrame(padPoll);
    Wy.audio = splashTakeMusic();   // the title's music, playing on
    requestAnimationFrame(() => root.classList.add('in'));
  }
  Wy.side = stage === 'choose' ? Wy.side : side;
  if (stage === 'choose') { if (!side) { Wy.pet = 'none'; Wy.coat = 0; Wy.partner = 'single'; Wy.diff = ''; choicesOut(); } Wy.stage = 'choose'; Wy.hover = ''; Wy.chosen = false; Wy.root.classList.remove('chosen'); renderChoose(); if (side) hover(side); }
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
