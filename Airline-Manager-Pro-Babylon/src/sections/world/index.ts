// Walking anywhere in the world, opened from the world map (the walking man under the 3D button, then "Place in
// world"): you stand on the real ground where the map's X was, with the real heights and the satellite picture round
// you (the same ground as the 3D viewer: Terrain Tiles on AWS and Sentinel-2 cloudless 2016 by EOX), and walk freely
// (W A S D, the stick, a controller). The loading screen stays until the ground and its pictures round you are in.
import { Scene, Vector3, Color3, Color4, UniversalCamera, HemisphericLight, DirectionalLight, DefaultRenderingPipeline, ImageProcessingConfiguration } from '@babylonjs/core';
import { registerSection, Section, SectionCtx } from '../../core/sections';
import { S } from '../../core/settings';
import { runLoop } from '../../core/engine';
import { inputFrame } from '../../core/input';
import { IS_PC, IS_TOUCH } from '../../core/app';
import { toast, esc } from '../../ui/icons';
import { buildTerrain, Terrain } from '../viewer3d/terrain';

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
    const ground = (x: number, z: number) => { const h = T.heightAt(lat + z / kz, lon + x / kx); return Math.max(-2, h == null ? 0 : h) - (x * x + z * z) / (2 * R) - 1.2; };
    const F = { x: 0, z: 0, yaw: 0, pitch: -0.05, v: 0, bob: 0 }; O.F = F;
    const sea = (T.heightAt(lat, lon) ?? 0) <= 0;

    const hud = document.createElement('div'); hud.className = 'hud';
    hud.innerHTML = `<div class="hg-info"><b>${esc(lat.toFixed(4))}°, ${esc(lon.toFixed(4))}°</b><small>${sea ? 'On the water' : 'On the ground'} · walk anywhere round here</small>
      <button class="wx-b" data-back style="margin-top:8px">Back to the map</button></div>`;
    ui.appendChild(hud); O.hud = hud;
    hud.querySelector('[data-back]')!.addEventListener('click', () => { if (ctx.toGame) ctx.toGame('map'); });
    toast(IS_TOUCH ? 'Left thumb: walk · Right: drag to look' : 'W A S D to walk · Shift to run · click to look with the mouse · a controller works too');
    progress(100, 'Ready');

    runLoop(() => {
      const dt = Math.min(0.05, engine.getDeltaTime() / 1000), inp = inputFrame(dt);
      F.yaw += inp.lx; F.pitch = Math.max(-1.3, Math.min(1.2, F.pitch - inp.ly));
      const vmax = inp.run ? 6 : 1.6, mag = Math.min(1, Math.hypot(inp.mx, inp.my)); F.v += ((mag > 0.05 ? vmax * mag : 0) - F.v) * Math.min(1, dt * 6);
      if (mag > 0.05) { const fx = Math.sin(F.yaw), fz = Math.cos(F.yaw), rx = Math.cos(F.yaw), rz = -Math.sin(F.yaw);
        let mx = fx * inp.my + rx * inp.mx, mz = fz * inp.my + rz * inp.mx; const ml = Math.hypot(mx, mz) || 1; mx /= ml; mz /= ml;
        const st = F.v * dt, nx = F.x + mx * st, nz = F.z + mz * st;
        if (Math.hypot(nx, nz) < REACH) { F.x = nx; F.z = nz; } F.bob += st; }
      const gy = ground(F.x, F.z);
      cam.position.set(F.x, gy + EYE + Math.sin(F.bob / 0.65 * Math.PI) * 0.015 * Math.min(1, F.v / 1.1), F.z);
      const cp = Math.cos(F.pitch); cam.setTarget(cam.position.add(new Vector3(Math.sin(F.yaw) * cp, Math.sin(F.pitch), Math.cos(F.yaw) * cp)));
      sc.render();
    });
    void IS_PC;
  },
  close() {
    const sc: Scene = O.scene; if (!sc) return;
    if (O.hud) O.hud.remove(); try { O.T && O.T.dispose(); } catch (e) { /* gone */ } sc.dispose(); O.scene = null;
  },
  onSettings(what: string) { apply(what === 'preset' ? 'all' : what); },
};
registerSection(World$);
export {};
