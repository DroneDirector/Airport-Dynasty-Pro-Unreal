// Builds the Babylon edition's terminal exactly as its own terminal section does (sections/terminal/index.ts: the
// shell, the doors, the furniture, the fixtures with stairs, lifts and jetways, the rental car centre), then saves the
// whole scene as one glTF binary with Babylon's own exporter and sends it to the local tool server, which writes it
// to disk for the Unreal import. Nothing in the Android edition is changed.
import { Engine, Scene, Color3, Color4, Camera, Light, Vector3, Vector4, PBRMaterial } from '@babylonjs/core';
import { GLTF2Export } from '@babylonjs/serializers/glTF';
import { makeKit } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/kit';
import { buildShell } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/shell';
import { buildDoors } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/doors';
import { furnish } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/furnish';
import { buildFixtures } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/fixtures';
import { buildRental } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/rental';
import { buildLounge } from '../../Airline-Manager-Pro-Babylon/src/sections/lounge/build';
import { buildOffice } from '../../Airline-Manager-Pro-Babylon/src/sections/office/build';
import { buildOps, opsClock } from '../../Airline-Manager-Pro-Babylon/src/sections/ops/build';
import { buildTraining } from '../../Airline-Manager-Pro-Babylon/src/sections/training/build';
import { buildAirliner, setGear } from '../../Airline-Manager-Pro-Babylon/src/sections/viewer3d/airliner';

const say = (s: string) => { const e = document.getElementById('log'); if (e) e.textContent += s + '\n'; fetch('/log', { method: 'POST', body: s }).catch(() => 0); };

for (const k of ['error', 'warn'] as const) { const o = (console as any)[k].bind(console); (console as any)[k] = (...a: any[]) => { o(...a); try { say(k + ': ' + a.map(x => (x && x.stack) || String(x)).join(' ').slice(0, 400)); } catch (e) { /* none */ } }; }
// The pet's body and fur are posed by their own shader (sections/office/pet.ts, petFurVertexShader), which glTF can't
// carry: the pose it shows now (asleep in its bed) is worked out here the same way, into the mesh itself, and the
// shader's material swapped for a plain one in the coat's colour (the average of the mesh's own colours).
function bakePet(sc: Scene) {
  for (const m of sc.meshes as any[]) {
    const mat = m.material; if (!mat || mat.getClassName() !== 'ShaderMaterial') continue;
    const F = (k: string) => (mat._floats && mat._floats[k]) || 0, V = (k: string) => mat._vectors3[k] || new Vector3(), L = mat._vectors4['uLegA'] || new Vector4();
    const pos = m.getVerticesData('position'), nor = m.getVerticesData('normal'), col = m.getVerticesData('color'), pw = m.getVerticesData('pw'), lw = m.getVerticesData('lw'), tw = m.getVerticesData('tw');
    if (!pos || !pw || !lw || !tw) continue;
    const rx = (a: number, v: number[]) => { const c = Math.cos(a), s = Math.sin(a); return [v[0], c * v[1] - s * v[2], s * v[1] + c * v[2]]; };
    const ry = (a: number, v: number[]) => { const c = Math.cos(a), s = Math.sin(a); return [c * v[0] + s * v[2], v[1], -s * v[0] + c * v[2]]; };
    const rz = (a: number, v: number[]) => { const c = Math.cos(a), s = Math.sin(a); return [c * v[0] - s * v[1], s * v[0] + c * v[1], v[2]]; };
    const about = (r: (a: number, v: number[]) => number[], a: number, o: Vector3, p: number[]) => { const q = r(a, [p[0] - o.x, p[1] - o.y, p[2] - o.z]); return [o.x + q[0], o.y + q[1], o.z + q[2]]; };
    const mix = (p: number[], q: number[], w: number) => [p[0] + (q[0] - p[0]) * w, p[1] + (q[1] - p[1]) * w, p[2] + (q[2] - p[2]) * w];
    const legs = [V('uLeg0'), V('uLeg1'), V('uLeg2'), V('uLeg3')], legA = [L.x, L.y, L.z, L.w];
    const n = pos.length / 3, P2 = new Float32Array(pos.length), N2 = nor ? new Float32Array(nor.length) : null;
    for (let i = 0; i < n; i++) {
      let p = [pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]], nn = nor ? [nor[i * 3], nor[i * 3 + 1], nor[i * 3 + 2]] : [0, 1, 0];
      const lwv = [lw[i * 4], lw[i * 4 + 1], lw[i * 4 + 2], lw[i * 4 + 3]], pwv = [pw[i * 4], pw[i * 4 + 1], pw[i * 4 + 2], pw[i * 4 + 3]], t = tw[i];
      for (let k = 0; k < 4; k++) { p = mix(p, about(rz, legA[k], legs[k], p), lwv[k]); nn = mix(nn, rz(legA[k], nn), lwv[k]); }
      p = mix(p, about(rx, F('uEarL'), V('uEarPL'), p), pwv[1]); p = mix(p, about(rx, -F('uEarR'), V('uEarPR'), p), pwv[2]);
      p = mix(p, about(rz, F('uHead'), V('uNeck'), p), pwv[0]); nn = mix(nn, rz(F('uHead'), nn), pwv[0]);
      const st = t > 0.001 ? 1 : 0; p = mix(p, about(rz, F('uTailY') * t, V('uTailP'), p), st); p = mix(p, about(ry, F('uTail') * t, V('uTailP'), p), st);
      p[1] += pwv[3] * F('uPaw') * 0.012;
      const sm = (e0: number, e1: number, x: number) => { const k = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return k * k * (3 - 2 * k); };
      p[1] += sm(0.03, 0.16, p[1]) * (1 - pwv[0]) * (1 - Math.max(lwv[0], lwv[1], lwv[2], lwv[3])) * F('uBreath') * 0.01;
      P2.set(p, i * 3); if (N2) { const l = Math.hypot(nn[0], nn[1], nn[2]) || 1; N2.set([nn[0] / l, nn[1] / l, nn[2] / l], i * 3); }
    }
    m.setVerticesData('position', P2, false); if (N2) m.setVerticesData('normal', N2, false);
    for (const k of ['pw', 'lw', 'tw']) m.removeVerticesData(k);
    let r = 0, g = 0, b = 0; if (col) { for (let i = 0; i < col.length; i += 4) { r += col[i]; g += col[i + 1]; b += col[i + 2]; } const c = col.length / 4; r /= c; g /= c; b /= c; } else { r = g = b = 0.6; }
    const pm = new PBRMaterial(m.name + '-coat', sc); pm.albedoColor = new Color3(r, g, b); pm.roughness = 0.9; pm.metallic = 0; pm.backFaceCulling = false; m.material = pm;
  }
}
// Unreal names each mesh after its node, and two of the same name come out under made-up names the placing can't find:
// the second and later of a name get a number (pet-bowl, pet-bowl-2). Click boxes (invisible, but drawn at no opacity) stay out.
function uniqueNames(sc: Scene) {
  const seen = new Map<string, number>();
  // (the groups too: a mesh named as the group it sits in comes out under a made-up name as well)
  for (const m of [...sc.transformNodes, ...sc.meshes] as any[]) { const k = (seen.get(m.name) || 0) + 1; seen.set(m.name, k); if (k > 1) m.name = m.name + '-' + k; }
}
const shown = (n: any) => !(n instanceof Camera) && !(n instanceof Light) && n.isEnabled(false) && (n.isVisible === undefined || n.isVisible) && n.visibility !== 0;
async function run() {
  const canvas = document.getElementById('c') as HTMLCanvasElement;
  const engine = new Engine(canvas, true, { preserveDrawingBuffer: true, stencil: true });
  const sc = new Scene(engine); sc.clearColor = new Color4(0.66, 0.78, 0.9, 1); (window as any).sc = sc;   // (for looking into an export from the page)
  // ?m=lounge: the business lounge (sections/lounge/build.ts) instead of the terminal
  const which = new URLSearchParams(location.search).get('m') || 'terminal';
  // ?m=airliner: the 3D viewer's airliner (sections/viewer3d/airliner.ts), gear down, in its own livery
  if (which === 'airliner') {
    say('Building the airliner…');
    const ac = buildAirliner(sc); setGear(ac, 1);
    await sc.whenReadyAsync();
    say(`Built: ${sc.meshes.length} meshes. Saving glTF…`);
    const g = await GLTF2Export.GLBAsync(sc, 'airliner', { shouldExportNode: (n: any) => !(n instanceof Camera) && !(n instanceof Light) && n.isEnabled(false) && (n.isVisible === undefined || n.isVisible) });
    const ab = g.glTFFiles['airliner.glb'] as Blob;
    say(`Sending ${(ab.size / 1e6).toFixed(1)} MB…`);
    await fetch('/save/airliner.glb', { method: 'POST', body: ab });
    // which nodes are the landing gear (folded away in the air) and the fans (turning)
    await fetch('/save/airliner-parts.json', { method: 'POST', body: JSON.stringify({ gear: ac.gear.map((g: any) => g.name), fans: ac.fans.map((f: any) => f.name) }) });
    say('DONE'); return;
  }
  // ?m=office&pet=dog|cat|none: the office's THINGS only (owner, 2026-11-11: never the room: walls, floor, ceiling,
  // window, door and its lamps stay out, the terminal's own office keeps its room); with the dog or the cat asleep in its
  // bed, or none. Coordinates as the office's own: the room's middle at the origin, x to the right, z towards its window.
  if (which === 'office') {
    const kind = new URLSearchParams(location.search).get('pet') || 'none';
    say(`Building the office (pet: ${kind})…`);
    const D: any = { airline: 'Your Airline', icao: 'ADP', color: '#1C2A4A', color2: '#3FD3E6', hub: 'BIAP', pilot: 'Captain', balance: '$12.4M', flying: 6, fleet: 12, routes: [], reports: 2,
      pet: kind === 'none' ? undefined : { kind, name: kind === 'dog' ? 'Max' : 'Luna', coat: 0, white: false, size: 'm', ears: kind === 'dog' ? 'floppy' : 'pointy', bed: 0, food: 1, water: 1 } };
    buildOffice(sc, D, (p, s) => say(`${p}% ${s}`));
    await sc.whenReadyAsync();
    bakePet(sc); uniqueNames(sc);
    const shell = sc.getTransformNodeByName('shell');
    const inShell = (n: any) => { for (let q = n; q; q = q.parent) if (q === shell) return true; return false; };
    const name = kind === 'none' ? 'office' : 'office' + kind;
    const g = await GLTF2Export.GLBAsync(sc, name, { shouldExportNode: (n: any) => shown(n) && !inShell(n) });
    const ob = g.glTFFiles[name + '.glb'] as Blob;
    say(`Sending ${(ob.size / 1e6).toFixed(1)} MB…`);
    await fetch('/save/' + name + '.glb', { method: 'POST', body: ob });
    const lights = sc.lights.map((l: any) => ({ name: l.name, kind: l.getClassName(), pos: l.position ? [l.position.x, l.position.y, l.position.z] : null, dir: null, color: l.diffuse ? [l.diffuse.r, l.diffuse.g, l.diffuse.b] : null, intensity: l.intensity, range: l.range }));
    await fetch('/save/' + name + '-lights.json', { method: 'POST', body: JSON.stringify(lights, null, 1) });
    say('DONE'); return;
  }
  // ?m=ops: the operations room's THINGS only (owner, 2026-11-11), as the office: its desk, the ultra-wide monitor, the
  // keyboard and mouse, the folders and the chair; the room, its window view, clock, archive rack and lights stay out
  if (which === 'ops') {
    say('Building the operations room…');
    // (its red clock's picture is drawn while the game runs: drawn once here, or the scene never reports ready)
    opsClock(buildOps(sc, '#2C8CFF', (p, s) => say(`${p}% ${s}`)));
    await sc.whenReadyAsync(); uniqueNames(sc);
    const shell = sc.getTransformNodeByName('shell');
    const inShell = (n: any) => { for (let q = n; q; q = q.parent) if (q === shell) return true; return false; };
    const g = await GLTF2Export.GLBAsync(sc, 'ops', { shouldExportNode: (n: any) => shown(n) && !inShell(n) });
    const ob = g.glTFFiles['ops.glb'] as Blob;
    say(`Sending ${(ob.size / 1e6).toFixed(1)} MB…`);
    await fetch('/save/ops.glb', { method: 'POST', body: ob });
    await fetch('/save/ops-lights.json', { method: 'POST', body: '[]' });
    say('DONE'); return;
  }
  // ?m=academy: the Academy's THINGS only (owner, 2026-11-11): the whiteboard, the instructor's desk, laptop and chair, the
  // class's desks and chairs with their notebooks, pens, calculators and paper, the plants; the room, its shelves, posters,
  // clock, doors and lights stay out, and so do the people (the game has its own)
  if (which === 'academy') {
    say('Building the Academy…');
    buildTraining(sc, 'Your Airline', '#1C2A4A', '#3FD3E6', { students: [] });
    await sc.whenReadyAsync(); uniqueNames(sc);
    const shell = sc.getTransformNodeByName('shell');
    const out = (n: any) => { for (let q = n; q; q = q.parent) if (q === shell || (q.metadata && q.metadata.act === 'trn')) return true; return false; };
    const g = await GLTF2Export.GLBAsync(sc, 'academy', { shouldExportNode: (n: any) => shown(n) && !out(n) });
    const ob = g.glTFFiles['academy.glb'] as Blob;
    say(`Sending ${(ob.size / 1e6).toFixed(1)} MB…`);
    await fetch('/save/academy.glb', { method: 'POST', body: ob });
    await fetch('/save/academy-lights.json', { method: 'POST', body: '[]' });
    say('DONE'); return;
  }
  if (which === 'lounge') {
    say('Building the lounge…');
    buildLounge(sc, (p, s) => say(`${p}% ${s}`));
    await sc.whenReadyAsync();
    say(`Built: ${sc.meshes.length} meshes. Saving glTF…`);
    say('probe: ' + sc.meshes.slice(0, 6).map((m: any) => `${m.name}:${m.getClassName()}:v${m.getTotalVertices()}:en${m.isEnabled()}:vis${m.isVisible}`).join(' | '));
    // only what you see in Babylon: its hidden helpers (the door's click box, the bottles' templates) stay out
    const g = await GLTF2Export.GLBAsync(sc, 'lounge', { shouldExportNode: (n: any) => !(n instanceof Camera) && !(n instanceof Light) && n.isEnabled(false) && (n.isVisible === undefined || n.isVisible) });
    say('files: ' + Object.keys(g.glTFFiles).join(','));
    const lb = g.glTFFiles['lounge.glb'] as Blob;
    say(`Sending ${(lb.size / 1e6).toFixed(1)} MB…`);
    await fetch('/save/lounge.glb', { method: 'POST', body: lb });
    // the lights as Babylon places them, for Unreal to put the same lights in the same places
    const lights = sc.lights.map((l: any) => ({ name: l.name, kind: l.getClassName(), pos: l.position ? [l.position.x, l.position.y, l.position.z] : null, dir: l.direction ? [l.direction.x, l.direction.y, l.direction.z] : null, color: l.diffuse ? [l.diffuse.r, l.diffuse.g, l.diffuse.b] : null, intensity: l.intensity, range: l.range }));
    await fetch('/save/lounge-lights.json', { method: 'POST', body: JSON.stringify(lights, null, 1) });
    say('DONE'); return;
  }
  say('Building the terminal…');
  const K = makeKit(sc);
  buildShell(K, (p, s) => say(`${p}% ${s}`)); await new Promise(r => setTimeout(r, 0));
  buildDoors(K);
  furnish(K, (p, s) => say(`${p}% ${s}`)); K.clearWalls(); await new Promise(r => setTimeout(r, 0));
  buildFixtures(K, '#1C2D52', '#F2C230', 'Airport Dynasty Pro', (p, s) => say(`${p}% ${s}`));
  buildRental(K);
  (K.struct as any).finish();
  // let every texture (the drawn ones and the files) finish loading before saving
  await sc.whenReadyAsync();
  // (Babylon's exporter converts to glTF's right-handed axes itself, by flipping x: the model is exported as it is)
  say(`Built: ${sc.meshes.length} meshes. Saving glTF…`);
  const glb = await GLTF2Export.GLBAsync(sc, 'terminal', { shouldExportNode: (n: any) => !(n instanceof Camera) && !(n instanceof Light) && !(n.name || '').startsWith('SRC:') || (n.name || '').startsWith('SRC:') && false });
  const blob = glb.glTFFiles['terminal.glb'] as Blob;
  say(`Sending ${(blob.size / 1e6).toFixed(1)} MB…`);
  await fetch('/save/terminal.glb', { method: 'POST', body: blob });
  say('DONE');
}
run().catch(e => say('FAILED: ' + (e && e.stack || e)));
