// The Weather app (owner, 2026-11-18, to-do 124): on your phone, your tablet and the computers. The real-world
// forecast for the airport you work at (Open-Meteo, free, no key: the same real weather that drives the 3D sky) -
// now, the next 24 hours hour by hour, the week ahead; the wind's strength and the way it comes from; and what it means
// for the work at the airport: the head- and crosswind on each runway end, gusts, visibility, freezing and de-icing.
const WX_SITE = { name: 'Mýrdalssandur', icao: 'BIAP', lat: 63.457988, lon: -18.713703, rwy: [{ id: '08', hdg: 68 }, { id: '26', hdg: 248 }] };   // (Data/airfield.json; true headings)
const WxA: any = { data: null as any, at: 0, busy: false, err: '' };

function wxAppLoad(force?: boolean) {
  if (WxA.busy || (!force && WxA.data && Date.now() - WxA.at < 15 * 60000)) return;
  WxA.busy = true; WxA.err = '';
  const q = 'latitude=' + WX_SITE.lat + '&longitude=' + WX_SITE.lon +
    '&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,cloud_cover,wind_speed_10m,wind_direction_10m,wind_gusts_10m,precipitation,visibility,is_day' +
    '&hourly=temperature_2m,weather_code,precipitation_probability,precipitation,wind_speed_10m,wind_direction_10m,wind_gusts_10m,visibility,is_day' +
    '&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant,sunrise,sunset' +
    '&wind_speed_unit=ms&timezone=auto&forecast_days=7';
  Net.json('https://api.open-meteo.com/v1/forecast?' + q, { timeout: 15000 })
    .then((d: any) => { WxA.data = d; WxA.at = Date.now(); })
    .catch((e: any) => { WxA.err = String(e && e.message || e); })
    .then(() => { WxA.busy = false; try { if (Lap.app === 'weather') lapRender(true); } catch (e) { /* closed */ } });
}

/** A WMO weather code in words, and the sign drawn for it. */
function wxWords(code: number, day: boolean): [string, string] {
  const c = +code;
  if (c === 0) return [day ? 'Sunny' : 'Clear', day ? 'sun' : 'star'];
  if (c === 1) return [day ? 'Mostly sunny' : 'Mostly clear', day ? 'sun' : 'star'];
  if (c === 2) return ['Partly cloudy', 'cloud'];
  if (c === 3) return ['Overcast', 'cloud'];
  if (c === 45 || c === 48) return [c === 48 ? 'Freezing fog' : 'Fog', 'cloud'];
  if (c >= 51 && c <= 55) return ['Drizzle', 'cloud'];
  if (c === 56 || c === 57) return ['Freezing drizzle', 'alert'];
  if (c >= 61 && c <= 65) return [c === 61 ? 'Light rain' : c === 63 ? 'Rain' : 'Heavy rain', 'cloud'];
  if (c === 66 || c === 67) return ['Freezing rain', 'alert'];
  if (c >= 71 && c <= 77) return [c === 71 ? 'Light snow' : c === 73 ? 'Snow' : c === 77 ? 'Snow grains' : 'Heavy snow', 'cloud'];
  if (c >= 80 && c <= 82) return [c === 82 ? 'Violent showers' : 'Rain showers', 'cloud'];
  if (c === 85 || c === 86) return ['Snow showers', 'cloud'];
  if (c === 95) return ['Thunderstorm', 'alert'];
  if (c === 96 || c === 99) return ['Thunderstorm with hail', 'alert'];
  return ['-', 'cloud'];
}
function wxCompass(deg: number): string { const P = ['N', 'NNE', 'NE', 'ENE', 'E', 'ESE', 'SE', 'SSE', 'S', 'SSW', 'SW', 'WSW', 'W', 'WNW', 'NW', 'NNW']; return P[Math.round(((deg % 360) + 360) % 360 / 22.5) % 16]; }
/** An arrow pointing the way the wind blows TO (it comes from deg). */
function wxArrow(deg: number, px = 18): string { return `<svg viewBox="0 0 24 24" width="${px}" height="${px}" style="transform:rotate(${deg + 180}deg)" aria-hidden="true"><path d="M12 3 L18 15 L12 12 L6 15 Z" fill="currentColor"/></svg>`; }
function wxKt(ms: number): number { return Math.round(ms * 1.94384); }

function wxAppHtml(): string {
  wxAppLoad();
  const d = WxA.data;
  if (!d) return `<div class="wx-app"><div class="wx-head"><b>${esc(WX_SITE.name)}</b><small>${esc(WX_SITE.icao)}</small></div>
    <div class="wx-wait">${WxA.err ? `The forecast couldn't be fetched (${esc(WxA.err)}). <button class="btn sm" data-act="wx-reload">${icon('reload')}Try again</button>` : 'Fetching the forecast…'}</div></div>`;
  const c = d.current, h = d.hourly, dy = d.daily;
  const [now, nowIc] = wxWords(c.weather_code, !!c.is_day);
  // the next 24 hours from now
  const t0 = h.time.findIndex((t: string) => new Date(t).getTime() >= Date.now() - 3600000);
  const hours: number[] = []; for (let i = Math.max(0, t0); i < h.time.length && hours.length < 24; ++i) hours.push(i);
  // the runways now: head- and crosswind on each end (kt), the one most into the wind first
  const wd = c.wind_direction_10m, ws = wxKt(c.wind_speed_10m), wg = wxKt(c.wind_gusts_10m);
  const rw = WX_SITE.rwy.map(r => { const a = (wd - r.hdg) * Math.PI / 180; return { id: r.id, head: Math.round(ws * Math.cos(a)), cross: Math.round(ws * Math.sin(a)), gustX: Math.round(wg * Math.abs(Math.sin(a))) }; }).sort((a, b) => b.head - a.head);
  const vis = c.visibility != null ? (c.visibility >= 10000 ? '10 km or more' : (c.visibility / 1000).toFixed(1) + ' km') : '-';
  const notes: string[] = [];
  if (c.temperature_2m <= 1 && (c.precipitation > 0 || [56, 57, 66, 67, 71, 73, 75, 77, 85, 86].indexOf(+c.weather_code) >= 0)) notes.push('De-icing likely: freezing or snowy weather with something falling.');
  if (wg >= 35) notes.push('Strong gusts: jetways and stairs with care, high loaders limited.');
  if (Math.abs(rw[0].cross) >= 20) notes.push('Strong crosswind on the runway in use.');
  if ([95, 96, 99].indexOf(+c.weather_code) >= 0) notes.push('Thunderstorm: ramp work stops while lightning is near.');
  if (c.visibility != null && c.visibility < 1500) notes.push('Low visibility: low-visibility procedures on the apron.');
  const dayName = (s: string, i: number) => i === 0 ? 'Today' : new Date(s + 'T12:00').toLocaleDateString('en-GB', { weekday: 'short' });
  return `<div class="wx-app">
    <div class="wx-head"><b>${esc(WX_SITE.name)}</b><small>${esc(WX_SITE.icao)} · updated ${new Date(WxA.at).toTimeString().slice(0, 5)}</small><button class="btn sm ghost" data-act="wx-reload" aria-label="Refresh">${icon('reload')}</button></div>
    <div class="wx-now">
      <span class="wx-big">${icon(nowIc)}</span>
      <div><b class="wx-temp">${Math.round(c.temperature_2m)}°</b><span>${esc(now)}</span><small>Feels like ${Math.round(c.apparent_temperature)}° · humidity ${Math.round(c.relative_humidity_2m)} % · clouds ${Math.round(c.cloud_cover)} %</small></div>
      <div class="wx-wind"><span class="wx-arrow">${wxArrow(wd, 40)}</span><b>${ws} kt</b><small>from ${wxCompass(wd)} (${Math.round(wd)}°)${wg > ws + 4 ? ` · gusts ${wg} kt` : ''}</small><small>${(c.wind_speed_10m).toFixed(1)} m/s · visibility ${vis}</small></div>
    </div>
    <div class="wx-sub">Next 24 hours</div>
    <div class="wx-hours">${hours.map(i => { const [w, ic] = wxWords(h.weather_code[i], !!h.is_day[i]); return `<div class="wx-h" title="${esc(w)}"><small>${h.time[i].slice(11, 16)}</small>${icon(ic)}<b>${Math.round(h.temperature_2m[i])}°</b><small class="wx-pp">${h.precipitation_probability[i] != null ? h.precipitation_probability[i] + ' %' : ''}</small><span class="wx-hw">${wxArrow(h.wind_direction_10m[i], 12)}${wxKt(h.wind_speed_10m[i])}</span></div>`; }).join('')}</div>
    <div class="wx-sub">The week</div>
    <div class="wx-days">${dy.time.map((t: string, i: number) => { const [w, ic] = wxWords(dy.weather_code[i], true); return `<div class="wx-d"><b>${dayName(t, i)}</b>${icon(ic)}<span>${esc(w)}</span><span class="wx-hl">${Math.round(dy.temperature_2m_max[i])}° / ${Math.round(dy.temperature_2m_min[i])}°</span><small>${dy.precipitation_sum[i] ? dy.precipitation_sum[i].toFixed(1) + ' mm' : 'dry'}${dy.precipitation_probability_max[i] != null ? ' · ' + dy.precipitation_probability_max[i] + ' %' : ''}</small><small>${wxArrow(dy.wind_direction_10m_dominant[i], 12)} ${wxKt(dy.wind_speed_10m_max[i])} kt, gusts ${wxKt(dy.wind_gusts_10m_max[i])}</small></div>`; }).join('')}</div>
    <div class="wx-sub">For your work at the airport</div>
    <div class="wx-work">
      ${rw.map((r, i) => `<div class="wx-rw ${i === 0 ? 'on' : ''}"><b>Runway ${r.id}${i === 0 ? ' · into the wind' : ''}</b><span>${r.head >= 0 ? 'Headwind ' + r.head : 'Tailwind ' + (-r.head)} kt</span><span>Crosswind ${Math.abs(r.cross)} kt from the ${r.cross >= 0 ? 'right' : 'left'}${wg > ws + 4 ? ` (gusts ${r.gustX})` : ''}</span></div>`).join('')}
      <div class="wx-notes">${notes.length ? notes.map(n => `<span>${icon('alert')}${esc(n)}</span>`).join('') : `<span>${icon('check')}No weather limits on the ramp right now.</span>`}</div>
      <small class="muted">Sunrise ${dy.sunrise[0].slice(11, 16)} · sunset ${dy.sunset[0].slice(11, 16)} · forecast by Open-Meteo.com</small>
    </div>
  </div>`;
}
ACTIONS['wx-reload'] = () => { wxAppLoad(true); Sound.tick(); try { lapRender(true); } catch (e) { /* closed */ } };
