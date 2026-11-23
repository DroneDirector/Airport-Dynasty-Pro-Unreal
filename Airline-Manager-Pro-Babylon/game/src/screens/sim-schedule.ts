// Your aircraft and your timetable on the simulation (owner, 2026-11-23: "the player buys Airbus A320s, hires, makes the
// schedule, and the passengers book those flights"): in the PC game, while a career runs in Unreal's simulation, the Fleet,
// Market, Routes and Schedule screens show and change the REAL fleet and timetable there (ue.adp.sim) - the aircraft that
// stand at the gates and the flights the passengers walk in for. Outside Unreal (Android, the web) the old screens stay.

const SimAir: any = { data: null as any, at: 0, busy: false, tab: 'fleet' };
const SimTt: any = { data: null as any, at: 0, busy: false, places: [] as any[], q: '', reg: '', to: '', time: '', days: 127, err: '', asked: '' };

function simAirLoad(force?: boolean) {
  const U = simLink(); if (!U || SimAir.busy) return;
  if (!force && Date.now() - SimAir.at < 4000) return;
  SimAir.busy = true;
  Promise.resolve(U.sim('aircraft', '')).then((s: any) => {
    SimAir.busy = false; SimAir.at = Date.now();
    try { const d = JSON.parse(String(s || '{}')); SimAir.data = d && d.ok ? d : null; } catch (e) { SimAir.data = null; }
    try { refresh(); } catch (e) { /* not on screen */ }
  }, () => { SimAir.busy = false; });
}
function simTtLoad(force?: boolean) {
  const U = simLink(); if (!U || SimTt.busy) return;
  if (!force && Date.now() - SimTt.at < 4000) return;
  SimTt.busy = true;
  Promise.resolve(U.sim('timetable', '')).then((s: any) => {
    SimTt.busy = false; SimTt.at = Date.now();
    try { const d = JSON.parse(String(s || '{}')); SimTt.data = d && d.ok ? d : null; } catch (e) { SimTt.data = null; }
    const act = document.activeElement as any;   // (never while something is typed into a field)
    if (!(act && /INPUT|SELECT/.test(String(act.tagName)))) { try { refresh(); } catch (e) { /* not on screen */ } }
  }, () => { SimTt.busy = false; });
}
function simPlaces() {
  const U = simLink(); if (!U || !SimTt.reg) return;
  const key = SimTt.reg + '|' + SimTt.q; if (SimTt.asked === key) return; SimTt.asked = key;
  Promise.resolve(U.sim('places', key)).then((s: any) => {
    try { const d = JSON.parse(String(s || '{}')); SimTt.places = d && d.ok ? d.places : []; } catch (e) { SimTt.places = []; }
    if (SimTt.places.length && !SimTt.places.some((p: any) => p.icao === SimTt.to)) SimTt.to = SimTt.places[0].icao;
    try { refresh(); } catch (e) { /* not on screen */ }
  });
}
function simAirDo(action: string, arg: string, ok: string) {
  const U = simLink(); if (!U) return;
  Promise.resolve(U.sim(action, arg)).then((s: any) => {
    let d: any = {}; try { d = JSON.parse(String(s || '{}')); } catch (e) { /* bad answer */ }
    if (d.ok) { Haptics.success(); toast(d.reg ? ok + ': ' + d.reg : ok, 'ok'); SimTt.err = ''; } else { SimTt.err = d.why || 'Not done'; toast('Not done: ' + (d.why || ''), 'warn'); }
    simAirLoad(true); simTtLoad(true);
  });
}

function simFleetHtml(market: boolean): string {
  simAirLoad(false);
  const d = SimAir.data;
  if (!d) return `<div class="pilot-hero"><div class="ph-main"><h1>${market ? 'Aircraft market' : 'Fleet'}</h1><p class="sub">Reading your airline...</p></div></div>`;
  const fl: any[] = d.fleet || [];
  const rows = fl.map((a: any, i: number) => `<div class="crew-row" style="--i:${i}"><span class="cr-name"><b>${esc(a.reg)}</b><small>${esc(a.type)} · ${a.own === 'leased' ? 'leased, ' + money(a.lease) + ' a month' : 'owned'} · ${num(a.hours)} h, ${num(a.cycles)} cycles</small>
      <small>${a.next ? 'Next: ' + esc(a.next) : a.lines ? '' : 'No flights: put it in the timetable'}</small></span>
    <span class="cr-now">${statusChip(a.status)}<small>at ${esc(a.at)}${a.stand ? ', ' + esc(String(a.stand).replace('STAND-', 'gate ')) : ''}</small></span>
    <span class="cr-rules"><i class="chip ${a.condition >= 60 ? 'ok' : 'amber'}">condition ${a.condition}%</i><i>${a.lines} ${a.lines === 1 ? 'line' : 'lines'} in the timetable</i></span>
    <span class="cr-btns"><button class="btn sm primary" data-act="sim-tt-for" data-id="${esc(a.reg)}">${icon('clock')}Timetable</button><button class="btn sm ghost" data-act="sim-sell" data-id="${esc(a.reg)}">${a.own === 'leased' ? 'Hand back' : 'Sell ' + money(a.value)}</button></span></div>`).join('');
  const shop = (d.types || []).map((t: any) => `<div class="crew-row"><span class="cr-name"><b>${esc(t.name)}</b><small>${t.seats} seats · ${num(t.range)} nm · crew: a captain, a first officer, ${t.fa} cabin crew</small></span>
    <span class="cr-now"><small>new</small></span><span class="cr-rules"></span>
    <span class="cr-btns"><button class="btn sm primary" data-act="sim-buy" data-id="${esc(t.id)}" data-m="buy" ${d.balance < t.price ? 'disabled' : ''}>Buy ${money(t.price)}</button><button class="btn sm" data-act="sim-buy" data-id="${esc(t.id)}" data-m="lease" ${d.balance < t.deposit ? 'disabled' : ''}>Lease ${money(t.lease)} a month</button></span></div>`).join('');
  const used = (d.used || []).map((u: any) => `<div class="crew-row"><span class="cr-name"><b>${esc(u.name)}</b><small>${u.age} years old</small></span><span class="cr-now"><small>used</small></span><span class="cr-rules"><i class="chip ${u.condition >= 70 ? 'ok' : 'amber'}">condition ${u.condition}%</i></span>
    <span class="cr-btns"><button class="btn sm primary" data-act="sim-buyused" data-id="${u.i}" ${d.balance < u.price ? 'disabled' : ''}>Buy ${money(u.price)}</button></span></div>`).join('');
  return `<div class="pilot-hero"><div class="ph-main"><small>based at ${esc(d.hub || '')}</small><h1>${market ? 'Aircraft market' : 'Fleet'}</h1><p class="sub">${fl.length} aircraft · ${money(d.balance || 0)} in the bank</p></div></div>
    ${market ? '' : `<h4 class="grp-h">Your aircraft</h4><div class="crew-list">${rows || emptyState('No aircraft yet', 'Hire the crew one aircraft needs, then buy or lease it here.')}</div>`}
    <h4 class="grp-h">Buy or lease</h4><div class="crew-list">${shop}</div>
    <h4 class="grp-h">Used aircraft (a new list every Monday)</h4><div class="crew-list">${used || emptyState('None offered just now', 'New offers come every Monday.')}</div>
    <p class="fine-print">${icon('info')}An aircraft is delivered to your base and stands at a gate. A lease: two months' rent as the deposit; handed back early it costs three more.</p>`;
}

const SIM_DAYS = ['Mo', 'Tu', 'We', 'Th', 'Fr', 'Sa', 'Su'];
function simHm(m: number): string { m = ((Math.round(m) % 1440) + 1440) % 1440; const h = Math.floor(m / 60), x = m % 60; return (h < 10 ? '0' : '') + h + ':' + (x < 10 ? '0' : '') + x; }
function simTimetableHtml(): string {
  simTtLoad(false);
  const d = SimTt.data;
  if (!d) return `<div class="pilot-hero"><div class="ph-main"><h1>Timetable</h1><p class="sub">Reading your timetable...</p></div></div>`;
  const fl: any[] = d.fleet || [];
  if (!SimTt.reg || !fl.some((a: any) => a.reg === SimTt.reg)) { SimTt.reg = fl.length ? fl[0].reg : ''; SimTt.asked = ''; }
  if (!SimTt.time) { const n = String(d.now || '08:00').split(':'); SimTt.time = simHm((Number(n[0]) * 60 + Number(n[1]) + 180) - (Number(n[1]) % 5)); }
  simPlaces();
  const p = (SimTt.places || []).filter((x: any) => x.icao === SimTt.to)[0];
  const t = String(SimTt.time).split(':'), dep = Number(t[0]) * 60 + Number(t[1] || 0);
  const slots = (d.slots || []).slice().sort((a: any, b: any) => a.dep - b.dep).map((s: any, i: number) => `<div class="crew-row" style="--i:${i}"><span class="cr-name"><b>${simHm(s.dep)} ${esc(s.town)} <small>(${esc(s.to)})</small></b><small>${esc(s.reg)} · flight ${s.number} out, ${s.number + 1} back · back about ${simHm(s.dep + s.out + 50 + s.back)}</small></span>
    <span class="cr-now"><small>${SIM_DAYS.filter((x, k) => s.days & (1 << k)).join(' ')}</small></span>
    <span class="cr-rules"><i>fare ${Math.round((s.fare || 1) * 100)}%</i>${s.load ? `<i>${s.load}% full</i><i>${money(s.profit)} a leg</i>` : ''}</span>
    <span class="cr-btns"><button class="btn sm ghost" data-act="sim-fare" data-id="${esc(s.route)}" data-v="${Math.max(0.5, (s.fare || 1) - 0.1).toFixed(2)}">Cheaper</button><button class="btn sm ghost" data-act="sim-fare" data-id="${esc(s.route)}" data-v="${Math.min(2, (s.fare || 1) + 0.1).toFixed(2)}">Dearer</button><button class="btn sm danger" data-act="sim-slotdel" data-id="${esc(s.id)}">Take out</button></span></div>`).join('');
  const flights = (d.flights || []).slice().sort((a: any, b: any) => a.at - b.at).slice(0, 30).map((f: any, i: number) => `<div class="crew-row" style="--i:${i}"><span class="cr-name"><b>${esc(f.time)} ${esc(f.flight)}</b><small>${esc(f.from)} to ${esc(f.town)} · ${esc(f.reg)}${f.gate ? ' · gate ' + f.gate : ''}</small></span>
    <span class="cr-now">${f.state === 'planned' ? `<small class="chip ${f.late >= 15 ? 'amber' : 'ok'}">${f.late >= 15 ? f.late + ' min late' : 'on time'}</small>` : `<small class="chip ${f.state === 'cancelled' ? 'bad' : 'ok'}">${f.state === 'gone' ? 'left' : esc(f.state)}</small>`}</span>
    <span class="cr-rules"><i>${f.booked} of ${f.seats} seats booked</i></span><span class="cr-btns">${f.why ? `<small>${esc(f.why)}</small>` : ''}</span></div>`).join('');
  return `<div class="pilot-hero"><div class="ph-main"><h1>Timetable</h1><p class="sub">${(d.slots || []).length} lines · the airport's time ${esc(d.now || '')}</p></div></div>
    ${d.missing ? `<p class="fine-print warn">${icon('alert')}Nobody to do the work right now: ${esc(d.missing)}. Your flights wait until someone is on shift - hire them under Crew.</p><div class="filter-row">${[['ground', 'Ground crew'], ['checkin', 'Check-in and boarding'], ['security', 'Security']].map(x => `<button class="btn sm" data-act="crew-area" data-area="${x[0]}">Hire: ${x[1]}</button>`).join('')}</div>` : ''}
    <h4 class="grp-h">A new line</h4>
    ${fl.length ? `<div class="sim-manage">
      <label>Aircraft <select class="sim-tt-reg">${fl.map((a: any) => `<option value="${esc(a.reg)}" ${a.reg === SimTt.reg ? 'selected' : ''}>${esc(a.reg)} (${esc(a.type)})</option>`).join('')}</select></label>
      <label>Find a place <input class="sim-tt-q" type="text" value="${esc(SimTt.q)}" placeholder="name or code"></label><button class="btn sm" data-act="sim-tt-find">Find</button>
      <label>To <select class="sim-tt-to">${(SimTt.places || []).map((x: any) => `<option value="${esc(x.icao)}" ${x.icao === SimTt.to ? 'selected' : ''}>${esc(x.town || x.name)} (${esc(x.iata || x.icao)}) · ${x.pax} travellers a day${x.schengen ? '' : ' · outside Schengen'}</option>`).join('')}</select></label>
      <label>Leaves at <input class="sim-tt-time" type="time" step="300" value="${esc(SimTt.time)}"></label>
      <span class="btn-row">${SIM_DAYS.map((x, k) => `<button class="fchip ${SimTt.days & (1 << k) ? 'on' : ''}" data-act="sim-tt-day" data-k="${k}">${x}</button>`).join('')}</span>
      <button class="btn primary" data-act="sim-tt-add">${icon('plus')}Put it in</button>
    </div>
    <p class="fine-print">${icon('info')}${p ? `${esc(p.name)}: ${num(p.nm)} nm, ${Math.floor(p.block / 60)} h ${p.block % 60} min each way - leaves ${simHm(dep)}, back about ${simHm(dep + 2 * p.block + 50)}. ` : ''}It goes on sale at once; the passengers book it, and come to the airport for it. The first one can leave two and a half hours from now; a day or more on sale fills it best.${SimTt.err ? ' <b>' + esc(SimTt.err) + '</b>' : ''}</p>`
      : emptyState('No aircraft yet', 'Buy or lease your first A320 under Fleet.')}
    <h4 class="grp-h">Your timetable</h4><div class="crew-list">${slots || emptyState('Nothing in it yet', 'Put in your first line above: no flights, no passengers.')}</div>
    <h4 class="grp-h">The flights</h4><div class="crew-list">${flights || emptyState('No flights planned', 'They come three days ahead of their day.')}</div>`;
}

function simTtRead() {
  const g = (c: string) => { const e = document.querySelector('.' + c) as any; return e ? String(e.value || '').trim() : ''; };
  const reg = g('sim-tt-reg'); if (reg && reg !== SimTt.reg) { SimTt.reg = reg; SimTt.asked = ''; }
  if (document.querySelector('.sim-tt-q')) SimTt.q = g('sim-tt-q');
  const to = g('sim-tt-to'); if (to) SimTt.to = to;
  const tm = g('sim-tt-time'); if (tm) SimTt.time = tm;
}
ACTIONS['sim-buy'] = el => simAirDo('buy', (el.getAttribute('data-id') || '') + '|' + (el.getAttribute('data-m') || 'buy'), el.getAttribute('data-m') === 'lease' ? 'Leased' : 'Bought');
ACTIONS['sim-buyused'] = el => simAirDo('buyused', el.getAttribute('data-id') || '', 'Bought');
ACTIONS['sim-sell'] = el => simAirDo('sell', el.getAttribute('data-id') || '', 'Gone from your fleet');
ACTIONS['sim-tt-for'] = el => { SimTt.reg = el.getAttribute('data-id') || ''; SimTt.asked = ''; go('schedule'); };
ACTIONS['sim-tt-day'] = el => { simTtRead(); SimTt.days ^= 1 << Number(el.getAttribute('data-k') || 0); Sound.tick(); refresh(); };
ACTIONS['sim-tt-find'] = () => { simTtRead(); SimTt.asked = ''; simPlaces(); refresh(); };
ACTIONS['sim-tt-add'] = () => { simTtRead(); simAirDo('slot', [SimTt.reg, SimTt.to, SimTt.time, SimTt.days].join('|'), 'In the timetable'); };
ACTIONS['sim-slotdel'] = el => simAirDo('slotdel', el.getAttribute('data-id') || '', 'Taken out');
ACTIONS['sim-fare'] = el => simAirDo('fare', (el.getAttribute('data-id') || '') + '|' + (el.getAttribute('data-v') || '1'), 'Fare changed');
document.addEventListener('change', (e: any) => {
  const c = e.target && e.target.classList; if (!c) return;
  if (c.contains('sim-tt-reg') || c.contains('sim-tt-to') || c.contains('sim-tt-time')) { simTtRead(); if (c.contains('sim-tt-reg')) simPlaces(); e.target.blur(); refresh(); }
});

/** The Fleet, Market, Routes and Schedule screens: the simulation's whenever Unreal runs a career, else the old ones. */
(function simAirHook() {
  const S: any = typeof SCREENS !== 'undefined' ? SCREENS : null;
  if (!S || !S.fleet || !S.market || !S.routes || !S.schedule) { setTimeout(simAirHook, 50); return; }
  if (S.fleet.__sim) return;
  const wrap = (k: string, html: () => string, store: any) => {
    const old = S[k].render;
    S[k].render = function () { if (!simLink() || (store.data === null && store.at && Date.now() - store.at < 4000)) return old.call(this); return html(); };
    S[k].__sim = true;
  };
  wrap('fleet', () => simFleetHtml(false), SimAir); wrap('market', () => simFleetHtml(true), SimAir);
  wrap('routes', simTimetableHtml, SimTt); wrap('schedule', simTimetableHtml, SimTt);
})();
