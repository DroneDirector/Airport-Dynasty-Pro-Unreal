// The new Overview (to-do 188; owner, 2026-11-20 night: "very nice, dynamic and responsive; every feature working on the
// simulation"): in the PC game while a career runs in Unreal's simulation, the Overview is one live dashboard - your
// airline's money, share price, league place and punctuality; your aircraft in the air with their progress; the airport's
// departures and arrivals; the fleet and its checks; the crew on duty, resting and off; the airport's real weather, curfew
// and fire cover; the newest messages, news and this week's challenges. Cards reflow from one column (phone) to four
// (a wide screen) and refresh every 3 seconds; each card opens its full screen.

const SimOv: any = { data: null as any, at: 0, busy: false };
function simOvLoad(force?: boolean) {
  const U = simLink(); if (!U || SimOv.busy) return;
  if (!force && Date.now() - SimOv.at < 3000) return;
  SimOv.busy = true;
  Promise.resolve(U.sim('overview', '')).then((s: any) => {
    SimOv.busy = false; SimOv.at = Date.now();
    try { const d = JSON.parse(String(s || '{}')); SimOv.data = d && d.ok ? d : null; } catch (e) { SimOv.data = null; }
    try { if ((State as any).screen === 'ops' || document.querySelector('.ov2')) refresh(); } catch (e) { /* not on screen */ }
  }, () => { SimOv.busy = false; });
}
function ovStyle() {
  if (document.getElementById('ov2-style')) return;
  const st = document.createElement('style'); st.id = 'ov2-style';
  st.textContent = `
  .ov2 { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 14px; padding: 4px 2px 24px; }
  .ov2-card { position: relative; border-radius: 16px; padding: 14px 16px; background: linear-gradient(160deg, rgba(14, 34, 60, .86), rgba(8, 18, 34, .9));
    border: 1px solid rgba(90, 200, 255, .18); box-shadow: 0 0 0 1px rgba(255,255,255,.02) inset, 0 10px 30px rgba(0,0,0,.35); overflow: hidden; cursor: pointer;
    transition: transform .18s ease, border-color .18s ease, box-shadow .18s ease; animation: ov2in .45s ease both; }
  .ov2-card:hover { transform: translateY(-2px); border-color: rgba(90, 220, 255, .55); box-shadow: 0 0 22px rgba(60, 200, 255, .18), 0 10px 30px rgba(0,0,0,.4); }
  .ov2-card.wide { grid-column: span 2; } @media (max-width: 680px) { .ov2-card.wide { grid-column: span 1; } }
  .ov2-card h3 { margin: 0 0 10px; font-size: 12px; letter-spacing: .14em; text-transform: uppercase; color: #7fd8ff; font-weight: 700; display: flex; justify-content: space-between; }
  .ov2-card h3 small { color: #9fb6cc; letter-spacing: 0; text-transform: none; font-weight: 500; }
  .ov2-big { font-size: 30px; font-weight: 800; color: #eaf6ff; line-height: 1.05; text-shadow: 0 0 18px rgba(80, 200, 255, .25); }
  .ov2-row { display: flex; justify-content: space-between; gap: 10px; padding: 5px 0; border-bottom: 1px solid rgba(255,255,255,.05); font-size: 13px; color: #d7e7f5; }
  .ov2-row:last-child { border-bottom: 0; } .ov2-row b { color: #fff; } .ov2-row .dim { color: #8ea6bd; }
  .ov2-kpis { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }
  .ov2-kpi small { display: block; color: #8ea6bd; font-size: 11px; text-transform: uppercase; letter-spacing: .08em; } .ov2-kpi b { font-size: 20px; color: #fff; }
  .ov2-bar { height: 6px; border-radius: 3px; background: rgba(255,255,255,.08); overflow: hidden; margin-top: 4px; }
  .ov2-bar i { display: block; height: 100%; background: linear-gradient(90deg, #22d3ee, #a78bfa); border-radius: 3px; transition: width 1s ease; }
  .ov2-st { font-weight: 700; } .ov2-st.ok { color: #3fd0a0; } .ov2-st.warn { color: #f2b544; } .ov2-st.bad { color: #f05a5a; }
  .ov2-msg { padding: 6px 0; border-bottom: 1px solid rgba(255,255,255,.05); font-size: 12.5px; color: #cfe0ef; } .ov2-msg b { color: #fff; } .ov2-msg.p2 b { color: #ff8a8a; } .ov2-msg.p1 b { color: #ffd27a; }
  .ov2-msg small { color: #7f97ad; margin-left: 6px; }
  .ov2-metar { font-family: ui-monospace, Consolas, monospace; font-size: 11px; color: #8ec9e8; margin-top: 8px; word-break: break-all; }
  .ov2-glow { position: absolute; inset: -40% -20% auto auto; width: 180px; height: 180px; background: radial-gradient(circle, rgba(34, 211, 238, .16), transparent 70%); pointer-events: none; }
  @keyframes ov2in { from { opacity: 0; transform: translateY(8px); } to { opacity: 1; transform: none; } }
  `;
  document.head.appendChild(st);
}
function ovCard(title: string, sub: string, body: string, go: string, i: number, wide?: boolean): string {
  return `<div class="ov2-card ${wide ? 'wide' : ''}" style="animation-delay:${i * 40}ms" data-act="go" data-screen="${go}"><span class="ov2-glow"></span><h3>${esc(title)}<small>${sub}</small></h3>${body}</div>`;
}
function simOverviewHtml(): string {
  ovStyle(); simOvLoad(false);
  const d = SimOv.data;
  if (!d) return `<div class="pilot-hero"><div class="ph-main"><h1>Overview</h1><p class="sub">Reading your airline...</p></div></div>`;
  const st = (s: string) => { const l = String(s).toLowerCase(); return `<span class="ov2-st ${/delay|divert|cancel|closed|grounded/.test(l) ? 'warn' : /board|final|landed|on time|departed/.test(l) ? 'ok' : ''}">${esc(s)}</span>`; };
  const wx = d.weather || {}; const hm = (h: number) => { const H = Math.floor(h), M = Math.round((h - H) * 60); return (H < 10 ? '0' : '') + H + ':' + (M < 10 ? '0' : '') + M; };
  let i = 0;
  const cards: string[] = [];
  cards.push(ovCard(d.airline || 'Your airline', `${esc(d.airport || '')} · ${esc(d.localTime || '')}`, `<div class="ov2-big">${money(d.balance || 0)}</div>
    <div class="ov2-kpis" style="margin-top:12px"><div class="ov2-kpi"><small>Share price</small><b>$${Number(d.sharePrice || 0).toFixed(2)}</b></div><div class="ov2-kpi"><small>League</small><b>${d.leaguePos || '-'} / ${d.leagueOf || '-'}</b></div>
    <div class="ov2-kpi"><small>On time</small><b>${d.onTime || 0}%</b><div class="ov2-bar"><i style="width:${d.onTime || 0}%"></i></div></div><div class="ov2-kpi"><small>Reputation</small><b>${d.reputation || 0}</b><div class="ov2-bar"><i style="width:${d.reputation || 0}%"></i></div></div>
    <div class="ov2-kpi"><small>Passengers, 30 days</small><b>${num(d.pax30 || 0)}</b></div><div class="ov2-kpi"><small>Profit, 30 days</small><b style="color:${(d.profit30 || 0) >= 0 ? '#3fd0a0' : '#f05a5a'}">${money(d.profit30 || 0)}</b></div></div>`, 'finance', i++, true));
  cards.push(ovCard('In the air', `${(d.inAir || []).length} flights`, (d.inAir || []).map((f: any) => `<div class="ov2-row"><span><b>${esc(f.reg)}</b> ${esc(f.from)} → ${esc(f.to)}<br><span class="dim">${esc(f.phase)} · ${num(f.alt)} ft · ${f.gs} kt · ${f.pax} on board</span><div class="ov2-bar"><i style="width:${Math.round(f.progress * 100)}%"></i></div></span><span class="dim">ETA ${esc(f.eta)}</span></div>`).join('') || '<div class="ov2-row dim">Nothing in the air right now.</div>', 'map', i++));
  const board = (L: any[], empty: string) => L.map((r: any) => `<div class="ov2-row"><span><b>${esc(r.time)}</b> ${esc(r.flight)} <span class="dim">${esc(r.place)}</span></span><span>${st(r.status)}${r.gate ? ` <span class="dim">gate ${r.gate}</span>` : ''}</span></div>`).join('') || `<div class="ov2-row dim">${empty}</div>`;
  cards.push(ovCard('Departures', esc(d.icao || ''), board(d.departures || [], 'No departures scheduled.'), 'schedule', i++));
  cards.push(ovCard('Arrivals', esc(d.icao || ''), board(d.arrivals || [], 'No arrivals expected.'), 'schedule', i++));
  cards.push(ovCard('Fleet', `${d.fleet || 0} aircraft`, `<div class="ov2-kpis"><div class="ov2-kpi"><small>Flying</small><b>${d.flying || 0}</b></div><div class="ov2-kpi"><small>On the ground</small><b>${d.parked || 0}</b></div><div class="ov2-kpi"><small>In a check</small><b>${d.inCheck || 0}</b></div><div class="ov2-kpi"><small>Grounded</small><b style="color:${d.grounded ? '#f05a5a' : '#fff'}">${d.grounded || 0}</b></div></div>
    ${(d.checks || []).slice(0, 4).map((c: any) => `<div class="ov2-row"><span><b>${esc(c.reg)}</b> <span class="dim">condition ${c.condition}%</span></span><span class="ov2-st ${c.due >= 110 ? 'bad' : c.due >= 90 ? 'warn' : 'ok'}">${esc(c.check || '-')}-check ${c.due}%</span></div>`).join('')}`, 'maint', i++));
  cards.push(ovCard('Crew', `${d.staff || 0} people`, `<div class="ov2-kpis"><div class="ov2-kpi"><small>On duty</small><b>${d.onDuty || 0}</b></div><div class="ov2-kpi"><small>Resting</small><b>${d.resting || 0}</b></div><div class="ov2-kpi"><small>Days off</small><b>${d.daysOff || 0}</b></div><div class="ov2-kpi"><small>Sick / on leave</small><b>${(d.sick || 0) + (d.leave || 0)}</b></div></div><p class="dim" style="margin:8px 0 0;font-size:11px;color:#8ea6bd">Rostered on the EASA flight time limitations.</p>`, 'pilots', i++));
  cards.push(ovCard('The airport', `${esc(d.airport || '')}`, `<div class="ov2-kpis"><div class="ov2-kpi"><small>Wind</small><b>${String(Math.round(wx.windDir || 0)).padStart(3, '0')}° ${Math.round(wx.windKt || 0)} kt${wx.gustKt > wx.windKt ? ` G${Math.round(wx.gustKt)}` : ''}</b></div><div class="ov2-kpi"><small>Temperature</small><b>${Math.round(wx.tempC || 0)} °C</b></div>
    <div class="ov2-kpi"><small>Night curfew</small><b>${d.curfewFrom === d.curfewTo ? 'none' : hm(d.curfewFrom) + '-' + hm(d.curfewTo)}</b></div><div class="ov2-kpi"><small>Fire cover</small><b>category ${d.fireCat || 0}</b></div></div>
    <div class="ov2-metar">${wx.live ? 'LIVE ' : 'CLIMATE '}${esc(wx.station || '')} ${esc(wx.metar || '')}</div>`, 'ranking', i++));
  cards.push(ovCard('Messages', '', (d.notices || []).map((n: any) => `<div class="ov2-msg p${n.prio}"><b>${esc(n.title || n.kind)}</b><small>${esc(n.at)}</small><br>${esc(n.text)}</div>`).join('') || '<div class="ov2-msg">Nothing new.</div>', 'ops', i++, true));
  cards.push(ovCard('This week', 'challenges', (d.challenges || []).map((c: any) => `<div class="ov2-row"><span>${esc(c.title)}<div class="ov2-bar"><i style="width:${Math.min(100, Math.round(100 * (c.progress || 0) / Math.max(1, c.target)))}%"></i></div></span><span class="ov2-st ${c.done ? 'ok' : ''}">${c.done ? 'Won' : ''}</span></div>`).join('') || '<div class="ov2-row dim">New challenges every Monday.</div>', 'ranking', i++));
  cards.push(ovCard('News', '', (d.news || []).map((n: string) => `<div class="ov2-msg">${esc(n)}</div>`).join('') || '<div class="ov2-msg">No news yet.</div>', 'ranking', i++));
  return `<div class="ov2">${cards.join('')}</div>`;
}
(function simOvHook() {
  const S: any = typeof SCREENS !== 'undefined' ? SCREENS : null;
  if (!S || !S.ops) { setTimeout(simOvHook, 50); return; }
  if (S.ops.__sim) return;
  const old = S.ops.render;
  S.ops.render = function () { if (!simLink() || (SimOv.data === null && SimOv.at && Date.now() - SimOv.at < 3000)) return old.call(this); return simOverviewHtml(); };
  S.ops.__sim = true;
  // (live: every 3 seconds while the Overview is on screen)
  setInterval(() => { try { if ((State as any).screen === 'ops' && simLink()) simOvLoad(false); } catch (e) { /* */ } }, 3000);
})();
