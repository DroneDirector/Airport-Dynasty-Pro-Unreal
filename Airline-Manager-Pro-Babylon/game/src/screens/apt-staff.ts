// Your airport's own staff and their days (owner, 2026-11-19, to-do 116 part 1; docs/Staff-Daily-Life-Plan.md): every
// person who works at your airport - gate agents, check-in, cleaners, security, fire crew, catering, fuellers, apron
// bus drivers, the control tower and the operations centre - with a home in Vík (the town by Mýrdalssandur), a way to
// work (own car, or the bus), a shift pattern and days off. How many each shift needs is worked out from the day's real
// schedule at the airport (your flights and the other airlines'), by the real-world numbers per flight; the rota puts
// people on the shifts; the Staff app (tablet and computers) shows each shift's need against who is on, who is coming
// in, and hires to cover a gap. The ramp crew turning aircraft round are your airline's ground crew (core/ground.ts).
// Parts after this one bring them to life in 3D: leaving home, driving or riding the bus, clocking in, the duties.

interface AptStaff { id: string; name: string; sex: 'm' | 'f'; job: string; home: string; commute: 'car' | 'bus'; shift: 'early' | 'late' | 'night' | 'day'; off: number; hired: string; salary: number; sickTill?: string; holTill?: string }
// the jobs: per-turn need by aircraft size (narrow, wide, A380) or a fixed floor per shift; monthly salary (EUR)
const APT_JOBS: { id: string; name: string; per?: [number, number, number]; arrPer?: [number, number, number]; floor: number; pay: number; office?: boolean }[] = [
  { id: 'gate', name: 'Gate agents', per: [2, 4, 6], arrPer: [1, 2, 2], floor: 2, pay: 2900 },
  { id: 'checkin', name: 'Check-in agents', per: [2, 3, 4], floor: 3, pay: 2700 },
  { id: 'sec', name: 'Security screeners', per: [1, 2, 3], floor: 4, pay: 3100 },
  { id: 'clean', name: 'Cleaners', per: [5, 10, 18], floor: 2, pay: 2400 },
  { id: 'cater', name: 'Catering', per: [2, 3, 5], floor: 1, pay: 2600 },
  { id: 'fuel', name: 'Fuellers', per: [1, 1, 2], floor: 1, pay: 3200 },
  { id: 'apbus', name: 'Apron bus drivers', floor: 2, pay: 2800 },
  { id: 'fire', name: 'Fire crew', floor: 8, pay: 3600 },
  { id: 'tower', name: 'Control tower', floor: 3, pay: 5200 },
  { id: 'ops', name: 'Operations centre', floor: 3, pay: 4200, office: true },
];
const APT_SHIFTS: { id: 'early' | 'late' | 'night'; name: string; from: number; to: number }[] = [
  { id: 'early', name: 'Early 06-14', from: 6, to: 14 }, { id: 'late', name: 'Late 14-22', from: 14, to: 22 }, { id: 'night', name: 'Night 22-06', from: 22, to: 30 },
];
const VIK_STREETS = ['Víkurbraut', 'Austurvegur', 'Sigtún', 'Mánabraut', 'Ránarbraut', 'Suðurvíkurvegur', 'Bakkabraut', 'Hátún', 'Smiðjuvegur', 'Kirkjuvegur'];

function aptStaffState(): any { const S: any = State; S.aptStaff = S.aptStaff || []; return S; }
function aptHome(): string { return 'BIAP'; }   // (the airport the 3D game is: Mýrdalssandur - Data/airfield.json)

/** The size of an aircraft on the board: 0 narrow-body, 1 wide-body, 2 the A380. */
function aptAcSize(ac: string): number { const a = String(ac || '').toUpperCase(); return /A38|380/.test(a) ? 2 : /77|78|35|33|34|74|76|A30|IL96|WIDE/.test(a) ? 1 : 0; }
/** The day's movements at the airport (scheduled time, ms) with their sizes. */
function aptMovements(day0: number): { dep: { t: number; s: number }[]; arr: { t: number; s: number }[] } {
  const base = aptHome(); let dep: any[] = [], arr: any[] = [];
  try { dep = fidsRows('dep', base, day0, 800); arr = fidsRows('arr', base, day0, 800); } catch (e) { /* no board yet */ }
  const inDay = (t: number) => t >= day0 - 3 * 3600e3 && t < day0 + 30 * 3600e3;
  return { dep: dep.filter(r => inDay(r.sched)).map(r => ({ t: r.sched, s: aptAcSize(r.ac) })), arr: arr.filter(r => inDay(r.sched)).map(r => ({ t: r.sched, s: aptAcSize(r.ac) })) };
}
/** How many of each job a shift needs: the busiest quarter hour of the shift, by the aircraft on the ground then. */
function aptNeed(day0: number): { [shift: string]: { [job: string]: number } } {
  const M = aptMovements(day0), out: any = {};
  const turn = [45, 90, 120];   // (minutes on the ground for a turn: narrow, wide, A380)
  for (const sh of APT_SHIFTS) {
    const need: any = {}; APT_JOBS.forEach(j => need[j.id] = j.floor);
    let largest = 0;
    for (let m = sh.from * 60; m < sh.to * 60; m += 15)
    {
      const t = day0 + m * 60e3; const per: any = {}; APT_JOBS.forEach(j => per[j.id] = 0);
      for (const d of M.dep) if (t >= d.t - turn[d.s] * 60e3 && t < d.t) { largest = Math.max(largest, d.s); APT_JOBS.forEach(j => { if (j.per) per[j.id] += j.per[d.s]; }); }
      for (const a of M.arr) if (t >= a.t && t < a.t + 20 * 60e3) { largest = Math.max(largest, a.s); APT_JOBS.forEach(j => { if (j.arrPer) per[j.id] += j.arrPer[a.s]; }); }
      APT_JOBS.forEach(j => { need[j.id] = Math.max(need[j.id], per[j.id]); });
    }
    // fire crew by the largest aircraft (the airport's fire category): 8 for narrow-bodies, 12 for wide-bodies, 15 for the A380
    need.fire = Math.max(need.fire, [8, 12, 15][largest]);
    if (sh.id === 'night') need.ops = Math.max(2, need.ops - 1);
    out[sh.id] = need;
  }
  return out;
}
/** Who works when: each person keeps their shift, 5 days on and 2 off (their own 'off' offset); sick or on holiday: off. */
function aptOnDuty(p: AptStaff, day0: number): boolean {
  const d = Math.floor(day0 / 864e5), ds = new Date(day0).toISOString().slice(0, 10);
  if (p.sickTill && ds <= p.sickTill) return false;
  if (p.holTill && ds <= p.holTill) return false;
  return ((d + p.off) % 7) < 5;
}
function aptRota(day0: number): { [shift: string]: { [job: string]: AptStaff[] } } {
  const out: any = {}; APT_SHIFTS.forEach(s => { out[s.id] = {}; APT_JOBS.forEach(j => out[s.id][j.id] = []); });
  for (const p of aptStaffState().aptStaff as AptStaff[]) {
    if (!aptOnDuty(p, day0)) continue;
    const sh = p.shift === 'day' ? 'early' : p.shift;   // (office day shifts count with the early one)
    if (out[sh] && out[sh][p.job]) out[sh][p.job].push(p);
  }
  return out;
}
/** Hire people for a job on a shift: names from the Faroese/Icelandic pools, a home in Vík, a car (most) or the bus. */
function aptHire(job: string, shift: 'early' | 'late' | 'night', n: number) {
  const S = aptStaffState(), J = APT_JOBS.filter(j => j.id === job)[0]; if (!J || n <= 0) return;
  for (let i = 0; i < n; ++i) {
    const id = 'as' + uid(), r = seeded(id);
    const p = makePerson(id, 'ground', State.companyId, aptHome(), r() < 0.5 ? 'f' : 'm', r() < 0.7 ? 'IS' : 'FO');
    // (5 on, 2 off: the offsets spread so a shift always has its people - each new hire takes the least-covered offset)
    const used: number[] = [0, 0, 0, 0, 0, 0, 0]; (S.aptStaff as AptStaff[]).filter(q => q.job === job && q.shift === shift).forEach(q => used[q.off]++);
    const off = used.indexOf(Math.min(...used));
    S.aptStaff.push({ id, name: p.name, sex: p.sex, job, home: VIK_STREETS[Math.floor(r() * VIK_STREETS.length)] + ' ' + (1 + Math.floor(r() * 40)) + ', Vík',
      commute: r() < 0.72 ? 'car' : 'bus', shift: J.office ? 'day' : shift, off, hired: new Date().toISOString().slice(0, 10), salary: Math.round(J.pay * (0.9 + r() * 0.25)) } as AptStaff);
  }
  Store.soon();
}
/** People needed for a job on a shift, so that 5-on-2-off still covers every day: the need x 7/5, rounded up. */
function aptHeadcount(need: number): number { return Math.ceil(need * 7 / 5); }

/* ---------- the Staff app (tablet, computers) ---------- */
const AptA: any = { shift: '' };
function aptStaffAppHtml(): string {
  const now = lapGameNow(), day0 = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime();
  const need = aptNeed(day0), rota = aptRota(day0), S = aptStaffState();
  const hNow = now.getHours() + now.getMinutes() / 60, cur = APT_SHIFTS.filter(s => (hNow >= s.from && hNow < s.to) || (s.id === 'night' && (hNow >= 22 || hNow < 6)))[0] || APT_SHIFTS[0];
  const sel = APT_SHIFTS.filter(s => s.id === (AptA.shift || cur.id))[0] || cur;
  const M = aptMovements(day0);
  let short = 0;
  const rows = APT_JOBS.map(j => {
    const n = need[sel.id][j.id], have = (rota[sel.id][j.id] || []).length, total = (S.aptStaff as AptStaff[]).filter(p => p.job === j.id && (p.shift === sel.id || (p.shift === 'day' && sel.id === 'early'))).length;
    const want = aptHeadcount(n), gap = Math.max(0, want - total); short += Math.max(0, n - have);
    return `<div class="as-row ${have < n ? 'short' : ''}"><b>${esc(j.name)}</b><span>${have} on / ${n} needed</span><small>${total} on this shift's team (${want} cover every day)</small>${gap ? `<button class="btn sm primary" data-act="apt-hire" data-job="${j.id}" data-shift="${sel.id}" data-n="${gap}">${icon('plus')}Hire ${gap}</button>` : `<span class="as-ok">${icon('check')}</span>`}</div>`;
  }).join('');
  // who is coming in for the next shift (they leave home about 25 min before by car, 45 by bus)
  const next = APT_SHIFTS[(APT_SHIFTS.indexOf(cur) + 1) % 3], comingDay = next.id === 'early' && cur.id === 'night' ? day0 + (hNow >= 22 ? 864e5 : 0) : day0;
  const coming = (S.aptStaff as AptStaff[]).filter(p => (p.shift === next.id || (p.shift === 'day' && next.id === 'early')) && aptOnDuty(p, comingDay)).slice(0, 14);
  return `<div class="as-app">
    <div class="as-head"><b>Your airport's staff</b><small>Mýrdalssandur · ${M.dep.length} departures and ${M.arr.length} arrivals today · ${(S.aptStaff as AptStaff[]).length} people employed</small></div>
    <div class="seg">${APT_SHIFTS.map(s => `<button class="${s.id === sel.id ? 'on' : ''}" data-act="apt-shift" data-s="${s.id}">${esc(s.name)}${s.id === cur.id ? ' · now' : ''}</button>`).join('')}</div>
    <div class="as-sum ${short ? 'short' : ''}">${short ? `${icon('alert')}${short} short on this shift: flights may run late` : `${icon('check')}Every job on this shift is covered`}</div>
    <div class="as-rows">${rows}</div>
    <div class="as-sub">Coming in for the ${esc(next.name.toLowerCase())}</div>
    <div class="as-coming">${coming.length ? coming.map(p => `<span><b>${esc(p.name)}</b><small>${esc((APT_JOBS.filter(j => j.id === p.job)[0] || { name: '' }).name.replace(/s$/, ''))} · from ${esc(p.home)} · ${p.commute === 'car' ? 'own car' : 'the bus'}</small></span>`).join('') : '<span class="muted">No one yet - hire for that shift</span>'}</div>
    <small class="muted">Needs follow the day's schedule (your flights and the other airlines'): per departure, per arrival, by aircraft size; fire crew by the largest aircraft (category).</small>
  </div>`;
}
ACTIONS['apt-shift'] = el => { AptA.shift = el.getAttribute('data-s') || ''; Sound.tick(); lapRender(true); };
ACTIONS['apt-hire'] = el => { const n = +(el.getAttribute('data-n') || 0); aptHire(el.getAttribute('data-job') || '', (el.getAttribute('data-shift') || 'early') as any, n); toast(`${n} hired`, 'ok'); lapRender(true); };
