// Design sketch of the fire station and its trucks (drawing aid only; the game build is in Unreal C++).
// The airport fire station (ARFF): the owner's references blended into one building. Left to right from the apron:
// the charcoal wing with the station's name (ref 1), the glass stair tower with red behind it and the emblem and
// number on its crown (refs 1, 2), the two-storey crew block in silver panels with the thin dark roof edge (ref 1)
// and its upper windows (ref 2), the apparatus hall in grey board-and-batten (ref 8) with five red sectional doors
// with their window bands (ref 4), the dark number band and a window over each door (refs 3, 8), wall lamps between
// (ref 3), the red-framed glass people door at the end (ref 8), the training tower behind (ref 4). Inside: open-web
// joists, skylights, exhaust drops, hose reels, lane lines (refs 5-7). x along the front, z back from it, y up.
import { Vector3, PointLight, SpotLight, Color3 } from '@babylonjs/core';
import { Kit, canvas, lettering } from './lib';

export const HALL = { x0: 18, x1: 62.5, z1: 28, h: 10.5, bays: 5, bay: 8, door: 5.6, doorH: 5.2 };
export const bayX = (i: number) => HALL.x0 + 4.5 + i * HALL.bay;

export function buildStation(k: Kit, dusk: boolean) {
  const M = {
    board: k.pbr({ tex: 'T_FS_Board', nrm: true, rough: 0.78 }), silver: k.pbr({ tex: 'T_FS_Silver', nrm: true, rough: 0.38, metal: 0.45 }),
    char: k.pbr({ tex: 'T_FS_Charcoal', nrm: true, rough: 0.72 }), red: k.pbr({ tex: 'T_FS_RedDoor', nrm: true, rough: 0.42, metal: 0.2 }),
    apron: k.pbr({ tex: 'T_FS_Apron', nrm: true, rough: 0.82, col: '#D5D8DA' }), tower: k.pbr({ tex: 'T_FS_Tower', nrm: true, rough: 0.85 }), floor: k.pbr({ tex: 'T_FS_HallFloor', nrm: true, rough: 0.22 }),
    dark: k.pbr({ col: '#2B2E32', rough: 0.45, metal: 0.4 }), fascia: k.pbr({ col: '#33363A', rough: 0.4, metal: 0.5 }), white: k.pbr({ col: '#E9E9E6', rough: 0.55 }), frameW: k.pbr({ col: '#F0F0EE', rough: 0.4, metal: 0.2 }),
    glass: k.pbr({ col: '#2A3A48', rough: 0.03, metal: 0.6, alpha: 0.55, two: true }), glassT: k.pbr({ col: '#9FB6C8', rough: 0.02, metal: 0.3, alpha: 0.22, two: true }),
    redWall: k.pbr({ col: '#C3241B', rough: 0.6 }), redFrame: k.pbr({ col: '#D02A1C', rough: 0.35, metal: 0.3 }), galv: k.pbr({ col: '#A3AAB0', rough: 0.35, metal: 0.85 }), steelW: k.pbr({ col: '#E4E6E8', rough: 0.4, metal: 0.4 }),
    asph: k.pbr({ tex: 'T_FS_Asphalt', nrm: true, rough: 0.9, col: '#A9A9A9' }), path: k.pbr({ col: '#C9C6BE', rough: 0.85 }), mulch: k.pbr({ col: '#4A3626', rough: 1 }), grass: k.pbr({ col: '#A8A774', rough: 0.85, two: true }), green: k.pbr({ col: '#6F7F4A', rough: 0.85, two: true }),
    yellow: k.pbr({ col: '#E2B007', rough: 0.55 }), bollard: k.pbr({ col: '#8A8D90', rough: 0.5, metal: 0.3 }), black: k.pbr({ col: '#121314', rough: 0.5 }),
    lamp: k.glow('#FFF3DC', dusk ? 2.2 : 1.2), win: dusk ? k.glow('#FFE2B0', 0.75) : k.pbr({ col: '#32414E', rough: 0.04, metal: 0.65 }),
  };
  const H = HALL, top = H.h;
  // ---- ground: apron in front of the hall, asphalt road and parking, the planted bed with its path
  k.box(M.asph, -60, -0.06, -80, 130, -0.02, 80, 4, 4, null, false);
  k.box(M.apron, H.x0 - 2, -0.04, -42, H.x1 + 8, 0.0, H.z1 + 2, 5, 5, null, false);
  for (let i = 0; i < H.bays; i++) { k.box(M.yellow, bayX(i) - 0.08, 0.0, -42, bayX(i) + 0.08, 0.006, H.z1 - 1, 0, 0, null, false); }
  k.box(M.yellow, H.x0 - 2, 0.0, -1.6, H.x1, 0.006, -1.45, 0, 0, null, false);
  k.box(M.mulch, -18, -0.04, -12, H.x0 - 2, 0.04, 0, 0, 0, null, false);
  k.box(M.path, 0.5, 0.0, -12, 3.5, 0.06, -1.2, 2, 2, null, false);
  k.box(M.path, -18, 0.0, -14.5, H.x0 - 2, 0.06, -12, 2, 2, null, false);
  // grasses in clumps over the bed (ref 1)
  for (let i = 0; i < 260; i++) { const x = -17.5 + ((i * 37) % 345) / 10, z = -11.6 + ((i * 53) % 108) / 10; if (x > 0.2 && x < 3.8) continue; if (x > 15.8) continue;
    const g = k.node(x, 0.04, z); const n = 12 + (i % 5);
    for (let b = 0; b < n; b++) { const a = b * 2.39 + i, lean = 0.25 + (b % 4) * 0.12, h = 0.45 + ((b * 7 + i) % 5) * 0.09;
      k.boxC(b % 3 ? M.grass : M.green, Math.cos(a) * 0.04 + Math.cos(a) * Math.sin(lean) * h / 2, h / 2 * Math.cos(lean), Math.sin(a) * 0.04 + Math.sin(a) * Math.sin(lean) * h / 2, 0.012, h, 0.004, Math.sin(a) * lean, 0, -Math.cos(a) * lean, g, false); } }
  // ---- the charcoal wing (ref 1): long and low, the station's name and number on its end facing the apron
  const cw = { x0: -14, x1: 0, z0: 4, z1: 34, h: 4.7 };
  k.box(M.char, cw.x0, 0, cw.z0, cw.x1, cw.h, cw.z1, 2.4, 2.4);
  k.box(M.fascia, cw.x0 - 0.3, cw.h, cw.z0 - 0.3, cw.x1, cw.h + 0.25, cw.z1 + 0.3);
  for (let z = 8; z < cw.z1 - 2; z += 3.2) k.box(M.win, cw.x0 - 0.02, 1.0, z, cw.x0 + 0.05, 2.5, z + 2.4, 0, 0, null, false);
  k.decal(lettering([{ t: 'Mýrdalssandur Airport', size: 92 }, { t: 'Aircraft Rescue and', size: 92 }, { t: 'Fire Fighting Station', size: 92 }], 1500, 380, '#ECEDEE', 'right'), 7.6, 1.92, cw.x0 + 6.6, 3.35, cw.z0 - 0.02, 0, null, dusk ? 0.55 : 0, true);
  k.decal(lettering([{ t: '1', size: 360, weight: 700 }], 260, 420, '#ECEDEE'), 0.9, 1.45, cw.x0 + 12.0, 3.05, cw.z0 - 0.02, 0, null, dusk ? 0.55 : 0, true);
  // ---- the two-storey crew block in silver panels, with the thin dark roof slab overhanging (ref 1)
  const sb = { x0: 0, x1: H.x0, z1: 30, h: 8.6 };
  k.box(M.silver, sb.x0, 0, 0, sb.x1, sb.h, sb.z1, 2, 3);
  k.box(M.fascia, sb.x0 - 1.4, sb.h, -1.4, sb.x1 + 0.2, sb.h + 0.32, sb.z1 + 0.6);
  // the ribbon of glass under the roof edge (ref 1), the upper floor's windows with their little canopies (ref 2),
  // a tall narrow window on the ground floor, the camera, the wall lamps
  k.box(M.win, 4.5, 6.6, -0.03, 17.5, 8.2, 0.05, 0, 0, null, false);
  for (let x = 4.5; x <= 17.5; x += 1.3) k.box(M.dark, x - 0.04, 6.6, -0.06, x + 0.04, 8.2, 0.06, 0, 0, null, false);
  k.box(M.dark, 4.5, 6.55, -0.06, 17.5, 6.62, 0.06, 0, 0, null, false);
  for (const x of [6, 10, 14]) { k.box(M.win, x, 4.6, -0.03, x + 2.4, 5.9, 0.05, 0, 0, null, false); k.box(M.fascia, x - 0.2, 6.05, -0.6, x + 2.6, 6.12, 0.0); }
  k.box(M.win, 8.4, 0.8, -0.03, 9.3, 3.1, 0.05, 0, 0, null, false); k.box(M.frameW, 8.35, 0.75, -0.06, 9.35, 0.8, 0.06, 0, 0, null, false);
  for (const [x, y] of [[6.2, 3.0], [12.0, 3.0]]) { k.box(M.dark, x - 0.25, y, -0.25, x + 0.25, y + 0.12, 0.0); k.box(M.lamp, x - 0.22, y - 0.01, -0.24, x + 0.22, y, -0.02, 0, 0, null, false); }
  k.cyl(M.white, 5.2, 5.6, -0.15, 0.07, 0.14, 'z');
  // ---- the glass stair tower (refs 1 and 2): mullioned glass all round, the red wall behind it, the stair inside,
  // the entrance door at its foot; its crown above the roofs in charcoal with our emblem and the station's number
  const gt = { x0: -1.5, x1: 3.5, z0: -1.4, z1: 3.6, h: 10.4 };
  k.box(M.redWall, gt.x0 + 0.6, 0, gt.z1 - 0.6, gt.x1 - 0.2, gt.h, gt.z1 - 0.3);
  k.box(M.redWall, gt.x0 + 0.3, 0, gt.z0 + 0.6, gt.x0 + 0.6, gt.h, gt.z1 - 0.3);
  for (let i = 0; i < 18; i++) { const y = 0.3 + i * 0.47, z = gt.z1 - 0.9 - (i % 9) * 0.32; k.box(M.dark, gt.x0 + 1.0, y, z - 0.3, gt.x1 - 0.6, y + 0.06, z); }
  k.box(M.dark, gt.x0 + 0.8, 4.7, gt.z0 + 0.6, gt.x1 - 0.4, 4.85, gt.z1 - 0.6);
  for (const [a0, a1, face] of [[gt.x0, gt.x1, 'z'], [gt.z0, gt.z1, 'x']] as [number, number, string][]) {
    if (face === 'z') { k.box(M.glassT, a0, 0, gt.z0 - 0.02, a1, gt.h - 1.4, gt.z0 + 0.02, 0, 0, null, false);
      for (let x = a0; x <= a1 + 0.01; x += 1.25) k.box(M.black, x - 0.05, 0, gt.z0 - 0.08, x + 0.05, gt.h - 1.4, gt.z0 + 0.08);
      for (let y = 0; y <= gt.h - 1.4; y += 1.8) k.box(M.black, a0, y - 0.04, gt.z0 - 0.08, a1, y + 0.04, gt.z0 + 0.08); }
    else { k.box(M.glassT, gt.x0 - 0.02, 0, a0, gt.x0 + 0.02, gt.h - 1.4, a1, 0, 0, null, false);
      for (let z = a0; z <= a1 + 0.01; z += 1.25) k.box(M.black, gt.x0 - 0.08, 0, z - 0.05, gt.x0 + 0.08, gt.h - 1.4, z + 0.05);
      for (let y = 0; y <= gt.h - 1.4; y += 1.8) k.box(M.black, gt.x0 - 0.08, y - 0.04, a0, gt.x0 + 0.08, y + 0.04, a1); }
  }
  k.box(M.black, gt.x0 - 0.12, 0, gt.z0 - 0.12, gt.x0 + 0.12, gt.h - 1.4, gt.z0 + 0.12);
  k.box(M.char, gt.x0 - 0.1, gt.h - 1.4, gt.z0 - 0.1, gt.x1, gt.h + 0.6, gt.z1, 2.4, 2.4);
  k.box(M.fascia, gt.x0 - 0.2, gt.h + 0.6, gt.z0 - 0.2, gt.x1 + 0.1, gt.h + 0.75, gt.z1);
  // the entrance: a glass door in a black frame, the number over it, the signs beside it
  k.box(M.black, 0.8, 0, gt.z0 - 0.12, 2.1, 2.45, gt.z0 - 0.06, 0, 0, null, false);
  k.box(M.glassT, 0.88, 0.05, gt.z0 - 0.13, 2.02, 2.38, gt.z0 - 0.11, 0, 0, null, false);
  k.box(M.galv, 1.75, 0.95, gt.z0 - 0.2, 1.78, 1.25, gt.z0 - 0.13, 0, 0, null, false);
  k.decal(lettering([{ t: '1120', size: 120, weight: 700 }], 300, 150, '#F2F2F2', 'center'), 0.6, 0.3, 1.45, 2.75, gt.z0 - 0.13, 0, null, dusk ? 0.6 : 0, true);
  // our emblem on the crown (a round badge: a flame over a wing, the airport's code) and the number below it
  { const c = canvas(512, 512), x = c.getContext('2d')!; x.fillStyle = '#F4F4F2'; x.beginPath(); x.arc(256, 256, 240, 0, 7); x.fill(); x.fillStyle = '#B5231A'; x.beginPath(); x.arc(256, 256, 210, 0, 7); x.fill();
    x.fillStyle = '#F4F4F2'; x.beginPath(); x.arc(256, 256, 170, 0, 7); x.fill(); x.fillStyle = '#B5231A';
    x.beginPath(); x.moveTo(256, 120); x.bezierCurveTo(200, 200, 230, 240, 256, 300); x.bezierCurveTo(282, 240, 312, 200, 256, 120); x.fill();
    x.fillStyle = '#1B1D20'; x.beginPath(); x.moveTo(130, 300); x.lineTo(382, 300); x.lineTo(330, 330); x.lineTo(182, 330); x.closePath(); x.fill();
    x.font = '800 50px Arial'; x.textAlign = 'center'; x.fillText('BIAP', 256, 390); x.fillStyle = '#F4F4F2'; x.font = '800 30px Arial'; x.fillText('AIRPORT FIRE RESCUE', 256, 470);
    k.decal(c, 1.4, 1.4, (gt.x0 + gt.x1) / 2 + 0.3, gt.h - 0.15, gt.z0 - 0.12, 0, null, dusk ? 0.9 : 0, true); }
  // ---- the apparatus hall: board-and-batten all round (ref 8), its doors, bands, windows, lamps
  const pierW = H.bay - H.door, D0 = H.doorH, bandY = D0 + 1.0, winY0 = bandY + 0.35, winY1 = winY0 + 2.0;
  // the front, built round its openings: piers between the doors, the bands and windows above
  const xs: [number, number][] = []; for (let i = 0; i < H.bays; i++) xs.push([bayX(i) - H.door / 2, bayX(i) + H.door / 2]);
  const pd = { x0: H.x1 - 3.9, x1: H.x1 - 1.3, h: 3.0 };
  xs.push([pd.x0, pd.x1]);
  let x = H.x0;
  for (const [a, b] of xs) { k.box(M.board, x, 0, -0.25, a, top, 0.25, 1.2, 1.2); x = b; }
  k.box(M.board, x, 0, -0.25, H.x1, top, 0.25, 1.2, 1.2);
  for (let i = 0; i < xs.length; i++) {
    const [a, b] = xs[i], dh = i < H.bays ? D0 : pd.h, by = dh + 1.0, wy0 = by + 0.35, wy1 = i < H.bays ? wy0 + 2.0 : wy0 + 1.6;
    // the dark band with the bay's number (ref 3), the window above (ref 8), the board above that
    k.box(M.dark, a, dh, -0.22, b, by, 0.22);
    if (i < H.bays) k.decal(lettering([{ t: String(i + 1), size: 300, weight: 500 }], 220, 320, '#EEF2F6', 'center'), 0.55, 0.8, a + 0.6, (dh + by) / 2, -0.24, 0, null, dusk ? 0.5 : 0, true);
    k.box(M.board, a, by, -0.22, b, wy0, 0.22, 1.2, 1.2);
    k.box(M.win, a + 0.1, wy0, -0.05, b - 0.1, wy1, 0.05, 0, 0, null, false);
    k.box(M.frameW, a, wy0 - 0.08, -0.12, b, wy0, 0.12); k.box(M.frameW, a, wy1, -0.12, b, wy1 + 0.08, 0.12);
    k.box(M.frameW, a, wy0, -0.12, a + 0.1, wy1, 0.12); k.box(M.frameW, b - 0.1, wy0, -0.12, b, wy1, 0.12);
    for (let q = 1; q < 3; q++) { const mx = a + (b - a) * q / 3; k.box(M.frameW, mx - 0.03, wy0, -0.08, mx + 0.03, wy1, 0.08, 0, 0, null, false); }
    k.box(M.board, a, wy1 + 0.08, -0.22, b, top, 0.22, 1.2, 1.2);
    // the door, shut: red sectional panels with the window band (ref 4), its guides and the dark reveal
    if (i < H.bays) {
      k.box(M.red, a + 0.05, 0, -0.12, b - 0.05, dh, -0.04, 1, 2.2);
      k.box(M.glass, a + 0.25, 1.2, -0.13, b - 0.25, 1.65, -0.11, 0, 0, null, false);
      for (let q = 0; q <= 6; q++) { const mx = a + 0.25 + (b - a - 0.5) * q / 6; k.box(M.frameW, mx - 0.03, 1.2, -0.14, mx + 0.03, 1.65, -0.11, 0, 0, null, false); }
      k.box(M.frameW, a + 0.25, 1.17, -0.14, b - 0.25, 1.2, -0.11, 0, 0, null, false); k.box(M.frameW, a + 0.25, 1.65, -0.14, b - 0.25, 1.68, -0.11, 0, 0, null, false);
      for (const e of [a, b - 0.08]) k.box(M.dark, e, 0, -0.2, e + 0.08, dh, 0.0);
      // two grey bollards before each pier (ref 8)
      for (const bx of [a - 0.55, b + 0.55]) { k.cyl(M.bollard, bx, 0.55, -0.9, 0.11, 1.1, 'y', null, 16); k.cyl(M.bollard, bx, 1.12, -0.9, 0.11, 0.06, 'y', null, 16, 0.05); }
    } else {
      // the people door (ref 8): red-framed glass double door
      k.box(M.glassT, a + 0.1, 0, -0.06, b - 0.1, dh, 0.0, 0, 0, null, false);
      for (const e of [a, (a + b) / 2 - 0.06, b - 0.12]) k.box(M.redFrame, e, 0, -0.14, e + 0.12, dh, 0.02);
      k.box(M.redFrame, a, dh - 0.12, -0.14, b, dh, 0.02); k.box(M.redFrame, a, 0, -0.14, b, 0.12, 0.02);
      k.box(M.redFrame, a, 1.0, -0.12, b, 1.06, 0.0); k.box(M.redFrame, a, 2.1, -0.12, b, 2.16, 0.0);
      for (const hx of [(a + b) / 2 - 0.2, (a + b) / 2 + 0.2]) k.box(M.galv, hx - 0.015, 0.9, -0.22, hx + 0.015, 1.4, -0.16, 0, 0, null, false);
    }
    // the wall lamp above (ref 3)
    const lx = (a + b) / 2; k.box(M.dark, lx - 0.3, top - 1.1, -0.55, lx + 0.3, top - 0.85, -0.2); k.box(M.lamp, lx - 0.26, top - 1.11, -0.52, lx + 0.26, top - 1.1, -0.23, 0, 0, null, false);
  }
  k.box(M.fascia, H.x0, top, -0.4, H.x1 + 0.1, top + 0.3, H.z1 + 0.1);
  // the side and back walls in board too, the back with its doors (drive-through), the roof with its skylights
  k.box(M.board, H.x1 - 0.3, 0, 0, H.x1 + 0.2, top, H.z1, 1.2, 1.2);
  k.box(M.board, H.x0, 0, H.z1 - 0.25, H.x1 + 0.2, top, H.z1 + 0.25, 1.2, 1.2);
  for (let i = 0; i < H.bays; i++) k.box(M.red, bayX(i) - H.door / 2, 0, H.z1 + 0.25, bayX(i) + H.door / 2, D0, H.z1 + 0.3, 1, 2.2);
  for (let i = 0; i < 6; i++) k.box(M.win, H.x1 + 0.2, 6.6, 3 + i * 4, H.x1 + 0.25, 8.6, 5.6 + i * 4, 0, 0, null, false);
  k.box(M.dark, H.x0, top - 0.2, 0, H.x1, top, H.z1);
  for (let i = 0; i < H.bays; i++) for (const z of [7, 14, 21]) { k.box(M.glassT, bayX(i) - 1.0, top, z - 0.8, bayX(i) + 1.0, top + 0.45, z + 0.8, 0, 0, null, false); k.box(M.galv, bayX(i) - 1.05, top, z - 0.85, bayX(i) + 1.05, top + 0.12, z + 0.85); }
  for (const [ux, uz] of [[26, 22], [44, 22]]) { k.box(M.steelW, ux, top, uz, ux + 3, top + 1.4, uz + 2); }
  // ---- inside the hall: white walls, floor, open-web joists, the exhaust drops over each bay, hose reels, lights
  k.box(M.floor, H.x0 + 0.25, 0.0, 0.25, H.x1 - 0.3, 0.01, H.z1 - 0.25, 6, 6, null, false);
  k.box(M.white, H.x0 + 0.25, 0, 0.25, H.x0 + 0.5, top - 0.2, H.z1 - 0.25);
  k.box(M.white, H.x0 + 0.25, 0, 0.25, H.x1 - 0.3, top - 0.2, 0.3, 0, 0, null, false);
  k.box(M.white, H.x0 + 0.25, 0, H.z1 - 0.3, H.x1 - 0.3, top - 0.2, H.z1 - 0.25, 0, 0, null, false);
  k.box(M.white, H.x1 - 0.35, 0, 0.25, H.x1 - 0.3, top - 0.2, H.z1 - 0.25, 0, 0, null, false);
  for (let i = 0; i < H.bays; i++) for (const sx of [-1, 1]) k.box(M.yellow, bayX(i) + sx * 1.8 - 0.06, 0.01, 1, bayX(i) + sx * 1.8 + 0.06, 0.016, H.z1 - 1, 0, 0, null, false);
  for (let z = 2; z < H.z1; z += 2.4) {
    k.box(M.steelW, H.x0 + 0.5, top - 1.3, z - 0.05, H.x1 - 0.3, top - 1.2, z + 0.05, 0, 0, null, false);
    k.box(M.steelW, H.x0 + 0.5, top - 0.5, z - 0.05, H.x1 - 0.3, top - 0.4, z + 0.05, 0, 0, null, false);
    for (let x0 = H.x0 + 0.6; x0 < H.x1 - 1; x0 += 1.2) k.boxC(M.steelW, x0 + 0.6, top - 0.85, z, 0.04, 0.95, 0.04, 0, 0, 0.76 * (Math.floor(x0) % 2 ? 1 : -1), null, false);
  }
  for (let z = 4; z < H.z1; z += 9) k.box(M.steelW, H.x0 + 0.5, top - 0.4, z - 0.15, H.x1 - 0.3, top - 0.2, z + 0.15, 0, 0, null, false);
  for (let i = 0; i < H.bays; i++) {
    // the exhaust drop (a striped hose hanging from its rail), the cord reel, the lights over the bay
    const bx = bayX(i);
    k.box(M.dark, bx - 0.08, top - 1.6, 4, bx + 0.08, top - 1.45, H.z1 - 2, 0, 0, null, false);
    for (let q = 0; q < 6; q++) k.cyl(q % 2 ? M.white : M.black, bx, top - 2.2 - q * 0.45, 6.5, 0.16, 0.45, 'y', null, 16);
    k.cyl(M.redFrame, bx + 2.8, 4.2, H.z1 - 0.6, 0.35, 0.25, 'z', null, 20);
    for (const z of [5, 13, 21]) { k.cyl(M.steelW, bx, top - 1.7, z, 0.3, 0.12, 'y', null, 20); k.cyl(M.lamp, bx, top - 1.77, z, 0.27, 0.02, 'y', null, 20); }
  }
  // turnout gear lockers along the back wall, a hose reel on the side wall, the service doors
  for (let x2 = H.x0 + 1; x2 < H.x0 + 14; x2 += 0.75) { k.box(M.redFrame, x2, 0, H.z1 - 1.0, x2 + 0.7, 2.0, H.z1 - 0.35); k.box(M.black, x2 + 0.05, 1.2, H.z1 - 1.01, x2 + 0.65, 1.85, H.z1 - 0.98, 0, 0, null, false); }
  k.cyl(M.redFrame, H.x1 - 0.6, 2.2, 10, 0.5, 0.3, 'x', null, 24); k.box(M.steelW, H.x0 + 0.5, 0, 9, H.x0 + 0.55, 2.2, 10.2, 0, 0, null, false);
  // the hall's lights (indoor, as bright as the terminal's hall)
  const lights: PointLight[] = [];
  for (const i of [0, 2, 4]) for (const z of [7, 18]) { const l = new PointLight('hl', new Vector3(bayX(i), top - 2.2, z), k.sc); l.intensity = 0.9; l.range = 22; l.diffuse = Color3.FromHexString('#FFF6E6'); lights.push(l); }
  // ---- the training tower (ref 4, made richer): a concrete drill shaft on a charcoal plinth, floor bands wrapping it,
  // staggered drill windows with red frames and louvred shutters, two cantilevered balconies with railings, a taller
  // charcoal stair shaft beside it (it echoes the main entrance's stair tower) with a red crown and a glass slot, an
  // overhanging roof with a railing, the rappelling arm, the hose-drying mast and its red beacon
  { const x0 = H.x1 + 2.5, x1 = x0 + 6, z0 = 17, z1 = 23, h = 22, sx0 = x1, sx1 = x1 + 3, sz0 = z0 + 1.2, sz1 = z1 - 0.6, sh = 26;
    k.box(M.char, x0 - 0.4, 0, z0 - 0.4, sx1 + 0.4, 1.4, z1 + 0.4, 2, 2);                       // the plinth
    k.box(M.tower, x0, 1.4, z0, x1, h, z1, 2.5, 2.5);                                           // the drill shaft
    for (let y = 5.4; y < h; y += 4) k.box(M.white, x0 - 0.18, y, z0 - 0.18, x1 + 0.18, y + 0.3, z1 + 0.18);   // floor bands
    // drill windows: dark openings, red frames, louvred shutters folded open beside them, staggered floor to floor
    const win = (cx: number, y: number, face: 'f' | 's') => { const w = 1.3, hh = 1.7;
      if (face === 'f') { k.box(M.black, cx - w / 2, y, z0 - 0.02, cx + w / 2, y + hh, z0 + 0.25, 0, 0, null, false);
        for (const [a2, b2] of [[cx - w / 2 - 0.1, cx - w / 2], [cx + w / 2, cx + w / 2 + 0.1]]) k.box(M.redFrame, a2, y - 0.1, z0 - 0.08, b2, y + hh + 0.1, z0 + 0.02, 0, 0, null, false);
        k.box(M.redFrame, cx - w / 2 - 0.1, y + hh, z0 - 0.08, cx + w / 2 + 0.1, y + hh + 0.1, z0 + 0.02, 0, 0, null, false);
        k.box(M.redFrame, cx - w / 2 - 0.1, y - 0.12, z0 - 0.25, cx + w / 2 + 0.1, y, z0 + 0.02);   // the sill
        const sh2 = cx + w / 2 + 0.12; k.box(M.redWall, sh2, y, z0 - 0.12, sh2 + 0.65, y + hh, z0 - 0.06);
        for (let q = 0; q < 9; q++) k.box(M.dark, sh2, y + 0.1 + q * 0.18, z0 - 0.15, sh2 + 0.65, y + 0.13 + q * 0.18, z0 - 0.12, 0, 0, null, false); }
      else { k.box(M.black, x0 - 0.25, y, cx - w / 2, x0 + 0.02, y + hh, cx + w / 2, 0, 0, null, false);
        for (const [a2, b2] of [[cx - w / 2 - 0.1, cx - w / 2], [cx + w / 2, cx + w / 2 + 0.1]]) k.box(M.redFrame, x0 - 0.02, y - 0.1, a2, x0 + 0.08, y + hh + 0.1, b2, 0, 0, null, false);
        k.box(M.redFrame, x0 - 0.25, y - 0.12, cx - w / 2 - 0.1, x0 + 0.02, y, cx + w / 2 + 0.1); } };
    for (let f = 0; f < 4; f++) { const y = 2.4 + f * 4 + 0.6; win(f % 2 ? x0 + 4.2 : x0 + 1.8, y, 'f'); win(f % 2 ? z0 + 1.8 : z0 + 4.2, y, 's'); }
    // two balconies on the front, cantilevered on steel brackets, with railings (rescues and ladder drills)
    for (const [y, bx0, bx1] of [[9.4, x0 + 0.6, x0 + 3.6], [17.4, x0 + 2.4, x0 + 5.6]]) {
      k.box(M.white, bx0, y, z0 - 1.6, bx1, y + 0.22, z0, 1, 1);
      for (const bx of [bx0 + 0.3, bx1 - 0.3]) k.boxC(M.dark, bx, y - 0.25, z0 - 0.75, 0.12, 0.5, 1.5, 0, 0, 0);
      for (let x2 = bx0; x2 <= bx1 + 0.01; x2 += 0.5) k.cyl(M.galv, x2, y + 0.77, z0 - 1.55, 0.025, 1.1, 'y', null, 8);
      for (const zz of [z0 - 0.6, z0 - 1.1]) { k.cyl(M.galv, bx0, y + 0.77, zz, 0.025, 1.1, 'y', null, 8); k.cyl(M.galv, bx1, y + 0.77, zz, 0.025, 1.1, 'y', null, 8); }
      k.box(M.galv, bx0, y + 1.3, z0 - 1.58, bx1, y + 1.36, z0 - 1.52); k.box(M.galv, bx0 - 0.03, y + 1.3, z0 - 1.58, bx0 + 0.03, y + 1.36, z0); k.box(M.galv, bx1 - 0.03, y + 1.3, z0 - 1.58, bx1 + 0.03, y + 1.36, z0);
      k.box(M.black, bx0 + 0.9, y + 0.22, z0 - 0.02, bx0 + 2.0, y + 2.4, z0 + 0.2, 0, 0, null, false); k.box(M.redFrame, bx0 + 0.82, y + 2.4, z0 - 0.06, bx0 + 2.08, y + 2.5, z0 + 0.02, 0, 0, null, false); }
    // the stair shaft: charcoal, taller, a full-height glass slot on its front, the red crown with the emblem's ring
    k.box(M.char, sx0, 1.4, sz0, sx1, sh, sz1, 2, 2);
    k.box(M.glassT, sx0 + 0.9, 2.0, sz0 - 0.05, sx1 - 0.9, sh - 3.2, sz0 + 0.05, 0, 0, null, false); k.box(M.black, sx0 + 0.9, 2.0, sz0 + 0.06, sx1 - 0.9, sh - 3.2, sz0 + 0.3, 0, 0, null, false);
    for (let y = 2.0; y < sh - 3.2; y += 1.5) k.box(M.dark, sx0 + 0.88, y, sz0 - 0.07, sx1 - 0.88, y + 0.06, sz0 - 0.02, 0, 0, null, false);
    k.box(M.redWall, sx0 - 0.1, sh - 2.6, sz0 - 0.1, sx1 + 0.1, sh, sz1 + 0.1); k.box(M.fascia, sx0 - 0.25, sh, sz0 - 0.25, sx1 + 0.25, sh + 0.25, sz1 + 0.25);
    k.cyl(M.white, (sx0 + sx1) / 2, sh - 1.3, sz0 - 0.12, 0.85, 0.06, 'z', null, 32); k.cyl(M.redFrame, (sx0 + sx1) / 2, sh - 1.3, sz0 - 0.15, 0.62, 0.06, 'z', null, 32);
    // the drill shaft's top: an overhanging roof slab, its dark fascia, the railing all round, the access hatch
    k.box(M.white, x0 - 0.7, h, z0 - 0.7, x1 + 0.2, h + 0.35, z1 + 0.7); k.box(M.fascia, x0 - 0.72, h + 0.35, z0 - 0.72, x1 + 0.22, h + 0.55, z1 + 0.72);
    const rl = (ax: number, az: number, bx: number, bz: number) => { const n = Math.ceil(Math.hypot(bx - ax, bz - az) / 0.8);
      for (let i = 0; i <= n; i++) k.cyl(M.galv, ax + (bx - ax) * i / n, h + 1.1, az + (bz - az) * i / n, 0.025, 1.1, 'y', null, 8);
      k.box(M.galv, Math.min(ax, bx) - 0.03, h + 1.62, Math.min(az, bz) - 0.03, Math.max(ax, bx) + 0.03, h + 1.68, Math.max(az, bz) + 0.03); };
    rl(x0 - 0.6, z0 - 0.6, x1, z0 - 0.6); rl(x0 - 0.6, z0 - 0.6, x0 - 0.6, z1 + 0.6); rl(x0 - 0.6, z1 + 0.6, x1, z1 + 0.6);
    k.box(M.dark, x0 + 3.5, h + 0.55, z0 + 3.5, x0 + 4.5, h + 0.75, z0 + 4.5);
    // the rappelling arm out over the front, its pulley and ropes hanging down
    k.box(M.redFrame, x0 + 0.6, h + 1.9, z0 - 2.6, x0 + 0.9, h + 2.2, z0 + 1.5); k.cyl(M.redFrame, x0 + 0.75, h + 1.0, z0 + 1.4, 0.12, 2.0, 'y', null, 12);
    k.cyl(M.galv, x0 + 0.75, h + 1.75, z0 - 2.5, 0.16, 0.08, 'x', null, 16);
    for (const dx of [-0.05, 0.05]) k.cyl(M.redWall, x0 + 0.75 + dx, h + 1.7 - 6, z0 - 2.5, 0.015, 12, 'y', null, 6);
    // the hose-drying mast on the stair shaft's crown, its red beacon (lit at dusk)
    k.cyl(M.galv, (sx0 + sx1) / 2, sh + 1.75, (sz0 + sz1) / 2, 0.08, 3.0, 'y', null, 12); k.cyl(k.glow('#FF2A18', dusk ? 2.5 : 1.0), (sx0 + sx1) / 2, sh + 3.35, (sz0 + sz1) / 2, 0.14, 0.25, 'y', null, 16);
    // a red stripe up the drill shaft's outer corner (the station's colour)
    k.box(M.redWall, x0 - 0.06, 1.4, z0 - 0.06, x0 + 0.45, h, z0 + 0.0, 0, 0, null, false); k.box(M.redWall, x0 - 0.06, 1.4, z0 - 0.06, x0 + 0.0, h, z0 + 0.45, 0, 0, null, false); }
  // ---- the flagpole (Iceland's flag) and the tall slim light poles (ref 1)
  k.cyl(M.black, -9, 5, -8, 0.07, 10, 'y', null, 16, 0.05); k.cyl(M.galv, -9, 10.05, -8, 0.1, 0.1, 'y', null, 12);
  { const c = canvas(500, 360), x2 = c.getContext('2d')!; x2.fillStyle = '#02529C'; x2.fillRect(0, 0, 500, 360); x2.fillStyle = '#FFFFFF'; x2.fillRect(125, 0, 90, 360); x2.fillRect(0, 135, 500, 90); x2.fillStyle = '#DC1E35'; x2.fillRect(147, 0, 46, 360); x2.fillRect(0, 157, 500, 46);
    const f = k.decal(c, 2.0, 1.44, -7.95, 9.2, -8, 0); f.rotation.y = 0.35; (f.material as any).backFaceCulling = false; }
  for (const [px, pz] of [[12, -16], [H.x1 + 4, -16], [-16, 2]]) { k.cyl(M.dark, px, 5, pz, 0.12, 10, 'y', null, 12, 0.08); k.box(M.dark, px - 0.1, 9.85, pz - 0.8, px + 0.1, 10.0, pz + 0.2); k.box(M.lamp, px - 0.08, 9.84, pz - 0.75, px + 0.08, 9.85, pz + 0.1, 0, 0, null, false); }
  // the bike stand by the entrance
  k.tube(M.black, [new Vector3(3.2, 0, -3), new Vector3(3.2, 0.8, -3), new Vector3(3.2, 0.85, -2.7), new Vector3(3.2, 0.8, -2.4), new Vector3(3.2, 0, -2.4)], 0.03);
  return { M, lights };
}
