// The Crew screen on the simulation (to-do 176; owner, 2026-11-20 night): in the PC game, while a career runs in Unreal's
// simulation, the Crew screen shows and changes the REAL crew there (ue.adp.sim) - one set of people, not this web game's
// own. Pilots, cabin crew, mechanics, ground staff and security, what the fleet needs, the hiring pool, and per person:
// pay, base, training and letting them go. Outside Unreal (Android, the web) the old screen stays.

const SimCrew: any = { data: null as any, at: 0, busy: false, tab: 'pilots', open: '' };

function simLink(): any { try { const t: any = window.top; return t && t.ue && t.ue.adp && t.ue.adp.sim ? t.ue.adp : null; } catch (e) { return null; } }

function simCrewLoad(force?: boolean) {
  const U = simLink(); if (!U || SimCrew.busy) return;
  if (!force && Date.now() - SimCrew.at < 4000) return;
  SimCrew.busy = true;
  Promise.resolve(U.sim('crew', '')).then((s: any) => {
    SimCrew.busy = false; SimCrew.at = Date.now();
    try { const d = JSON.parse(String(s || '{}')); SimCrew.data = d && d.ok ? d : null; } catch (e) { SimCrew.data = null; }
    try { if (typeof refresh === 'function') refresh(); } catch (e) { /* not on screen */ }
  }, () => { SimCrew.busy = false; });
}

function simCrewDo(action: string, arg: string, okMsg: string) {
  const U = simLink(); if (!U) return;
  Promise.resolve(U.sim(action, arg)).then((s: any) => {
    let d: any = {}; try { d = JSON.parse(String(s || '{}')); } catch (e) { /* bad answer */ }
    if (d.ok) { Haptics.success(); toast(okMsg, 'ok'); } else toast(d.why ? 'Not done: ' + d.why : 'Not done', 'warn');
    simCrewLoad(true);
  });
}

const SIM_ROLES: [string, string, string[]][] = [
  ['pilots', 'Pilots', ['captain', 'fo']], ['cabin', 'Cabin crew', ['fa']], ['mech', 'Mechanics', ['mech']],
  ['checkin', 'Passenger service', ['checkin']], ['ground', 'Ground staff', ['ground', 'fuel', 'catering', 'cleaning']], ['security', 'Security', ['security']], ['hire', 'Hire staff', []],
];
const SIM_ROLE_NAME: Record<string, string> = { captain: 'Captain', fo: 'First officer', fa: 'Flight attendant', mech: 'Mechanic', ground: 'Baggage handler', security: 'Security officer', checkin: 'Passenger service agent', fuel: 'Fuel truck driver', catering: 'Catering loader', cleaning: 'Aircraft cleaner' };

function simPersonRow(p: any, i: number, hire: boolean): string {
  const moodC = p.mood >= 60 ? 'ok' : p.mood >= 35 ? 'amber' : 'bad';
  const payVs = p.going ? Math.round((p.salary / p.going - 1) * 100) : 0;
  const open = SimCrew.open === p.id;
  return `<div class="crew-row sim-row" style="--i:${i}">
    <span class="avatar"><b>${esc(String(p.name).split(/\s+/).map((w: string) => w.charAt(0)).join('').slice(0, 2))}</b></span>
    <span class="cr-name"><b>${esc(p.name)}</b><small>${esc(p.rank || SIM_ROLE_NAME[p.role] || p.role)}, ${p.woman ? 'she' : 'he'}, ${p.age}${p.base ? ', based ' + esc(p.base) : ''}${p.home ? ', lives in ' + esc(p.home) : ''}</small>
      <small>${p.hours ? num(p.hours) + ' flight hours · ' : ''}${(p.languages || []).map((l: string) => esc(l)).join(', ')}${p.contract === 'part' ? ' · part time' : ''} · notice ${p.noticeDays} days</small>
      ${p.interviewed ? `<small class="sim-iv">${icon('check')}Strong: ${esc(p.strengths || '')} · Weak: ${esc(p.weaknesses || '')}</small>` : ''}</span>
    <span class="cr-now">${hire ? `<small>Rating ${Number(p.rating).toFixed(1)} · level ${p.level}</small>` : `${statusChip(p.status)}<small class="chip ${moodC}">mood ${p.mood}</small>`}</span>
    <span class="cr-rules">${(p.types || []).map((t: string) => `<i class="rating">${esc(t)}</i>`).join('')}${p.training ? `<i class="rating training">${esc(p.training)} course</i>` : ''}${hire ? '' : `<i>${p.fly28 || 0} h / 28 days${p.with ? ' · on ' + esc(p.with) : ''}${p.offUntil ? ' · days off' : p.restUntil ? ' · resting' : ''}</i>`}<i>${money(p.salary)}/wk${payVs ? ` (${payVs > 0 ? '+' : ''}${payVs}% vs going rate)` : ''}</i></span>
    <span class="cr-btns">${hire ? `${p.interviewed ? '' : `<button class="btn sm ghost" data-act="sim-interview" data-id="${esc(p.id)}">Interview</button>`}<button class="btn sm primary" data-act="sim-hire" data-id="${esc(p.id)}">${icon('plus')}Hire at ${money(p.asks || p.salary)}/wk</button>` : `<button class="btn sm ghost" data-act="sim-open" data-id="${esc(p.id)}">${open ? 'Close' : 'Manage'}</button>`}</span>
    ${open ? `<div class="sim-manage">
      <label>Weekly pay <input class="sim-pay" type="number" min="1" step="50" value="${Math.round(p.salary)}" data-id="${esc(p.id)}"></label><button class="btn sm" data-act="sim-pay" data-id="${esc(p.id)}">Set pay</button>
      <label>Base <input class="sim-base" type="text" maxlength="4" value="${esc(p.base)}" data-id="${esc(p.id)}"></label><button class="btn sm" data-act="sim-base" data-id="${esc(p.id)}">Move</button>
      ${p.role === 'captain' || p.role === 'fo' ? `<label>Type rating <select class="sim-type" data-id="${esc(p.id)}">${(SimCrew.data.types || []).map((t: any) => `<option value="${esc(t.id)}">${esc(t.name)}</option>`).join('')}</select></label><button class="btn sm" data-act="sim-train" data-id="${esc(p.id)}">Start course</button>` : ''}
      <button class="btn sm ghost" data-act="sim-dayoff" data-id="${esc(p.id)}">Give 2 days off</button>
      <button class="btn sm ghost" data-act="sim-contract" data-id="${esc(p.id)}" data-c="${p.contract === 'part' ? 'full' : 'part'}">Make ${p.contract === 'part' ? 'full time' : 'part time (55% pay)'}</button>
      ${p.status === 'dismissed' ? '<small>Leaving after their notice.</small>' : `<button class="btn sm danger" data-act="sim-dismiss" data-id="${esc(p.id)}">Let go (notice ${p.noticeDays} days, severance)</button>`}
    </div>` : ''}
  </div>`;
}

function simCrewHtml(): string {
  simCrewLoad(false);
  const d = SimCrew.data;
  if (!d) return `<div class="pilot-hero"><div class="ph-main"><h1>Crew</h1><p class="sub">Reading your airline's people...</p></div></div>`;
  const ppl: any[] = d.people || [], pool: any[] = d.pool || [], need = d.need || {};
  const have = (r: string) => ppl.filter(p => p.role === r && p.status !== 'dismissed').length;
  const chip = (label: string, r: string) => { const h = have(r), n = need[r] || 0; return `<span class="need ${h < n ? 'short' : 'ok'}">${icon(h < n ? 'alert' : 'check')}<span><b>${label}</b><small>${h} of ${n} needed${h < n ? `, hire ${n - h} more` : ''}</small></span></span>`; };
  const tab = SIM_ROLES.filter(t => t[0] === SimCrew.tab)[0] || SIM_ROLES[0];
  const list = tab[0] === 'hire' ? pool : ppl.filter(p => tab[2].indexOf(p.role) >= 0);
  const tabs = SIM_ROLES.map(t => `<button class="tab ${SimCrew.tab === t[0] ? 'on' : ''}" data-act="sim-tab" data-t="${t[0]}">${t[1]}<i>${t[0] === 'hire' ? pool.length : ppl.filter(p => t[2].indexOf(p.role) >= 0).length}</i></button>`).join('');
  return `<div class="pilot-hero"><div class="ph-main"><small>${esc(d.airline || '')} · based at ${esc(d.hub || '')}</small><h1>Crew</h1>
      <p class="sub">${ppl.length} people on your staff · ${money(d.balance || 0)} in the bank</p></div></div>
    <div class="need-row">${chip('Captains', 'captain')}${chip('First officers', 'fo')}${chip('Flight attendants', 'fa')}${chip('Mechanics', 'mech')}${chip('Passenger service agents', 'checkin')}${chip('Security officers', 'security')}${chip('Baggage handlers', 'ground')}
      <p class="fine-print">${icon('info')}Real-world rules: two pilots per airliner, one flight attendant per 50 seats from 20 seats, a licensed mechanic at every base. First hire the crew one aircraft needs, then buy or lease it.</p></div>
    ${(d.leave || []).filter((l: any) => l.state === 'asked').length ? `<div class="sim-leave"><h4 class="grp-h">Holiday requests</h4>${(d.leave || []).filter((l: any) => l.state === 'asked').map((l: any) => `<div class="crew-row"><span class="cr-name"><b>${esc(l.name)}</b><small>${esc(SIM_ROLE_NAME[l.role] || l.role)} · ${l.days} days from ${esc(l.from)}</small></span><span class="cr-btns"><button class="btn sm primary" data-act="sim-leave" data-i="${l.i}" data-a="yes">Approve</button><button class="btn sm ghost" data-act="sim-leave" data-i="${l.i}" data-a="no">Refuse</button></span></div>`).join('')}</div>` : ''}
    ${SimCrew.tab === 'ground' ? `<div class="need-row">${chip('Baggage handlers', 'ground')}${chip('Fuel drivers', 'fuel')}${chip('Catering', 'catering')}${chip('Cleaners', 'cleaning')}
      <p class="fine-print">${icon('info')}Your ground teams work every turnaround here: about 40% of each team is on shift at any hour (half at night). Too few people or no vehicle: a slower turnaround and a late departure. Vehicles: ${d.fuelTrucks || 0} fuel, ${d.cateringTrucks || 0} catering, ${d.tractors || 0} baggage tractors.</p>
      <span class="btn-row">${[['fuel', 'Fuel truck', 260000], ['catering', 'Catering truck', 185000], ['tractor', 'Baggage tractor', 65000]].map((v: any) => `<button class="btn sm" data-act="sim-vehicle" data-k="${v[0]}">Buy a ${v[1]} (${money(v[2])})</button>`).join('')}</span></div>` : ''}
    <div class="tabs big">${tabs}</div>
    ${tab[0] === 'hire' ? `<div class="filter-row">${[['captain', 'Captains', 6000], ['fo', 'First officers', 4000], ['fa', 'Cabin crew', 1500], ['mech', 'Mechanics', 2500], ['checkin', 'Passenger service', 1500], ['security', 'Security', 1500], ['ground', 'Baggage handlers', 1500], ['fuel', 'Fuel drivers', 1500], ['catering', 'Catering', 1500], ['cleaning', 'Cleaners', 1500]].map((r: any) => `<button class="fchip" data-act="sim-advert" data-r="${r[0]}">Advertise: ${r[1]} (${money(r[2])})</button>`).join('')}<p class="fine-print">${icon('info')}A paid advert brings three better candidates. Interview before you hire to see their strong and weak points.</p></div>` : ''}
    <div class="crew-list">${list.map((p, i) => simPersonRow(p, i, tab[0] === 'hire')).join('') || emptyState(tab[0] === 'hire' ? 'Nobody looking for work right now' : 'Nobody here yet', tab[0] === 'hire' ? 'New people come every few hours.' : 'Hire them under Hire staff.')}</div>`;
}

ACTIONS['sim-tab'] = el => { SimCrew.tab = el.getAttribute('data-t') || 'pilots'; SimCrew.open = ''; Sound.tick(); refresh(); };
ACTIONS['sim-open'] = el => { const id = el.getAttribute('data-id') || ''; SimCrew.open = SimCrew.open === id ? '' : id; Sound.tick(); refresh(); };
ACTIONS['sim-hire'] = el => simCrewDo('hire', (el.getAttribute('data-id') || '') + '|', 'Hired');
ACTIONS['sim-dayoff'] = el => simCrewDo('dayoff', (el.getAttribute('data-id') || '') + '|2', 'Two days off rostered');
ACTIONS['sim-interview'] = el => simCrewDo('interview', el.getAttribute('data-id') || '', 'Interviewed');
ACTIONS['sim-advert'] = el => simCrewDo('advert', el.getAttribute('data-r') || 'fa', 'Advert placed');
ACTIONS['sim-contract'] = el => simCrewDo('contract', (el.getAttribute('data-id') || '') + '|' + (el.getAttribute('data-c') || 'full'), 'Contract changed');
ACTIONS['sim-leave'] = el => simCrewDo('leave', (el.getAttribute('data-i') || '') + '|' + (el.getAttribute('data-a') || 'no'), 'Answered');
ACTIONS['sim-vehicle'] = el => simCrewDo('vehicle', el.getAttribute('data-k') || 'fuel', 'Vehicle bought');
ACTIONS['sim-dismiss'] = el => simCrewDo('dismiss', el.getAttribute('data-id') || '', 'Notice given');
function simField(cls: string, id: string): string { const e = document.querySelector(`.${cls}[data-id="${id}"]`) as HTMLInputElement | null; return e ? String(e.value || '').trim() : ''; }
ACTIONS['sim-pay'] = el => { const id = el.getAttribute('data-id') || ''; simCrewDo('pay', id + '|' + simField('sim-pay', id), 'Pay changed'); };
ACTIONS['sim-base'] = el => { const id = el.getAttribute('data-id') || ''; simCrewDo('base', id + '|' + simField('sim-base', id).toUpperCase(), 'Base changed'); };
ACTIONS['sim-train'] = el => { const id = el.getAttribute('data-id') || ''; simCrewDo('train', id + '|' + simField('sim-type', id), 'Course started'); };

/** The Crew screen: the simulation's crew whenever Unreal runs a career, else the old screen. */
(function simCrewHook() {
  const S: any = typeof SCREENS !== 'undefined' ? SCREENS : null;
  if (!S || !S.pilots) { setTimeout(simCrewHook, 50); return; }
  if (S.pilots.__sim) return;
  const old = S.pilots.render;
  S.pilots.render = function () { return simLink() && SimCrew.data !== false ? simCrewOrOld(old, this) : old.call(this); };
  S.pilots.__sim = true;
})();
function simCrewOrOld(old: any, self: any): string {
  // (no career in the simulation: the old screen)
  if (SimCrew.data === null && SimCrew.at && Date.now() - SimCrew.at < 4000) return old.call(self);
  return simCrewHtml();
}

/* ---------- Maintenance on the simulation (to-do 181) ----------
 * In the PC game with a career running, the Maintenance screen shows the simulation's fleet: each aircraft's condition,
 * hours and cycles, its next check (A / C / D) and how far due, checks running, the parts stock - and starts a check or
 * buys parts. */
const SimMx: any = { data: null as any, at: 0, busy: false };
function simMxLoad(force?: boolean) {
  const U = simLink(); if (!U || SimMx.busy) return;
  if (!force && Date.now() - SimMx.at < 4000) return;
  SimMx.busy = true;
  Promise.resolve(U.sim('fleet', '')).then((s: any) => {
    SimMx.busy = false; SimMx.at = Date.now();
    try { const d = JSON.parse(String(s || '{}')); SimMx.data = d && d.ok ? d : null; } catch (e) { SimMx.data = null; }
    try { refresh(); } catch (e) { /* not on screen */ }
  }, () => { SimMx.busy = false; });
}
function simMxHtml(): string {
  simMxLoad(false);
  const d = SimMx.data;
  if (!d) return `<div class="pilot-hero"><div class="ph-main"><h1>Maintenance</h1><p class="sub">Reading your fleet...</p></div></div>`;
  const rows = (d.aircraft || []).map((a: any, i: number) => {
    const c = a.due >= 110 ? 'bad' : a.due >= 90 ? 'amber' : 'ok';
    return `<div class="crew-row" style="--i:${i}"><span class="cr-name"><b>${esc(a.reg)}</b><small>${esc(a.type)} at ${esc(a.at)} · ${num(a.hours)} h, ${num(a.cycles)} cycles</small></span>
      <span class="cr-now">${statusChip(a.status)}<small>condition ${a.condition}%</small></span>
      <span class="cr-rules">${a.inCheck ? `<i class="rating training">${esc(a.inCheck)}-check until ${esc(String(a.until || '').slice(5, 16).replace('T', ' '))}</i>` : `<i class="chip ${c}">next: ${esc(a.nextCheck || '-')}-check, ${a.due}% of its interval</i>`}</span>
      <span class="cr-btns">${a.inCheck ? '' : ['A', 'C', 'D'].map(k => `<button class="btn sm ${k === a.nextCheck ? 'primary' : 'ghost'}" data-act="sim-check" data-id="${esc(a.reg)}" data-k="${k}">${k}-check</button>`).join('')}</span></div>`;
  }).join('');
  return `<div class="pilot-hero"><div class="ph-main"><h1>Maintenance</h1><p class="sub">Parts in stock ${money(d.partsStock || 0)} · parts market ${Math.round((d.partsIndex || 1) * 100)}%</p>
      <span class="btn-row">${[50000, 250000, 1000000].map(v => `<button class="btn sm" data-act="sim-parts" data-v="${v}">Buy ${money(v)} of parts</button>`).join('')}</span></div></div>
    <p class="fine-print">${icon('info')}Real-world rules: A-checks every 750 flight hours, 750 cycles or 120 days; C-checks every 2 years in a hangar; D-checks every 6 years. A check 10% overdue grounds the aircraft. Mechanics at the base do the work; short of parts, they are bought on the spot at 30% more.</p>
    <div class="crew-list">${rows || emptyState('No aircraft yet', 'Buy or lease your first aircraft.')}</div>`;
}
ACTIONS['sim-check'] = el => { const U = simLink(); if (!U) return; Promise.resolve(U.sim('check', (el.getAttribute('data-id') || '') + '|' + (el.getAttribute('data-k') || 'A'))).then((s: any) => { let d: any = {}; try { d = JSON.parse(String(s)); } catch (e) { /* */ } toast(d.ok ? 'Check started' : 'Not started: ' + (d.why || ''), d.ok ? 'ok' : 'warn'); simMxLoad(true); }); };
ACTIONS['sim-parts'] = el => { const U = simLink(); if (!U) return; Promise.resolve(U.sim('parts', el.getAttribute('data-v') || '0')).then((s: any) => { let d: any = {}; try { d = JSON.parse(String(s)); } catch (e) { /* */ } toast(d.ok ? 'Parts bought' : 'Not bought: ' + (d.why || ''), d.ok ? 'ok' : 'warn'); simMxLoad(true); }); };
(function simMxHook() {
  const S: any = typeof SCREENS !== 'undefined' ? SCREENS : null;
  if (!S || !S.maint) { setTimeout(simMxHook, 50); return; }
  if (S.maint.__sim) return;
  const old = S.maint.render;
  S.maint.render = function () { if (!simLink() || (SimMx.data === null && SimMx.at && Date.now() - SimMx.at < 4000)) return old.call(this); return simMxHtml(); };
  S.maint.__sim = true;
})();
