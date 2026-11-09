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

const say = (s: string) => { const e = document.getElementById('log'); if (e) e.textContent += s + '\n'; fetch('/log', { method: 'POST', body: s }).catch(() => 0); };

async function run() {
  const canvas = document.getElementById('c') as HTMLCanvasElement;
  const engine = new Engine(canvas, true, { preserveDrawingBuffer: true, stencil: true });
  const sc = new Scene(engine); sc.clearColor = new Color4(0.66, 0.78, 0.9, 1);
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
