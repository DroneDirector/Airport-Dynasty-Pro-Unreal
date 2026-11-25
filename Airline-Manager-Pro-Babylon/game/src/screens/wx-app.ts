// The Weather app (owner, 2026-11-18, to-do 124; in step with the game since 2026-11-25): on your phone, your tablet and the
// computers. Now: the weather of the game itself at the airport you work at - the same report the sky, the ground and the
// crew go by. The hours and days to come: MET Norway's forecast. Both from the game (Sim/ADPWeather, .../phone), nothing is
// asked of another site here. And what it means for the work at the airport: the head- and crosswind on each runway end,
// gusts, visibility, freezing and de-icing. (Credits: Settings.)
const WX_SITE: any = (() => { const o: any = (window as any).__ownSite; if (o) return { name: o.name, icao: o.icao, lat: o.lat, lon: o.lon, rwy: [{ id: OWN_AIRPORT_LIST[0][8].id1, hdg: o.hdg < 180 ? o.hdg : o.hdg - 180 }, { id: OWN_AIRPORT_LIST[0][8].id2, hdg: o.hdg < 180 ? o.hdg + 180 : o.hdg }] }; return { name: 'Mýrdalssandur', icao: 'BIAP', lat: 63.457988, lon: -18.713703, rwy: [{ id: '08', hdg: 68 }, { id: '26', hdg: 248 }] }; })();   // (Data/airfield.json; true headings)
const WxA: any = { data: null as any, at: 0, busy: false, err: '', tries: 0 };

function wxAppLoad(force?: boolean) {
  if (WxA.busy || (!force && WxA.data && Date.now() - WxA.at < 55000)) return;
  WxA.busy = true; WxA.err = '';
  let again = 60000;
  Net.json('https://aviationweather.gov/phone', { timeout: 8000 })
    .then((d: any) => {
      // (the first answer is the one made for the last time the page was open: the next is today's)
      if (Math.abs(Date.now() / 1000 - d.now.utc) > 90 && WxA.tries < 5) { ++WxA.tries; again = 700; if (!WxA.data) return; }
      else WxA.tries = 0;
      WxA.data = d; WxA.at = Date.now();
      if (d.now.busy) { again = 2500; WxA.at = 0; }
    })
    .catch((e: any) => { if (!WxA.data && ++WxA.tries <= 8) again = 800; else { WxA.err = String(e && e.message || e); WxA.tries = 0; } })
    .then(() => {
      WxA.busy = false;
      const open = () => { try { return Lap.root && Lap.app === 'weather'; } catch (e) { return false; } };
      if (!open()) return;
      if (WxA.data || WxA.err) try { lapRender(true); } catch (e) { /* closed */ }
      if (!WxA.err) setTimeout(() => { if (open()) wxAppLoad(true); }, again);
    });
}

/** MET Norway's sign for an hour or a day (clearsky_day, lightrainshowersandthunder_night ...) in words, and the sign drawn for it. */
function wxWords(code: string, day: boolean): [string, string] {
  let c = String(code || '').replace(/_(day|night|polartwilight)$/, '');
  if (/andthunder$/.test(c)) return ['Thunderstorm', 'alert'];
  if (c === 'clearsky') return [day ? 'Sunny' : 'Clear', day ? 'sun' : 'star'];
  if (c === 'fair') return [day ? 'Mostly sunny' : 'Mostly clear', day ? 'sun' : 'star'];
  if (c === 'partlycloudy') return ['Partly cloudy', 'cloud'];
  if (c === 'cloudy') return ['Overcast', 'cloud'];
  if (c === 'fog') return ['Fog', 'cloud'];
  const m = /^(light|heavy)?s?(rain|leet|sleet|now|snow)(showers)?$/.exec(c);
  if (!m) return ['-', 'cloud'];
  const w = /rain/.test(m[2]) ? 'rain' : /leet/.test(m[2]) ? 'sleet' : 'snow';
  const t = (m[1] ? m[1] + ' ' : '') + w + (m[3] ? ' showers' : '');
  return [t.charAt(0).toUpperCase() + t.slice(1), 'cloud'];
}
/** The game's own sky now (its eight skies: clear, few clouds, broken, overcast, fog, rain, storm, snow), and the report's freezing. */
function wxNowWords(n: any, day: boolean): [string, string] {
  switch (+n.sky) {
    case 0: return [day ? 'Sunny' : 'Clear', day ? 'sun' : 'star'];
    case 1: return [day ? 'Mostly sunny' : 'Mostly clear', day ? 'sun' : 'star'];
    case 2: return ['Mostly cloudy', 'cloud'];
    case 4: return ['Fog', 'cloud'];
    case 5: return n.freezing ? ['Freezing rain', 'alert'] : ['Rain', 'cloud'];
    case 6: return ['Thunderstorm', 'alert'];
    case 7: return ['Snow', 'cloud'];
  }
  return ['Overcast', 'cloud'];
}
function wxCompass(deg: number): string { const P = ['N', 'NNE', 'NE', 'ENE', 'E', 'ESE', 'SE', 'SSE', 'S', 'SSW', 'SW', 'WSW', 'W', 'WNW', 'NW', 'NNW']; return P[Math.round(((deg % 360) + 360) % 360 / 22.5) % 16]; }
/** An arrow pointing the way the wind blows TO (it comes from deg). */
function wxArrow(deg: number, px = 18): string { return `<svg viewBox="0 0 24 24" width="${px}" height="${px}" style="transform:rotate(${deg + 180}deg)" aria-hidden="true"><path d="M12 3 L18 15 L12 12 L6 15 Z" fill="currentColor"/></svg>`; }
function wxKt(ms: number): number { return Math.round(ms * 1.94384); }
/** Sunrise and sunset (unix s) of the day around a moment at a place, or null where it doesn't rise or set that day. */
function wxSun(utc: number, lat: number, lon: number): [number, number] | null {
  const R = Math.PI / 180, n = Math.round(utc / 86400 + 2440587.5 - 2451545 + lon / 360 - 0.0008);
  const J = n - lon / 360 + 0.0008, M = (357.5291 + 0.98560028 * J) % 360, C = 1.9148 * Math.sin(M * R) + 0.02 * Math.sin(2 * M * R) + 0.0003 * Math.sin(3 * M * R);
  const L = (M + C + 282.9372) % 360, noon = J + 0.0053 * Math.sin(M * R) - 0.0069 * Math.sin(2 * L * R), dec = Math.asin(Math.sin(L * R) * Math.sin(23.44 * R));
  const w = (Math.sin(-0.833 * R) - Math.sin(lat * R) * Math.sin(dec)) / (Math.cos(lat * R) * Math.cos(dec));
  if (w < -1 || w > 1) return null;
  const a = Math.acos(w) / R / 360, u = (d: number) => (d + 2451545 - 2440587.5) * 86400;
  return [u(noon - a), u(noon + a)];
}

function wxAppHtml(): string {
  wxAppLoad();
  const d = WxA.data;
  if (!d) return `<div class="wx-app"><div class="wx-head"><b>${esc(WX_SITE.name)}</b><small>${esc(WX_SITE.icao)}</small></div>
    <div class="wx-wait">${WxA.err ? `The weather couldn't be fetched (${esc(WxA.err)}). <button class="btn sm" data-act="wx-reload">${icon('reload')}Try again</button>` : 'Fetching the weather…'}</div></div>`;
  const n = d.now, tz = (+n.tz || 0) * 60;
  const loc = (u: number) => new Date((u + tz) * 1000), hm = (u: number) => loc(u).toISOString().slice(11, 16), ymd = (u: number) => loc(u).toISOString().slice(0, 10);   // (the airport's own time)
  const sun = wxSun(n.utc, +n.lat, +n.lon);
  const isDay = (u: number) => { const s = wxSun(u, +n.lat, +n.lon); return s ? u >= s[0] && u < s[1] : /_day$/.test(codeAt(u)); };
  const fc: any[] = ((d.forecast && d.forecast.properties && d.forecast.properties.timeseries) || []).map((e: any) => {
    const D = e.data, x = D.next_1_hours || D.next_6_hours || D.next_12_hours || {};
    return { u: Date.parse(e.time) / 1000, i: D.instant.details, one: D.next_1_hours, six: D.next_6_hours, code: (x.summary && x.summary.symbol_code) || '' };
  });
  const codeAt = (u: number): string => { let c = ''; for (const e of fc) { if (e.u > u) break; c = e.code; } return c; };
  const [now, nowIc] = wxNowWords(n, isDay(n.utc));
  const ws = Math.round(n.kt), wg = Math.max(ws, Math.round(n.gust || 0)), wd = +n.dir || 0, T = +n.temp;
  // how humid: the report's dew point (".. 05/M02 .."), else the forecast's hour
  let hum: number | null = null;
  const m = / (M?\d\d)\/(M?\d\d)(?: |$)/.exec(String(n.metar || ''));
  if (m) { const v = (s: string) => s[0] === 'M' ? -+s.slice(1) : +s, g = (t: number) => Math.exp(17.625 * t / (243.04 + t)); hum = Math.min(100, Math.round(100 * g(v(m[2])) / g(v(m[1])))); }
  else { let e0: any = null; for (const e of fc) { if (e.u > n.utc) break; e0 = e; } if (e0 && e0.i.relative_humidity != null) hum = Math.round(e0.i.relative_humidity); }
  // what it feels like (wind chill, as the weather services reckon it)
  const kmh = n.kt * 1.852, feels = T <= 10 && kmh > 4.8 ? 13.12 + 0.6215 * T - 11.37 * Math.pow(kmh, 0.16) + 0.3965 * T * Math.pow(kmh, 0.16) : T;
  // the next 24 hours, from the next full one
  const hours = fc.filter(e => e.u > n.utc && e.u <= n.utc + 86400 && e.one);
  // the days: what is left of today, then each day whole
  const days: any[] = [];
  for (let k = 0; k < fc.length; ++k) {
    const e = fc[k], key = ymd(e.u); if (e.u < n.utc - 3600) continue;
    let D = days[days.length - 1];
    if (!D || D.key !== key) { D = { key, hi: -99, lo: 99, mm: 0, w: 0, g: 0, dir: 0, code: '', off: 99, n: 0 }; days.push(D); }
    const t = e.i.air_temperature, six = !e.one && e.six ? e.six.details || {} : {};
    D.hi = Math.max(D.hi, t, six.air_temperature_max != null ? six.air_temperature_max : t); D.lo = Math.min(D.lo, t, six.air_temperature_min != null ? six.air_temperature_min : t); ++D.n;
    D.mm += e.one ? (e.one.details || {}).precipitation_amount || 0 : six.precipitation_amount || 0;
    if (e.i.wind_speed >= D.w) { D.w = e.i.wind_speed; D.dir = e.i.wind_from_direction || 0; }
    D.g = Math.max(D.g, e.i.wind_speed_of_gust || 0);
    const off = Math.abs(loc(e.u).getUTCHours() - 12); if (e.code && off < D.off) { D.off = off; D.code = e.six && e.one && off <= 1 && e.six.summary ? e.six.summary.symbol_code : e.code; }
  }
  while (days.length > 9 || (days.length > 1 && days[days.length - 1].n < 2)) days.pop();
  const today = ymd(n.utc), dayName = (s: string) => s === today ? 'Today' : new Date(s + 'T12:00').toLocaleDateString('en-GB', { weekday: 'short' });
  // the runways now: head- and crosswind on each end (kt), the one most into the wind first
  const rw = WX_SITE.rwy.map((r: any) => { const a = (wd - r.hdg) * Math.PI / 180; return { id: r.id, head: Math.round(ws * Math.cos(a)), cross: Math.round(ws * Math.sin(a)), gustX: Math.round(wg * Math.abs(Math.sin(a))) }; }).sort((a: any, b: any) => b.head - a.head);
  const vis = n.vis >= 9900 ? '10 km or more' : (n.vis / 1000).toFixed(1) + ' km';
  const wet = n.rain || n.snow || +n.sky === 5 || +n.sky === 7;
  const notes: string[] = [];
  if (n.freezing || (T <= 1 && wet)) notes.push('De-icing likely: freezing or snowy weather with something falling.');
  if (wg >= 35) notes.push('Strong gusts: jetways and stairs with care, high loaders limited.');
  if (Math.abs(rw[0].cross) >= 20) notes.push('Strong crosswind on the runway in use.');
  if (n.storm || +n.sky === 6) notes.push('Thunderstorm: ramp work stops while lightning is near.');
  if (n.vis < 1500) notes.push('Low visibility: low-visibility procedures on the apron.');
  return `<div class="wx-app">
    <div class="wx-head"><b>${esc(WX_SITE.name)}</b><small>${esc(WX_SITE.icao)} · ${hm(n.utc)}</small><button class="btn sm ghost" data-act="wx-reload" aria-label="Refresh">${icon('reload')}</button></div>
    <div class="wx-now">
      <span class="wx-big">${icon(nowIc)}</span>
      <div><b class="wx-temp">${Math.round(T)}°</b><span>${esc(now)}</span><small>Feels like ${Math.round(feels)}°${hum != null ? ` · humidity ${hum} %` : ''}</small></div>
      <div class="wx-wind"><span class="wx-arrow">${wxArrow(wd, 40)}</span><b>${ws} kt</b><small>${ws ? `from ${wxCompass(wd)} (${Math.round(wd)}°)` : 'calm'}${wg > ws + 4 ? ` · gusts ${wg} kt` : ''}</small><small>${(n.kt / 1.94384).toFixed(1)} m/s · visibility ${vis}</small></div>
    </div>
    ${fc.length ? `<div class="wx-sub">Next 24 hours</div>
    <div class="wx-hours"><div class="wx-h" title="${esc(now)}"><small>Now</small>${icon(nowIc)}<b>${Math.round(T)}°</b><small class="wx-pp"></small><span class="wx-hw">${wxArrow(wd, 12)}${ws}</span></div>${hours.map(e => { const [w, ic] = wxWords(e.code, isDay(e.u)), mm = (e.one.details || {}).precipitation_amount || 0; return `<div class="wx-h" title="${esc(w)}"><small>${hm(e.u)}</small>${icon(ic)}<b>${Math.round(e.i.air_temperature)}°</b><small class="wx-pp">${mm > 0 ? mm.toFixed(1) + ' mm' : ''}</small><span class="wx-hw">${wxArrow(e.i.wind_from_direction || 0, 12)}${wxKt(e.i.wind_speed)}</span></div>`; }).join('')}</div>
    <div class="wx-sub">The days ahead</div>
    <div class="wx-days">${days.map(D => { const [w, ic] = wxWords(D.code, true); return `<div class="wx-d"><b>${dayName(D.key)}</b>${icon(ic)}<span>${esc(w)}</span><span class="wx-hl">${Math.round(D.hi)}° / ${Math.round(D.lo)}°</span><small>${D.mm > 0.05 ? D.mm.toFixed(1) + ' mm' : 'dry'}</small><small>${wxArrow(D.dir, 12)} ${wxKt(D.w)} kt${D.g > D.w + 2 ? `, gusts ${wxKt(D.g)}` : ''}</small></div>`; }).join('')}</div>` : `<div class="wx-wait">${n.busy ? 'Fetching the forecast…' : 'No forecast to be had just now - it is asked for again by itself.'}</div>`}
    <div class="wx-sub">For your work at the airport</div>
    <div class="wx-work">
      ${rw.map((r: any, i: number) => `<div class="wx-rw ${i === 0 ? 'on' : ''}"><b>Runway ${r.id}${i === 0 ? ' · into the wind' : ''}</b><span>${r.head >= 0 ? 'Headwind ' + r.head : 'Tailwind ' + (-r.head)} kt</span><span>Crosswind ${Math.abs(r.cross)} kt from the ${r.cross >= 0 ? 'right' : 'left'}${wg > ws + 4 ? ` (gusts ${r.gustX})` : ''}</span></div>`).join('')}
      <div class="wx-notes">${notes.length ? notes.map(x => `<span>${icon('alert')}${esc(x)}</span>`).join('') : `<span>${icon('check')}No weather limits on the ramp right now.</span>`}</div>
      <small class="muted">${sun ? `Sunrise ${hm(sun[0])} · sunset ${hm(sun[1])}` : ''}${n.live ? '' : ' · no report from near here just now'}</small>
    </div>
  </div>`;
}
ACTIONS['wx-reload'] = () => { wxAppLoad(true); Sound.tick(); try { lapRender(true); } catch (e) { /* closed */ } };
