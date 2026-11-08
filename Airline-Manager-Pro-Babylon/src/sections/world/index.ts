// Walking anywhere in the world, opened from the world map (the walking man under the 3D button, then "Place in
// world"): you stand on the real ground where the map's X was, with the real heights and the satellite picture round
// you (the same ground as the 3D viewer: Terrain Tiles on AWS and Sentinel-2 cloudless 2016 by EOX), and walk freely
// (W A S D, the stick, a controller). The loading screen stays until the ground and its pictures round you are in.
import { Scene, Vector3, Color3, Color4, UniversalCamera, HemisphericLight, DirectionalLight, DefaultRenderingPipeline, ImageProcessingConfiguration } from '@babylonjs/core';
import { registerSection, Section, SectionCtx } from '../../core/sections';
import { S } from '../../core/settings';
import { runLoop } from '../../core/engine';
import { inputFrame } from '../../core/input';
import { IS_TOUCH } from '../../core/app';
import { toast, esc } from '../../ui/icons';
import { buildTerrain, groundAt, Terrain } from '../viewer3d/terrain';
import { holoMap } from '../../ui/holomap';
import { makePhotoMode } from '../viewer3d/photo';
import { savePhoto } from '../../core/photo';
import { loadingShow, loadingProgress, loadingHide } from '../../ui/loading';

const EYE = 1.65, R = 6371000, D2R = Math.PI / 180, REACH = 17000;
const O: any = { scene: null as Scene | null };

function apply(what: string) {
  const sc: Scene = O.scene; if (!sc) return; const g = S.gfx, cam: UniversalCamera = O.cam, all = what === 'all' || what === 'preset';
  if (all || what === 'fov') cam.fov = g.fov * Math.PI / 180;
  const dp: DefaultRenderingPipeline = O.dp;
  if (all || what === 'aa') { dp.fxaaEnabled = g.aa === 'fxaa'; dp.samples = g.aa === 'msaa' ? 4 : 1; }
  if (all || what === 'aces' || what === 'exposure') { dp.imageProcessingEnabled = true; const ip = dp.imageProcessing; ip.toneMappingEnabled = g.aces; ip.toneMappingType = ImageProcessingConfiguration.TONEMAPPING_ACES; ip.exposure = g.exposure; }
  if (all || what === 'sharpen') { dp.sharpenEnabled = g.sharpen; dp.sharpen.edgeAmount = 0.22; }
}

const World$: Section = {
  id: 'world', title: 'Placing avatar in world',
  async open(ctx: SectionCtx) {
    const { engine, ui, progress } = ctx; O.ctx = ctx;
    const lat = Math.max(-84, Math.min(84, Number(ctx.opts && ctx.opts.lat) || 0)), lon = Number(ctx.opts && ctx.opts.lon) || 0;
    progress(5, 'Placing avatar in world…'); await new Promise(r => setTimeout(r, 30));
    const sc = new Scene(engine); O.scene = sc; sc.skipPointerMovePicking = true;
    const sky = Color3.FromHexString('#9CC4E8'); sc.clearColor = new Color4(sky.r, sky.g, sky.b, 1);
    sc.fogMode = Scene.FOGMODE_EXP2; sc.fogDensity = 0.000045; sc.fogColor = sky;
    const hemi = new HemisphericLight('hemi', new Vector3(0, 1, 0), sc); hemi.intensity = 0.75; hemi.groundColor = Color3.FromHexString('#5A5040');
    const sun = new DirectionalLight('sun', new Vector3(0.35, -0.8, 0.45).normalize(), sc); sun.intensity = 2.2; sun.diffuse = Color3.FromHexString('#FFF3E2');
    const cam = new UniversalCamera('eyes', new Vector3(0, EYE, 0), sc); cam.minZ = 0.1; cam.maxZ = 60000; cam.inertia = 0; O.cam = cam; sc.activeCamera = cam;
    O.dp = new DefaultRenderingPipeline('dp', true, sc, [cam]);
    apply('all');

    // the ground round the spot; the loading screen stays until its heights and its satellite pictures are in
    progress(12, 'Loading the ground…');
    const T: Terrain = buildTerrain(sc, lat, lon); O.T = T; T.root.position.set(0, 0, 0);
    const t0 = performance.now();
    await new Promise<void>(done => { const iv = setInterval(() => {
      const tiles = T.tiles.length || 25, pics = T.mats.filter(m => m.albedoTexture && m.albedoTexture.isReady()).length;
      progress(12 + Math.round(80 * (T.ready + pics) / (tiles * 2)), T.ready < tiles ? 'Loading the ground…' : 'Loading the satellite pictures…');
      const waited = performance.now() - t0;
      if ((T.ready >= tiles && pics >= tiles) || (T.ready >= tiles && waited > 20000) || waited > 45000 || sc.isDisposed) { clearInterval(iv); done(); } }, 200); });
    if (sc.isDisposed) return;
    progress(96, 'Placing avatar in world…');

    // the ground's height under a point (east x, north z, metres from the spot), as the ground is drawn
    const kx = 111320 * Math.cos(lat * D2R), kz = 110540;
    // the drawn surface's own height (never the finer picture under it, which can lie metres above or below a slope);
    // the highest of a few points round your feet, so a slope never comes up through them
    const surf = (x: number, z: number) => { const h = groundAt(lat + z / kz, lon + x / kx); return (h == null ? 0 : h) - (x * x + z * z) / (2 * R) - 1.2; };
    const ground = (x: number, z: number) => Math.max(surf(x, z), surf(x + 0.3, z), surf(x - 0.3, z), surf(x, z + 0.3), surf(x, z - 0.3));
    const F = { x: 0, z: 0, yaw: 0, pitch: -0.05, v: 0, bob: 0 }; O.F = F;
    const sea = (T.heightAt(lat, lon) ?? 0) <= 0;

    const hud = document.createElement('div'); hud.className = 'hud wd-world';
    // only what walking needs: the way back (top left), the moves (bottom right) and the holographic map (bottom left)
    hud.innerHTML = `<div class="wd-top"><button class="wx-b" data-back>‹ Back to the overview</button><button class="wx-b" data-cine>Cinema</button><button class="wx-b" data-hide>Hide UI</button></div>
      <button class="wx-b wd-rec" data-rec style="display:none">● Record video</button><button class="wx-b wd-xc" data-xc style="display:none">✕ Exit cinema</button>
      <div class="site-moves">${[['jump', 'Jump'], ['crouch', 'Crouch'], ['prone', 'Prone'], ['sprint', 'Run']].map(([k, l]) => `<button class="wx-b" data-mv="${k}">${l}</button>`).join('')}</div>`;
    ui.appendChild(hud); O.hud = hud;
    // the holographic map in the bottom-left corner, as in the airport (only the airport is drawn on it so far, so out
    // here it shows its grid round you; more is added to it later)
    holoMap(hud, () => ({ x: F.x + 1e6, z: F.z + 1e6, f: 1 as any, yaw: F.yaw }));
    // leaving: a loading screen while the Overview comes back (the Back button, or Esc with the controls showing)
    const leave = () => { if (O.leaving) return; O.leaving = true; loadingShow('Loading Overview UI'); loadingProgress(35, 'Loading Overview UI…');
      setTimeout(() => { if (ctx.toGame) ctx.toGame('ops'); else if (ctx.back) ctx.back(); loadingProgress(100, 'Ready'); setTimeout(() => loadingHide(), 450); }, 260); };
    hud.querySelector('[data-back]')!.addEventListener('click', leave);
    // the stances and moves: standing, crouching, lying prone, a jump, running (buttons, keys and a controller's buttons)
    const ST: { [k: string]: { eye: number; walk: number; run: number } } = { stand: { eye: EYE, walk: 1.6, run: 6.2 }, crouch: { eye: 1.0, walk: 0.9, run: 1.6 }, prone: { eye: 0.32, walk: 0.35, run: 0.5 } };
    const W = { stance: 'stand', eye: EYE, vy: 0, jy: 0, run: false, fy: ground(0, 0) + 1.5, fv: 0 }; O.W = W;
    const setStance = (k: string) => { W.stance = W.stance === k ? 'stand' : k; hud.querySelectorAll('[data-mv="crouch"],[data-mv="prone"]').forEach(b => b.classList.toggle('on', b.getAttribute('data-mv') === W.stance)); };
    const jump = () => { if (W.jy > 0.001) return; if (W.stance !== 'stand') { setStance(W.stance); return; } W.vy = 4.4; };
    const setRun = (on: boolean) => { W.run = on; const b = hud.querySelector('[data-mv="sprint"]'); if (b) b.classList.toggle('on', on); };
    hud.querySelector('.site-moves')!.addEventListener('click', e => { const b = (e.target as HTMLElement).closest('[data-mv]'); if (!b) return; e.stopPropagation(); const k = b.getAttribute('data-mv');
      if (k === 'jump') jump(); else if (k === 'sprint') setRun(!W.run); else setStance(k!); });
    // Cinema: photos (looks, wide frame, depth of field, caption) and videos of what you see
    O.photo = makePhotoMode({ hud, root: document.body, engine, canvas: ctx.canvas, dp: () => O.dp, cam: () => O.cam, focus: () => 12, caption: () => '' });
    const recB = hud.querySelector('[data-rec]') as HTMLButtonElement;
    hud.querySelector('[data-cine]')!.addEventListener('click', e => { e.stopPropagation(); O.photo.toggle(); });
    const xcB = hud.querySelector('[data-xc]') as HTMLButtonElement; xcB.addEventListener('click', e => { e.stopPropagation(); if (O.rec) O.rec.stop(); if (O.photo.on) O.photo.toggle(); });
    recB.addEventListener('click', e => { e.stopPropagation(); if (O.rec) { O.rec.stop(); return; }
      const cs: MediaStream | null = (ctx.canvas as any).captureStream ? (ctx.canvas as any).captureStream(30) : null; if (!cs || typeof MediaRecorder === 'undefined') { toast('Video recording is not available on this device'); return; }
      const type = ['video/webm;codecs=vp9', 'video/webm;codecs=vp8', 'video/webm', 'video/mp4'].find(t => MediaRecorder.isTypeSupported(t)) || '';
      const mr = new MediaRecorder(cs, type ? { mimeType: type, videoBitsPerSecond: 8e6 } : undefined), parts: Blob[] = []; O.rec = mr;
      mr.ondataavailable = ev => { if (ev.data && ev.data.size) parts.push(ev.data); };
      mr.onstop = () => { O.rec = null; recB.classList.remove('on'); recB.textContent = '● Record video'; clearTimeout(O.recT);
        const mime = (mr.mimeType || 'video/webm').split(';')[0], blob = new Blob(parts, { type: mime }), fr = new FileReader();
        fr.onload = () => { savePhoto(fr.result as string, `AirportDynastyPro-${new Date().toISOString().replace(/[:.]/g, '-')}.${mime.indexOf('mp4') >= 0 ? 'mp4' : 'webm'}`).then(w => toast('Video saved to ' + w)).catch(() => toast('The video could not be saved')); };
        fr.readAsDataURL(blob); };
      mr.start(1000); recB.classList.add('on'); recB.textContent = '■ Stop recording'; O.recT = setTimeout(() => { try { mr.stop(); } catch (er) { /* stopped */ } }, 120000); });
    // Hide UI: everything on the screen goes; a double tap (or a double click) or Esc brings it back
    const uiOff = (off: boolean) => { document.body.classList.toggle('ui-off', off); if (off) toast(IS_TOUCH ? 'Double-tap to bring the controls back' : 'Esc or a double click brings the controls back'); };
    hud.querySelector('[data-hide]')!.addEventListener('click', e => { e.stopPropagation(); uiOff(true); });
    O.dbl = () => { if (document.body.classList.contains('ui-off')) uiOff(false); }; ctx.canvas.addEventListener('dblclick', O.dbl);
    let lastTap = 0; O.tap = () => { if (!document.body.classList.contains('ui-off')) return; const t = performance.now(); if (t - lastTap < 350) O.dbl(); lastTap = t; }; document.addEventListener('pointerup', O.tap, true); document.addEventListener('dblclick', O.dbl, true);
    O.kd = (e: KeyboardEvent) => { if (e.key === 'Escape') { if (document.body.classList.contains('ui-off')) uiOff(false); else if (O.photo && O.photo.on) O.photo.toggle(); else leave(); return; } if (e.repeat) return; const k = e.key.toLowerCase(); if (k === ' ') jump(); else if (k === 'c' || k === 'control') setStance('crouch'); else if (k === 'z') setStance('prone'); };
    window.addEventListener('keydown', O.kd);
    toast(IS_TOUCH ? 'Left thumb: walk · Right: drag to look' : 'W A S D walk · mouse look · Shift run · Space jump · C crouch · Z prone · a controller works too');
    progress(100, 'Ready');

    runLoop(() => {
      const dt = Math.min(0.05, engine.getDeltaTime() / 1000), inp = inputFrame(dt);
      F.yaw += inp.lx; F.pitch = Math.max(-1.3, Math.min(1.2, F.pitch - inp.ly));
      const sp = ST[W.stance], vmax = (inp.run || W.run) ? sp.run : sp.walk;
      // the eye moves smoothly to the stance's height; a jump rises and falls back
      W.eye += (sp.eye - W.eye) * Math.min(1, dt * 8); if (W.vy || W.jy > 0) { W.jy += W.vy * dt; W.vy -= 9.81 * dt; if (W.jy <= 0) { W.jy = 0; W.vy = 0; } }
      const mag = Math.min(1, Math.hypot(inp.mx, inp.my)); F.v += ((mag > 0.05 ? vmax * mag : 0) - F.v) * Math.min(1, dt * 6);
      if (mag > 0.05) { const fx = Math.sin(F.yaw), fz = Math.cos(F.yaw), rx = Math.cos(F.yaw), rz = -Math.sin(F.yaw);
        let mx = fx * inp.my + rx * inp.mx, mz = fz * inp.my + rz * inp.mx; const ml = Math.hypot(mx, mz) || 1; mx /= ml; mz /= ml;
        const st = F.v * dt, nx = F.x + mx * st, nz = F.z + mz * st;
        if (Math.hypot(nx, nz) < REACH) { F.x = nx; F.z = nz; } F.bob += st; }
      // your feet: on the ground, or falling onto it (a short drop when you are placed, or off a step)
      const gy0 = ground(F.x, F.z); if (W.fy > gy0 + 0.02) { W.fv -= 9.81 * dt; W.fy = Math.max(gy0, W.fy + W.fv * dt); } else { W.fy = gy0; W.fv = 0; }
      const gy = W.fy;
      cam.position.set(F.x, gy + W.eye + W.jy + Math.sin(F.bob / 0.65 * Math.PI) * 0.015 * Math.min(1, F.v / 1.1), F.z);
      const cp = Math.cos(F.pitch); cam.setTarget(cam.position.add(new Vector3(Math.sin(F.yaw) * cp, Math.sin(F.pitch), Math.cos(F.yaw) * cp)));
      { const on = !!(O.photo && O.photo.on); const d = on ? '' : 'none'; if (recB.style.display !== d) { recB.style.display = d; xcB.style.display = d; } } if (O.photo) O.photo.frame();
      sc.render();
    });
  },
  close() {
    const sc: Scene = O.scene; if (!sc) return; O.leaving = false;
    if (O.kd) window.removeEventListener('keydown', O.kd); O.kd = null;
    try { if (O.rec) O.rec.stop(); } catch (e) { /* stopped */ } document.body.classList.remove('ui-off', 'photo-on'); O.ctx.canvas.style.filter = '';
    try { O.ctx.canvas.removeEventListener('dblclick', O.dbl); document.removeEventListener('pointerup', O.tap, true); document.removeEventListener('dblclick', O.dbl, true); } catch (e) { /* gone */ }
    if (O.hud) O.hud.remove(); try { O.T && O.T.dispose(); } catch (e) { /* gone */ } sc.dispose(); O.scene = null;
  },
  onSettings(what: string) { apply(what === 'preset' ? 'all' : what); },
};
registerSection(World$);
export {};
