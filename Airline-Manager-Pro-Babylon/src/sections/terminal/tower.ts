// The control tower (ADC, aerodrome control), in the terminal's own design: white cladding, warm timber, the same outer
// glazing as the gates. Heights against the terminal (its roof at 12 m): the cab's floor at 17.6 m, its roof at 22 m.
//  - The shaft (0–13.5 m): round, white, with vertical timber fins and a slot of glass up the side facing the apron;
//    at its foot the entrance under a timber canopy. Inside it, against its north wall, the lift from the ground floor
//    to the lobby level, its doors facing west, clear of the spiral stair above.
//  - The lobby level (13.5–17.3 m, a little taller than a normal floor): where the lift stops, one level below the cab;
//    a band of windows round it, lockers and a small kitchen. From its middle a spiral stair climbs round its column
//    up through the cab's floor.
//  - The cab (17.6–22 m), wider than everything below it: glass all round, leaning out at the top (no reflections of
//    the room in it), thin black mullions; the roof over it with its timber soffit, the walkway round it, reached by a glass door on the
//    east side (behind a lift-up flap in the desk).
//    Inside, the control positions run in one unbroken ring just inside the glass: a continuous desk following the cab's
//    circle all the way round and meeting itself without a joint, monitors along it, a chair at each position (each chair
//    its own object, ADC/CHAIR-nn, ready to be swapped for a better model later).
// x east, z north, y up, metres; the tower's foot at (x, 0, z).
import { MeshBuilder, Mesh, Vector3, TransformNode, VertexData, AbstractMesh, Quaternion } from '@babylonjs/core';
import { buildLift, LiftCtl } from './lifts';
import type { Kit } from './kit';
import { glassUV } from './kit';

export const TOWER = { liftZ: 2.45, doorA: Math.PI / 8, doorW: 1.0, doorH: 2.25, shaftR: 3.6, lobbyY: 13.5, lobbyTop: 17.3, cabY: 17.6, glassY: 18.45, cabTop: 21.4, roofTop: 22.0, cabR0: 6.0, cabR1: 6.7, roofR: 7.5, deskR0: 4.75, deskR1: 5.65, positions: 24, chairs: 12 };
/** Where the tower stands at your airport (plan metres, the terminal's frame): site A, west of the pier at the apron's
 *  west edge, 75 m clear of link P1 and taxilane T (sheet: ground layout, tower options). */
export const TOWER_AT = { x: -205, z: 120 };
export interface Tower { root: TransformNode; chairs: TransformNode[]; glass: Mesh[]; cutaway: AbstractMesh[];
  /** the lift (it runs with the terminal's lifts); what moves (never fixed or merged); what is walked through (not solid);
   *  the curved shells (kept as they are, so they stay solid from both sides); the frame step: doors and the desk flap */
  lift: LiftCtl; moving: AbstractMesh[]; soft: AbstractMesh[]; shells: AbstractMesh[]; step: (px: number, py: number, pz: number, dt: number) => void }

/** A flat convex floor piece: the disc of radius r cut by half-planes (keep where nx·x + nz·z ≤ d), facing up, at y. */
function discPiece(name: string, sc: any, r: number, y: number, cuts: [number, number, number][], n = 96): Mesh {
  let poly: [number, number][] = []; for (let i = 0; i < n; i++) { const a = i / n * Math.PI * 2; poly.push([Math.cos(a) * r, Math.sin(a) * r]); }
  for (const [nx, nz, d] of cuts) { const out: [number, number][] = [], f = (p: [number, number]) => nx * p[0] + nz * p[1] - d;
    for (let i = 0; i < poly.length; i++) { const A = poly[i], B = poly[(i + 1) % poly.length], fa = f(A), fb = f(B);
      if (fa <= 0) out.push(A); if ((fa <= 0) !== (fb <= 0)) { const t = fa / (fa - fb); out.push([A[0] + (B[0] - A[0]) * t, A[1] + (B[1] - A[1]) * t]); } } poly = out; }
  const cx = poly.reduce((s, p) => s + p[0], 0) / poly.length, cz = poly.reduce((s, p) => s + p[1], 0) / poly.length;
  const P: number[] = [cx, y, cz], I: number[] = [], U: number[] = [cx / 3, cz / 3]; for (const p of poly) { P.push(p[0], y, p[1]); U.push(p[0] / 3, p[1] / 3); }
  for (let i = 0; i < poly.length; i++) I.push(0, 1 + (i + 1) % poly.length, 1 + i);
  const m = new Mesh(name, sc), d = new VertexData(), N: number[] = []; d.positions = P; d.indices = I; d.uvs = U; VertexData.ComputeNormals(P, I, N); d.normals = N; d.applyToMesh(m); return m;
}

/** A flat ring (annulus) between r0 and r1 at height y, facing up (or down). */
function annulus(name: string, sc: any, r0: number, r1: number, y: number, n = 128, down = false, a0 = 0, a1 = Math.PI * 2): Mesh {
  const P: number[] = [], I: number[] = [], U: number[] = [];
  for (let i = 0; i <= n; i++) { const a = a0 + (a1 - a0) * i / n, c = Math.cos(a), s = Math.sin(a); P.push(c * r0, y, s * r0, c * r1, y, s * r1); U.push(c * r0 / 3, s * r0 / 3, c * r1 / 3, s * r1 / 3); }
  for (let i = 0; i < n; i++) { const a = i * 2; if (down) I.push(a, a + 1, a + 2, a + 1, a + 3, a + 2); else I.push(a, a + 2, a + 1, a + 1, a + 2, a + 3); }
  const m = new Mesh(name, sc), d = new VertexData(), N: number[] = []; d.positions = P; d.indices = I; d.uvs = U; VertexData.ComputeNormals(P, I, N); d.normals = N; d.applyToMesh(m); return m;
}
/** An open tube wall from radius r0 at y0 to r1 at y1 (all the way round, or over an arc). */
function wall(name: string, sc: any, r0: number, r1: number, y0: number, y1: number, n = 128, a0 = 0, a1 = Math.PI * 2, inward = false): Mesh {
  const P: number[] = [], I: number[] = [], U: number[] = [];
  for (let i = 0; i <= n; i++) { const a = a0 + (a1 - a0) * i / n, c = Math.cos(a), s = Math.sin(a); P.push(c * r0, y0, s * r0, c * r1, y1, s * r1); U.push(a * r0 / 3, y0 / 3, a * r1 / 3, y1 / 3); }
  for (let i = 0; i < n; i++) { const a = i * 2; if (inward) I.push(a, a + 1, a + 2, a + 1, a + 3, a + 2); else I.push(a, a + 2, a + 1, a + 1, a + 2, a + 3); }
  const m = new Mesh(name, sc), d = new VertexData(), N: number[] = []; d.positions = P; d.indices = I; d.uvs = U; VertexData.ComputeNormals(P, I, N); d.normals = N; d.applyToMesh(m); return m;
}

export function buildTower(K: Kit, x: number, z: number, face = 0): Tower {
  const { sc, M } = K, T = TOWER, root = new TransformNode('ADC-TOWER', sc); root.position.set(x, 0, z); root.rotation.y = face;
  root.metadata = { id: 'ADC-TOWER', kind: 'tower', name: 'Control tower (ATC)' };
  const glass: Mesh[] = [], cutaway: AbstractMesh[] = [], chairs: TransformNode[] = [], moving: AbstractMesh[] = [], soft: AbstractMesh[] = [], shells: AbstractMesh[] = [];
  const toK = (lx: number, lz: number): [number, number] => [x + lx * Math.cos(face) + lz * Math.sin(face), z - lx * Math.sin(face) + lz * Math.cos(face)];
  const own = <M2 extends AbstractMesh>(m: M2, mat: any, cut = false): M2 => { m.material = mat; m.parent = root; m.receiveShadows = true; K.casters.push(m); if (cut) cutaway.push(m); if (/^tw-/.test(m.name)) shells.push(m); return m; };
  const box = (w: number, h: number, d: number, mat: any, px: number, py: number, pz: number, ry = 0, cut = false) => { const b = MeshBuilder.CreateBox(K.nm('tw'), { width: w, height: h, depth: d }, sc); b.position.set(px, py, pz); b.rotation.y = ry; return own(b, mat, cut); };
  const cyl = (r: number, h: number, mat: any, px: number, py: number, pz: number, rt?: number, n = 48, cut = false) => { const c = MeshBuilder.CreateCylinder(K.nm('tw'), { diameterBottom: r * 2, diameterTop: (rt ?? r) * 2, height: h, tessellation: n }, sc); c.position.set(px, py, pz); return own(c, mat, cut); };
  const glazing = (m: Mesh) => { m.material = M.glassOuter; m.parent = root; glassUV(m, false, 3); glass.push(m); cutaway.push(m); shells.push(m); return m; };
  const ang = (i: number, n: number) => i / n * Math.PI * 2;

  // ---- the foot: the plinth, the entrance hall (walked into: a ramp up to the plinth, sliding glass doors) under its timber canopy ----
  own(annulus('tw-plinth', sc, 0, T.shaftR + 1.2, 0.02), M.terrazzo);
  cyl(T.shaftR + 0.6, 0.4, M.concrete, 0, 0.2, 0, undefined, 64);
  const EX0 = -6.6, EX1 = -3.0, EZ = 1.75, gapA = Math.asin(EZ / T.shaftR), ex = -T.shaftR - 1.6;
  for (const sz of [-1, 1]) box(EX1 - EX0, 3.6, 0.2, M.white, (EX0 + EX1) / 2, 1.8, sz * (EZ + 0.1));          // its side walls
  box(EX1 - EX0, 0.2, EZ * 2 + 0.4, M.white, (EX0 + EX1) / 2, 3.5, 0);                                          // its roof
  { const L = Math.hypot(-4.2 - EX0, 0.38), rp = box(L, 0.06, EZ * 2, M.terrazzo, (EX0 - 4.2) / 2, 0.19, 0); rp.rotation.z = Math.atan2(0.38, -4.2 - EX0); }   // the ramp up to the plinth
  box(EX1 + 4.2, 0.02, EZ * 2, M.terrazzo, (EX1 - 4.2) / 2, 0.41, 0);                                           // its floor on the plinth
  for (const sz of [-1, 1]) box(0.06, 2.4, EZ - 0.8, M.glassBlue, EX0 + 0.04, 1.2, sz * (0.8 + (EZ - 0.8) / 2));  // the fixed glass either side of the doors
  box(0.06, 1.0, EZ * 2, M.glassBlue, EX0 + 0.04, 2.9, 0); box(0.1, 0.08, EZ * 2, M.black, EX0 + 0.04, 2.42, 0);   // the glass over them, the door head
  const eDoors: TransformNode[] = [];
  for (const sz of [-1, 1]) { const n = new TransformNode('ADC/ENTRANCE-DOOR-' + (sz < 0 ? 'S' : 'N'), sc); n.parent = root; n.position.set(EX0 + 0.12, 0, sz * 0.4); eDoors.push(n);
    const lf = MeshBuilder.CreateBox(K.nm('tw'), { width: 0.04, height: 2.38, depth: 0.8 }, sc); lf.position.y = 1.19; lf.parent = n; lf.material = M.glassBlue; moving.push(lf); soft.push(lf);
    const fr = MeshBuilder.CreateBox(K.nm('tw'), { width: 0.06, height: 2.38, depth: 0.05 }, sc); fr.position.set(0, 1.19, -sz * 0.38); fr.parent = n; fr.material = M.steel; moving.push(fr); soft.push(fr); }
  box(4.6, 0.25, 6.4, M.timber, ex - 0.2, 3.75, 0); box(4.62, 0.08, 6.42, M.white, ex - 0.2, 3.92, 0);
  { const sg = MeshBuilder.CreatePlane(K.nm('tw'), { width: 3.0, height: 0.42 }, sc); sg.position.set(EX0 - 0.03, 3.2, 0); sg.rotation.y = Math.PI / 2; own(sg, K.picture(signText('CONTROL TOWER · ATC'), 0.8)); soft.push(sg); }
  // ---- the shaft: white, timber fins all round, a slot of glass up the apron side, the way in from the entrance hall ----
  own(wall('tw-shaft', sc, T.shaftR, T.shaftR, 0.4, T.lobbyY, 96, Math.PI + gapA, Math.PI - gapA + Math.PI * 2), M.white, true);
  own(wall('tw-shaft-overdoor', sc, T.shaftR, T.shaftR, 3.6, T.lobbyY, 12, Math.PI - gapA, Math.PI + gapA), M.white, true);
  for (let i = 0; i < 28; i++) { const a = ang(i, 28); if (Math.abs(Math.atan2(Math.sin(a), Math.cos(a))) < 0.32 || Math.abs(Math.atan2(Math.sin(a - Math.PI), Math.cos(a - Math.PI))) < gapA + 0.12) continue;
    const f = box(0.16, T.lobbyY - 1.6, 0.42, M.timber, Math.cos(a) * (T.shaftR + 0.2), 0.8 + (T.lobbyY - 1.6) / 2 + 0.4, Math.sin(a) * (T.shaftR + 0.2)); f.rotation.y = -a; }
  { const slot = wall('tw-slot', sc, T.shaftR + 0.01, T.shaftR + 0.01, 1.2, T.lobbyY - 0.4, 12, -0.28, 0.28); glazing(slot); }
  // ---- the lift inside the shaft, against its north wall and clear of the spiral stair (which climbs from the middle
  //      of the lobby level): from the ground floor to the lobby level, its doors facing west at both stops ----
  const LZ = T.liftZ, LO = 0.95;
  const [lkx, lkz] = toK(0, LZ);
  const lift = buildLift(K, { id: 'ADC-LIFT', room: 'ADC', x: lkx, z: lkz, face: face - Math.PI / 2, lo: 0.4, hi: T.lobbyY, top: T.lobbyY + 2.75, out: LO, car: 0.76, door: 0.45, speed: 2.0, names: ['G', 'ATC'], sign: 'TOWER LIFT' });
  for (const m of lift.moving_parts) moving.push(m);
  // the escape stair up the shaft (round the south side, away from the lift and its landings), shown in the cut; not walked
  for (let s = 0; s < 60; s++) { const a = s * 0.42, r = 2.4, y = 0.6 + s * (T.lobbyY - 0.8) / 60; if (Math.sin(a) > 0.3) continue; soft.push(box(1.0, 0.06, 0.32, M.concrete, Math.cos(a) * r, y, Math.sin(a) * r, -a, true)); }

  // ---- the lobby level: a touch wider, white, a band of windows round it ----
  const LR = T.shaftR + 0.8;
  own(annulus('tw-lobby-under', sc, T.shaftR, LR, T.lobbyY - 0.2, 96, true), M.timber);
  own(wall('tw-lobby-lo', sc, LR, LR, T.lobbyY - 0.2, T.lobbyY + 1.1, 96), M.white, true);
  glazing(wall('tw-lobby-glass', sc, LR, LR, T.lobbyY + 1.1, T.lobbyY + 2.9, 96));
  for (let i = 0; i < 20; i++) { const a = ang(i, 20); const mu = box(0.06, 1.8, 0.06, M.black, Math.cos(a) * LR, T.lobbyY + 2.0, Math.sin(a) * LR); cutaway.push(mu); }
  own(wall('tw-lobby-hi', sc, LR, LR, T.lobbyY + 2.9, T.lobbyTop, 96), M.white, true);
  // the floor, open over the lift's shaft (the car rises into it)
  { const z0 = LZ - LO, z1 = LZ + LO; for (const [nm, cuts] of [['S', [[0, 1, z0]]], ['W', [[0, -1, -z0], [1, 0, -LO]]], ['E', [[0, -1, -z0], [-1, 0, -LO]]], ['N', [[0, -1, -z1], [1, 0, LO], [-1, 0, LO]]]] as [string, [number, number, number][]][])
    own(discPiece('tw-lobby-floor-' + nm, sc, LR, T.lobbyY + 0.01, cuts), M.carpetGrey); }
  own(annulus('tw-lobby-ceil', sc, 1.45, LR, T.lobbyTop - 0.01, 96, true), M.plaster);
  // in it: lockers along the wall, a small kitchen, a bench; downlights
  for (let i = 0; i < 6; i++) { const a = Math.PI * 0.95 + i * 0.13, r = LR - 0.35; const l = box(0.55, 2.0, 0.5, M.steel, Math.cos(a) * r, T.lobbyY + 1.0, Math.sin(a) * r); l.rotation.y = -a + Math.PI / 2; }
  { const a = Math.PI * 1.45, r = LR - 0.38; const k = box(2.2, 0.9, 0.6, M.walnut, Math.cos(a) * r, T.lobbyY + 0.45, Math.sin(a) * r); k.rotation.y = -a + Math.PI / 2; const t = box(2.25, 0.04, 0.65, M.whiteGloss, Math.cos(a) * r, T.lobbyY + 0.92, Math.sin(a) * r); t.rotation.y = -a + Math.PI / 2; }
  for (let i = 0; i < 8; i++) { const a = ang(i, 8), r = 2.9; cyl(0.1, 0.02, M.light, Math.cos(a) * r, T.lobbyTop - 0.02, Math.sin(a) * r, undefined, 16); }

  // ---- the spiral stair from the lobby level up through the cab's floor, round its column ----
  const SR = 1.35, rise = T.cabY - T.lobbyY, steps = 22, turn = Math.PI * 1.85;
  cyl(0.14, rise + 1.1, M.steel, 0, T.lobbyY + (rise + 1.1) / 2, 0, undefined, 24);
  const rail: Vector3[] = [];
  for (let s = 0; s < steps; s++) { const a = s / steps * turn, y = T.lobbyY + (s + 1) * rise / steps;
    const st = box(SR - 0.15, 0.05, 0.42, M.oak, Math.cos(a) * (SR / 2 + 0.07), y - 0.025, Math.sin(a) * (SR / 2 + 0.07)); st.rotation.y = -a; soft.push(st);
    cyl(0.012, 1.0, M.steel, Math.cos(a) * (SR - 0.06), y + 0.5, Math.sin(a) * (SR - 0.06), undefined, 8); }
  for (let k = 0; k <= 60; k++) { const a = k / 60 * turn, y = T.lobbyY + 1.0 + k / 60 * rise; rail.push(new Vector3(Math.cos(a) * (SR - 0.06), y, Math.sin(a) * (SR - 0.06))); }
  own(MeshBuilder.CreateTube(K.nm('tw'), { path: rail, radius: 0.025, tessellation: 10 }, sc), M.walnut);
  // walked as a smooth ramp winding up over the treads (unseen), from the lobby's floor to the cab's; a landing at the top
  { const a0 = -turn / steps, N = 72, P: number[] = [], I: number[] = [];
    for (let k = 0; k <= N; k++) { const a = a0 + (turn - a0) * k / N, y = T.lobbyY + 0.02 + (rise - 0.02) * k / N; for (const r of [0.2, SR - 0.08]) P.push(Math.cos(a) * r, y, Math.sin(a) * r); }
    for (let k = 0; k < N; k++) { const q = k * 2; I.push(q, q + 2, q + 1, q + 1, q + 2, q + 3, q, q + 1, q + 2, q + 1, q + 3, q + 2); }
    const rm = new Mesh('ADC/STAIR-RAMP', sc), vd = new VertexData(); vd.positions = P; vd.indices = I; vd.applyToMesh(rm); rm.parent = root; rm.isVisible = false; rm.isPickable = false; rm.metadata = { kind: 'ramp' }; shells.push(rm); }
  own(annulus('tw-stair-landing', sc, 0.14, 1.45, T.cabY, 24, false, turn, Math.PI * 2 + 0.45), M.oak);

  // ---- the cab: its floor (wider than everything under it), the walkway round it, the upstand, the glass, the roof ----
  own(annulus('tw-cab-under', sc, LR, T.roofR - 0.1, T.cabY - 0.32, 128, true), M.timber);
  own(wall('tw-cab-edge', sc, T.roofR - 0.1, T.roofR - 0.1, T.cabY - 0.32, T.cabY - 0.05, 128), M.white);
  own(annulus('tw-walkway', sc, T.cabR0, T.roofR - 0.1, T.cabY - 0.05), M.rubber);
  { const g = wall('tw-walk-rail', sc, T.roofR - 0.12, T.roofR - 0.12, T.cabY - 0.05, T.cabY + 1.05, 128); g.material = M.glassBlue; g.parent = root; cutaway.push(g);
    const top: Vector3[] = []; for (let k = 0; k <= 128; k++) { const a = k / 128 * Math.PI * 2; top.push(new Vector3(Math.cos(a) * (T.roofR - 0.12), T.cabY + 1.07, Math.sin(a) * (T.roofR - 0.12))); } const rt = own(MeshBuilder.CreateTube(K.nm('tw'), { path: top, radius: 0.035, tessellation: 10 }, sc), M.steel); cutaway.push(rt); }
  own(annulus('tw-cab-floor', sc, 1.45, T.cabR0, T.cabY), M.carpetGrey);
  // the door out to the walkway (east, between two mullions): the upstand and the glass leave a gap for it
  const dHalf = T.doorW / 2 / T.cabR0 + 0.02, dA0 = T.doorA - dHalf, dA1 = T.doorA + dHalf, A2 = Math.PI * 2;
  const rAt = (y: number) => T.cabR0 + 0.02 + (T.cabR1 - T.cabR0 - 0.02) * (y - T.glassY) / (T.cabTop - T.glassY), dTop = T.cabY + T.doorH;
  own(wall('tw-cab-upstand', sc, T.cabR0, T.cabR0 + 0.04, T.cabY, T.glassY, 120, dA1, dA0 + A2), M.white, true);
  own(wall('tw-cab-upstand-in', sc, T.cabR0 - 0.08, T.cabR0 - 0.04, T.cabY, T.glassY, 120, dA1, dA0 + A2, true), M.whiteGloss, true);
  const rTopGlass = T.cabR1;
  glazing(wall('tw-cab-glass', sc, T.cabR0 + 0.02, rTopGlass, T.glassY, T.cabTop, 124, dA1, dA0 + A2));
  glazing(wall('tw-cab-glass-overdoor', sc, rAt(dTop + 0.08), rTopGlass, dTop + 0.08, T.cabTop, 6, dA0, dA1));
  let bDoor: TransformNode = null as any;
  { const dr = T.cabR0 + 0.02, d = new TransformNode('ADC/BALCONY-DOOR', sc); d.parent = root; d.position.set(Math.cos(T.doorA) * dr, T.cabY, Math.sin(T.doorA) * dr); d.rotation.y = Math.PI / 2 - T.doorA;
    d.metadata = { id: 'ADC/BALCONY-DOOR', kind: 'door', opens: 'out', hinge: 'left', note: 'the door from the cab out to the walkway round it' };
    const dp = (w: number, h: number, dd: number, mat: any, px: number, py: number, pz: number) => { const b = MeshBuilder.CreateBox(K.nm('tw'), { width: w, height: h, depth: dd }, sc); b.position.set(px, py, pz); b.parent = d; b.material = mat; b.receiveShadows = true; K.casters.push(b); cutaway.push(b); return b; };
    const W = dHalf * 2 * dr;
    for (const sx of [-1, 1]) dp(0.08, T.doorH + 0.08, 0.14, M.black, sx * (W / 2 - 0.04), (T.doorH + 0.08) / 2, 0);   // the frame
    dp(W, 0.08, 0.14, M.black, 0, T.doorH + 0.04, 0); dp(W, 0.03, 0.2, M.steel, 0, 0.015, 0);                        // its head and threshold
    // the leaf: glass in a thin frame, hinged on its left, opening out onto the walkway
    const hx = -(W / 2 - 0.1); bDoor = new TransformNode('ADC/BALCONY-DOOR-LEAF', sc); bDoor.parent = d; bDoor.position.set(hx, 0, 0);
    const lp = (w: number, h: number, dd: number, mat: any, px: number, py: number, pz: number) => { const b = MeshBuilder.CreateBox(K.nm('tw'), { width: w, height: h, depth: dd }, sc); b.position.set(px - hx, py, pz); b.parent = bDoor; b.material = mat; b.receiveShadows = true; K.casters.push(b); cutaway.push(b); moving.push(b); soft.push(b); return b; };
    lp(W - 0.2, T.doorH - 0.1, 0.03, M.glassBlue, 0, T.doorH / 2, 0);
    lp(W - 0.16, 0.06, 0.05, M.black, 0, 0.06, 0); lp(W - 0.16, 0.06, 0.05, M.black, 0, T.doorH - 0.06, 0);
    for (const sz of [-1, 1]) lp(0.03, 0.5, 0.03, M.chrome, W / 2 - 0.22, 1.05, sz * 0.07);                            // the pull handles
    const sill = annulus('tw-door-step', sc, T.cabR0 - 0.05, T.cabR0 + 0.25, T.cabY - 0.04, 6, false, dA0, dA1); own(sill, M.steel); }
  for (let i = 0; i < 24; i++) { const a = ang(i, 24), p0 = new Vector3(Math.cos(a) * (T.cabR0 + 0.02), T.glassY, Math.sin(a) * (T.cabR0 + 0.02)), p1 = new Vector3(Math.cos(a) * rTopGlass, T.cabTop, Math.sin(a) * rTopGlass);
    const mu = own(MeshBuilder.CreateTube(K.nm('tw'), { path: [p0, p1], radius: 0.035, tessellation: 6 }, sc), M.black); cutaway.push(mu); }
  own(annulus('tw-roof-soffit', sc, 0, T.roofR, T.cabTop, 128, true), M.timber, true);
  own(wall('tw-roof-fascia', sc, T.roofR, T.roofR - 0.25, T.cabTop, T.roofTop, 128), M.white, true);
  own(annulus('tw-roof-top', sc, 0, T.roofR - 0.25, T.roofTop), M.white, true);
  for (let i = 0; i < 16; i++) { const a = ang(i, 16), r = T.cabR1 - 0.4; cyl(0.08, 0.02, M.light, Math.cos(a) * r, T.cabTop - 0.02, Math.sin(a) * r, undefined, 16).metadata = { cut: true }; }
  // on the roof: the mast with its antennas and the red obstruction light, the anemometer, a small radar dome
  cyl(0.12, 4.2, M.steel, 0, T.roofTop + 2.1, 0, 0.06, 12, true); cyl(0.18, 0.25, M.red, 0, T.roofTop + 4.3, 0, undefined, 16, true);
  for (const [h, w] of [[1.2, 1.4], [2.2, 1.0], [3.1, 0.7]]) box(w, 0.04, 0.04, M.steel, 0, T.roofTop + h, 0, 0.6, true);
  cyl(0.05, 1.8, M.steel, 3.2, T.roofTop + 0.9, 2.0, undefined, 8, true); for (let i = 0; i < 3; i++) { const a = ang(i, 3); cyl(0.07, 0.1, M.white, 3.2 + Math.cos(a) * 0.18, T.roofTop + 1.8, 2.0 + Math.sin(a) * 0.18, undefined, 10, true); }
  { const d = MeshBuilder.CreateSphere(K.nm('tw'), { diameter: 1.4, slice: 0.5, segments: 20 }, sc); d.position.set(-3.0, T.roofTop, -1.6); own(d, M.white, true); }
  // ---- the stair's opening in the cab's floor, with a glass balustrade round it ----
  { const g = wall('tw-hole-glass', sc, 1.45, 1.45, T.cabY, T.cabY + 1.0, 48, 0.45, turn); g.material = M.glassBlue; g.parent = root; const ring = own(wall('tw-hole-edge', sc, 1.45, 1.45, T.cabY - 0.3, T.cabY, 48), M.white); void ring; }

  // ---- the control positions: one continuous desk round the cab, monitors all along it, a chair at each position ----
  const DY = T.cabY, R0 = T.deskR0, R1 = T.deskR1;
  // the desk runs all the way round; in front of the balcony door its top is a lift-up counter flap (same lacquer, same
  // lip, so the ring still reads as one), with the walkway through under it to the door
  const fHalf = 0.5 / R0, fA0 = T.doorA - fHalf, fA1 = T.doorA + fHalf;
  own(annulus('tw-desk-top', sc, R0, R1, DY + 0.78, 186, false, fA1, fA0 + A2), M.lacquer);
  own(annulus('tw-desk-top-under', sc, R0, R1, DY + 0.74, 186, true, fA1, fA0 + A2), M.black);
  const hinge = new TransformNode('ADC/DESK-FLAP', sc); hinge.parent = root; hinge.position.set(Math.cos(T.doorA) * R1, DY + 0.78, Math.sin(T.doorA) * R1); hinge.rotationQuaternion = Quaternion.Identity();
  const flapIn = new TransformNode('ADC/DESK-FLAP-IN', sc); flapIn.parent = hinge; flapIn.position.set(-hinge.position.x, -hinge.position.y, -hinge.position.z);
  { hinge.metadata = { id: 'ADC/DESK-FLAP', kind: 'counterFlap', hingeAxis: 'tangent at the desk back, lifts up toward the glass', note: 'lift to walk through to the balcony door' };
    const ft = annulus('tw-desk-flap', sc, R0, R1, DY + 0.78, 6, false, fA0 + 0.002, fA1 - 0.002); ft.parent = flapIn; ft.material = M.lacquer; ft.receiveShadows = true; K.casters.push(ft); moving.push(ft); soft.push(ft);
    const fu = annulus('tw-desk-flap-under', sc, R0, R1, DY + 0.74, 6, true, fA0 + 0.002, fA1 - 0.002); fu.parent = flapIn; fu.material = M.black; moving.push(fu); soft.push(fu);
    const fl: Vector3[] = []; for (let k = 0; k <= 8; k++) { const a = fA0 + 0.002 + (fA1 - fA0 - 0.004) * k / 8; fl.push(new Vector3(Math.cos(a) * R0, DY + 0.76, Math.sin(a) * R0)); }
    const flip = MeshBuilder.CreateTube(K.nm('tw'), { path: fl, radius: 0.03, tessellation: 12 }, sc); flip.parent = flapIn; flip.material = M.walnut; moving.push(flip); soft.push(flip); }
  own(wall('tw-desk-front', sc, R0, R0, DY + 0.08, DY + 0.78, 186, fA1, fA0 + A2, true), M.walnut);
  own(wall('tw-desk-back', sc, R1, R1, DY + 0.08, DY + 0.78, 186, fA1, fA0 + A2), M.white);
  own(wall('tw-desk-kick', sc, R0 + 0.06, R0 + 0.06, DY, DY + 0.08, 186, fA1, fA0 + A2, true), M.black);
  for (const ea of [fA0, fA1]) { const e = box(R1 - R0, 0.7, 0.04, M.walnut, Math.cos(ea) * (R0 + R1) / 2, DY + 0.43, Math.sin(ea) * (R0 + R1) / 2); e.rotation.y = -ea; }  // the desk's end panels either side of the way through
  // the edge of the desk: a soft rounded lip in walnut, all the way round
  { const lip: Vector3[] = []; for (let k = 0; k <= 186; k++) { const a = fA1 + (fA0 + A2 - fA1) * k / 186; lip.push(new Vector3(Math.cos(a) * R0, DY + 0.76, Math.sin(a) * R0)); } own(MeshBuilder.CreateTube(K.nm('tw'), { path: lip, radius: 0.03, tessellation: 12 }, sc), M.walnut); }
  // the monitors, on a continuous riser at the back of the desk: three per position, all following the circle
  own(wall('tw-riser', sc, R1 - 0.22, R1 - 0.22, DY + 0.78, DY + 0.86, 186, fA1, fA0 + A2, true), M.black);
  const scr = K.picture(screenPic(), 1.1);
  const NP = T.positions;
  for (let p = 0; p < NP; p++) for (let k = -1; k <= 1; k++) {
    const a = ang(p, NP) + k * (Math.PI * 2 / NP / 3); if (Math.abs(Math.atan2(Math.sin(a - T.doorA), Math.cos(a - T.doorA))) < fHalf + 0.05) continue;   // none on the flap
    const r = R1 - 0.25, cx = Math.cos(a) * r, cz = Math.sin(a) * r, yaw = Math.PI / 2 - a;
    const n = new TransformNode(K.nm('mon'), sc); n.parent = root; n.position.set(cx, DY + 0.86, cz); n.rotation.y = yaw;
    const st = MeshBuilder.CreateBox(K.nm('tw'), { width: 0.06, height: 0.22, depth: 0.04 }, sc); st.position.set(0, 0.11, 0.02); st.parent = n; st.material = M.steel;
    const bz = MeshBuilder.CreateBox(K.nm('tw'), { width: 0.5, height: 0.32, depth: 0.03 }, sc); bz.position.set(0, 0.38, 0); bz.rotation.x = -0.12; bz.parent = n; bz.material = M.screenBlack;
    const sp = MeshBuilder.CreatePlane(K.nm('tw'), { width: 0.47, height: 0.29 }, sc); sp.position.set(0, 0.38, -0.017); sp.rotation.x = -0.12; sp.parent = n; sp.material = scr;
    for (const m of [st, bz, sp]) { m.receiveShadows = true; }
    if (k === 0) { const kb = MeshBuilder.CreateBox(K.nm('tw'), { width: 0.44, height: 0.02, depth: 0.15 }, sc); kb.position.set(0, -0.07, -0.42); kb.parent = n; kb.material = M.black;
      const rd = MeshBuilder.CreateBox(K.nm('tw'), { width: 0.22, height: 0.05, depth: 0.16 }, sc); rd.position.set(0.36, -0.06, -0.3); rd.rotation.y = -0.3; rd.parent = n; rd.material = M.screenBlack;
      const strip = MeshBuilder.CreateBox(K.nm('tw'), { width: 0.4, height: 0.03, depth: 0.12 }, sc); strip.position.set(-0.36, -0.065, -0.32); strip.rotation.y = 0.3; strip.parent = n; strip.material = M.tealUI; }
  }
  // the chairs: one at each second position, facing out to the glass; each its own object to swap for a better model
  for (let c = 0; c < T.chairs; c++) {
    let a = ang(c, T.chairs) + Math.PI / NP; const da = Math.atan2(Math.sin(a - T.doorA), Math.cos(a - T.doorA)); if (Math.abs(da) < 0.3) a += Math.sign(da) * 0.07;   // a clear way to the flap
    const r = R0 - 0.55, ch = new TransformNode(`ADC/CHAIR-${String(c + 1).padStart(2, '0')}`, sc);
    ch.parent = root; ch.position.set(Math.cos(a) * r, DY, Math.sin(a) * r); ch.rotation.y = -a + Math.PI / 2;
    ch.metadata = { id: ch.name, kind: 'chair', swappable: true, modelSlot: 'adc-chair', note: 'a placeholder chair: swap for a better model (same origin: the floor under the seat, facing +z, out to the glass)' };
    const part = (m: Mesh, mat: any) => { m.parent = ch; m.material = mat; m.receiveShadows = true; K.casters.push(m); return m; };
    for (let l = 0; l < 5; l++) { const la = ang(l, 5), leg = part(MeshBuilder.CreateBox(K.nm('tw'), { width: 0.04, height: 0.035, depth: 0.32 }, sc), M.chrome); leg.position.set(Math.sin(la) * 0.16, 0.06, Math.cos(la) * 0.16); leg.rotation.y = la;
      const w = part(MeshBuilder.CreateSphere(K.nm('tw'), { diameter: 0.06, segments: 8 }, sc), M.black); w.position.set(Math.sin(la) * 0.31, 0.03, Math.cos(la) * 0.31); }
    part(MeshBuilder.CreateCylinder(K.nm('tw'), { diameter: 0.05, height: 0.38, tessellation: 12 }, sc), M.chrome).position.y = 0.26;
    const seat = part(MeshBuilder.CreateBox(K.nm('tw'), { width: 0.5, height: 0.09, depth: 0.48 }, sc), M.leatherBlack); seat.position.y = 0.49;
    const back = part(MeshBuilder.CreateBox(K.nm('tw'), { width: 0.48, height: 0.66, depth: 0.08 }, sc), M.leatherBlack); back.position.set(0, 0.88, -0.26); back.rotation.x = -0.12;
    const head = part(MeshBuilder.CreateBox(K.nm('tw'), { width: 0.3, height: 0.16, depth: 0.08 }, sc), M.leatherBlack); head.position.set(0, 1.29, -0.31); head.rotation.x = -0.12;
    for (const sx of [-1, 1]) { const ar = part(MeshBuilder.CreateBox(K.nm('tw'), { width: 0.05, height: 0.03, depth: 0.3 }, sc), M.black); ar.position.set(sx * 0.27, 0.7, 0.0); const ap = part(MeshBuilder.CreateBox(K.nm('tw'), { width: 0.03, height: 0.2, depth: 0.03 }, sc), M.black); ap.position.set(sx * 0.27, 0.6, -0.05); }
    chairs.push(ch);
  }
  // the supervisor's console by the stair, the wall clock and the weather display on the stair's column
  { const sv = box(1.6, 0.78, 0.7, M.walnut, -2.4, DY + 0.39, 0, Math.PI / 2); void sv; box(1.64, 0.04, 0.74, M.lacquer, -2.4, DY + 0.8, 0, Math.PI / 2); }
  // ---- each frame (airport mode): the entrance doors slide open, the balcony door swings out, the desk's flap lifts,
  //      as you come to them ----
  let eK = 0, bK = 0, fK = 0; const tA = new Vector3(-Math.sin(T.doorA), 0, Math.cos(T.doorA)); const rq = new Quaternion();
  const ease = (k: number) => k * k * (3 - 2 * k);
  const step = (px: number, py: number, pz: number, dt: number) => {
    const dx = px - x, dz = pz - z, lx = dx * Math.cos(face) - dz * Math.sin(face), lz = dx * Math.sin(face) + dz * Math.cos(face);
    const toward = (k: number, open: boolean, sp: number) => Math.max(0, Math.min(1, k + (open ? dt : -dt) * sp));
    eK = toward(eK, py < 2.5 && Math.abs(lx - EX0) < 3.2 && Math.abs(lz) < 2.6, 1.6);
    const dcx = Math.cos(T.doorA) * T.cabR0, dcz = Math.sin(T.doorA) * T.cabR0, atCab = py > T.cabY - 1.2 && py < T.cabY + 2.5;
    bK = toward(bK, atCab && Math.hypot(lx - dcx, lz - dcz) < 2.3, 1.2);
    fK = toward(fK, atCab && Math.hypot(lx - Math.cos(T.doorA) * 5.0, lz - Math.sin(T.doorA) * 5.0) < 2.1, 1.4);
    eDoors.forEach((n, i) => { n.position.z = (i ? 1 : -1) * (0.4 + ease(eK) * 0.78); });
    bDoor.rotation.y = -1.55 * ease(bK);
    Quaternion.RotationAxisToRef(tA, -1.45 * ease(fK), rq); hinge.rotationQuaternion!.copyFrom(rq);
  };
  return { root, chairs, glass, cutaway, lift, moving, soft, shells, step };
}

function signText(t: string): HTMLCanvasElement { const c = document.createElement('canvas'); c.width = 1024; c.height = 144; const x = c.getContext('2d')!; x.fillStyle = '#14202E'; x.fillRect(0, 0, 1024, 144); x.fillStyle = '#F2F4F7'; x.font = '700 64px Arial, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText(t, 512, 76); return c; }
/** An ATC display: a dark radar/ground picture with runways, traffic labels and a strip bay. */
function screenPic(): HTMLCanvasElement {
  const c = document.createElement('canvas'); c.width = 512; c.height = 320; const x = c.getContext('2d')!;
  x.fillStyle = '#071018'; x.fillRect(0, 0, 512, 320); x.strokeStyle = 'rgba(80,200,255,.25)'; x.lineWidth = 1; for (let r = 30; r < 300; r += 40) { x.beginPath(); x.arc(170, 160, r, 0, Math.PI * 2); x.stroke(); }
  x.strokeStyle = '#7A8896'; x.lineWidth = 6; x.beginPath(); x.moveTo(60, 230); x.lineTo(290, 90); x.stroke(); x.beginPath(); x.moveTo(80, 260); x.lineTo(310, 120); x.stroke();
  x.fillStyle = '#3FD3E6'; x.font = '600 13px monospace'; for (const [px, py, t] of [[220, 70, 'AMP412 FL120'], [110, 200, 'ICE33 A030'], [300, 210, 'NOZ7 FL240'], [60, 90, 'AMP77 G']] as [number, number, string][]) { x.fillRect(px - 3, py - 3, 6, 6); x.fillText(t, px + 8, py + 4); }
  x.fillStyle = '#0E1C28'; x.fillRect(340, 0, 172, 320); x.fillStyle = '#E6EDF3'; x.font = '600 12px monospace';
  for (let i = 0; i < 10; i++) { x.fillStyle = i % 3 === 0 ? '#2B6C3A' : '#1C3550'; x.fillRect(346, 8 + i * 31, 160, 26); x.fillStyle = '#E6EDF3'; x.fillText(['AMP412 DEP 07', 'ICE33 ARR 25', 'AMP77 TAXI B', 'NOZ7 OVF', 'AMP15 PUSH S3', 'SAS21 ARR 07', 'AMP90 LINE UP', 'DLH8 HOLD', 'AMP4 CLR', 'OPS7 RWY INSP'][i], 352, 25 + i * 31); }
  return c;
}
