// The Babylon model exporter's tool: bundles entry.ts with esbuild (the Android edition's terminal code and Babylon's
// own glTF exporter), serves it with the Android edition's www/ files (its textures) on http://localhost:8642, and
// writes what the page sends back (terminal.glb) to the output folder (default F:/ADP-Export).
//   node Tools/BabylonExport/export.mjs [outdir]   then open http://localhost:8642/ in a browser
import { build } from 'esbuild';
import { createServer } from 'node:http';
import { readFileSync, writeFileSync, existsSync, mkdirSync, statSync } from 'node:fs';
import { join, dirname, extname, normalize } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const www = join(here, '../../Airline-Manager-Pro-Babylon/www');
const out = process.argv[2] || 'F:/ADP-Export';
mkdirSync(out, { recursive: true });

await build({
  entryPoints: [join(here, 'entry.ts')], bundle: true, format: 'iife', target: ['chrome110'], outfile: join(here, 'dist/export.js'),
  nodePaths: [join(here, 'node_modules')], legalComments: 'none', logLevel: 'warning',
  // one copy of Babylon for everything (the game's code would otherwise bring its own, and the exporter would not
  // recognise its meshes as meshes)
  alias: { '@babylonjs/core': join(here, 'node_modules/@babylonjs/core'), '@babylonjs/loaders': join(here, 'node_modules/@babylonjs/loaders') },
  // the phone-only Capacitor modules: a stand-in (the export runs in a desktop browser)
  plugins: [{ name: 'capacitor-stub', setup(b) { b.onResolve({ filter: /^@capacitor\// }, () => ({ path: join(here, 'capacitor-stub.ts') })); } }],
  define: { __WHATS_NEW__: '[]', __APP__: JSON.stringify({ name: 'Airport Dynasty Pro', short: 'ADP', id: 'export', version: '0', updates: {}, babylon: '9.30.0' }) },
});

const page = `<!doctype html><meta charset="utf-8"><title>Terminal export</title><body style="margin:0;background:#111;color:#ddd;font:12px monospace">
<canvas id="c" width="1280" height="720" style="width:640px;height:360px"></canvas><pre id="log"></pre><script src="/export.js"></script>`;
const types = { '.js': 'text/javascript', '.png': 'image/png', '.jpg': 'image/jpeg', '.webp': 'image/webp', '.json': 'application/json', '.glb': 'model/gltf-binary', '.ktx2': 'image/ktx2', '.svg': 'image/svg+xml' };

createServer((req, res) => {
  const url = decodeURIComponent(req.url.split('?')[0]);
  if (req.method === 'POST') {
    const chunks = []; req.on('data', c => chunks.push(c)); req.on('end', () => {
      const body = Buffer.concat(chunks);
      if (url === '/log') console.log(body.toString());
      else if (url.startsWith('/save/')) { const f = join(out, url.slice(6)); writeFileSync(f, body); console.log(`Saved ${f} (${(body.length / 1e6).toFixed(1)} MB)`); }
      res.end('ok');
    }); return;
  }
  if (url === '/' || url === '/index.html') { res.setHeader('Content-Type', 'text/html'); return res.end(page); }
  if (url === '/export.js') { res.setHeader('Content-Type', 'text/javascript'); return res.end(readFileSync(join(here, 'dist/export.js'))); }
  const f = normalize(join(www, url));
  if (f.startsWith(normalize(www)) && existsSync(f) && statSync(f).isFile()) { res.setHeader('Content-Type', types[extname(f)] || 'application/octet-stream'); return res.end(readFileSync(f)); }
  res.statusCode = 404; res.end();
}).listen(8642, () => console.log('Open http://localhost:8642/'));
