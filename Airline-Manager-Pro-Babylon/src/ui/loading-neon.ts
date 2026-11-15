// The new career's loading screen (owner, 2026-11-12), in the title screen's neon style: "Loading game assets", a long
// animated bar, under it (a little shorter) the file being loaded with the percentage at its end, and game tips at the
// bottom. It covers the game's own loader and whatever the game shows until the game says it is in and showing
// (onGameReady), and never less than a few seconds, so it can be read.
const Ld: any = { root: null as HTMLElement | null, t0: 0, raf: 0, pct: 0, ready: false, seen: 0, file: '', tip: 0, tipAt: 0, ue: null as any[] | null, ueDone: false };

// what Unreal itself loads (the PC version, to-do 32h): you, your hair and the terminal, each with its real progress,
// sent by Unreal (UI/NewGameLoad.cpp) - the bar only finishes when they are in too
(window as any).__ueLoad = (list: any[], done: boolean) => { Ld.ue = list; Ld.ueDone = !!done; };

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
  const web = Ld.ready ? 100 : Math.min(92, 100 * (1 - Math.exp(-t / 4.5)));
  const ueAvg = Ld.ue && Ld.ue.length ? 100 * Ld.ue.reduce((a: number, r: any) => a + Math.min(1, +r.p || 0), 0) / Ld.ue.length : 100;
  const pcWait = /[?&]pc=/.test(location.search) && !Ld.ue;   // (the PC version: wait for Unreal's own list)
  const target = Ld.ue ? Math.min(web, Ld.ueDone ? 100 : Math.min(97, ueAvg)) : pcWait ? Math.min(web, 90) : web;
  Ld.pct += (target - Ld.pct) * (Ld.ready ? 0.25 : 0.08);
  const pct = Math.round(Ld.pct);
  const names = fileNames(); if (names.length) Ld.file = names[names.length - 1];
  const fill = r.querySelector('.ln-fill') as HTMLElement, fl = r.querySelector('.ln-file') as HTMLElement, pc = r.querySelector('.ln-pct') as HTMLElement;
  if (fill) fill.style.width = pct + '%';
  if (fl) fl.textContent = Ld.file ? `Loading ${Ld.file}` : (Ld.ready ? 'Your airline is ready' : 'Loading game assets');
  if (pc) pc.textContent = pct + '%';
  if (t - Ld.tipAt > 5) { Ld.tipAt = t; Ld.tip = (Ld.tip + 1) % TIPS.length; const tp = r.querySelector('.ln-tip b') as HTMLElement; if (tp) { tp.classList.remove('in'); void tp.offsetWidth; tp.textContent = TIPS[Ld.tip]; tp.classList.add('in'); } }
  const ul = r.querySelector('.ln-ue') as HTMLElement;
  if (ul && Ld.ue) ul.innerHTML = Ld.ue.map((x: any) => { const p = Math.round(100 * Math.min(1, +x.p || 0)); return `<span style="margin:0 10px;opacity:${p >= 100 ? 0.65 : 1}">${p >= 100 ? '&#10003; ' : ''}${String(x.n).replace(/[<>&]/g, '')} ${p}%</span>`; }).join('');
  // everything in: the bar full, then four seconds of "Loading complete · Get ready for takeoff" (owner, 2026-11-15)
  if (Ld.ready && (Ld.ue ? Ld.ueDone : !/[?&]pc=/.test(location.search)) && Ld.pct > 99.5 && t > 3.2) {
    if (!Ld.doneAt) {
      Ld.doneAt = performance.now(); Ld.pct = 100;
      const tl = r.querySelector('.ln-t') as HTMLElement; if (tl) tl.textContent = 'Loading complete';
      r.classList.add('done');
    }
    if (fl) fl.textContent = 'Get ready for takeoff';
    if (pc) pc.textContent = '100%';
    if (fill) fill.style.width = '100%';
    if (performance.now() - Ld.doneAt > 4000) { hide(); return; }
  }
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
      <div class="ln-ue" style="margin-top:10px;font-size:13px;letter-spacing:.04em;text-align:center;color:#9fe8ff"></div>
    </div>
    <div class="ln-tip"><small>Tip</small><b class="in">${TIPS[0]}</b></div>`;
  document.body.appendChild(root); Ld.root = root; Ld.t0 = performance.now(); Ld.pct = 0; Ld.ready = false; Ld.seen = 0; Ld.file = ''; Ld.tip = 0; Ld.tipAt = 0; Ld.doneAt = 0;
  // the PC version: Unreal loads the rest of the world now (the roads, the town, the land along the roads, you, your
  // hair, the terminal) and tells this screen each one's progress (window.__ueLoad)
  try { const u = (window as any).ue && (window as any).ue.adp; if (u && u.startload) u.startload(); } catch (e) { /* no link */ }
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


// ---- the game's start (owner, 2026-11-15): the same screen while Unreal builds the world, its bar only full when
// everything is in (Unreal sends the real progress: window.__ueBoot(list, done)), then four seconds of "Loading
// complete · Get ready for takeoff" before the title screen shows ----
const Bt: any = { root: null as HTMLElement | null, raf: 0, pct: 0, want: 0, list: [] as any[], done: false, doneAt: 0, tip: 0, tipAt: 0, t0: 0 };
function bootFrame() {
  const r = Bt.root; if (!r) return;
  const t = (performance.now() - Bt.t0) / 1000;
  // the bar follows the real progress (never ahead of it); full only when Unreal says everything is in
  Bt.pct += (Bt.want - Bt.pct) * 0.12;
  if (Bt.done && Bt.want >= 100 && Bt.pct > 99.5) Bt.pct = 100;
  const fill = r.querySelector('.ln-fill') as HTMLElement, fl = r.querySelector('.ln-file') as HTMLElement, pc = r.querySelector('.ln-pct') as HTMLElement, tl = r.querySelector('.ln-t') as HTMLElement;
  if (fill) fill.style.width = Bt.pct.toFixed(2) + '%';
  if (pc) pc.textContent = Math.floor(Bt.pct) + '%';
  const now = Bt.list.find((x: any) => (+x.p || 0) < 1);
  if (Bt.pct >= 100 && Bt.done) {
    if (!Bt.doneAt) { Bt.doneAt = performance.now(); if (tl) tl.textContent = 'Loading complete'; if (fl) fl.textContent = 'Get ready for takeoff'; r.classList.add('done'); }
    if (performance.now() - Bt.doneAt > 4000) { bootHide(); return; }
  } else if (fl) fl.textContent = now ? `Loading ${String(now.n).toLowerCase()}` : 'Loading the game';
  const ul = r.querySelector('.ln-ue') as HTMLElement;
  if (ul) ul.innerHTML = Bt.list.map((x: any) => { const p = Math.round(100 * Math.min(1, +x.p || 0)); return `<span style="margin:0 10px;opacity:${p >= 100 ? 0.65 : 1}">${p >= 100 ? '&#10003; ' : ''}${String(x.n).replace(/[<>&]/g, '')} ${p}%</span>`; }).join('');
  if (t - Bt.tipAt > 5 && !Bt.doneAt) { Bt.tipAt = t; Bt.tip = (Bt.tip + 1) % TIPS.length; const tp = r.querySelector('.ln-tip b') as HTMLElement; if (tp) { tp.classList.remove('in'); void tp.offsetWidth; tp.textContent = TIPS[Bt.tip]; tp.classList.add('in'); } }
  Bt.raf = requestAnimationFrame(bootFrame);
}
function bootShow() {
  if (Bt.root) return;
  const root = document.createElement('div'); root.className = 'ln boot';
  root.innerHTML = `<div class="ln-fx"></div>
    <div class="ln-box">
      <div class="wd-k">Airport Dynasty Pro  ·  starting</div><div class="ln-t">Loading the airport</div>
      <div class="ln-bar"><div class="ln-fill"></div><div class="ln-sheen"></div></div>
      <div class="ln-line"><span class="ln-file">Loading the game</span><span class="ln-pct">0%</span></div>
      <div class="ln-ue" style="margin-top:10px;font-size:13px;letter-spacing:.04em;text-align:center;color:#9fe8ff"></div>
    </div>
    <div class="ln-tip"><small>Tip</small><b class="in">${TIPS[0]}</b></div>`;
  document.body.appendChild(root); Bt.root = root; Bt.t0 = performance.now(); Bt.tipAt = 0;
  root.classList.add('in');
  Bt.raf = requestAnimationFrame(bootFrame);
}
function bootHide() {
  const r = Bt.root; if (!r) return;
  Bt.root = null; cancelAnimationFrame(Bt.raf);
  r.classList.remove('in'); r.classList.add('out');
  setTimeout(() => r.remove(), 700);
}
(window as any).__ueBoot = (list: any[], done: boolean) => {
  if (!Bt.root && !Bt.doneAt) bootShow();   // (in case the page started without it)
  Bt.list = list || []; Bt.done = !!done;
  const avg = Bt.list.length ? 100 * Bt.list.reduce((a: number, x: any) => a + Math.min(1, +x.p || 0), 0) / Bt.list.length : 100;
  Bt.want = done ? 100 : Math.min(99, avg);
};
