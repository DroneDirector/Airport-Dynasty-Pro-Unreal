// Air traffic control for the hired and AI flights: every flight talks to the airports it uses, the way real crews
// do. Each step of a flight brings its radio calls with the right unit of the right airport: Delivery (the route
// clearance with its departure), Ground (push and taxi to the runway in use), Tower (lining up, the take-off and
// the landing clearance with the wind), Departure, the area control centre en route, Approach (the arrival and the
// approach), and Ground again to the gate. Your own airport, Mýrdalssandur (BIAP), has its own tower and units
// like any other ("Mýrdalssandur Tower"); its 3D tower building comes later, in airport mode on the PC.
// The calls are kept per airport (the newest 60), shown under Radio in the airport's view, and a flight never
// gets a take-off or landing clearance while another is cleared on the same runway (it holds instead).

interface AtcLine { at: number; who: 'atc' | 'pilot'; unit: string; cs: string; text: string }
const ATC_LOG: { [icao: string]: AtcLine[] } = {};
const ATC_RWY: { [key: string]: { cs: string; until: number } } = {};

// #region ── The airport's units and the words ──
function atcName(icao: string): string { const A: any = apt(icao); const n = A ? String(A.city || A.name || icao) : icao; return n.replace(/\s+(International|Airport|Intl\.?)$/i, ''); }
function atcUnit(icao: string, u: 'Delivery' | 'Ground' | 'Tower' | 'Departure' | 'Approach' | 'Control'): string { return `${atcName(icao)} ${u}`; }
function atcCs(f: any): string { const cs = String(f.callsign || f.reg || ''); return cs; }
function atcWind(icao: string): string {
  try { const A: any = apt(icao), w = airportWind(icao, A.pos); return w && w.kt > 0 ? `wind ${String(Math.round(w.dir / 10) * 10 || 360).padStart(3, '0')} degrees, ${Math.round(w.kt)} knots` : 'wind calm'; } catch (e) { return 'wind calm'; }
}
function atcRwy(f: any, dep: boolean): string {
  try { const r = routeRunways(f.from, f.to), e = dep ? r.dep : r.arr; return e ? e.name : '' ; } catch (e) { return ''; }
}
function atcProcs(f: any): { sid: string; star: string; app: string } {
  try { const p: any = flightPathFor(f.from, f.to), name = (k: string) => { const x = (p.parts || []).filter((q: any) => q.kind === k)[0]; return x ? x.name : ''; };
    return { sid: name('sid'), star: name('star'), app: name('app') }; } catch (e) { return { sid: '', star: '', app: '' }; }
}
function atcSay(icao: string, unit: string, cs: string, who: 'atc' | 'pilot', text: string) {
  const L = ATC_LOG[icao] || (ATC_LOG[icao] = []); L.unshift({ at: Date.now(), who, unit, cs, text }); if (L.length > 60) L.length = 60;
}
// #endregion

// #region ── One runway, one clearance at a time ──
/** May this flight take off or land now? Another flight cleared on the same runway in the last 90 seconds holds it. */
function atcRunwayFree(icao: string, rwy: string, cs: string): boolean {
  const k = icao + '/' + rwy, r = ATC_RWY[k], now = Date.now();
  if (r && r.cs !== cs && r.until > now) return false;
  ATC_RWY[k] = { cs, until: now + 90e3 / Math.max(1, (typeof aiSpeed === 'function' ? aiSpeed() : 1)) }; return true;
}
// #endregion

// #region ── The calls at each step of a flight ──
/** A flight moved from one step to the next: the radio calls that go with it. */
function atcStep(f: any, was: string, now: string) {
  if (!f || !now || was === now) return;
  const cs = atcCs(f), dep = f.from, arr = f.to, P = atcProcs(f), rd = atcRwy(f, true), ra = atcRwy(f, false), lvl = f.alt >= 18000 ? 'flight level ' + Math.round(f.alt / 100) : Math.round(f.alt / 100) * 100 + ' feet';
  const T = (icao: string, u: any, who: 'atc' | 'pilot', text: string) => atcSay(icao, atcUnit(icao, u), cs, who, text);
  if (/Taxi out/i.test(now)) {
    T(dep, 'Delivery', 'pilot', `${atcUnit(dep, 'Delivery')}, ${cs}, at the stand, request clearance to ${atcName(arr)}.`);
    T(dep, 'Delivery', 'atc', `${cs}, cleared to ${atcName(arr)}${P.sid ? `, ${P.sid} departure` : ''}${rd ? `, runway ${rd}` : ''}, climb 5,000 feet, squawk ${String(1000 + (_hashStr(cs) % 6000)).slice(0, 4)}.`);
    T(dep, 'Ground', 'pilot', `${atcUnit(dep, 'Ground')}, ${cs}, request push and start.`);
    T(dep, 'Ground', 'atc', `${cs}, push and start approved, taxi to holding point runway ${rd || 'in use'}.`);
  } else if (/Takeoff/i.test(now)) {
    T(dep, 'Tower', 'pilot', `${atcUnit(dep, 'Tower')}, ${cs}, holding point runway ${rd}, ready for departure.`);
    if (!atcRunwayFree(dep, rd, cs)) T(dep, 'Tower', 'atc', `${cs}, hold position, traffic on the runway.`);
    T(dep, 'Tower', 'atc', `${cs}, ${atcWind(dep)}, runway ${rd}, cleared for take-off.`);
  } else if (/Climb/i.test(now)) {
    T(dep, 'Departure', 'pilot', `${atcUnit(dep, 'Departure')}, ${cs}, passing ${lvl}${P.sid ? `, ${P.sid}` : ''}.`);
    T(dep, 'Departure', 'atc', `${cs}, identified, climb flight level 240.`);
  } else if (/Cruise/i.test(now)) {
    const ctr = atcName(arr) + ' Control';
    atcSay(arr, ctr, cs, 'pilot', `${ctr}, ${cs}, ${lvl}.`); atcSay(arr, ctr, cs, 'atc', `${cs}, ${ctr}, identified.`);
  } else if (/Descent/i.test(now)) {
    T(arr, 'Approach', 'pilot', `${atcUnit(arr, 'Approach')}, ${cs}, descending, information received.`);
    T(arr, 'Approach', 'atc', `${cs}, ${P.star ? `${P.star} arrival, ` : ''}descend to 5,000 feet, expect ${P.app ? `${/^I/.test(P.app) ? 'ILS' : 'RNP'} approach ` : ''}runway ${ra}.`);
  } else if (/Approach/i.test(now)) {
    T(arr, 'Approach', 'atc', `${cs}, cleared ${P.app ? `${/^I/.test(P.app) ? 'ILS' : 'RNP'} approach ` : 'approach '}runway ${ra}, report established.`);
  } else if (/Final/i.test(now)) {
    T(arr, 'Tower', 'pilot', `${atcUnit(arr, 'Tower')}, ${cs}, established runway ${ra}.`);
    if (!atcRunwayFree(arr, ra, cs)) T(arr, 'Tower', 'atc', `${cs}, continue approach, one departing ahead.`);
    T(arr, 'Tower', 'atc', `${cs}, ${atcWind(arr)}, runway ${ra}, cleared to land.`);
  } else if (/Taxi in/i.test(now)) {
    T(arr, 'Ground', 'pilot', `${atcUnit(arr, 'Ground')}, ${cs}, runway vacated.`);
    T(arr, 'Ground', 'atc', `${cs}, taxi to the stand, welcome to ${atcName(arr)}.`);
  }
}
/** The newest calls at an airport (for the airport's Radio panel). */
function atcLog(icao: string, n = 30): AtcLine[] { return (ATC_LOG[String(icao || '').toUpperCase()] || []).slice(0, n); }
// #endregion
