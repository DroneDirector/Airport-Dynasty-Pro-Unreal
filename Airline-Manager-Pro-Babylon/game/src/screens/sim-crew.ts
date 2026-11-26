// The Crew screen on the simulation (to-do 176; owner, 2026-11-20 night): in the PC game, while a career runs in Unreal's
// simulation, the Crew screen shows and changes the REAL crew there (ue.adp.sim) - one set of people, not this web game's
// own. Every area of the airport (to-do 206 part 3) with what it needs and who is missing, its people - shift, wage, where
// they are now - and who looks for that work; per person: pay, shift, base, training and letting them go. Outside Unreal (Android, the web) the old screen stays.

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

// (the areas and jobs come from the simulation's one table - ADPCrew.cpp: areas, jobs, shifts in the 'crew' answer)
function simJob(role: string): any { return ((SimCrew.data && SimCrew.data.jobs) || []).filter((j: any) => j.id === role)[0] || { id: role, name: role, area: '' }; }
function simRoleName(role: string): string { return simJob(role).name; }
function simCrewStyle() {
  if (document.getElementById('simcrew-style')) return;
  const st = document.createElement('style'); st.id = 'simcrew-style';
  st.textContent = `
  .sc-areas { display: grid; grid-template-columns: repeat(auto-fill, minmax(158px, 1fr)); gap: 8px; margin-bottom: 14px; }
  .sc-area { display: flex; justify-content: space-between; align-items: center; gap: 8px; min-height: 44px; padding: 7px 11px; border-radius: 12px; border: 1px solid var(--line); background: var(--panel); color: var(--ink-2); font-size: 13px; font-weight: 600; text-align: left; }
  .sc-area:hover, .sc-area:focus-visible { border-color: var(--cyan); }
  .sc-area.on { border-color: var(--cyan); color: var(--ink); background: var(--panel-2); box-shadow: 0 0 0 1px var(--cyan) inset; }
  .sc-area i { font-style: normal; font-size: 11.5px; padding: 2px 8px; border-radius: 9px; background: var(--panel-3); white-space: nowrap; }
  .sc-area.short i { color: var(--amber); }
  .sc-head { display: flex; flex-wrap: wrap; align-items: center; gap: 6px 14px; margin: 4px 0 10px; } .sc-head h2 { margin: 0; font-size: 18px; } .sc-head small { color: var(--ink-2); }
  .sc-adv { display: flex; flex-wrap: wrap; align-items: center; gap: 6px; margin-bottom: 10px; }
  .sim-crew .sim-manage { grid-column: 1 / -1; display: flex; flex-wrap: wrap; align-items: center; gap: 8px; padding-top: 8px; border-top: 1px solid var(--line); }
  .sim-crew .sc-shifts { display: inline-flex; flex-wrap: wrap; align-items: center; gap: 6px; }
  .sim-crew .cr-name small, .sim-crew .cr-now small { white-space: normal; }
  .sim-crew .cr-btns { flex-wrap: wrap; justify-content: flex-end; }
  @media (max-width: 900px) { .sim-crew .sim-row .cr-now, .sim-crew .sim-row .cr-rules { display: flex; flex-wrap: wrap; gap: 4px; grid-column: 1 / -1; } }
  `;
  document.head.appendChild(st);
}

function simPersonRow(p: any, i: number, hire: boolean): string {
  const moodC = p.mood >= 60 ? 'ok' : p.mood >= 35 ? 'amber' : 'bad';
  const payVs = p.going ? Math.round((p.salary / p.going - 1) * 100) : 0;
  const open = SimCrew.open === p.id, job = simJob(p.role);
  const sh = (SimCrew.data.shifts || []).filter((x: any) => x.id === p.shift)[0];
  const turn = job.shift ? (sh ? sh.label : '') : p.role === 'fire' ? '24 hour watches' : '';
  // (airport people: what they do and where now, from the 3D airport where their job is there)
  const now = job.local && p.doing ? `<small><b>${esc(p.doing)}</b>${p.where ? ', ' + esc(p.where) : ''}</small>${job.d3 ? '' : '<small>3D job coming</small>'}` : statusChip(p.status);
  return `<div class="crew-row sim-row" style="--i:${i}">
    <span class="avatar"><b>${esc(String(p.name).split(/\s+/).map((w: string) => w.charAt(0)).join('').slice(0, 2))}</b></span>
    <span class="cr-name"><b>${esc(p.name)}</b><small>${esc(p.rank || simRoleName(p.role))}, ${p.woman ? 'she' : 'he'}, ${p.age}${p.base ? ', based ' + esc(p.base) : ''}${p.home ? ', lives in ' + esc(p.home) : ''}${!hire && turn ? ' · ' + esc(turn) : ''}</small>
      <small>${p.hours ? num(p.hours) + ' flight hours · ' : ''}${(p.languages || []).map((l: string) => esc(l)).join(', ')}${p.contract === 'part' ? ' · part time' : ''} · notice ${p.noticeDays} days</small>
      ${p.interviewed ? `<small class="sim-iv">${icon('check')}Strong: ${esc(p.strengths || '')} · Weak: ${esc(p.weaknesses || '')}</small>` : ''}</span>
    ${!hire && p.warnings > 0 ? `<span class="sim-conduct"><small class="chip amber">${p.warnings} conduct report${p.warnings > 1 ? 's' : ''}</small>${simOfficeBtn(p)}</span>` : ''}
    <span class="cr-now">${hire ? `<small>Rating ${Number(p.rating).toFixed(1)} · level ${p.level}</small>` : `${now}<small class="chip ${moodC}">mood ${p.mood}</small>`}</span>
    <span class="cr-rules">${(p.types || []).map((t: string) => `<i class="rating">${esc(t)}</i>`).join('')}${p.training ? `<i class="rating training">${esc(p.training)} course</i>` : ''}${hire || job.local ? '' : `<i>${p.fly28 || 0} h / 28 days${p.with ? ' · on ' + esc(p.with) : ''}${p.offUntil ? ' · days off' : p.restUntil ? ' · resting' : ''}</i>`}<i>${money(p.salary)}/wk${payVs ? ` (${payVs > 0 ? '+' : ''}${payVs}% vs going rate)` : ''}</i></span>
    <span class="cr-btns">${hire ? `${p.interviewed ? '' : `<button class="btn sm ghost" data-act="sim-interview" data-id="${esc(p.id)}">Interview</button>`}<button class="btn sm primary" data-act="sim-hire" data-id="${esc(p.id)}">${icon('plus')}Hire at ${money(p.asks || p.salary)}/wk</button>` : `<button class="btn sm ghost" data-act="sim-open" data-id="${esc(p.id)}">${open ? 'Close' : 'Manage'}</button>`}</span>
    ${open ? `<div class="sim-manage">
      <label>Weekly pay <input class="sim-pay" type="number" min="1" step="50" value="${Math.round(p.salary)}" data-id="${esc(p.id)}"></label><button class="btn sm" data-act="sim-pay" data-id="${esc(p.id)}">Set pay</button>
      <label>Base <input class="sim-base" type="text" maxlength="4" value="${esc(p.base)}" data-id="${esc(p.id)}"></label><button class="btn sm" data-act="sim-base" data-id="${esc(p.id)}">Move</button>
      ${p.role === 'captain' || p.role === 'fo' ? `<label>Type rating <select class="sim-type" data-id="${esc(p.id)}">${(SimCrew.data.types || []).map((t: any) => `<option value="${esc(t.id)}">${esc(t.name)}</option>`).join('')}</select></label><button class="btn sm" data-act="sim-train" data-id="${esc(p.id)}">Start course</button>` : ''}
      ${job.shift ? `<span class="sc-shifts">Shift ${(SimCrew.data.shifts || []).map((x: any) => `<button class="fchip ${x.id === p.shift ? 'on' : ''}" data-act="sim-shift" data-id="${esc(p.id)}" data-s="${esc(x.id)}">${esc(x.label)}</button>`).join('')}</span>` : ''}
      ${simOfficeBtn(p)}
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
  simCrewStyle();
  const ppl: any[] = d.people || [], pool: any[] = d.pool || [], jobs: any[] = d.jobs || [], areas: any[] = d.areas || [];
  const jobsOf = (area: string) => jobs.filter(j => j.area === area);
  const inArea = (L: any[], area: string) => { const r = jobsOf(area).map(j => j.id); return L.filter(p => r.indexOf(p.role) >= 0); };
  const missing = (area: string) => jobsOf(area).reduce((n, j) => n + Math.max(0, (j.need || 0) - (j.have || 0)), 0);
  if (SimCrew.tab !== 'hire' && !areas.some(a => a.id === SimCrew.tab)) SimCrew.tab = 'pilots';
  const all = SimCrew.tab === 'hire', area = areas.filter(a => a.id === SimCrew.tab)[0];
  const chips = areas.map(a => { const m = missing(a.id), n = inArea(ppl, a.id).length; return `<button class="sc-area ${SimCrew.tab === a.id ? 'on' : ''} ${m ? 'short' : ''}" data-act="sim-tab" data-t="${esc(a.id)}"><span>${esc(a.label)}</span><i>${n}${m ? `, ${m} missing` : ''}</i></button>`; }).join('')
    + `<button class="sc-area ${all ? 'on' : ''}" data-act="sim-tab" data-t="hire"><span>Everyone looking for work</span><i>${pool.length}</i></button>`;
  // what an area needs: the game's own rules; a job nothing depends on yet says so
  const needChip = (j: any) => {
    const ruled = j.need > 0, n = ruled ? j.need : j.local && !j.sim ? 1 : 0, short = j.have < n, hard = ruled && short;
    return `<span class="need ${hard ? 'short' : 'ok'}">${icon(hard ? 'alert' : 'check')}<span><b>${esc(j.name)}</b><small>${j.have}${n ? ` of ${n} ${ruled ? 'needed' : 'advised'}${short ? `, hire ${n - j.have} more` : ''}` : ' hired, none needed until you have aircraft'}${j.onShift >= 0 ? ` · ${j.onShift} on shift now` : ''}</small></span></span>`;
  };
  const note = (L: any[]) => {
    const l = L.filter(j => j.local); if (!l.length) return '';
    const t = l.map(j => j.d3 ? 'These are the people you see at work in the 3D airport.' : j.sim ? 'The game counts them today; their 3D job is coming.' : 'You can hire them now; nothing in the game depends on them yet, their 3D job is coming.');
    return `<p class="fine-print">${icon('info')}${t.filter((x, i) => t.indexOf(x) === i).join(' ')}${l.some(j => j.sim && j.shift) ? ' About 40% of each team is on duty at any hour (20% at night).' : ''}</p>`;
  };
  const adverts = (L: any[]) => `<div class="sc-adv">${L.map(j => `<button class="fchip" data-act="sim-advert" data-r="${esc(j.id)}">Advertise: ${esc(j.name)} (${money(j.advert)})</button>`).join('')}</div>`;
  const veh = { ground: [['tractor', 'Baggage tractor', 65000, d.tractors]], fuel: [['fuel', 'Fuel truck', 260000, d.fuelTrucks]], catering: [['catering', 'Catering truck', 185000, d.cateringTrucks]] } as any;
  const leave = (d.leave || []).filter((l: any) => l.state === 'asked');
  const staff = area ? inArea(ppl, area.id) : [], cand = all ? pool : area ? inArea(pool, area.id) : [], aj = all ? jobs : area ? jobsOf(area.id) : [];
  return `<div class="sim-crew"><div class="pilot-hero"><div class="ph-main"><small>${esc(d.airline || '')} · based at ${esc(d.hub || '')}</small><h1>Crew</h1>
      <p class="sub">${ppl.length} people on your staff · ${money(d.balance || 0)} in the bank</p></div></div>
    ${leave.length ? `<div class="sim-leave"><h4 class="grp-h">Holiday requests</h4>${leave.map((l: any) => `<div class="crew-row"><span class="cr-name"><b>${esc(l.name)}</b><small>${esc(simRoleName(l.role))} · ${l.days} days from ${esc(l.from)}</small></span><span class="cr-btns"><button class="btn sm primary" data-act="sim-leave" data-i="${l.i}" data-a="yes">Approve</button><button class="btn sm ghost" data-act="sim-leave" data-i="${l.i}" data-a="no">Refuse</button></span></div>`).join('')}</div>` : ''}
    <div class="sc-areas">${chips}</div>
    ${area ? `<div class="sc-head"><h2>${esc(area.label)}</h2><small>${esc(area.duties)}</small></div>
    <div class="need-row">${aj.map(needChip).join('')}${note(aj)}</div>
    ${(veh[area.id] || []).map((v: any) => `<div class="sc-adv"><button class="btn sm" data-act="sim-vehicle" data-k="${v[0]}">Buy a ${v[1].toLowerCase()} (${money(v[2])})</button><small class="muted">You have ${v[3] || 0}.</small></div>`).join('')}
    <div class="crew-list">${staff.map((p, i) => simPersonRow(p, i, false)).join('') || emptyState('Nobody here yet', 'Hire from the people below, or place an advert.')}</div>` : ''}
    <h4 class="grp-h">Looking for work</h4>
    ${adverts(aj)}<p class="fine-print">${icon('info')}A paid advert brings three better candidates. Interview before you hire to see their strong and weak points.</p>
    <div class="crew-list">${cand.map((p, i) => simPersonRow(p, i, true)).join('') || emptyState('Nobody looking for work right now', 'New people come every few hours, or place an advert.')}</div></div>`;
}

ACTIONS['sim-tab'] = el => { SimCrew.tab = el.getAttribute('data-t') || 'pilots'; SimCrew.open = ''; Sound.tick(); refresh(); };
ACTIONS['sim-open'] = el => { const id = el.getAttribute('data-id') || ''; SimCrew.open = SimCrew.open === id ? '' : id; Sound.tick(); refresh(); };
ACTIONS['sim-hire'] = el => simCrewDo('hire', (el.getAttribute('data-id') || '') + '|', 'Hired');
ACTIONS['sim-shift'] = el => simCrewDo('shift', (el.getAttribute('data-id') || '') + '|' + (el.getAttribute('data-s') || ''), 'Shift changed');
/** A direct link from another page to one area of the Crew screen. */
ACTIONS['crew-area'] = el => { SimCrew.tab = el.getAttribute('data-area') || 'pilots'; SimCrew.open = ''; Sound.tick(); try { closeAll(); } catch (e) { /* nothing open */ } go('pilots'); };
/** The Staff app of the tablet and the computers: the same crew while a career runs in Unreal. */
function simStaffApp(): string { if (!simLink()) return ''; simCrewLoad(false); return SimCrew.data ? simCrewHtml() : ''; }
// (live: where everyone is, every 5 seconds while the page is open and nothing is being typed)
setInterval(() => { try { const f: any = document.activeElement; if ((State as any).screen === 'pilots' && simLink() && document.querySelector('#view .sim-crew') && !(f && /INPUT|SELECT|TEXTAREA/.test(f.tagName))) simCrewLoad(false); } catch (e) { /* not on screen */ } }, 5000);
// "Call to my office" (owner 11-26): the one way in, UOfficeCall::Ask - now, or queued until they are free, with a line
// saying which; only for people in the 3D airport just now (callable), and "coming" / "queued" once asked
function simOfficeBtn(p: any): string {
  if (p.office === 'coming') return '<small class="chip ok">On the way to your office</small>';
  if (p.office === 'queued') return '<small class="chip amber">Busy - comes to your office when free</small>';
  return p.callable ? `<button class="btn sm ghost" data-act="sim-office" data-id="${esc(p.id)}">Call to my office</button>` : '';
}
ACTIONS['sim-office'] = el => {
  const U = simLink(); if (!U) return;
  Promise.resolve(U.sim('office', el.getAttribute('data-id') || '')).then((s: any) => {
    let d: any = {}; try { d = JSON.parse(String(s || '{}')); } catch (e) { /* bad answer */ }
    if (d.ok) { Haptics.success(); toast(d.why || 'Called to your office', 'ok'); } else toast(d.why ? 'Not called: ' + d.why : 'Not called', 'warn');
    simCrewLoad(true);
  });
};
ACTIONS['sim-dayoff'] = el => simCrewDo('dayoff', (el.getAttribute('data-id') || '') + '|2', 'Two days off rostered');
ACTIONS['sim-interview'] = el => simCrewDo('interview', el.getAttribute('data-id') || '', 'Interviewed');
ACTIONS['sim-advert'] = el => simCrewDo('advert', el.getAttribute('data-r') || 'ground', 'Advert placed');
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
