// Grass, trees and ground detail while walking in the world.
// Where things grow comes from the satellite picture itself (Sentinel-2 cloudless 2016 by EOX, the same picture the
// ground wears): each spot's colour says green and light (grass), green and dark (trees) or anything else (rock,
// sand, water, snow, towns: nothing grows). No other map data is needed. The grass and trees are our own shapes,
// drawn as copies of one model each (one draw call per kind), placed on the ground as it is drawn, only round you:
// the grass close by, the trees further out, laid out again as you walk. Settings: Grass and Trees (0 = none at
// all, nothing is made or drawn) and Ground detail (a fine grain on the ground close up, so it isn't a blur).
import { Scene, Mesh, MeshBuilder, StandardMaterial, Color3, Color4, Matrix, Quaternion, Vector3, DynamicTexture, Texture, VertexData } from '@babylonjs/core';
import { PBRMaterial } from '@babylonjs/core';
import { S } from '../../core/settings';
import { IS_PC } from '../../core/app';

const SAT = 'https://tiles.maps.eox.at/wmts/1.0.0/s2cloudless_3857/default/g/', ZC = 15, D2R = Math.PI / 180;

// #region ── What grows where: the satellite picture's colours ──
type Grow = 0 | 1 | 2;   // nothing, grass, trees
const pics: { [k: string]: Uint8ClampedArray | null | 'wait' } = {};
const tileX = (lon: number) => (lon + 180) / 360 * 2 ** ZC;
const tileY = (lat: number) => (1 - Math.log(Math.tan(lat * D2R) + 1 / Math.cos(lat * D2R)) / Math.PI) / 2 * 2 ** ZC;
function pic(x: number, y: number) {
  const k = `${x}/${y}`; if (k in pics) return pics[k]; pics[k] = 'wait';
  const im = new Image(); im.crossOrigin = 'anonymous';
  im.onload = () => { try { const c = document.createElement('canvas'); c.width = c.height = 256; const g = c.getContext('2d', { willReadFrequently: true })!; g.drawImage(im, 0, 0); pics[k] = g.getImageData(0, 0, 256, 256).data; } catch (e) { pics[k] = null; } };
  im.onerror = () => { pics[k] = null; }; im.src = `${SAT}${ZC}/${y}/${x}.jpg`;
  return pics[k];
}
/** What grows at (lat, lon): from the colour of the satellite picture there (or null while it is loading). */
export function growAt(lat: number, lon: number): Grow | null {
  const X = tileX(lon), Y = tileY(lat), d = pic(Math.floor(X), Math.floor(Y)); if (d === 'wait') return null; if (!d) return 0;
  const px = Math.min(255, Math.floor((X % 1) * 256)), py = Math.min(255, Math.floor((Y % 1) * 256)), i = (py * 256 + px) * 4, r = d[i], g = d[i + 1], b = d[i + 2], l = (r + g + b) / 3;
  if (b > g * 1.02 && b > r) return 0;                 // water, sky-blue roofs
  if (l > 175) return 0;                               // snow, sand, concrete
  const green = g > r * 1.04 && g > b * 1.02;
  if (green && l < 62) return 2;                       // dark green: woods
  if (green || (g >= r * 0.98 && g > b * 1.08 && l < 140)) return 1;   // green or olive: grass
  return 0;
}
// #endregion

// #region ── The models: a grass tuft and two kinds of tree (our own) ──
function grassTex(sc: Scene) {
  const t = new DynamicTexture('grassT', { width: 128, height: 128 }, sc, true), c = t.getContext() as CanvasRenderingContext2D; c.clearRect(0, 0, 128, 128);
  for (let i = 0; i < 26; i++) { const x = 6 + Math.random() * 116, h = 60 + Math.random() * 66, lean = (Math.random() - 0.5) * 30, w = 2 + Math.random() * 3;
    const gr = c.createLinearGradient(0, 128, 0, 128 - h); gr.addColorStop(0, '#3E5A22'); gr.addColorStop(1, Math.random() < 0.3 ? '#B9C46A' : '#8AAE48');
    c.fillStyle = gr; c.beginPath(); c.moveTo(x - w, 128); c.quadraticCurveTo(x + lean * 0.4, 128 - h * 0.6, x + lean, 128 - h); c.quadraticCurveTo(x + lean * 0.4 + w * 0.3, 128 - h * 0.55, x + w, 128); c.fill(); }
  t.hasAlpha = true; t.update(); return t;
}
function grassMesh(sc: Scene) {
  const m = new Mesh('grass', sc), pos: number[] = [], ind: number[] = [], uv: number[] = [], nor: number[] = [];
  for (let k = 0; k < 3; k++) { const a = k * Math.PI / 3, cx = Math.cos(a) * 0.32, cz = Math.sin(a) * 0.32, o = pos.length / 3;
    pos.push(-cx, 0, -cz, cx, 0, cz, cx, 0.42, cz, -cx, 0.42, -cz); uv.push(0, 0, 1, 0, 1, 1, 0, 1); for (let q = 0; q < 4; q++) nor.push(0, 1, 0); ind.push(o, o + 1, o + 2, o, o + 2, o + 3); }
  const vd = new VertexData(); vd.positions = pos; vd.indices = ind; vd.uvs = uv; vd.normals = nor; vd.applyToMesh(m);
  const mat = new StandardMaterial('grassM', sc); mat.diffuseTexture = grassTex(sc); mat.diffuseTexture.hasAlpha = true; mat.useAlphaFromDiffuseTexture = false; mat.transparencyMode = 1; mat.alphaCutOff = 0.4;
  mat.backFaceCulling = false; mat.specularColor = Color3.Black(); mat.emissiveColor = new Color3(0.08, 0.1, 0.05); m.material = mat; m.isPickable = false; m.renderingGroupId = 1; return m;
}
function treeMesh(sc: Scene, conifer: boolean) {
  const parts: Mesh[] = [], col = (m: Mesh, c: Color4) => { const n = m.getTotalVertices(), cs: number[] = []; for (let i = 0; i < n; i++) { const j = 0.85 + Math.random() * 0.3; cs.push(c.r * j, c.g * j, c.b * j, 1); } m.setVerticesData('color', cs); return m; };
  const trunk = MeshBuilder.CreateCylinder('tr', { height: conifer ? 3 : 3.2, diameterTop: 0.22, diameterBottom: 0.38, tessellation: 6 }, sc); trunk.position.y = conifer ? 1.5 : 1.6; parts.push(col(trunk, new Color4(0.32, 0.22, 0.14, 1)));
  if (conifer) for (let k = 0; k < 3; k++) { const c = MeshBuilder.CreateCylinder('cn', { height: 4.2 - k * 0.9, diameterTop: 0, diameterBottom: 4.2 - k * 1.1, tessellation: 7 }, sc); c.position.y = 2.8 + k * 2.1; parts.push(col(c, new Color4(0.13, 0.27, 0.15, 1))); }
  else for (const [x, y, z, r] of [[0, 5.2, 0, 2.6], [1.1, 4.4, 0.6, 1.8], [-1, 4.6, -0.5, 1.9], [0.2, 6.3, -0.3, 1.7]]) { const s = MeshBuilder.CreateIcoSphere('cr', { radius: r, subdivisions: 1 }, sc); s.position.set(x, y, z); parts.push(col(s, new Color4(0.22, 0.38, 0.16, 1))); }
  const m = Mesh.MergeMeshes(parts, true, true)!; m.name = conifer ? 'conifer' : 'leafy';
  const mat = new StandardMaterial('treeM', sc); mat.diffuseColor = Color3.White(); mat.specularColor = Color3.Black(); mat.emissiveColor = new Color3(0.05, 0.06, 0.04); m.material = mat; m.useVertexColors = true; m.isPickable = false; m.renderingGroupId = 1; return m;
}
// #endregion

// #region ── Ground detail: a fine grain on the ground close up ──
export function groundDetail(sc: Scene, mats: PBRMaterial[], tileMetres: number) {
  const t = new DynamicTexture('grain', { width: 256, height: 256 }, sc, true), c = t.getContext() as CanvasRenderingContext2D, img = c.createImageData(256, 256);
  // red: the grain's brightness round mid-grey; green and alpha: a flat normal; blue: a middling roughness
  for (let i = 0; i < 256 * 256; i++) { const v = 128 + (Math.random() - 0.5) * 70 + (Math.random() < 0.04 ? (Math.random() - 0.5) * 120 : 0); img.data[i * 4] = Math.max(0, Math.min(255, v)); img.data[i * 4 + 1] = 128; img.data[i * 4 + 2] = 128; img.data[i * 4 + 3] = 128; }
  c.putImageData(img, 0, 0); t.update(); t.wrapU = t.wrapV = Texture.WRAP_ADDRESSMODE; t.uScale = t.vScale = tileMetres / 2.5; t.anisotropicFilteringLevel = 8;
  const on = (yes: boolean) => { for (const m of mats) { try { m.detailMap.texture = yes ? t : null; m.detailMap.isEnabled = yes; m.detailMap.diffuseBlendLevel = 0.45; m.detailMap.bumpLevel = 0; m.detailMap.roughnessBlendLevel = 0; } catch (e) { /* not on this device */ } } };
  on(!!S.gfx.groundDetail);
  return { set: on, dispose() { on(false); t.dispose(); } };
}
// #endregion

// #region ── Placing them round you ──
export function vegetation(sc: Scene, lat: number, lon: number, ground: (x: number, z: number) => number) {
  const kx = 111320 * Math.cos(lat * D2R), kz = 110540;
  let grass: Mesh | null = null, trees: Mesh[] = [], at: { x: number; z: number } | null = null, lastG = -1, lastT = -1;
  const rnd = (i: number, j: number, s: number) => { const v = Math.sin(i * 127.1 + j * 311.7 + s * 74.7) * 43758.5453; return v - Math.floor(v); };
  const layout = (px: number, pz: number) => {
    const gq = Math.max(0, Math.min(100, S.gfx.grass || 0)), tq = Math.max(0, Math.min(100, S.gfx.trees || 0));
    // grass: up to 18-55 m round you, closer together the higher the setting
    if (gq > 0) { if (!grass) grass = grassMesh(sc);
      const R = (IS_PC ? 30 : 18) + gq / 100 * (IS_PC ? 25 : 12), step = 1.1 - gq / 100 * 0.55, out: number[] = [], m = new Matrix(), q = new Quaternion();
      for (let gz = Math.floor((pz - R) / step); gz <= (pz + R) / step; gz++) for (let gx = Math.floor((px - R) / step); gx <= (px + R) / step; gx++) {
        const x = (gx + rnd(gx, gz, 1)) * step, z = (gz + rnd(gx, gz, 2)) * step; if ((x - px) ** 2 + (z - pz) ** 2 > R * R) continue;
        if (growAt(lat + z / kz, lon + x / kx) !== 1) continue;
        const s = 0.7 + rnd(gx, gz, 3) * 0.8; Quaternion.RotationYawPitchRollToRef(rnd(gx, gz, 4) * 6.28, 0, 0, q); Matrix.ComposeToRef(new Vector3(s, s * (0.8 + rnd(gx, gz, 6) * 0.5), s), q, new Vector3(x, ground(x, z) - 0.02, z), m);
        for (let i = 0; i < 16; i++) out.push(m.m[i]); }
      grass.thinInstanceSetBuffer('matrix', new Float32Array(out), 16, false); grass.setEnabled(out.length > 0);
    } else if (grass) { grass.dispose(); grass = null; }
    // trees: up to 150-600 m round you, in the dark green of woods (and now and then on grass)
    if (tq > 0) { if (!trees.length) trees = [treeMesh(sc, true), treeMesh(sc, false)];
      const R = 150 + tq / 100 * (IS_PC ? 450 : 200), step = 9 - tq / 100 * 4, outs: number[][] = [[], []], m = new Matrix(), q = new Quaternion();
      for (let gz = Math.floor((pz - R) / step); gz <= (pz + R) / step; gz++) for (let gx = Math.floor((px - R) / step); gx <= (px + R) / step; gx++) {
        const x = (gx + rnd(gx, gz, 11)) * step, z = (gz + rnd(gx, gz, 12)) * step; if ((x - px) ** 2 + (z - pz) ** 2 > R * R) continue;
        const gw = growAt(lat + z / kz, lon + x / kx); if (gw !== 2 && !(gw === 1 && rnd(gx, gz, 15) < 0.015)) continue;
        const s = 0.75 + rnd(gx, gz, 13) * 0.7, kind = rnd(gx, gz, 16) < (lat > 55 ? 0.7 : 0.4) ? 0 : 1; Quaternion.RotationYawPitchRollToRef(rnd(gx, gz, 14) * 6.28, 0, 0, q);
        Matrix.ComposeToRef(new Vector3(s, s, s), q, new Vector3(x, ground(x, z) - 0.1, z), m); for (let i = 0; i < 16; i++) outs[kind].push(m.m[i]); }
      trees.forEach((t, i) => { t.thinInstanceSetBuffer('matrix', new Float32Array(outs[i]), 16, false); t.setEnabled(outs[i].length > 0); });
    } else if (trees.length) { trees.forEach(t => t.dispose()); trees = []; }
    lastG = gq; lastT = tq;
  };
  let wait = 0;
  return {
    /** Each frame: laid out again when you have walked a little, when the pictures came in, or the settings changed. */
    frame(px: number, pz: number, dt: number) {
      wait -= dt; const changed = (S.gfx.grass || 0) !== lastG || (S.gfx.trees || 0) !== lastT, loading = Object.values(pics).some(v => v === 'wait');
      if (changed || (wait <= 0 && (!at || Math.hypot(px - at.x, pz - at.z) > 8 || loading))) { at = { x: px, z: pz }; wait = loading ? 2 : 0.6; layout(px, pz); }
    },
    dispose() { if (grass) grass.dispose(); trees.forEach(t => t.dispose()); grass = null; trees = []; },
  };
}
// #endregion
