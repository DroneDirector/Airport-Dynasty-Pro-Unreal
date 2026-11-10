// Airport Dynasty Pro, Babylon.js edition. It starts with the start screen (the film), then one place: the lounge.
// The gear in the corner is always there: graphics and controls settings. More parts of the game are added as
// sections (core/sections.ts) and opened from here.
import { logProblem } from './core/errlog';
import { makeEngine, applyScale, getEngine } from './core/engine';
import { onSettings } from './core/settings';
import { inputInit, onButton, setPlay, onPad } from './core/input';
import { openSection, currentSection, closeSection, SectionCtx } from './core/sections';
import { splashOpen, splashEnter, splashIsOpen, splashNav, splashGo } from './ui/splash';
import { settingsShow, settingsHide, settingsOpen, settingsButton } from './ui/settings-panel';
import { loadingShow, loadingProgress, loadingHide } from './ui/loading';
import { icon, toast } from './ui/icons';
import { exitGame } from './core/exit';
import { gameOpen, gameIsOpen, gameClose, gameShow, gameExists, gameSend, onGameRoom, onGameFids, onGameFlight, gameEval, gameHidden } from './ui/game-frame';
import { UE_PC, UE_ENGINE, ue } from './core/uelink';
import { fidsCanvas, gateCanvas } from './sections/terminal/fids';
import { announcerReady, announcerIds } from './ui/announcer-voice';
import { APP, IS_PC } from './core/app';
import { updateInit } from './core/update';
import { whatsNewOnStart, whatsNewIsOpen, whatsNewClose } from './ui/whats-new';
import './sections/lounge/index';
import './sections/viewer3d/index';
import './sections/office/index';
import './sections/ops/index';
import './sections/hangar/index';
import './sections/building/index';
import './sections/salute/index';
import './sections/court/index';
import './sections/terminal/index';
import './sections/world/index';
import './sections/site/index';

const canvas = document.getElementById('bjs') as HTMLCanvasElement, ui = document.getElementById('ui') as HTMLElement;
let inPlace = false;
inputInit(canvas, ui);

// the gear: always on the screen
const gear = document.createElement('button'); gear.className = 'gear'; gear.setAttribute('aria-label', 'Settings'); gear.innerHTML = icon('gear'); ui.appendChild(gear);
gear.addEventListener('click', () => toggleSettings());
// Exit, beside the gear: the game shuts down completely and you are back on the desktop (PC) or home screen (Android)
const exitB = document.createElement('button'); exitB.className = 'exit'; exitB.setAttribute('aria-label', 'Exit the game'); exitB.innerHTML = `${icon('power')}<span>Exit</span>`; ui.appendChild(exitB);
exitB.addEventListener('click', () => exitGame());
// Title screen, beside Exit, in every place of the game (the 3D viewer, the lounge): back to the start screen
const titleB = document.createElement('button'); titleB.className = 'to-title'; titleB.hidden = true; titleB.setAttribute('aria-label', 'Back to the title screen'); titleB.innerHTML = `${icon('back')}<span>Title screen</span>`; ui.appendChild(titleB);
titleB.addEventListener('click', () => backToTitle());
// the version number at the bottom of the screen in every place (the start screen has its own)
const verB = document.createElement('small'); verB.className = 'place-ver'; verB.hidden = true; verB.textContent = UE_PC ? `v${UE_PC} Â· Unreal Engine ${UE_ENGINE}` : `v${APP.version} Â· Babylon.js ${APP.babylon}`; ui.appendChild(verB);
function toggleSettings() {
  // the PC (Unreal) version: the gear opens Unreal's own Settings (the same panel as in the airport, over the menus)
  if (UE_PC && ue()) { try { ue().opensettings(); return; } catch (e) { /* the page's own below */ } }
  if (settingsOpen()) { settingsHide(); return; }
  setPlay(false); settingsShow(() => { if (inPlace) setPlay(true); });
}
onSettings(what => {
  if (what === 'scale' || what === 'preset') applyScale();
  const s = currentSection(); if (s && s.onSettings) s.onSettings(what);
});
onButton(b => {
  if (gameIsOpen()) return;   // the game's interface has its own controls
  if (whatsNewIsOpen()) { if (b === 'back' || b === 'confirm' || b === 'menu') whatsNewClose(); return; }
  if (settingsOpen()) { settingsButton(b); return; }
  if (b === 'menu') { toggleSettings(); return; }
  if (b === 'back' && inPlace) { if (gameExists()) backToGame(); else backToTitle(); return; }
  if (splashIsOpen() && b === 'confirm') splashEnter();
  if (splashIsOpen() && (b === 'up' || b === 'down')) splashNav(b === 'up' ? -1 : 1);
});
onPad(name => toast(name ? `${name} connected` : 'Controller disconnected'));

/** Into a place of the game (today: the lounge), with its loading screen. */
async function enter(id: string, title: string, opts?: any) {
  // the PC version: the terminal and the airport site are Unreal's own 3D (the menus go out of sight; G brings them back)
  const u = ue(); if (u && id === 'terminal' && u.openterminal) { u.openterminal(); return; }
  if (u && id === 'site' && u.opensite) { u.opensite(); return; }
  if (u && id === 'lounge' && u.openlounge) { u.openlounge(); return; }
  // the 3D viewer following a flight: Unreal's own viewer (the airport's and crash sites' views stay the Babylon viewer)
  if (u && id === 'viewer3d' && u.openviewer && !(opts && (opts.crash || opts.apt))) {
    let fid = opts && opts.live ? String(opts.live) : '';
    if (!fid) try { fid = String(gameEval(`(function(){try{var l=liveOf();return l.length?l[0].id:''}catch(e){return ''}})()`) || ''); } catch (e) { fid = ''; }
    if (fid) { ueFollow = fid; gameSend({ ampb: 'follow', id: fid }); u.openviewer(); return; }
  }
  loadingShow(title);
  const engine = await makeEngine(canvas);
  // where the loading got to: if it is still loading after two minutes, that goes to the log (Settings â†’ Updates and errors)
  let at = { p: 0, s: 'Starting' }, opened = false;
  const ctx: SectionCtx = { engine, canvas, ui, progress: (p, s) => { at = { p, s }; loadingProgress(p, s); }, back: backToTitle, opts, toGame: opts && gameExists() ? backToGame : undefined, goTo: goToPlace };
  const watch = setTimeout(() => { if (!opened) logProblem('3d places', `${title} is taking too long to open`, `Still loading after 2 minutes, at ${Math.round(at.p)}% (${at.s}).`, `Place: ${id}\nOptions: ${JSON.stringify(opts || {}).slice(0, 300)}\nDevice: ${navigator.userAgent}`); }, 120000);
  try { await openSection(id, ctx); } catch (e: any) { clearTimeout(watch); console.error(e); loadingProgress(100, 'This place could not be opened on this device');
    logProblem('3d places', `${title} could not be opened`, `${(e && e.message) || String(e)} (at ${Math.round(at.p)}%, ${at.s})`, `Place: ${id}\nVersion: ${APP.version}\nDevice: ${navigator.userAgent}\n\n${(e && e.stack) || ''}`.slice(0, 4000)); return; }
  opened = true; clearTimeout(watch);
  loadingProgress(100, 'Welcome'); canvas.classList.add('on');
  setTimeout(() => { loadingHide(); inPlace = true; titleB.hidden = false; verB.hidden = false; document.body.classList.toggle('in-viewer', id === 'viewer3d'); if (!settingsOpen()) setPlay(true); }, 350);
}
// The PC version's 3D viewer is Unreal's: the followed flight's positions go to it as the game sends them
let ueFollow = '';
onGameFlight((d: any) => { const u = ue(); if (!ueFollow || !u || !u.viewerflight) return; if (d.gone) { const nx = (d.list || []).filter((x: any) => x.id !== ueFollow)[0]; if (nx) { ueFollow = nx.id; gameSend({ ampb: 'follow', id: ueFollow }); } return; } if (d.me) u.viewerflight(JSON.stringify({ me: d.me })); });
(window as any).__ueViewerClosed = () => { if (ueFollow) { ueFollow = ''; gameSend({ ampb: 'stop' }); } };
/** From one place straight to another (the 3D viewer's Airport button): this one is closed and its memory given back. */
function goToPlace(id: string, opts?: any) {
  if (!inPlace) return; inPlace = false; setPlay(false); if (settingsOpen()) settingsHide();
  canvas.classList.remove('on'); const e = getEngine(); if (e) e.stopRenderLoop(); closeSection(); enter(id, TITLES[id] || id, opts);
}
/** Back to the start screen from a place: it is closed and its memory given back, then the film starts again. */
function backToTitle() {
  if (!inPlace) return; inPlace = false; titleB.hidden = true; verB.hidden = true; document.body.classList.remove('in-viewer'); setPlay(false); if (settingsOpen()) settingsHide();
  canvas.classList.remove('on'); const e = getEngine(); if (e) e.stopRenderLoop();
  closeSection(); gameClose(); splashOpen(pickPlace);
}
/** Back from the 3D viewer to the game's interface (it kept running behind). */
function backToGame(screen?: string) {
  if (!inPlace) return; inPlace = false; titleB.hidden = true; verB.hidden = true; document.body.classList.remove('in-viewer'); setPlay(false); if (settingsOpen()) settingsHide();
  canvas.classList.remove('on'); const e = getEngine(); if (e) e.stopRenderLoop();
  closeSection(); gameShow(); if (screen) gameSend({ ampb: 'go', screen });
}
const TITLES: { [id: string]: string } = { viewer3d: '3D viewer Â· above the clouds', lounge: 'Business lounge', terminal: 'The terminal Â· Grand Hall', site: 'Airport site Â· MÃ½rdalssandur' };
/** What was chosen on the title screen: the game (New, Continue, Load, Save, UI) or one of the demos. */
function pickPlace(id: string) {
  if (id.indexOf('game:') === 0) {
    gameOpen(id.slice(5) as any, { title: () => splashOpen(pickPlace), exit: () => exitGame(), viewer: (fid: string) => enter('viewer3d', /^crash:/.test(fid) ? '3D viewer Â· the crash site' : /^apt:/.test(fid) ? '3D viewer Â· your airport' : fid ? '3D viewer Â· following the flight' : TITLES.viewer3d, /^crash:/.test(fid) ? { crash: fid.slice(6) } : /^apt:/.test(fid) ? { apt: fid.slice(4) } : fid ? { live: fid } : undefined) });
    return;
  }
  enter(id, TITLES[id] || id);
}
splashOpen(pickPlace);
// a 3D place opened from the game (your officeâ€¦): the game waits behind it
onGameRoom(d => { if (d.room === 'world') { const w = d.data || {}; enter('world', 'Placing avatar in world', { lat: w.lat, lon: w.lon }); return; } if (d.room === 'site') { if (!IS_PC) return; enter('site', 'Your airport', { site: 'myrdalssandur', walk: true, data: d.data || {} }); return; } if (d.room === 'court') enter('court', d.data && d.data.mode === 'hearing' ? 'Court is in session' : 'The courtroom', { data: d.data || {} }); else if (d.room === 'salute') enter('salute', 'Water salute', { data: d.data || {} }); else if (d.room === 'building') enter('building', ({ office: 'Your office', ops: 'The Operations office', lounge: 'The Business lounge', museum: 'Your museum' } as any)[d.at] || 'The airport', { at: d.at || 'concourse', data: d.data || {} }); else if (d.room === 'office') enter('office', 'Your office', { data: d.data || {} }); else if (d.room === 'lounge') enter('lounge', 'Business lounge', { data: d.data || {} }); else if (d.room === 'ops') enter('ops', 'The Operations office', { data: d.data || {} }); else if (d.room === 'hangar') enter('hangar', 'The hangar', { data: d.data || {} }); });
// after an update: what's new, once; and a quiet look for a newer version now and then
whatsNewOnStart();
updateInit(v => toast(`Version ${v} is ready: Settings â†’ Updates`));

// The PC version: back from Unreal's 3D to the menus (G, Y / Triangle): the title screen again, or the game where it was
(window as any).__ueMenus = () => { if (inPlace || splashIsOpen()) return; if (gameExists()) { if (gameHidden()) gameShow(); } else splashOpen(pickPlace); };
// and the airline for Unreal's own interface: the departures and arrivals at your airport, the airline's name and money
if (UE_PC) {
  setInterval(() => {
    const u = ue(); if (!u || !gameExists()) return;
    gameSend({ ampb: 'fids', icao: '' });
    const s = gameEval(`(function(){try{var c=company();return JSON.stringify({name:c.name,icao:c.icao,hub:c.hub||c.base||'',money:money(c.balance,{compact:true})})}catch(e){return ''}})()`);
    if (s && u.gamestate) u.gamestate(String(s));
  }, 4000);
  // the terminal's timetable screens in Unreal: each board drawn here with the Babylon edition's own drawing and handed
  // over as a picture when its rows change, and every minute for the clock (the sample timetable with no game)
  let boards = { dep: [] as any[], arr: [] as any[], base: 'BIAP', live: false, at: Date.now() }, sent = '';
  const sendBoards = () => { const u = ue(); if (!u || !u.board) { if (u && u.log) u.log('no board link'); return; }
    const sig = new Date().getMinutes() + JSON.stringify([boards.dep, boards.arr].map(l => l.map((r: any) => [r.flight, r.sched, r.exp, r.status]))); if (sig === sent) return; sent = sig;
    for (const k of ['depwall', 'arrwall', 'deptotem', 'arrtotem', 'depwide', 'arrwide', 'opswall']) { try { u.board(k, fidsCanvas(k, boards).toDataURL('image/png').split(',')[1]); } catch (e: any) { if (u.log) u.log('board ' + k + ': ' + (e && e.message || e)); } }
    // each gate's own screen: its next departure, live (the same gate the loudspeaker calls name)
    for (let g = 1; g <= 8; g++) { try { u.board('gate' + g, gateCanvas(g, boards).toDataURL('image/png').split(',')[1]); } catch (e: any) { if (u.log) u.log('gate ' + g + ': ' + (e && e.message || e)); } } };
  setInterval(sendBoards, 15000); setTimeout(sendBoards, 3000);
  onGameFids(d => { const u = ue(); if (u && u.gamefids) u.gamefids(JSON.stringify({ base: d.base || '', dep: (d.dep || []).slice(0, 12), arr: (d.arr || []).slice(0, 12) }));
    boards = { dep: d.dep || [], arr: d.arr || [], base: String(d.base || 'BIAP'), live: true, at: Date.now() }; sendBoards(); });
}
// The PC version's airport announcements: Unreal asks for a line to be spoken (window.__ueSay). The announcer is
// always a woman: the game's own natural voice (Kokoro, a British female voice) when its voices are on this PC, made
// here and handed to Unreal as sound (Unreal adds the loudspeakers' sound and the volume where you stand); without
// them, Unreal speaks it with Windows' own female voice.
(window as any).__ueSay = async (id: number, text: string, vol: number) => {
  const u = ue(); console.log('[ue] say ' + id + (u ? '' : ' (no link)') + (u && u.speechfail ? '' : ' (no speechfail)')); if (!u) return;
  // the natural voice made by Unreal itself (Kokoro in the game, its voice pack shipped with it): the words as sound numbers
  if (u.kokoro) { try { if (await announcerReady()) { const ids = announcerIds(text); if (ids.length) { u.kokoro(id, JSON.stringify(ids)); return; } } } catch (e) { /* the game's voice below */ } }
  try {
    const b64 = await gameEval(`(async function (t) {
      if (typeof VoicePack === 'undefined' || VoicePack.state !== 'ready') return '';
      const sp = Object.assign({}, resolveSpeaker('gate', t), { voice: 'bf_emma', accent: 'b', speed: 0.94, pitch: 1, title: 'Announcer' });
      await VoiceEngine.ready();
      const parts = []; for (const s of Voice.sentences(t, sp)) parts.push(await voiceSentence(s, sp, true));
      const gap = 24000 * 0.32, n = parts.reduce((a, p) => a + p.length + gap, 0), i16 = new Int16Array(n); let o = 0;
      for (const p of parts) { for (let i = 0; i < p.length; i++) i16[o + i] = Math.max(-32767, Math.min(32767, Math.round(p[i] * 32767))); o += p.length + gap; }
      const b = new Uint8Array(i16.buffer); let s = ''; for (let i = 0; i < b.length; i += 0x8000) s += String.fromCharCode.apply(null, b.subarray(i, i + 0x8000));
      return btoa(s);
    })(${JSON.stringify(text)})`);
    if (b64 && u.speechpcm) { u.speechpcm(id, String(b64)); return; }
  } catch (e) { /* Windows' own voice below */ }
  console.log('[ue] windows voice for ' + id); if (u.speechfail) u.speechfail(id);   // Unreal speaks it with Windows' own female voice
};
// ...and every announcement as text in the game's messages (the Overview), word for word: flight calls as flight
// messages, the rest as the airport's
(window as any).__ueNotice = (kind: number, text: string) => {
  if (!gameExists()) return;
  const title = kind === 1 ? 'Airport announcement: flights' : kind === 2 ? 'Airport announcement: passengers' : 'Airport announcement';
  gameEval(`(function (k, t, b) { try { State.notices.unshift({ id: uid(), kind: k, title: t, body: b, time: 'Now', read: false }); if (Store.soon) Store.soon(); if (typeof refresh === 'function') refresh(); } catch (e) { /* not ready */ } })(${JSON.stringify(kind === 1 ? 'flight' : 'company')}, ${JSON.stringify(title)}, ${JSON.stringify(text)})`);
};

// The PC version's Esc. From the 3D world (Unreal): to the Overview, always (the game opened where it was, or continued).
(window as any).__ueOverview = () => {
  if (gameExists()) { if (gameHidden()) gameShow(); gameSend({ ampb: 'go', screen: 'ops' }); return; }
  if (inPlace) backToTitle();   // a 3D place of this page (the lounge, the viewer): closed first
  if (splashIsOpen()) splashGo('game:continue'); else pickPlace('game:continue');
};
// In the game: Esc closes what is open in it (the game's own way), from another screen goes to the Overview, and in the
// Overview asks before leaving to the title screen: save first, yes or no.
function leaveAsk() {
  if (document.querySelector('.ue-leave')) return;
  const d = document.createElement('div'); d.className = 'ue-leave';
  d.innerHTML = `<div class="ue-leave-box"><b>Leave to the title screen?</b><small>Your airline keeps going; anything not saved since the last save is lost.</small>
    <div class="ue-leave-row"><button data-a="save">Save game</button><button data-a="yes">Yes, leave</button><button data-a="no">No</button></div></div>`;
  document.body.appendChild(d);
  const close = () => d.remove();
  d.addEventListener('click', e => { const a = (e.target as HTMLElement).closest('[data-a]') as HTMLElement; if (!a) { if (e.target === d) close(); return; }
    const k = a.getAttribute('data-a'); close();
    if (k === 'save') { gameSend({ ampb: 'save' }); setTimeout(() => pickPlace('game:save'), 300); }
    else if (k === 'yes') { gameClose(); splashOpen(pickPlace); } });
  const key = (e: KeyboardEvent) => { if (e.key === 'Escape') { e.preventDefault(); close(); window.removeEventListener('keydown', key, true); } };
  window.addEventListener('keydown', key, true);
}
if (UE_PC) {
  (window as any).__ueGameEsc = (w: any) => {
    w.addEventListener('keydown', (e: KeyboardEvent) => {
      if (e.key !== 'Escape' || document.querySelector('.ue-leave')) return;
      if (w.document.querySelector('.layer, .modal-back, .sheet-back, .settings-pop, .ct-modal:not([hidden])')) return;   // the game closes it
      let screen = 'ops'; try { screen = String(w.eval('State.screen')); } catch (err) { /* the Overview */ }
      e.preventDefault(); e.stopImmediatePropagation();
      if (screen !== 'ops') gameSend({ ampb: 'go', screen: 'ops' }); else leaveAsk();
    }, true);
  };
}

