// Design sketch of the fire station and its trucks (drawing aid only; the game build is in Unreal C++).
// The airport crash tender (ARFF 6x6), our own design in the style of the owner's references: chartreuse body,
// black chevron stripes, the airport's own name and number, a low cab with a big wrap-round windscreen, the roof
// turret and bumper turret, roller-shutter lockers, six big wheels. Its own object (a node) so it can be swapped.
// Local frame: x across (right +), y up, z along (front at -z), the ground at y 0, the middle of the wheelbase at 0.
import { Vector3, TransformNode, Color3 } from '@babylonjs/core';
import { Kit, canvas } from './lib';

const LIME = '#C6D80F', LIME_D = '#9DB00A';

export function buildTruck(k: Kit, no: number, at: Vector3, ry = 0) {
  const T = k.node(at.x, at.y, at.z, ry); T.name = 'TRUCK_' + no;
  const body = k.pbr({ col: LIME, rough: 0.32, metal: 0.1, clear: 0.6 });
  const bodyD = k.pbr({ col: LIME_D, rough: 0.4, metal: 0.1 });
  const black = k.pbr({ col: '#0E0F10', rough: 0.45 }), satin = k.pbr({ col: '#1B1D20', rough: 0.6 });
  const tyre = k.pbr({ col: '#141414', rough: 0.85 }), chrome = k.pbr({ col: '#D6DADF', rough: 0.12, metal: 1 });
  const alu = k.pbr({ col: '#A9AFB5', rough: 0.3, metal: 0.9 }), shutter = k.pbr({ col: '#C8CDD2', rough: 0.35, metal: 0.8 });
  const glass = k.pbr({ col: '#202A33', rough: 0.03, metal: 0.2, alpha: 0.42, two: true });
  const red = k.glow('#FF2A1A', 1.2), blue = k.glow('#2A6BFF', 1.1), white = k.glow('#FFFBEF', 1.6), amber = k.glow('#FF9A1A', 1.0), tail = k.glow('#D0100A', 0.8);
  const L = -6.4, R = 6.3, HW = 1.5;   // front, rear, half width

  // the chassis and the wheels (front axle and the rear tandem), tyres with deep treads, lime rims
  k.box(black, -0.55, 0.55, L + 0.6, 0.55, 0.95, R - 0.4, 0, 0, T);
  for (const z of [-3.7, 2.0, 3.75]) for (const sx of [-1, 1]) {
    const x = sx * (HW - 0.2);
    k.cyl(tyre, x, 0.7, z, 0.7, 0.62, 'x', T, 36);
    for (let i = 0; i < 18; i++) { const a = i / 18 * Math.PI * 2; k.boxC(tyre, x, 0.7 + Math.sin(a) * 0.69, z + Math.cos(a) * 0.69, 0.6, 0.08, 0.16, -a, 0, 0, T); }
    k.cyl(bodyD, x + sx * 0.3, 0.7, z, 0.4, 0.04, 'x', T, 24);
    k.cyl(satin, x + sx * 0.32, 0.7, z, 0.14, 0.04, 'x', T, 12);
    for (let i = 0; i < 8; i++) { const a = i / 8 * Math.PI * 2; k.cyl(chrome, x + sx * 0.33, 0.7 + Math.sin(a) * 0.25, z + Math.cos(a) * 0.25, 0.025, 0.03, 'x', T, 8); }
  }
  // the body: lockers low between the wheels, the water/foam tank above, sloping down to the rear
  k.box(body, -HW, 1.0, -2.25, HW, 2.75, R, 0, 0, T);
  k.box(body, -HW + 0.08, 2.75, -2.2, HW - 0.08, 3.35, R - 0.6, 0, 0, T);
  k.boxC(body, 0, 3.12, R - 0.35, 2.84, 0.5, 0.75, -0.6, 0, 0, T);
  for (const sx of [-1, 1]) {
    // wheel arches (black), roller-shutter lockers, the shutters' handles, step treads, the marker light strips
    for (const [z0, z1] of [[-4.55, -2.85], [1.15, 4.6]]) k.box(black, sx * HW - 0.02, 0.95, z0, sx * (HW + 0.03), 1.55, z1, 0, 0, T);
    for (const [z0, z1] of [[-2.1, -0.3], [-0.15, 1.05], [4.75, 6.15]]) {
      k.box(shutter, sx * (HW + 0.005), 1.15, z0, sx * (HW + 0.02), 2.55, z1, 0, 0, T);
      for (let y = 1.2; y < 2.55; y += 0.07) k.box(satin, sx * (HW + 0.02), y, z0, sx * (HW + 0.025), y + 0.012, z1, 0, 0, T, false);
      k.box(black, sx * (HW + 0.02), 1.17, (z0 + z1) / 2 - 0.2, sx * (HW + 0.05), 1.22, (z0 + z1) / 2 + 0.2, 0, 0, T, false);
    }
    for (const z of [-1.2, 5.4]) k.box(alu, sx * (HW - 0.05), 0.85, z - 0.35, sx * (HW + 0.12), 0.9, z + 0.35, 0, 0, T);
    k.box(amber, sx * (HW + 0.03), 2.6, -1.6, sx * (HW + 0.035), 2.66, -1.3, 0, 0, T, false);
    k.box(amber, sx * (HW + 0.03), 2.6, 5.6, sx * (HW + 0.035), 2.66, 5.9, 0, 0, T, false);
    // the vertical LED warning strips at the body's corners
    for (const z of [-2.2, R - 0.05]) k.box(red, sx * (HW + 0.01), 1.9, z - 0.04, sx * (HW + 0.03), 2.7, z + 0.04, 0, 0, T, false);
    // the livery: the airport's name, the number, the double black chevrons and stripe, our emblem
    const c = canvas(2048, 512), x = c.getContext('2d')!;
    x.fillStyle = '#0E0F10';
    const band = (y: number, h: number) => { x.beginPath(); x.moveTo(0, y); x.lineTo(1500, y); x.lineTo(1580, y - 70); x.lineTo(2048, y - 70); x.lineTo(2048, y - 70 + h); x.lineTo(1580 + h * 0.4, y - 70 + h); x.lineTo(1500 + h * 0.4, y + h); x.lineTo(0, y + h); x.closePath(); x.fill(); };
    band(380, 22); band(418, 12);
    for (let i = 0; i < 4; i++) { x.beginPath(); const x0 = sx > 0 ? 1700 + i * 70 : 1700 + i * 70; x.moveTo(x0, 470); x.lineTo(x0 + 38, 470); x.lineTo(x0 + 98, 300); x.lineTo(x0 + 60, 300); x.closePath(); x.fill(); }
    x.font = '800 120px Arial'; x.textBaseline = 'alphabetic'; x.fillText('AIRPORT', 520, 210); x.fillText('FIRE RESCUE', 520, 340);
    x.font = '900 230px Arial'; x.fillText(String(no), 1820, 250);
    x.strokeStyle = '#0E0F10'; x.lineWidth = 12; x.beginPath(); x.arc(330, 230, 120, 0, 7); x.stroke();
    x.font = '800 44px Arial'; x.textAlign = 'center'; x.fillText('BIAP', 330, 190); x.font = '800 30px Arial'; x.fillText('ARFF', 330, 300);
    x.beginPath(); x.moveTo(330, 205); x.quadraticCurveTo(300, 235, 330, 270); x.quadraticCurveTo(360, 235, 330, 205); x.fill();
    x.font = '700 40px Arial'; x.textAlign = 'left'; x.fillText('MÝRDALSSANDUR', 520, 80);
    const w = R + 2.25;
    k.decal(c, w, w / 4, sx * (HW + 0.03), 2.25, (R - 2.25) / 2, sx > 0 ? -Math.PI / 2 : Math.PI / 2, T, 0, true);
    if (sx < 0) { const m = (k as any).root; void m; }
  }
  // the rear: hazard stripes, tail and brake lights, the ladder up to the roof
  { const c = canvas(512, 512), x = c.getContext('2d')!; x.fillStyle = LIME; x.fillRect(0, 0, 512, 512); x.fillStyle = '#C21F17';
    for (let i = -8; i < 8; i++) { x.beginPath(); x.moveTo(i * 64, 512); x.lineTo(i * 64 + 32, 512); x.lineTo(i * 64 + 32 + 512, 0); x.lineTo(i * 64 + 512, 0); x.closePath(); x.fill(); }
    k.decal(c, 2.6, 1.3, 0, 1.85, R + 0.02, Math.PI, T); }
  for (const sx of [-1, 1]) { k.box(tail, sx * 1.2 - 0.12, 1.15, R, sx * 1.2 + 0.12, 1.45, R + 0.04, 0, 0, T, false); k.box(amber, sx * 1.2 - 0.12, 1.5, R, sx * 1.2 + 0.12, 1.6, R + 0.04, 0, 0, T, false); }
  for (const sx of [-1, 1]) k.box(chrome, sx * 0.35 - 0.02, 1.0, R + 0.08, sx * 0.35 + 0.02, 3.4, R + 0.12, 0, 0, T);
  for (let y = 1.2; y < 3.3; y += 0.3) k.box(chrome, -0.35, y, R + 0.08, 0.35, y + 0.03, R + 0.12, 0, 0, T);
  // the roof: rails, the hose reel, the folded roof turret with its nozzle, the light bars
  for (const sx of [-1, 1]) { for (let z = -1.8; z <= R - 0.9; z += 1.6) k.cyl(chrome, sx * 1.25, 3.55, z, 0.025, 0.4, 'y', T, 8); k.tube(chrome, [new Vector3(sx * 1.25, 3.75, -1.8), new Vector3(sx * 1.25, 3.75, R - 0.9)], 0.03, T); }
  k.box(alu, -0.8, 3.35, 0.2, 0.8, 3.45, 4.5, 0, 0, T);
  { const t = k.node(0, 3.4, -2.6, 0, T); k.cyl(satin, 0, 0.12, 0, 0.32, 0.25, 'y', t, 24); k.boxC(body, 0, 0.42, 0.9, 0.34, 0.26, 2.0, 0.08, 0, 0, t); k.cyl(black, 0, 0.36, -0.2, 0.09, 0.6, 'z', t, 16); k.cyl(chrome, 0, 0.36, -0.55, 0.12, 0.18, 'z', t, 20, 0.07); }
  // the cab: low and forward, the wrap-round windscreen sloping back, black pillars, the doors' seams
  const cz0 = L, cz1 = -2.25;
  k.box(body, -HW, 1.0, cz0 + 0.55, HW, 1.75, cz1, 0, 0, T);
  k.box(black, -HW + 0.05, 1.75, cz0 + 0.9, HW - 0.05, 3.25, cz1, 0, 0, T);
  k.boxC(glass, 0, 2.55, cz0 + 0.62, 2.84, 1.55, 0.04, -0.28, 0, 0, T);
  for (const sx of [-1, 1]) {
    k.box(glass, sx * (HW - 0.04) - 0.02, 1.85, cz0 + 1.2, sx * (HW - 0.04) + 0.02, 3.15, cz1 - 0.15, 0, 0, T);
    k.boxC(black, sx * (HW - 0.05), 2.55, cz0 + 0.65, 0.12, 1.6, 0.14, -0.28, 0, 0, T);
    k.box(satin, sx * HW - 0.01, 1.05, cz0 + 1.6, sx * HW + 0.01, 3.2, cz0 + 1.65, 0, 0, T, false);    // the door's seam
    k.box(satin, sx * HW - 0.01, 1.05, cz1 - 0.55, sx * HW + 0.01, 3.2, cz1 - 0.5, 0, 0, T, false);
    k.box(chrome, sx * (HW + 0.01), 1.6, cz1 - 0.9, sx * (HW + 0.04), 1.63, cz1 - 0.7, 0, 0, T);          // its handle
    k.tube(black, [new Vector3(sx * HW, 2.9, cz0 + 0.9), new Vector3(sx * (HW + 0.45), 3.05, cz0 + 0.7)], 0.025, T);   // the mirror arm
    k.boxC(black, sx * (HW + 0.5), 2.85, cz0 + 0.7, 0.08, 0.42, 0.24, 0, 0, 0, T);
    k.box(blue, sx * (HW - 0.06), 2.0, cz1 - 0.05, sx * (HW + 0.0), 2.9, cz1 + 0.02, 0, 0, T, false);   // blue strip behind the door
  }
  k.box(black, -HW, 3.25, cz0 + 0.7, HW, 3.42, cz1 + 0.1, 0, 0, T);
  // the light bar on the cab's roof (red and white), the beacons on the body's back corners
  for (let i = 0; i < 10; i++) k.box(i % 2 ? white : red, -1.1 + i * 0.22, 3.42, cz0 + 0.95, -0.92 + i * 0.22, 3.55, cz0 + 1.15, 0, 0, T, false);
  for (const sx of [-1, 1]) k.cyl(amber, sx * 1.3, 3.4, R - 0.3, 0.07, 0.12, 'y', T, 12);
  // the front: black bumper with the turret, LED headlights and the low beams, the lime front plate, the step
  k.box(black, -HW, 0.65, cz0 + 0.2, HW, 1.0, cz0 + 0.65, 0, 0, T);
  k.box(body, -HW + 0.25, 1.0, cz0 + 0.45, HW - 0.25, 1.75, cz0 + 0.6, 0, 0, T);
  for (let i = 0; i < 6; i++) k.box(satin, -0.9 + i * 0.36, 1.1, cz0 + 0.43, -0.75 + i * 0.36, 1.6, cz0 + 0.46, 0, 0, T, false);
  for (const sx of [-1, 1]) { k.box(white, sx * 1.15 - 0.22, 1.25, cz0 + 0.44, sx * 1.15 + 0.22, 1.36, cz0 + 0.46, 0, 0, T, false); k.box(white, sx * 1.15 - 0.15, 1.42, cz0 + 0.44, sx * 1.15 + 0.15, 1.48, cz0 + 0.46, 0, 0, T, false); k.box(amber, sx * 1.42 - 0.04, 1.25, cz0 + 0.5, sx * 1.42 + 0.04, 1.5, cz0 + 0.52, 0, 0, T, false); }
  { const b = k.node(0, 0.85, cz0 + 0.1, 0, T); k.cyl(satin, 0, 0.0, 0, 0.12, 0.2, 'y', b, 16); k.cyl(black, 0, 0.12, -0.25, 0.06, 0.5, 'z', b, 12); k.cyl(chrome, 0, 0.12, -0.55, 0.08, 0.12, 'z', b, 16, 0.05); }
  // ---- the cab inside: the driver's seat in the middle, two seats beside, the crew's seats behind with their breathing
  // sets, the dashboard round the driver with screens and switch panels, the steering wheel, the turret's joystick
  const I = k.node(0, 0, 0, 0, T);
  const seat = k.pbr({ col: '#22252A', rough: 0.7 }), dash = k.pbr({ col: '#1A1C1F', rough: 0.55 }), floor = k.pbr({ col: '#2B2D30', rough: 0.9 }), trim = k.pbr({ col: '#5A5F66', rough: 0.5 });
  const screen = k.glow('#2A6F8C', 0.9), screen2 = k.glow('#3E8F4A', 0.7), btnR = k.glow('#FF3020', 0.9), btnG = k.glow('#3CFF60', 0.8), btnA = k.glow('#FFB020', 0.8);
  k.box(floor, -HW + 0.08, 1.75, cz0 + 0.9, HW - 0.08, 1.8, cz1 - 0.05, 0, 0, I, false);
  k.box(trim, -HW + 0.08, 3.18, cz0 + 1.0, HW - 0.08, 3.22, cz1 - 0.05, 0, 0, I, false);
  // the dashboard: a curved console stepped round the driver, its screens and switch panels
  k.boxC(dash, 0, 2.15, cz0 + 1.25, 2.7, 0.35, 0.55, 0.2, 0, 0, I, false);
  for (const [x, w, mat] of [[-0.45, 0.5, screen], [0.0, 0.32, screen2], [0.45, 0.5, screen]] as [number, number, any][]) k.boxC(mat, x, 2.38, cz0 + 1.38, w, 0.28, 0.02, -0.5, 0, 0, I, false);
  for (const sx of [-1, 1]) for (let i = 0; i < 8; i++) k.boxC(i % 3 === 0 ? btnR : i % 3 === 1 ? btnG : btnA, sx * (0.85 + (i % 4) * 0.09), 2.33, cz0 + 1.3 + Math.floor(i / 4) * 0.08, 0.04, 0.02, 0.04, 0, 0, 0, I, false);
  // the steering wheel on its column, the instrument cluster, the pedals
  { const sw = k.node(0, 2.25, cz0 + 1.7, 0, I); sw.rotation.x = -0.9; const r = k.tube(seat, Array.from({ length: 33 }, (_, i) => { const a = i / 32 * Math.PI * 2; return new Vector3(Math.cos(a) * 0.22, Math.sin(a) * 0.22, 0); }), 0.022, sw); void r;
    k.boxC(seat, 0, 0, 0, 0.42, 0.04, 0.03, 0, 0, 0, sw); k.cyl(trim, 0, 0, 0.12, 0.05, 0.25, 'z', sw); }
  for (const x of [-0.12, 0.1, 0.22]) k.boxC(trim, x, 1.9, cz0 + 1.15, 0.09, 0.03, 0.2, -0.6, 0, 0, I, false);
  // the seats: the driver's in the middle, one each side, three for the crew behind (with their breathing sets)
  const chair = (x: number, z: number, back = true) => { k.box(seat, x - 0.27, 1.8, z - 0.26, x + 0.27, 2.25, z + 0.26, 0, 0, I, false); k.boxC(seat, x, 2.65, z + 0.27, 0.52, 0.85, 0.12, 0.12, 0, 0, I, false); k.boxC(seat, x, 3.08, z + 0.32, 0.3, 0.18, 0.1, 0.12, 0, 0, I, false); if (back) { k.cyl(k.pbr({ col: '#B8BEC4', rough: 0.3, metal: 0.8 }), x, 2.6, z + 0.42, 0.09, 0.55, 'y', I); } };
  chair(0, cz0 + 2.25, false); chair(-1.0, cz0 + 2.45, false); chair(1.0, cz0 + 2.45, false);
  for (const x of [-0.9, 0, 0.9]) chair(x, cz1 - 0.6);
  // the turret's joystick on the right seat's arm, the grab handles, the radios overhead
  k.cyl(black, 0.72, 2.35, cz0 + 2.1, 0.03, 0.18, 'y', I); k.cyl(btnR, 0.72, 2.46, cz0 + 2.1, 0.02, 0.03, 'y', I);
  for (const sx of [-1, 1]) k.tube(chrome, [new Vector3(sx * 1.35, 2.2, cz0 + 2.0), new Vector3(sx * 1.35, 2.9, cz0 + 2.0)], 0.018, I);
  k.box(dash, -0.6, 3.0, cz0 + 1.6, 0.6, 3.18, cz0 + 2.0, 0, 0, I, false); for (let i = 0; i < 3; i++) k.box(screen2, -0.45 + i * 0.32, 3.04, cz0 + 1.59, -0.25 + i * 0.32, 3.1, cz0 + 1.6, 0, 0, I, false);
  return T;
}
