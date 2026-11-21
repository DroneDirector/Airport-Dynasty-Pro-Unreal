// The airport you are at (to-do 167; owner, 2026-11-20 night - very high priority): in the PC game at another site (Chablais
// Airport, Switzerland) Unreal tells the page which airport it is (?site=<id>&apt=ICAO|IATA|name|town|country|lon|lat|
// elevFt|hdg|magvar, GameView.cpp). Before anything reads them, your own airport's entry and its layout become that
// airport's: the same airfield (the same plan), placed at the site and turned to its runway heading. Iceland's stays as it
// is when no site is given (Android, the web, the default site).
(function ownSiteInit() {
  let q = '';
  try { q = (window.top && window.top.location && window.top.location.search) || ''; } catch (e) { /* another origin */ }
  if (!q || q.indexOf('apt=') < 0) { try { q = location.search || ''; } catch (e) { q = ''; } }
  let apt = '';
  try { apt = new URLSearchParams(q).get('apt') || ''; } catch (e) { apt = ''; }
  const f = apt.split('|');
  if (f.length < 9 || !f[0] || f[0] === 'BIAP') return;
  const [icao, iata, name, town, country] = f;
  const lon = +f[5], lat = +f[6], elev = +f[7], hdg = +f[8], magvar = +(f[9] || 0);
  const base = OWN_AIRPORT_LAYOUTS.BIAP, row = OWN_AIRPORT_LIST[0];
  if (!base || !row) return;
  // the plan turned from Iceland's runway heading to this one (x east, y north, metres from the runway's middle)
  const d = (hdg - base.runways[0].hdg) * Math.PI / 180, c = Math.cos(-d), s = Math.sin(-d);
  const rot = (p: number[]) => { const x = p[0], y = p[1]; return [x * c - y * s, x * s + y * c]; };
  const L: any = JSON.parse(JSON.stringify(base));
  L.icao = icao; L.name = name; L.lat = lat; L.lon = lon; L.magvar = -magvar; L.iata = iata;
  L.runways = L.runways.map((r: any) => Object.assign(r, { lat, lon, hdg }));
  // runway names from the magnetic heading (336 true, 3 E -> 33 / 15)
  const mag = Math.round(((hdg - magvar) % 360 + 360) % 360 / 10) || 36, opp = (mag + 18 - 1) % 36 + 1;
  const nm = (n: number) => (n < 10 ? '0' : '') + n;
  L.runways[0].a = nm(Math.min(mag, opp)); L.runways[0].b = nm(Math.max(mag, opp));
  L.points = L.points.map((p: number[]) => rot(p).concat(p.slice(2)));
  L.parks = L.parks.map((p: number[]) => { const r = rot(p); return [r[0], r[1], ((p[2] + hdg - base.runways[0].hdg) % 360 + 360) % 360].concat(p.slice(3)); });
  L.areas = L.areas.map((a: any) => Object.assign(a, { pts: a.pts.map(rot) }));
  L.holds = L.holds.map((h: number[]) => { const r = rot(h); return [r[0], r[1], ((h[2] + hdg - base.runways[0].hdg) % 360 + 360) % 360].concat(h.slice(3)); });
  L.labels = L.labels.map((l: any[]) => { const r = rot(l as number[]); return [r[0], r[1]].concat(l.slice(2)); });
  OWN_AIRPORT_LAYOUTS[icao] = L;
  // the runway's ends: half its length each way along the true heading
  const lenM = row[8].lenFt * 0.3048, k = lenM / 2 / 111320, h = hdg * Math.PI / 180;
  const e1 = [lat - k * Math.cos(h), lon - k * Math.sin(h) / Math.cos(lat * Math.PI / 180)], e2 = [lat + k * Math.cos(h), lon + k * Math.sin(h) / Math.cos(lat * Math.PI / 180)];
  OWN_AIRPORT_LIST[0] = [icao, iata, name, town || name, country, lon, lat, elev, Object.assign({}, row[8], { id1: L.runways[0].a, id2: L.runways[0].b, e1, e2, hdg })];
  (window as any).__ownSite = { icao, iata, name, town, country, lon, lat, elev, hdg };
})();
/** The airport you play at (its ICAO): the site's own on the PC, else Mýrdalssandur. */
function ownHomeIcao(): string { try { return OWN_AIRPORT_LIST[0][0]; } catch (e) { return 'BIAP'; } }
/** Its name ("Chablais Airport", "Mýrdalssandur"). */
function ownHomeName(): string { try { return OWN_AIRPORT_LIST[0][2]; } catch (e) { return 'Mýrdalssandur'; } }
