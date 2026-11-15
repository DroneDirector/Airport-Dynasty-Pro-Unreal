// The update screen (owner, to-do 51): while the PC version downloads and installs an update, a screen in the new
// career's neon look - the same dark room glow, scan lines, Audiowide title with its neon light, the glass bar with its
// running sheen - instead of the small bar in Settings that flashed. Only the parts that change are touched each time
// (the bar's length, the numbers): nothing is redrawn, so nothing flashes. Unreal tells the page its state through
// window.__ueUpdate (settings-panel.ts passes it on here).
const Un: any = { root: null as HTMLElement | null, shown: 0, pct: 0, want: 0, raf: 0, tip: 0, tipAt: 0 };

const LINES = [
  'Only the parts that changed are downloaded.',
  'Your saves, your airline and your settings stay as they are.',
  'The game closes when the download is done, installs the update and starts again by itself.',
  'What the new version holds is in Settings, under What\'s new.',
];

function frame() {
  const r = Un.root; if (!r) return;
  // the bar glides to the newest figure (the figures arrive a few times a second; the bar moves every frame)
  Un.pct += (Un.want - Un.pct) * 0.18;
  const fill = r.querySelector('.ln-fill') as HTMLElement, pc = r.querySelector('.ln-pct') as HTMLElement;
  if (fill) fill.style.width = Un.pct.toFixed(2) + '%';
  if (pc) pc.textContent = Math.floor(Un.pct + 0.001) + '%';
  const t = (performance.now() - Un.shown) / 1000;
  if (t - Un.tipAt > 6) {
    Un.tipAt = t; Un.tip = (Un.tip + 1) % LINES.length;
    const tp = r.querySelector('.ln-tip b') as HTMLElement;
    if (tp) { tp.classList.remove('in'); void tp.offsetWidth; tp.textContent = LINES[Un.tip]; tp.classList.add('in'); }
  }
  Un.raf = requestAnimationFrame(frame);
}

function show(ver: string) {
  if (Un.root) return;
  const root = document.createElement('div'); root.className = 'ln un';
  root.innerHTML = `<div class="ln-fx"></div>
    <div class="ln-box">
      <div class="wd-k">Airport Dynasty Pro  ·  update</div><div class="ln-t">Updating to <span class="un-ver"></span></div>
      <div class="ln-bar"><div class="ln-fill"></div><div class="ln-sheen"></div></div>
      <div class="ln-line"><span class="ln-file">Starting the download</span><span class="ln-pct">0%</span></div>
      <div class="un-mbs"></div>
    </div>
    <div class="ln-tip"><small>Good to know</small><b class="in">${LINES[0]}</b></div>`;
  document.body.appendChild(root); Un.root = root; Un.shown = performance.now(); Un.pct = 0; Un.want = 0; Un.tip = 0; Un.tipAt = 0;
  const v = root.querySelector('.un-ver') as HTMLElement; if (v) v.textContent = ver;
  requestAnimationFrame(() => root.classList.add('in'));
  Un.raf = requestAnimationFrame(frame);
}

function hide() {
  const r = Un.root; if (!r) return;
  Un.root = null; cancelAnimationFrame(Un.raf);
  r.classList.remove('in'); r.classList.add('out');
  setTimeout(() => r.remove(), 700);
}

/** Unreal's updater state: the screen shows while it downloads or installs, and goes when it stops (done or failed). */
export function updateNeon(status: string, busy: boolean, frac: number, mbs: number) {
  const downloading = busy && /^Downloading/.test(status), installing = /^Installing/.test(status);
  if (!downloading && !installing) { hide(); return; }
  const m = /to (\d+\.\d+\.\d+)/.exec(status);
  show(m ? m[1] : '');
  const r = Un.root as HTMLElement;
  const v = r.querySelector('.un-ver') as HTMLElement; if (v && m && v.textContent !== m[1]) v.textContent = m[1];
  if (frac >= 0) Un.want = Math.max(Un.want, Math.min(100, frac * 100));
  if (installing) Un.want = 100;
  const fl = r.querySelector('.ln-file') as HTMLElement;
  const mb = /(\d+) of (\d+) MB/.exec(status), part = /\((\d+) of (\d+)\)/.exec(status);
  const text = installing ? 'Installing: the game closes and starts again' : mb ? `Downloading ${mb[1]} of ${mb[2]} MB` : part ? `Downloading part ${part[1]} of ${part[2]}` : 'Downloading';
  if (fl && fl.textContent !== text) fl.textContent = text;
  const sp = r.querySelector('.un-mbs') as HTMLElement;
  const st = installing ? '' : mbs > 0 ? `${mbs.toFixed(1)} MB/s` : '';
  if (sp && sp.textContent !== st) sp.textContent = st;
}
