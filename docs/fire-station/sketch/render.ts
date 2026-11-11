// Design sketch of the fire station and its trucks (drawing aid only; the game build is in Unreal C++).
// Artist's renderings of the airport fire station, lit as the game is (sun, sky light, the terminal's indoor lamp
// strength, nothing washed out). ?v=<view>
import { Engine, Scene, Vector3, Color3, Color4, UniversalCamera, HemisphericLight, DirectionalLight, ShadowGenerator, PointLight, DefaultRenderingPipeline, MeshBuilder, StandardMaterial, RawCubeTexture, DynamicTexture, AbstractMesh, ImageProcessingConfiguration } from '@babylonjs/core';
import { Kit } from './lib';
import { buildStation, bayX, HALL } from './station';
import { buildTruck } from './truck';

const Q = new URLSearchParams(location.search), VIEW = Q.get('v') || 'hero';
type Vw = { eye: number[]; at: number[]; fov: number; dusk?: boolean; inside?: boolean };
const VIEWS: { [k: string]: Vw } = {
  hero: { eye: [-17, 1.7, -21], at: [9, 5.0, 3], fov: 1.02 },
  side: { eye: [96, 1.75, -46], at: [44, 8, 6], fov: 0.9 },
  aerial: { eye: [92, 46, -70], at: [22, 2, 10], fov: 0.72, dusk: true },
  bays: { eye: [40, 2.0, -36], at: [40, 5.2, 0], fov: 0.92, dusk: true },
  inside: { eye: [53, 1.7, 21], at: [bayX(1.5), 2.2, 6], fov: 1.05, inside: true },
  truck: { eye: [bayX(1) + 4.2, 1.6, 3.8], at: [bayX(1) - 0.4, 1.8, 9.5], fov: 1.0, inside: true },
  cab: { eye: [bayX(2) + 1.0, 2.95, 7.75], at: [bayX(2) - 0.35, 2.25, 5.4], fov: 1.2, inside: true },
};
const v = VIEWS[VIEW] || VIEWS.hero, dusk = !!v.dusk;
const cv = document.getElementById('c') as HTMLCanvasElement;
const eng = new Engine(cv, true, { preserveDrawingBuffer: true, antialias: true, stencil: true });
const sc = new Scene(eng); sc.clearColor = new Color4(0.5, 0.65, 0.85, 1);
// the sky: a dome and the same sky the metal and glass reflect (day, or the blue hour after sunset)
{ const N = 128, faces: Float32Array[] = [];
  const L = (x: number, y: number, z: number) => { const l = Math.hypot(x, y, z); x /= l; y /= l; z /= l;
    if (dusk) { const s = y > 0 ? [0.05 + 0.1 * (1 - y), 0.08 + 0.12 * (1 - y), 0.2 + 0.15 * (1 - y)] : [0.03, 0.035, 0.045]; const g = Math.max(0, -z) * Math.max(0, 0.25 - Math.abs(y)) * 0.6; return [s[0] + g, s[1] + g * 0.55, s[2] + g * 0.3]; }
    const sky = y > 0 ? [0.42 - 0.24 * y, 0.6 - 0.22 * y, 0.95 - 0.1 * y] : [0.36, 0.36, 0.35];
    const sun = Math.max(0, x * -0.55 + y * 0.45 + z * -0.7); return [sky[0] + Math.pow(sun, 80) * 25, sky[1] + Math.pow(sun, 80) * 22, sky[2] + Math.pow(sun, 80) * 18]; };
  const dir = (f: number, u: number, w: number): number[] => [[1, -w, -u], [-1, -w, u], [u, 1, w], [u, -1, -w], [u, -w, 1], [-u, -w, -1]][f];
  for (let f = 0; f < 6; f++) { const d = new Float32Array(N * N * 4); for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) { const u = (i + 0.5) / N * 2 - 1, w = (j + 0.5) / N * 2 - 1, q = dir(f, u, w), c = L(q[0], q[1], q[2]), o = (j * N + i) * 4; d[o] = c[0]; d[o + 1] = c[1]; d[o + 2] = c[2]; d[o + 3] = 1; } faces.push(d); }
  const env = new RawCubeTexture(sc, faces as any, N, 5, 1, true, false); env.gammaSpace = false; sc.environmentTexture = env; sc.environmentIntensity = dusk ? 0.55 : 0.85;
  const dome = MeshBuilder.CreateSphere('dome', { diameter: 3000, segments: 24, sideOrientation: 1 }, sc), t = new DynamicTexture('dt', { width: 16, height: 512 }, sc, false), x = t.getContext() as CanvasRenderingContext2D;
  const g = x.createLinearGradient(0, 0, 0, 512);
  if (dusk) { g.addColorStop(1, '#0B1630'); g.addColorStop(0.58, '#1D3762'); g.addColorStop(0.5, '#C98A5E'); g.addColorStop(0.48, '#2A2C2E'); g.addColorStop(0, '#151618'); }
  else { g.addColorStop(1, '#2F67B8'); g.addColorStop(0.58, '#7FB2E6'); g.addColorStop(0.5, '#D7E5EF'); g.addColorStop(0.48, '#8C9384'); g.addColorStop(0, '#6E7863'); }
  x.fillStyle = g; x.fillRect(0, 0, 16, 512); t.update();
  const dm = new StandardMaterial('dm', sc); dm.emissiveTexture = t; dm.disableLighting = true; dm.backFaceCulling = false; dome.material = dm; dome.infiniteDistance = true; }
// the light: the morning sun from the front left (ref 1), or the last of the evening; the sky's own light
const hemi = new HemisphericLight('h', new Vector3(0.1, 1, -0.2), sc); hemi.intensity = dusk ? 0.12 : 0.42; hemi.diffuse = Color3.FromHexString(dusk ? '#5C6E9A' : '#EAF0F8'); hemi.groundColor = Color3.FromHexString('#6E6A62');
const sun = new DirectionalLight('s', dusk ? new Vector3(-0.2, -0.25, 0.95).normalize() : new Vector3(0.62, -0.5, 0.6).normalize(), sc);
sun.position = sun.direction.scale(-300); sun.intensity = dusk ? 0.12 : 2.3; sun.diffuse = Color3.FromHexString(dusk ? '#E8A070' : '#FFEBD2');
const K = new Kit(sc);
const S = buildStation(K, dusk);
// the two crash tenders, each its own object, parked in bays 2 and 3 behind their shut doors, noses to the doors
buildTruck(K, 1, new Vector3(bayX(1), 0, 11.5));
buildTruck(K, 2, new Vector3(bayX(2), 0, 11.5));
// grass beyond the asphalt
{ const g = MeshBuilder.CreateGround('gr', { width: 3000, height: 3000 }, sc); g.position.y = -0.08; const gm = new StandardMaterial('gm', sc); gm.diffuseColor = Color3.FromHexString(dusk ? '#3A4630' : '#55613F'); gm.specularColor = new Color3(0, 0, 0); if (dusk) gm.emissiveColor = Color3.FromHexString('#1C2418'); g.material = gm; }
const sg = new ShadowGenerator(4096, sun); sg.usePercentageCloserFiltering = true; sg.filteringQuality = ShadowGenerator.QUALITY_HIGH; sg.bias = 0.0006; sg.normalBias = 0.02; sg.darkness = 0.15;
for (const m of K.casters) sg.addShadowCaster(m as AbstractMesh); sun.autoCalcShadowZBounds = true;
// indoors: the hall's lamps lit (as the terminal's), dimmer sky light; outdoors at dusk: the hall lit behind its windows
if (v.inside) { hemi.intensity = 0.16; sc.environmentIntensity = 0.35; for (const l of S.lights) l.intensity = 1.0; }
else if (!dusk) for (const l of S.lights) l.setEnabled(false);
const cam = new UniversalCamera('c', new Vector3(v.eye[0], v.eye[1], v.eye[2]), sc); cam.setTarget(new Vector3(v.at[0], v.at[1], v.at[2])); cam.fov = v.fov; cam.minZ = 0.05; cam.maxZ = 5000;
sc.activeCamera = cam;
const dp = new DefaultRenderingPipeline('dp', true, sc, [cam]); dp.fxaaEnabled = true; dp.samples = 4; dp.imageProcessingEnabled = true;
dp.imageProcessing.toneMappingEnabled = true; dp.imageProcessing.toneMappingType = ImageProcessingConfiguration.TONEMAPPING_ACES;
dp.imageProcessing.exposure = v.inside ? 1.05 : dusk ? 1.35 : 0.95; dp.imageProcessing.contrast = 1.1;
dp.bloomEnabled = true; dp.bloomThreshold = dusk || v.inside ? 0.8 : 0.95; dp.bloomWeight = dusk ? 0.3 : 0.12; dp.bloomKernel = 48;
console.log('built', sc.meshes.length);
sc.executeWhenReady(() => { let n = 0; eng.runRenderLoop(() => { sc.render(); console.log('f' + n); if (++n === 5) { eng.stopRenderLoop(); (window as any).DONE = true; } }); });
void HALL; void PointLight;
