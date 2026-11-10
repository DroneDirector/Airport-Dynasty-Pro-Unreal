// Builds the Babylon edition's terminal exactly as its own terminal section does (sections/terminal/index.ts: the
// shell, the doors, the furniture, the fixtures with stairs, lifts and jetways, the rental car centre), then saves the
// whole scene as one glTF binary with Babylon's own exporter and sends it to the local tool server, which writes it
// to disk for the Unreal import. Nothing in the Android edition is changed.
import { Engine, Scene, Color4, Camera, Light } from '@babylonjs/core';
import { GLTF2Export } from '@babylonjs/serializers/glTF';
import { makeKit } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/kit';
import { buildShell } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/shell';
import { buildDoors } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/doors';
import { furnish } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/furnish';
import { buildFixtures } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/fixtures';
import { buildRental } from '../../Airline-Manager-Pro-Babylon/src/sections/terminal/rental';
import { buildLounge } from '../../Airline-Manager-Pro-Babylon/src/sections/lounge/build';
import { buildAirliner, setGear } from '../../Airline-Manager-Pro-Babylon/src/sections/viewer3d/airliner';

const say = (s: string) => { const e = document.getElementById('log'); if (e) e.textContent += s + '\n'; fetch('/log', { method: 'POST', body: s }).catch(() => 0); };

for (const k of ['error', 'warn'] as const) { const o = (console as any)[k].bind(console); (console as any)[k] = (...a: any[]) => { o(...a); try { say(k + ': ' + a.map(x => (x && x.stack) || String(x)).join(' ').slice(0, 400)); } catch (e) { /* none */ } }; }
async function run() {
  const canvas = document.getElementById('c') as HTMLCanvasElement;
  const engine = new Engine(canvas, true, { preserveDrawingBuffer: true, stencil: true });
  const sc = new Scene(engine); sc.clearColor = new Color4(0.66, 0.78, 0.9, 1);
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
