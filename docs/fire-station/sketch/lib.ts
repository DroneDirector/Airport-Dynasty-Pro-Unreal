// Design sketch of the fire station and its trucks (drawing aid only; the game build is in Unreal C++).
// Small building helpers for the fire station renders: boxes with their textures tiled at true size, cylinders,
// lettering, and materials from the photo textures (tex/).
import { Scene, Mesh, MeshBuilder, TransformNode, Vector3, Vector4, Color3, PBRMaterial, StandardMaterial, DynamicTexture, Texture, AbstractMesh } from '@babylonjs/core';

export class Kit {
  casters: AbstractMesh[] = [];
  root: TransformNode;
  private n = 0;
  private texCache: { [k: string]: Texture } = {};
  constructor(public sc: Scene, name = 'root') { this.root = new TransformNode(name, sc); }
  tex(file: string) { if (!this.texCache[file]) { const t = new Texture('tex/' + file, this.sc); t.anisotropicFilteringLevel = 16; t.wrapU = t.wrapV = Texture.WRAP_ADDRESSMODE; this.texCache[file] = t; } return this.texCache[file]; }
  /** a PBR material: colour, or a photo texture set (D and optional N) tiled every (tu, tv) metres */
  pbr(o: { col?: string; tex?: string; nrm?: boolean; rough?: number; metal?: number; alpha?: number; emis?: string; emisK?: number; two?: boolean; clear?: number; name?: string }) {
    const m = new PBRMaterial(o.name ?? 'm' + this.n++, this.sc); m.maxSimultaneousLights = 12;
    m.albedoColor = o.col ? Color3.FromHexString(o.col) : Color3.White();
    if (o.tex) { m.albedoTexture = this.tex(o.tex + '_D.png'); if (o.nrm) { m.bumpTexture = this.tex(o.tex + '_N.png'); m.bumpTexture.level = 0.8; } }
    m.metallic = o.metal ?? 0; m.roughness = o.rough ?? 0.6;
    if (o.alpha !== undefined) { m.alpha = o.alpha; m.transparencyMode = 2; }
    if (o.emis) m.emissiveColor = Color3.FromHexString(o.emis).scale(o.emisK ?? 1);
    if (o.clear) { m.clearCoat.isEnabled = true; m.clearCoat.intensity = o.clear; m.clearCoat.roughness = 0.08; }
    m.backFaceCulling = !o.two; return m;
  }
  glow(col: string, k = 1) { const m = new StandardMaterial('g' + this.n++, this.sc); m.emissiveColor = Color3.FromHexString(col).scale(k); m.disableLighting = true; return m; }
  add<T extends AbstractMesh>(m: T, par?: TransformNode | null, cast = true) { m.parent = par || this.root; m.receiveShadows = true; if (cast) this.casters.push(m); return m; }
  /** a box between two corners; its texture tiled every tu x tv metres (u along the face's width, v up) */
  box(mat: any, x0: number, y0: number, z0: number, x1: number, y1: number, z1: number, tu = 0, tv = 0, par?: TransformNode | null, cast = true) {
    const w = Math.abs(x1 - x0), h = Math.abs(y1 - y0), d = Math.abs(z1 - z0); tv = tv || tu;
    const fu = tu ? [new Vector4(0, 0, w / tu, h / tv), new Vector4(0, 0, w / tu, h / tv), new Vector4(0, 0, d / tu, h / tv), new Vector4(0, 0, d / tu, h / tv), new Vector4(0, 0, w / tu, d / tv), new Vector4(0, 0, w / tu, d / tv)] : undefined;
    const m = MeshBuilder.CreateBox('b' + this.n++, { width: w, height: h, depth: d, faceUV: fu, wrap: true }, this.sc);
    m.material = mat; m.position.set((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2); return this.add(m, par, cast);
  }
  /** a box by its centre and size, turned (radians) */
  boxC(mat: any, x: number, y: number, z: number, w: number, h: number, d: number, rx = 0, ry = 0, rz = 0, par?: TransformNode | null, cast = true) {
    const m = MeshBuilder.CreateBox('c' + this.n++, { width: w, height: h, depth: d }, this.sc); m.material = mat; m.position.set(x, y, z); m.rotation.set(rx, ry, rz); return this.add(m, par, cast);
  }
  cyl(mat: any, x: number, y: number, z: number, r: number, h: number, axis: 'x' | 'y' | 'z' = 'y', par?: TransformNode | null, seg = 24, rTop?: number) {
    const m = MeshBuilder.CreateCylinder('y' + this.n++, { height: h, diameterBottom: r * 2, diameterTop: (rTop ?? r) * 2, tessellation: seg }, this.sc); m.material = mat; m.position.set(x, y, z);
    if (axis === 'x') m.rotation.z = Math.PI / 2; if (axis === 'z') m.rotation.x = Math.PI / 2; return this.add(m, par);
  }
  tube(mat: any, pts: Vector3[], r: number, par?: TransformNode | null) { const m = MeshBuilder.CreateTube('t' + this.n++, { path: pts, radius: r, tessellation: 10, cap: 3 }, this.sc); m.material = mat; return this.add(m, par); }
  node(x = 0, y = 0, z = 0, ry = 0, par?: TransformNode | null) { const t = new TransformNode('n' + this.n++, this.sc); t.parent = par || this.root; t.position.set(x, y, z); t.rotation.y = ry; return t; }
  /** a flat picture (a canvas) on a plane facing -z (rotate to turn it), optionally glowing */
  decal(c: HTMLCanvasElement, w: number, h: number, x: number, y: number, z: number, ry = 0, par?: TransformNode | null, glow = 0, alpha = false) {
    const t = new DynamicTexture('d' + this.n++, { width: c.width, height: c.height }, this.sc, true); (t.getContext() as any).drawImage(c, 0, 0); t.update(true); t.hasAlpha = alpha;
    const m = new StandardMaterial('dm' + this.n++, this.sc); m.diffuseTexture = t; m.useAlphaFromDiffuseTexture = alpha; m.specularColor = new Color3(0.06, 0.06, 0.06);
    if (glow) { m.emissiveTexture = t; m.emissiveColor = new Color3(glow, glow, glow); }
    const p = MeshBuilder.CreatePlane('p' + this.n++, { width: w, height: h }, this.sc); p.material = m; p.position.set(x, y, z); p.rotation.y = ry; return this.add(p, par, false);
  }
}

export const canvas = (w: number, h = w) => { const c = document.createElement('canvas'); c.width = w; c.height = h; return c; };
/** lettering on a transparent canvas */
export function lettering(lines: { t: string; size: number; weight?: number }[], w: number, h: number, col = '#FFFFFF', align: CanvasTextAlign = 'left') {
  const c = canvas(w, h), x = c.getContext('2d')!; x.fillStyle = col; x.textBaseline = 'top'; x.textAlign = align;
  let y = 0; for (const l of lines) { x.font = `${l.weight ?? 600} ${l.size}px Arial, Helvetica, sans-serif`; x.fillText(l.t, align === 'left' ? 0 : align === 'right' ? w : w / 2, y); y += l.size * 1.18; }
  return c;
}
