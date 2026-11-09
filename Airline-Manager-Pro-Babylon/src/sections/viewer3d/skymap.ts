// The holographic map while following a flight in the 3D view, in the bottom-left corner (the same look as the
// airport's map). Outside the aircraft: the land round it as far as you can see from outside (the ground the 3D view
// has loaded, about 20 km each way), coastlines as glowing lines and the land inside them in a darker holographic
// blue, turned so the aircraft points up. Inside: the aircraft's own plan (fuselage, seats, wings, tail) with you on
// it, moving as you walk the cabin. Tap it for the full-screen map: the whole world's coastlines (the game's own
// outlines), zoom and pan, and close to the aircraft the same detailed ground as the corner map. Kept light: the land is worked out once a second, from heights already loaded.
import { IS_TOUCH } from '../../core/app';
import { seatAt, CABIN_ROWS, CABIN_Z } from './cabin';
import type { Terrain } from './terrain';
import { LAND, WATER, ISLETS } from '../../data/world-land';

export interface SkyState { lat: number; lon: number; hdg: number; ter: Terrain | null; cabin: boolean; me: { x: number; z: number } | null }
const D2R = Math.PI / 180, RANGE = 20000, N = 64;

// #region ── The land round the aircraft (worked out once a second) ──
interface Land { lat: number; lon: number; cell: Int8Array }   // 1 land, 0 sea, -1 not loaded
function landAround(lat: number, lon: number, ter: Terrain): Land {
  const cell = new Int8Array(N * N), kx = 111320 * Math.cos(lat * D2R), kz = 110540, step = 2 * RANGE / N;
  for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
    const e = -RANGE + (i + 0.5) * step, n = RANGE - (j + 0.5) * step, h = ter.heightAt(lat + n / kz, lon + e / kx);
    cell[j * N + i] = h == null ? -1 : h > 0.5 ? 1 : 0;
  }
  return { lat, lon, cell };
}
// #endregion

export function skyMap(host: HTMLElement, get: () => SkyState | null) {
  const box = document.createElement('div'); box.className = 'holomap sky-map';
  box.innerHTML = '<canvas></canvas>'; host.appendChild(box);
  const cv = box.querySelector('canvas') as HTMLCanvasElement, c = cv.getContext('2d')!;
  let land: Land | null = null, landT = 0; const t0 = performance.now();

  // #region ── Drawing: the frame ──
  const frame = (W: number, H: number, dpr: number, label: string) => {
    c.strokeStyle = 'rgba(110,200,255,.45)'; c.lineWidth = dpr; c.strokeRect(.5 * dpr, .5 * dpr, W - dpr, H - dpr);
    c.strokeStyle = 'rgba(170,230,255,.95)'; c.lineWidth = 2 * dpr; const L = 14 * dpr;
    for (const [x, y, dx, dy] of [[0, 0, 1, 1], [W, 0, -1, 1], [0, H, 1, -1], [W, H, -1, -1]]) { c.beginPath(); c.moveTo(x + dx * dpr, y + dy * L); c.lineTo(x + dx * dpr, y + dy * dpr); c.lineTo(x + dx * L, y + dy * dpr); c.stroke(); }
    c.textAlign = 'left'; c.textBaseline = 'top'; c.font = `700 ${10 * dpr}px Inter, sans-serif`; c.fillStyle = 'rgba(205,240,255,.95)'; c.shadowColor = 'rgba(40,150,255,.9)'; c.shadowBlur = IS_TOUCH ? 0 : 5 * dpr; c.fillText(label, 10 * dpr, 9 * dpr); c.shadowBlur = 0;
  };
  const back = (W: number, H: number) => { const bg = c.createRadialGradient(W / 2, H / 2, W * 0.05, W / 2, H / 2, W * 0.75); bg.addColorStop(0, 'rgba(10,40,70,.78)'); bg.addColorStop(1, 'rgba(3,12,26,.86)'); c.fillStyle = bg; c.fillRect(0, 0, W, H); };
  // #endregion

  // #region ── Drawing: outside (the land and the aircraft) ──
  const outside = (W: number, H: number, dpr: number, st: SkyState) => {
    const k = Math.min(W, H) / 2 / RANGE, kx = 111320 * Math.cos(st.lat * D2R), kz = 110540, step = 2 * RANGE / N;
    c.save(); c.translate(W / 2, H / 2); c.rotate(-st.hdg * D2R);
    // the grid every 5 km
    c.strokeStyle = 'rgba(90,180,255,.08)'; c.lineWidth = dpr; c.beginPath();
    for (let g = -RANGE; g <= RANGE; g += 5000) { c.moveTo(g * k, -RANGE * k); c.lineTo(g * k, RANGE * k); c.moveTo(-RANGE * k, g * k); c.lineTo(RANGE * k, g * k); } c.stroke();
    if (land) {
      // where the land was worked out from, against where the aircraft is now
      const ox = (land.lon - st.lon) * kx * k, oy = -(land.lat - st.lat) * kz * k, s = step * k, at = (i: number, j: number) => (i < 0 || j < 0 || i >= N || j >= N) ? -1 : land!.cell[j * N + i];
      c.fillStyle = 'rgba(30,90,170,.42)'; c.beginPath();
      for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) if (at(i, j) === 1) c.rect(ox + (-RANGE + i * step) * k, oy + (-RANGE + j * step) * k, s + 0.5, s + 0.5);
      c.fill();
      // the coast: the edges between land and sea
      c.strokeStyle = 'rgba(150,220,255,.95)'; c.lineWidth = 1.4 * dpr; c.shadowColor = 'rgba(50,160,255,.9)'; c.shadowBlur = IS_TOUCH ? 0 : 6 * dpr; c.beginPath();
      for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) { if (at(i, j) !== 1) continue; const x = ox + (-RANGE + i * step) * k, y = oy + (-RANGE + j * step) * k;
        if (at(i - 1, j) === 0) { c.moveTo(x, y); c.lineTo(x, y + s); } if (at(i + 1, j) === 0) { c.moveTo(x + s, y); c.lineTo(x + s, y + s); }
        if (at(i, j - 1) === 0) { c.moveTo(x, y); c.lineTo(x + s, y); } if (at(i, j + 1) === 0) { c.moveTo(x, y + s); c.lineTo(x + s, y + s); } }
      c.stroke(); c.shadowBlur = 0;
    }
    c.restore();
    // the aircraft, pointing up
    const pulse = 0.75 + 0.25 * Math.sin((performance.now() - t0) / 260), s = 9 * dpr; c.save(); c.translate(W / 2, H / 2);
    c.fillStyle = '#EAF8FF'; c.shadowColor = 'rgba(120,220,255,1)'; c.shadowBlur = IS_TOUCH ? 0 : 10 * dpr * pulse; c.beginPath();
    c.moveTo(0, -s); c.lineTo(s * 0.18, -s * 0.3); c.lineTo(s, s * 0.15); c.lineTo(s, s * 0.35); c.lineTo(s * 0.18, s * 0.1); c.lineTo(s * 0.14, s * 0.7); c.lineTo(s * 0.45, s); c.lineTo(-s * 0.45, s); c.lineTo(-s * 0.14, s * 0.7); c.lineTo(-s * 0.18, s * 0.1); c.lineTo(-s, s * 0.35); c.lineTo(-s, s * 0.15); c.lineTo(-s * 0.18, -s * 0.3); c.closePath(); c.fill(); c.restore(); c.shadowBlur = 0;
    c.textAlign = 'right'; c.textBaseline = 'bottom'; c.font = `600 ${9 * dpr}px Inter, sans-serif`; c.fillStyle = 'rgba(160,215,255,.7)'; c.fillText('10 km', W - 9 * dpr, H - 7 * dpr);
  };
  // #endregion

  // #region ── Drawing: inside (the aircraft's plan and you) ──
  const inside = (W: number, H: number, dpr: number, st: SkyState) => {
    const f = CABIN_Z.front, b = CABIN_Z.back, sg = Math.sign(f - b) || 1, nose = f + sg * 7, tail = b - sg * 9, mid = (f + b) / 2;
    const len = Math.abs(nose - tail), k = Math.min(H * 0.9 / len, W * 0.9 / 36);
    const P = (x: number, z: number): [number, number] => [W / 2 + x * k, H / 2 - (z - (nose + tail) / 2) * sg * k];
    const poly = (pts: [number, number][], fill: string, stroke: string) => { c.beginPath(); pts.forEach((p, i) => { const q = P(p[0], p[1]); if (i) c.lineTo(q[0], q[1]); else c.moveTo(q[0], q[1]); }); c.closePath(); c.fillStyle = fill; c.fill(); c.strokeStyle = stroke; c.lineWidth = 1.3 * dpr; c.stroke(); };
    c.shadowColor = 'rgba(50,160,255,.9)'; c.shadowBlur = IS_TOUCH ? 0 : 6 * dpr;
    // the wings and the tailplane (under the fuselage)
    const wz = mid - sg * 1.5;
    poly([[1.9, wz + sg * 3], [17.9, wz - sg * 4.5], [17.9, wz - sg * 6], [1.9, wz - sg * 3.5]], 'rgba(30,90,170,.35)', 'rgba(140,215,255,.85)');
    poly([[-1.9, wz + sg * 3], [-17.9, wz - sg * 4.5], [-17.9, wz - sg * 6], [-1.9, wz - sg * 3.5]], 'rgba(30,90,170,.35)', 'rgba(140,215,255,.85)');
    poly([[0.8, tail + sg * 4], [6.2, tail + sg * 1.2], [6.2, tail + sg * 0.2], [0.8, tail + sg * 1.6]], 'rgba(30,90,170,.35)', 'rgba(140,215,255,.85)');
    poly([[-0.8, tail + sg * 4], [-6.2, tail + sg * 1.2], [-6.2, tail + sg * 0.2], [-0.8, tail + sg * 1.6]], 'rgba(30,90,170,.35)', 'rgba(140,215,255,.85)');
    // the fuselage: a rounded nose, the straight cabin, the tail cone
    c.beginPath(); { const a = P(-1.98, f + sg * 1), bb = P(1.98, f + sg * 1), n = P(0, nose), tl = P(0, tail), ra = P(1.98, b - sg * 1.5), la = P(-1.98, b - sg * 1.5);
      c.moveTo(a[0], a[1]); c.quadraticCurveTo(a[0], n[1], n[0], n[1]); c.quadraticCurveTo(bb[0], n[1], bb[0], bb[1]); c.lineTo(ra[0], ra[1]); c.quadraticCurveTo(ra[0] - 0.6 * k, tl[1], tl[0], tl[1]); c.quadraticCurveTo(la[0] + 0.6 * k, tl[1], la[0], la[1]); c.closePath(); }
    c.fillStyle = 'rgba(14,50,96,.75)'; c.fill(); c.strokeStyle = 'rgba(160,230,255,.95)'; c.lineWidth = 1.6 * dpr; c.stroke(); c.shadowBlur = 0;
    // the seats
    c.fillStyle = 'rgba(120,200,255,.55)';
    for (let r = 0; r < CABIN_ROWS; r++) for (let s = 0; s < 6; s++) { const p = seatAt(r, s), q = P(p.x, p.z); c.fillRect(q[0] - 0.2 * k, q[1] - 0.2 * k, 0.4 * k, 0.4 * k); }
    // you
    if (st.me) { const q = P(st.me.x, st.me.z), pulse = 0.5 + 0.5 * Math.sin((performance.now() - t0) / 220);
      c.strokeStyle = `rgba(255,214,140,${0.4 + 0.4 * pulse})`; c.lineWidth = 1.4 * dpr; c.beginPath(); c.arc(q[0], q[1], (5 + 3 * pulse) * dpr, 0, Math.PI * 2); c.stroke();
      c.fillStyle = '#FFE2AE'; c.shadowColor = 'rgba(255,170,60,1)'; c.shadowBlur = IS_TOUCH ? 0 : 8 * dpr; c.beginPath(); c.arc(q[0], q[1], 3 * dpr, 0, Math.PI * 2); c.fill(); c.shadowBlur = 0; }
  };
  // #endregion

  const draw = () => {
    if (!box.isConnected) { clearInterval(iv); return; }
    const st = get(); if (!st) { box.style.display = 'none'; return; } box.style.display = '';
    const now = performance.now();
    if (!st.cabin && st.ter && now > landT) { landT = now + 1000; try { land = landAround(st.lat, st.lon, st.ter); } catch (e) { /* the ground not in yet */ } }
    const dpr = Math.min(2, window.devicePixelRatio || 1), W = Math.round(box.clientWidth * dpr), H = Math.round(box.clientHeight * dpr);
    if (!W || !H) return; if (cv.width !== W || cv.height !== H) { cv.width = W; cv.height = H; }
    c.clearRect(0, 0, W, H); back(W, H); c.save(); c.beginPath(); c.rect(0, 0, W, H); c.clip();
    if (st.cabin) inside(W, H, dpr, st); else outside(W, H, dpr, st);
    c.restore(); frame(W, H, dpr, st.cabin ? 'CABIN' : 'OUTSIDE');
  };
  const iv = setInterval(draw, IS_TOUCH ? 100 : 50);

  // #region ── The full-screen map (tap the corner map) ──
  let big: HTMLElement | null = null;
  const openBig = () => {
    if (big) return; const st0 = get(); if (!st0) return;
    const ov = document.createElement('div'); ov.className = 'sky-big'; big = ov;
    ov.innerHTML = `<canvas></canvas><div class="sky-big-bar"><button class="wx-b" data-z="-1">−</button><button class="wx-b" data-z="1">+</button><button class="wx-b" data-c>Centre</button><button class="wx-b" data-x>✕ Close</button></div>`;
    document.body.appendChild(ov);
    const bc = ov.querySelector('canvas') as HTMLCanvasElement, g = bc.getContext('2d')!;
    const M = { lat: st0.lat, lon: st0.lon, k: 0.25, follow: true };   // k: pixels per km (css)
    const close = () => { if (!big) return; big.remove(); big = null; window.removeEventListener('keydown', key, true); };
    const key = (e: KeyboardEvent) => { if (e.key === 'Escape') { e.stopPropagation(); close(); } };
    window.addEventListener('keydown', key, true);
    const zoom = (f: number) => { M.k = Math.max(0.004, Math.min(40, M.k * f)); };
    ov.querySelector('.sky-big-bar')!.addEventListener('click', e => { const b = (e.target as HTMLElement).closest('button'); if (!b) return; e.stopPropagation();
      if (b.hasAttribute('data-x')) close(); else if (b.hasAttribute('data-c')) M.follow = true; else zoom(Number(b.getAttribute('data-z')) > 0 ? 1.6 : 1 / 1.6); });
    // drag to look round, pinch or the wheel to zoom
    const pts = new Map<number, { x: number; y: number }>(); let pinch = 0;
    bc.addEventListener('pointerdown', e => { bc.setPointerCapture(e.pointerId); pts.set(e.pointerId, { x: e.clientX, y: e.clientY }); });
    bc.addEventListener('pointermove', e => { const p = pts.get(e.pointerId); if (!p) return;
      if (pts.size === 1) { const kx = 111.32 * Math.cos(M.lat * D2R) * M.k, kz = 110.54 * M.k; M.lon -= (e.clientX - p.x) / kx; M.lat = Math.max(-85, Math.min(85, M.lat + (e.clientY - p.y) / kz)); M.follow = false; }
      p.x = e.clientX; p.y = e.clientY;
      if (pts.size === 2) { const [a, b] = [...pts.values()], d = Math.hypot(a.x - b.x, a.y - b.y); if (pinch) zoom(d / pinch); pinch = d; } });
    const up = (e: PointerEvent) => { pts.delete(e.pointerId); if (pts.size < 2) pinch = 0; };
    bc.addEventListener('pointerup', up); bc.addEventListener('pointercancel', up);
    bc.addEventListener('wheel', e => { e.preventDefault(); zoom(Math.exp(-e.deltaY * 0.0015)); }, { passive: false });
    const frameBig = () => {
      if (!big) return; requestAnimationFrame(frameBig);
      const st = get(); if (!st) return; if (M.follow) { M.lat = st.lat; M.lon = st.lon; }
      const dpr = Math.min(2, window.devicePixelRatio || 1), W = Math.round(bc.clientWidth * dpr), H = Math.round(bc.clientHeight * dpr);
      if (!W || !H) return; if (bc.width !== W || bc.height !== H) { bc.width = W; bc.height = H; }
      const kx = 111.32 * Math.cos(M.lat * D2R) * M.k * dpr, kz = 110.54 * M.k * dpr, X = (lo: number) => W / 2 + (lo - M.lon) * kx, Y = (la: number) => H / 2 - (la - M.lat) * kz;
      const bg = g.createRadialGradient(W / 2, H / 2, 0, W / 2, H / 2, Math.max(W, H) * 0.7); bg.addColorStop(0, '#0A2846'); bg.addColorStop(1, '#030C1A'); g.fillStyle = bg; g.fillRect(0, 0, W, H);
      // the grid of latitude and longitude
      const span = W / kx, stp = span > 120 ? 10 : span > 30 ? 5 : span > 6 ? 1 : 0.25;
      g.strokeStyle = 'rgba(90,180,255,.10)'; g.lineWidth = dpr; g.beginPath();
      for (let lo = Math.floor((M.lon - span) / stp) * stp; lo < M.lon + span; lo += stp) { g.moveTo(X(lo), 0); g.lineTo(X(lo), H); }
      const vs = H / kz; for (let la = Math.floor((M.lat - vs) / stp) * stp; la < M.lat + vs; la += stp) { g.moveTo(0, Y(la)); g.lineTo(W, Y(la)); } g.stroke();
      // the world's land: a darker holographic blue with glowing coasts; inland seas cut out
      const path = (poly: number[][]) => { g.beginPath(); poly.forEach((q, i) => { const x = X(q[0]), y = Y(q[1]); if (i) g.lineTo(x, y); else g.moveTo(x, y); }); g.closePath(); };
      g.shadowColor = 'rgba(50,160,255,.9)'; g.shadowBlur = IS_TOUCH ? 0 : 8 * dpr;
      for (const poly of LAND) { path(poly); g.fillStyle = 'rgba(30,90,170,.40)'; g.fill(); g.strokeStyle = 'rgba(150,220,255,.9)'; g.lineWidth = 1.3 * dpr; g.stroke(); }
      for (const poly of WATER) { path(poly); g.fillStyle = '#071A30'; g.fill(); g.stroke(); }
      g.shadowBlur = 0; g.fillStyle = 'rgba(150,220,255,.85)';
      for (const q of ISLETS) { g.beginPath(); g.arc(X(q[0]), Y(q[1]), Math.max(1.5 * dpr, 6 * M.k * dpr), 0, Math.PI * 2); g.fill(); }
      // close in: the detailed ground round the aircraft, as in the corner map
      if (land && span < 150) { const s2 = 2 * RANGE / N / 1000, ox = X(land.lon), oy = Y(land.lat), ppkx = kx, ppkz = kz, at = (i: number, j: number) => (i < 0 || j < 0 || i >= N || j >= N) ? -1 : land!.cell[j * N + i];
        const lonPerKm = 1 / (111.32 * Math.cos(land.lat * D2R)), latPerKm = 1 / 110.54, cx = (i: number) => ox + (-RANGE / 1000 + i * s2) * lonPerKm * ppkx, cy = (j: number) => oy + (-RANGE / 1000 + j * s2) * latPerKm * ppkz;
        g.fillStyle = 'rgba(40,110,200,.55)'; g.beginPath(); for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) if (at(i, j) === 1) g.rect(cx(i), cy(j), cx(i + 1) - cx(i) + 0.5, cy(j + 1) - cy(j) + 0.5); g.fill();
        g.strokeStyle = 'rgba(190,235,255,.95)'; g.lineWidth = 1.2 * dpr; g.beginPath();
        for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) { if (at(i, j) !== 1) continue; const x0 = cx(i), x1 = cx(i + 1), y0 = cy(j), y1 = cy(j + 1);
          if (at(i - 1, j) === 0) { g.moveTo(x0, y0); g.lineTo(x0, y1); } if (at(i + 1, j) === 0) { g.moveTo(x1, y0); g.lineTo(x1, y1); } if (at(i, j - 1) === 0) { g.moveTo(x0, y0); g.lineTo(x1, y0); } if (at(i, j + 1) === 0) { g.moveTo(x0, y1); g.lineTo(x1, y1); } }
        g.stroke(); }
      // the aircraft
      const ax = X(st.lon), ay = Y(st.lat), s = 11 * dpr, pulse = 0.5 + 0.5 * Math.sin(performance.now() / 260);
      g.strokeStyle = `rgba(255,214,140,${0.35 + 0.4 * pulse})`; g.lineWidth = 1.4 * dpr; g.beginPath(); g.arc(ax, ay, (14 + 6 * pulse) * dpr, 0, Math.PI * 2); g.stroke();
      g.save(); g.translate(ax, ay); g.rotate(st.hdg * D2R); g.fillStyle = '#FFE2AE'; g.shadowColor = 'rgba(255,170,60,1)'; g.shadowBlur = IS_TOUCH ? 0 : 10 * dpr; g.beginPath();
      g.moveTo(0, -s); g.lineTo(s * 0.18, -s * 0.3); g.lineTo(s, s * 0.15); g.lineTo(s, s * 0.35); g.lineTo(s * 0.18, s * 0.1); g.lineTo(s * 0.14, s * 0.7); g.lineTo(s * 0.45, s); g.lineTo(-s * 0.45, s); g.lineTo(-s * 0.14, s * 0.7); g.lineTo(-s * 0.18, s * 0.1); g.lineTo(-s, s * 0.35); g.lineTo(-s, s * 0.15); g.lineTo(-s * 0.18, -s * 0.3); g.closePath(); g.fill(); g.restore(); g.shadowBlur = 0;
      // where it is, and the scale
      g.font = `600 ${11 * dpr}px Inter, sans-serif`; g.fillStyle = 'rgba(200,235,255,.9)'; g.textAlign = 'left'; g.textBaseline = 'top';
      g.fillText(`${Math.abs(st.lat).toFixed(2)}°${st.lat >= 0 ? 'N' : 'S'}  ${Math.abs(st.lon).toFixed(2)}°${st.lon >= 0 ? 'E' : 'W'}  ·  heading ${Math.round((st.hdg % 360 + 360) % 360)}°`, 16 * dpr, 16 * dpr);
      const km = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000].find(v => v * M.k * dpr > 80 * dpr) || 5000, bw = km * M.k * dpr;
      g.strokeStyle = 'rgba(200,235,255,.9)'; g.lineWidth = 2 * dpr; g.beginPath(); g.moveTo(16 * dpr, H - 20 * dpr); g.lineTo(16 * dpr + bw, H - 20 * dpr); g.stroke(); g.textBaseline = 'bottom'; g.fillText(`${km} km`, 16 * dpr, H - 26 * dpr);
    };
    requestAnimationFrame(frameBig);
  };
  box.style.pointerEvents = 'auto'; box.style.cursor = 'pointer';
  box.addEventListener('click', e => { e.stopPropagation(); openBig(); });
  // #endregion
  return { dispose() { clearInterval(iv); box.remove(); if (big) { big.remove(); big = null; } } };
}
