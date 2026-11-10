// The lifts (E-01, E-G1–4): each in its own shaft through both floors, clad outside, with a landing door on each
// floor and the car riding up and down inside it. The shaft is a little wider than the hole cut for it in Floor 2's
// slab and in the ceilings, so its walls cover the slab's edge: you never see into the gap between the floors, and the
// car never passes through anything. The car: a floor, brushed steel walls with a mirror at the back, a handrail, a
// lit ceiling, the button panel with the floor shown, and its own doors that open with the landing doors.
// In airport mode they work as lifts do: come up to the doors and the lift is called to your floor; it opens; step in
// and after a moment the doors close and it takes you to the other floor (gently, speeding up and slowing down), then
// opens again. Tap, click, E or a controller's Cross while in front of it or inside does the same at once.
import { TransformNode, Vector3, MeshBuilder, AbstractMesh, DynamicTexture } from '@babylonjs/core';
import type { Kit } from './kit';
import { LIFTS, F2Y, F2CEIL } from './plan';

export const LIFT_HOLE = 1.02;               // half the size of the hole in the slab and the ceilings round each lift
const OUT = 1.06, T = 0.07;                     // the terminal's shafts: outer half width, wall thickness
const DOOR = 0.5, DH = 2.2, CAR = 0.84, CH = 2.4, SPEED = 1.1;
export interface LiftCtl { id: string; x: number; z: number; face: number; node: TransformNode; car: TransformNode; y: number; to: number; k: number; hold: number; inT: number; moving: boolean; v: number;
  land: { a: AbstractMesh; b: AbstractMesh }[]; carDoor: { a: AbstractMesh; b: AbstractMesh }; display: DynamicTexture; shown: string; moving_parts: AbstractMesh[];
  /** its two stops (floor levels), sizes and speed, and what the stops are called */
  lo: number; hi: number; out: number; cw: number; dw: number; speed: number; names: [string, string] }
export interface LiftSpec { id: string; room: string; x: number; z: number; face: number; lo: number; hi: number; top: number; out?: number; car?: number; door?: number; speed?: number; names?: [string, string]; sign?: string; holes?: boolean }

export function liftHoles() { return LIFTS.map(l => ({ x0: l.x - LIFT_HOLE, z0: l.z - LIFT_HOLE, x1: l.x + LIFT_HOLE, z1: l.z + LIFT_HOLE })); }

const plainSign = (t: string, bg: string, fg: string, w: number, h: number, font: string) => { const c = document.createElement('canvas'); c.width = w; c.height = h; const x = c.getContext('2d')!; x.fillStyle = bg; x.fillRect(0, 0, w, h); x.fillStyle = fg; x.font = font; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText(t, w / 2, h / 2 + 4); return c; };

export function buildLifts(K: Kit, signC: (t: string, bg: string, fg: string, w: number, h: number, font: string) => HTMLCanvasElement): LiftCtl[] {
  return LIFTS.map(l => buildLift(K, { id: l.id, room: l.from, x: l.x, z: l.z, face: l.face, lo: 0, hi: F2Y, top: F2CEIL, holes: true }, signC));
}
/** One lift: its shaft from the lower stop up to the top, a landing door at each stop, the car riding inside. */
export function buildLift(K: Kit, sp: LiftSpec, signC: (t: string, bg: string, fg: string, w: number, h: number, font: string) => HTMLCanvasElement = plainSign): LiftCtl {
  const { sc, M } = K;
  const clad = K.pbr({ key: 'liftClad', col: '#D9DCE0', rough: 0.45, metal: 0.15 }), brushed = K.pbr({ key: 'liftBrushed', col: '#B9C0C8', rough: 0.28, metal: 0.9 });
  const mirror = K.pbr({ key: 'liftMirror', col: '#E8EEF2', rough: 0.02, metal: 1 }), floorM = K.pbr({ key: 'liftFloor', col: '#3A3F46', rough: 0.6, metal: 0.1 });
  const doorM = K.pbr({ key: 'liftDoor', col: '#C8CED6', rough: 0.22, metal: 0.95 }), trim = M.chrome, dark = M.black;
  const O_ = sp.out ?? OUT, C_ = sp.car ?? CAR, D_ = sp.door ?? DOOR, lo = sp.lo, hi = sp.hi, top = sp.top, H = top - lo;
  {
    const l = sp; const o = K.obj(l.room, 'LIFT', l.x, 0, l.z, l.face, l.id); o.metadata.kind = 'lift'; o.metadata.act = 'lift';
    const parts: AbstractMesh[] = [];
    // the shaft: the back and the sides from the lower stop up to the top, over the edge of the holes
    K.box(o, O_ * 2, H, T, clad, 0, lo + H / 2, -O_ + T / 2); K.box(o, T, H, O_ * 2, clad, -O_ + T / 2, lo + H / 2, 0); K.box(o, T, H, O_ * 2, clad, O_ - T / 2, lo + H / 2, 0);
    // its front: panels either side of the doors all the way up, and over the doors up to the next stop
    for (const sx of [-1, 1]) K.box(o, O_ - D_, H, T, clad, sx * (D_ + (O_ - D_) / 2), lo + H / 2, O_ - T / 2);
    for (const fy of [lo, hi]) { const up = fy === lo ? hi : top; K.box(o, D_ * 2, up - fy - DH, T, clad, 0, fy + DH + (up - fy - DH) / 2, O_ - T / 2); }
    // the cap over the shaft
    K.box(o, O_ * 2, 0.1, O_ * 2, clad, 0, top - 0.05, 0);
    // at each stop: the door frame, the landing doors (two leaves sliding apart behind the front panels), the call button and the sign
    const land: LiftCtl['land'] = [];
    for (const fy of [lo, hi]) {
      K.box(o, 0.05, DH + 0.05, 0.1, trim, -D_ - 0.02, fy + DH / 2, O_ + 0.01); K.box(o, 0.05, DH + 0.05, 0.1, trim, D_ + 0.02, fy + DH / 2, O_ + 0.01); K.box(o, D_ * 2 + 0.09, 0.05, 0.1, trim, 0, fy + DH + 0.02, O_ + 0.01);
      const a = K.box(o, D_, DH, 0.03, doorM, -D_ / 2, fy + DH / 2, O_ - T - 0.02), b = K.box(o, D_, DH, 0.03, doorM, D_ / 2, fy + DH / 2, O_ - T - 0.02);
      a.metadata = b.metadata = o.metadata; land.push({ a, b }); parts.push(a, b);
      K.box(o, 0.09, 0.16, 0.03, dark, D_ + 0.2, fy + 1.15, O_ + 0.02); K.box(o, 0.04, 0.04, 0.02, M.green, D_ + 0.2, fy + 1.18, O_ + 0.035);
      const p = MeshBuilder.CreatePlane(K.nm('lf'), { width: 0.9, height: 0.24 }, sc); p.material = K.picture(signC(sp.sign || `${l.id} · LIFT`, '#14202E', '#F2F4F7', 512, 154, '700 72px Arial, sans-serif'), 1); p.parent = o; p.position.set(0, fy + DH + 0.3, O_ + 0.015); p.rotation.y = Math.PI; p.metadata = o.metadata;
    }
    // the car
    const car = new TransformNode('lift-car-' + l.id, sc); car.parent = o; car.metadata = o.metadata;
    const cp = (m: AbstractMesh) => { parts.push(m); m.metadata = o.metadata; return m; };
    cp(K.box(car, C_ * 2, 0.12, C_ * 2, floorM, 0, -0.06, 0));
    cp(K.box(car, C_ * 2, CH, 0.04, brushed, 0, CH / 2, -C_)); cp(K.box(car, 0.04, CH, C_ * 2, brushed, -C_, CH / 2, 0)); cp(K.box(car, 0.04, CH, C_ * 2, brushed, C_, CH / 2, 0));
    cp(K.box(car, C_ * 2 - 0.2, 1.1, 0.01, mirror, 0, 1.55, -C_ + 0.03));
    cp(K.box(car, C_ * 2 - 0.2, 0.04, 0.04, trim, 0, 0.95, -C_ + 0.07)); for (const sx of [-1, 1]) cp(K.box(car, 0.04, 0.04, C_ * 2 - 0.4, trim, sx * (C_ - 0.07), 0.95, -0.1));
    cp(K.box(car, C_ * 2, 0.08, C_ * 2, brushed, 0, CH + 0.04, 0)); cp(K.box(car, C_ * 1.4, 0.02, C_ * 1.4, M.light, 0, CH - 0.01, 0));
    for (const sx of [-1, 1]) cp(K.box(car, C_ - D_ + 0.04, CH, 0.05, brushed, sx * (D_ + (C_ - D_) / 2), CH / 2, C_));
    cp(K.box(car, D_ * 2, CH - DH, 0.05, brushed, 0, DH + (CH - DH) / 2, C_));
    // the button panel by the door, with the stop shown over it
    cp(K.box(car, 0.02, 0.5, 0.16, dark, C_ - 0.03, 1.15, C_ - 0.25));
    for (const [y, c] of [[1.25, M.green], [1.1, M.green], [0.95, M.redLed]] as [number, any][]) cp(K.box(car, 0.02, 0.05, 0.05, c, C_ - 0.045, y, C_ - 0.25));
    const display = new DynamicTexture('lift-disp', { width: 128, height: 64 }, sc, false); const dm = K.picture(document.createElement('canvas'), 1.4); dm.diffuseTexture = display; dm.emissiveTexture = display;
    const dp = MeshBuilder.CreatePlane(K.nm('ld'), { width: 0.26, height: 0.13 }, sc); dp.material = dm; dp.parent = car; dp.position.set(0, DH + 0.1, C_ - 0.03); parts.push(dp);
    const ca = cp(K.box(car, D_, DH, 0.03, doorM, -D_ / 2, DH / 2, C_ - 0.06)), cb = cp(K.box(car, D_, DH, 0.03, doorM, D_ / 2, DH / 2, C_ - 0.06));
    if (sp.holes) { K.block(1, l.x - O_, l.z - O_, l.x + O_, l.z + O_); K.block(2, l.x - O_, l.z - O_, l.x + O_, l.z + O_); }
    const L: LiftCtl = { id: l.id, x: l.x, z: l.z, face: l.face, node: o, car, y: lo, to: lo, k: 0, hold: 0, inT: 0, moving: false, v: 0, land, carDoor: { a: ca, b: cb }, display, shown: '', moving_parts: parts,
      lo, hi, out: O_, cw: C_, dw: D_, speed: sp.speed ?? SPEED, names: sp.names || ['1', '2'] };
    car.position.y = lo; showFloor(L); return L;
  }
}
const upper = (L: LiftCtl, y: number) => y > (L.lo + L.hi) / 2;
const stopName = (L: LiftCtl, up: boolean) => { const n = L.names[up ? 1 : 0]; return /^\d+$/.test(n) ? 'Floor ' + n : n; };
function showFloor(L: LiftCtl) {
  const up = upper(L, L.y), n = L.names[up ? 1 : 0], f = L.moving ? (L.to > L.y ? '▲' : '▼') + ' ' + n : n; if (f === L.shown) return; L.shown = f;
  const x = L.display.getContext() as CanvasRenderingContext2D; x.fillStyle = '#05070A'; x.fillRect(0, 0, 128, 64); x.fillStyle = '#FF9A2E'; x.font = `700 ${f.length > 4 ? 30 : 44}px Arial, sans-serif`; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText(f, 64, 34); L.display.update();
}
/** The doors: 0 shut … 1 open, the landing doors at the car's stop with the car's own. */
function setDoors(L: LiftCtl) {
  const e = L.k * L.k * (3 - 2 * L.k), f = upper(L, L.y) ? 1 : 0, off = e * (L.dw - 0.04);
  L.land.forEach((d, i) => { const o = i === f && !L.moving ? off : 0; d.a.position.x = -L.dw / 2 - o; d.b.position.x = L.dw / 2 + o; });
  L.carDoor.a.position.x = -L.dw / 2 - off; L.carDoor.b.position.x = L.dw / 2 + off;
}
/** Where a point is in a lift's own frame (x across, z out of its doors). */
function local(L: LiftCtl, px: number, pz: number): [number, number] { const dx = px - L.x, dz = pz - L.z, c = Math.cos(L.face), s = Math.sin(L.face); return [dx * c - dz * s, dx * s + dz * c]; }
export function liftInside(L: LiftCtl, px: number, py: number, pz: number): boolean { const [lx, lz] = local(L, px, pz); return Math.abs(lx) < L.cw - 0.12 && lz > -L.cw + 0.1 && lz < L.cw - 0.05 && Math.abs(py - L.y) < 1.0; }
function waiting(L: LiftCtl, px: number, py: number, pz: number): number { const [lx, lz] = local(L, px, pz); if (Math.abs(lx) > 1.4 || lz < L.out - 0.05 || lz > L.out + 2.4) return -1; if (Math.abs(py - L.lo) > 1.5 && Math.abs(py - L.hi) > 1.5) return -1; return upper(L, py) ? 1 : 0; }

/** Each frame in airport mode: calls, doors, the ride. Returns how far up (or down) to carry whoever is inside, and a line to show. */
export function stepLifts(list: LiftCtl[], px: number, py: number, pz: number, dt: number, press: boolean): { dy: number; hint: string } {
  let dy = 0, hint = '';
  for (const L of list) {
    const inside = liftInside(L, px, py, pz), at = waiting(L, px, py, pz), here = upper(L, L.y) ? 1 : 0;
    if (L.moving) {
      // the ride: speeding up, cruising, slowing down to stop level with the floor
      const left = L.to - L.y, dist = Math.abs(left), want = Math.sign(left) * Math.min(L.speed, Math.sqrt(2 * 0.9 * dist) + 0.02);
      L.v += Math.max(-0.9 * dt, Math.min(0.9 * dt, want - L.v)); let step = L.v * dt; if (Math.abs(step) >= dist) { step = left; L.moving = false; L.v = 0; L.hold = 6; }
      if (inside) dy = step; L.y += step; L.car.position.y = L.y; showFloor(L);
      if (inside) hint = `${L.id} · ${L.to > L.y ? 'going up to ' : 'going down to '}${stopName(L, upper(L, L.to))}`;
    } else {
      // standing at a stop: open for whoever is there, close after a while, set off once someone is inside
      const someone = inside || at === here;
      if (at >= 0 && at !== here && !inside) { if (L.k > 0) L.k = Math.max(0, L.k - dt * 1.6); else { L.to = at ? L.hi : L.lo; L.moving = true; } hint = `${L.id} · coming to ${stopName(L, !!at)}`; }
      else if (someone) { L.k = Math.min(1, L.k + dt * 1.4); L.hold = Math.max(L.hold, 4); }
      else { L.hold -= dt; if (L.hold <= 0) L.k = Math.max(0, L.k - dt * 1.2); }
      if (inside) { L.inT += dt; hint = `${L.id} · to ${stopName(L, !here)}${L.inT < 1.6 && !press ? ' in a moment' : ''}`;
        if ((L.inT > 1.6 || press) && L.k >= 1) L.inT = Math.max(L.inT, 1.6);
        if (L.inT >= 1.6) { L.k = Math.max(0, L.k - dt * 1.6); if (L.k <= 0) { L.to = here ? L.lo : L.hi; L.moving = true; L.inT = 0; } } }
      else L.inT = 0;
      if (press && at === here && !inside) L.k = Math.min(1, L.k + 0.5);
    }
    setDoors(L);
  }
  return { dy, hint };
}
/** Straight to a stop (the terminal demo walks floor to floor without the ride): the car there with its doors open. */
export function liftAt(L: LiftCtl, floor: number) { L.y = floor === 2 ? L.hi : L.lo; L.to = L.y; L.moving = false; L.car.position.y = L.y; L.k = 1; L.hold = 5; setDoors(L); showFloor(L); }
export { Vector3 };
