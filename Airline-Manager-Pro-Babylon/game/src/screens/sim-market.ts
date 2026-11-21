// League, challenges, alliances, shares, fuel hedging and frequent flyers on the simulation (to-do 184-186; owner,
// 2026-11-20 night): the Ranking screen in the PC game while a career runs in Unreal (ue.adp.sim 'market').

const SimMk: any = { data: null as any, at: 0, busy: false };
function simMkLoad(force?: boolean) {
  const U = simLink(); if (!U || SimMk.busy) return;
  if (!force && Date.now() - SimMk.at < 4000) return;
  SimMk.busy = true;
  Promise.resolve(U.sim('market', '')).then((s: any) => {
    SimMk.busy = false; SimMk.at = Date.now();
    try { const d = JSON.parse(String(s || '{}')); SimMk.data = d && d.ok ? d : null; } catch (e) { SimMk.data = null; }
    try { refresh(); } catch (e) { /* not on screen */ }
  }, () => { SimMk.busy = false; });
}
function simMkDo(action: string, arg: string, okMsg: string) {
  const U = simLink(); if (!U) return;
  Promise.resolve(U.sim(action, arg)).then((s: any) => {
    let d: any = {}; try { d = JSON.parse(String(s || '{}')); } catch (e) { /* bad answer */ }
    toast(d.ok ? okMsg : 'Not done: ' + (d.why || ''), d.ok ? 'ok' : 'warn'); simMkLoad(true);
  });
}
function simMkHtml(): string {
  simMkLoad(false);
  const d = SimMk.data;
  if (!d) return `<div class="pilot-hero"><div class="ph-main"><h1>League</h1><p class="sub">Reading the league...</p></div></div>`;
  const ch = (d.challenges || []).map((c: any) => `<div class="crew-row"><span class="cr-name"><b>${esc(c.title)}</b><small>${c.done ? 'Won' : `${num(Math.round(c.progress))} of ${num(c.target)}`} · ${money(c.money)} and ${Math.round(c.xp)} XP</small></span><span class="cr-now">${c.done ? statusChip('done') : ''}</span></div>`).join('');
  const lg = (d.league || []).map((r: any) => `<div class="crew-row ${r.me ? 'mine' : ''}"><span class="cr-name"><b>${r.pos}. ${esc(r.name)}${r.me ? ' (you)' : ''}</b><small>hub ${esc(r.hub || '')} · ${num(r.pax30)} passengers in 30 days · ${r.onTime}% on time · reputation ${r.reputation}</small></span>
      <span class="cr-rules"><i>score ${r.score}</i><i>share $${Number(r.price || 0).toFixed(2)}</i>${r.held ? `<i>you hold ${num(r.held)}</i>` : ''}</span>
      <span class="cr-btns">${r.me ? '' : `<button class="btn sm ghost" data-act="sim-shares" data-id="${esc(r.id)}" data-n="100000">Buy 100k shares</button>${r.held ? `<button class="btn sm ghost" data-act="sim-shares" data-id="${esc(r.id)}" data-n="${-r.held}">Sell all</button>` : ''}`}</span></div>`).join('');
  const al = (d.alliances || []).map((a: any) => `<div class="crew-row"><span class="cr-name"><b>${esc(a.name)}${a.mine ? ' (yours)' : ''}</b><small>${(a.members || []).map((m: string) => esc(m)).join(', ') || 'no members yet'}</small></span><span class="cr-btns">${a.mine ? `<button class="btn sm ghost" data-act="sim-mk" data-a="leaveAlliance" data-v="">Leave</button>` : `<button class="btn sm" data-act="sim-mk" data-a="join" data-v="${esc(a.name)}">Join ($2M)</button>`}</span></div>`).join('');
  return `<div class="pilot-hero"><div class="ph-main"><h1>League and markets</h1><p class="sub">Your share price $${Number(d.sharePrice || 0).toFixed(2)}${d.public ? ' · listed' : ''} · reputation ${Math.round(d.reputation || 0)} · fuel market ${Math.round((d.fuelIndex || 1) * 100)}%</p>
      <span class="btn-row">${d.public ? '' : `<button class="btn sm primary" data-act="sim-mk" data-a="ipo" data-v="">Go public (IPO: sell 30%)</button>`}
      <button class="btn sm" data-act="sim-mk" data-a="ff" data-v="${d.ff ? 'off' : 'on'}">${d.ff ? 'Stop' : 'Start'} the frequent-flyer scheme${d.ff ? ` (${num(d.ffMembers)} members)` : ''}</button>
      ${[100, 500, 2000].map(t => `<button class="btn sm ghost" data-act="sim-mk" data-a="hedge" data-v="${t}|3">Hedge ${t} t fuel, 3 months</button>`).join('')}</span>
      ${d.hedgeTonnes ? `<p class="fine-print">${icon('info')}${num(d.hedgeTonnes)} t of fuel hedged at $${Number(d.hedgePrice).toFixed(3)}/kg until ${esc(d.hedgeUntil || '')}.</p>` : ''}</div></div>
    <h4 class="grp-h">This week's challenges</h4><div class="crew-list">${ch || emptyState('No challenges yet', 'New ones come every Monday.')}</div>
    <h4 class="grp-h">The league</h4><div class="crew-list">${lg}</div>
    <h4 class="grp-h">Alliances</h4><div class="crew-list">${al}</div>
    <div class="btn-row"><button class="btn sm" data-act="sim-found">Found your own alliance (reputation 65+)</button></div>
    ${simTownHtml(d)}`;
}
/** Your airport and the town (to-do 197 / 199): fire cover, the night curfew, the town's mood and the ways to change it. */
function simTownHtml(d: any): string {
  const hm = (h: number) => { const H = Math.floor(h), M = Math.round((h - H) * 60); return (H < 10 ? '0' : '') + H + ':' + (M < 10 ? '0' : '') + M; };
  const fireOk = (d.fireCat || 0) >= (d.fireNeed || 0);
  const curfew = d.curfewFrom === d.curfewTo ? 'no night curfew' : `closed ${hm(d.curfewFrom)}-${hm(d.curfewTo)} local`;
  return `<h4 class="grp-h">Your airport and the town</h4>
    <div class="need-row"><span class="need ${fireOk ? 'ok' : 'short'}">${icon(fireOk ? 'check' : 'alert')}<span><b>Fire cover: category ${d.fireCat || 0}</b><small>your largest aircraft needs ${d.fireNeed || 0} · ${d.fireTrucks || 0} crash tenders</small></span></span>
      <button class="btn sm" data-act="sim-mk" data-a="firetruck" data-v="">Buy a crash tender ($850,000)</button>
      <p class="fine-print">${icon('info')}Real-world rule (ICAO): an airport may only take aircraft its fire station covers - category 6 an A320, 7 an A321, 9 a 777, 10 an A380 - with enough fire-fighters on duty (hire them under Crew) and trucks.</p></div>
    <div class="crew-row"><span class="cr-name"><b>Night curfew: ${esc(curfew)}</b><small>the town's mood ${d.townMood || 0} / 100 · ${d.complaints || 0} noise complaints this month · ${d.insulated || 0} homes insulated · syndic ${esc(d.syndic || '')}${d.scandal ? ' · after the scandal' : ''}</small></span></div>
    <div class="btn-row">
      <button class="btn sm" data-act="sim-mk" data-a="fund" data-v="250000">Community fund ($250,000)</button>
      <button class="btn sm" data-act="sim-mk" data-a="insulate" data-v="50">Insulate 50 homes ($900,000)</button>
      <button class="btn sm" data-act="sim-mk" data-a="compensate" data-v="100000">Compensate residents ($100,000)</button>
      <button class="btn sm" data-act="sim-mk" data-a="lobby" data-v="" ${d.lobbying ? 'disabled' : ''}>${d.lobbying ? 'Lobbying the council...' : 'Lobby the council ($250,000, 3 weeks)'}</button>
      <button class="btn sm danger" data-act="sim-mk" data-a="bribe" data-v="300000">Slip the syndic $300,000</button></div>
    <p class="fine-print">${icon('info')}The legal ways are slow and safe. A bribe works at once - but a journalist or an audit may find it any week: fines, a court case, reputation lost and a harder curfew.</p>`;
}
ACTIONS['sim-mk'] = el => simMkDo(el.getAttribute('data-a') || '', el.getAttribute('data-v') || '', 'Done');
ACTIONS['sim-shares'] = el => simMkDo('shares', (el.getAttribute('data-id') || '') + '|' + (el.getAttribute('data-n') || '0'), 'Shares traded');
ACTIONS['sim-found'] = () => { const n = window.prompt ? window.prompt('Your alliance\'s name') : ''; if (n) simMkDo('found', n, 'Alliance founded'); };
(function simMkHook() {
  const S: any = typeof SCREENS !== 'undefined' ? SCREENS : null;
  if (!S || !S.ranking) { setTimeout(simMkHook, 50); return; }
  if (S.ranking.__sim) return;
  const old = S.ranking.render;
  S.ranking.render = function () { if (!simLink() || (SimMk.data === null && SimMk.at && Date.now() - SimMk.at < 4000)) return old.call(this); return simMkHtml(); };
  S.ranking.__sim = true;
})();
