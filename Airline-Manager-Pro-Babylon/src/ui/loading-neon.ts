// The new career's loading screen (owner, 2026-11-12), in the title screen's neon style: "Loading game assets", a long
// animated bar, under it (a little shorter) the file being loaded with the percentage at its end, and game tips at the
// bottom. It covers the game's own loader and whatever the game shows until the game says it is in and showing
// (onGameReady), and never less than a few seconds, so it can be read.
const Ld: any = { root: null as HTMLElement | null, t0: 0, raf: 0, pct: 0, ready: false, seen: 0, file: '', tip: 0, tipAt: 0 };

const TIPS = [
  'Hire the crew your first aircraft needs before you look at a second one.',
  'A used aircraft flies as well as a new one, with more hours on the clock and a lower price.',
  'Fuel bought at your bases is cheaper than fuel bought on the road.',
  'Your mechanics keep the fleet in condition; grounded aircraft earn nothing.',
  'Passengers remember delays. Keep a spare crew for the days somebody calls in sick.',
  'Loans are cheap on Easy and dear on Hard: borrow with the difficulty in mind.',
  'Your office wardrobe holds every outfit made for you: change there before a big day.',
  'Autosave keeps a copy every ten minutes; Save game keeps the one you choose.',
  'A route pays when the aircraft fits it: not too big, not too small.',
  'Check the weather before the first departure: the Faroes have their own ideas.',
];

function fileNames(): string[] {
  const names: string[] = [];
  const take = (w: any) => { try { const es = w.performance.getEntriesByType('resource'); for (let i = Ld.seen; i < es.length; i++) names.push(String(es[i].name).split('?')[0].split('/').pop() || ''); } catch (e) { /* alone */ } };
  const f = document.querySelector('.game-frame') as HTMLIFrameElement | null;
  if (f && f.contentWindow) { take(f.contentWindow); try { Ld.seen = f.contentWindow.performance.getEntriesByType('resource').length; } catch (e) { /* alone */ } }
  return names.filter(Boolean);
}

function frame() {
  const r = Ld.root; if (!r) return;
  const t = (performance.now() - Ld.t0) / 1000;
  // the bar: up to 92 % while it loads (eased, slower as it goes), the rest when the game says it is in
  const target = Ld.ready ? 100 : Math.min(92, 100 * (1 - Math.exp(-t / 4.5)));
  Ld.pct += (target - Ld.pct) * (Ld.ready ? 0.25 : 0.08);
  const pct = Math.round(Ld.pct);
  const names = fileNames(); if (names.length) Ld.file = names[names.length - 1];
  const fill = r.querySelector('.ln-fill') as HTMLElement, fl = r.querySelector('.ln-file') as HTMLElement, pc = r.querySelector('.ln-pct') as HTMLElement;
  if (fill) fill.style.width = pct + '%';
  if (fl) fl.textContent = Ld.file ? `Loading ${Ld.file}` : (Ld.ready ? 'Your airline is ready' : 'Loading game assets');
  if (pc) pc.textContent = pct + '%';
  if (t - Ld.tipAt > 5) { Ld.tipAt = t; Ld.tip = (Ld.tip + 1) % TIPS.length; const tp = r.querySelector('.ln-tip b') as HTMLElement; if (tp) { tp.classList.remove('in'); void tp.offsetWidth; tp.textContent = TIPS[Ld.tip]; tp.classList.add('in'); } }
  if (Ld.ready && Ld.pct > 99.5 && t > 3.2) { hide(); return; }
  Ld.raf = requestAnimationFrame(frame);
}

export function loadingNeonShow() {
  if (Ld.root) return;
  const root = document.createElement('div'); root.className = 'ln';
  root.innerHTML = `<div class="ln-fx"></div>
    <div class="ln-box">
      <div class="wd-k">Airport Dynasty Pro  ·  a new career</div><div class="ln-t">Loading game assets</div>
      <div class="ln-bar"><div class="ln-fill"></div><div class="ln-sheen"></div></div>
      <div class="ln-line"><span class="ln-file">Loading game assets</span><span class="ln-pct">0%</span></div>
    </div>
    <div class="ln-tip"><small>Tip</small><b class="in">${TIPS[0]}</b></div>`;
  document.body.appendChild(root); Ld.root = root; Ld.t0 = performance.now(); Ld.pct = 0; Ld.ready = false; Ld.seen = 0; Ld.file = ''; Ld.tip = 0; Ld.tipAt = 0;
  requestAnimationFrame(() => root.classList.add('in'));
  Ld.raf = requestAnimationFrame(frame);
}

/** The game is in and showing: the bar runs to the end, then the screen goes. */
export function loadingNeonDone() { Ld.ready = true; }

/** Gone at once (the game needs you first: its Found-your-airline form). */
export function loadingNeonHide() { hide(); }
function hide() {
  const r = Ld.root; if (!r) return;
  Ld.root = null; cancelAnimationFrame(Ld.raf);
  r.classList.remove('in'); r.classList.add('out');
  setTimeout(() => r.remove(), 700);
}
