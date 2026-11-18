// The phone's tones (owner, 2026-11-18, to-do 122): 20 ringtones for incoming calls, 20 calling tunes you hear while it
// rings at theirs, 20 message tones - each made here by the browser's own sound (no files), chosen in the phone's
// Settings (each can be played to hear it), and the voicemail beep.
type PhTune = { name: string; notes: number[]; step: number; wave: OscillatorType; gap: number; len?: number; bell?: boolean };
// (notes: semitones from A4 = 0; -99 a rest; step: seconds per note; gap: the pause before it plays again)
const PH_RING: PhTune[] = [
  { name: 'Classic', notes: [7, 3, 7, 3, 7, 3], step: 0.09, wave: 'square', gap: 1.6 },
  { name: 'Marimba', notes: [0, 4, 7, 12, 7, 4], step: 0.13, wave: 'sine', gap: 1.2, bell: true },
  { name: 'Harbour', notes: [-5, 0, 4, 2, 0, -99, -5, 0], step: 0.16, wave: 'triangle', gap: 1.0 },
  { name: 'North wind', notes: [12, 11, 7, 4, 7, 11], step: 0.14, wave: 'sine', gap: 1.1, bell: true },
  { name: 'Old bell', notes: [16, 16, 16, 16], step: 0.06, wave: 'square', gap: 1.8, len: 0.05 },
  { name: 'Sunrise', notes: [0, 2, 4, 7, 9, 12, 16], step: 0.12, wave: 'sine', gap: 1.0, bell: true },
  { name: 'Runway', notes: [4, 4, 7, 4, 12, 11, 7], step: 0.13, wave: 'triangle', gap: 1.0 },
  { name: 'Puffin', notes: [19, 16, 19, 16, 12], step: 0.1, wave: 'sine', gap: 1.4, bell: true },
  { name: 'Fjord', notes: [-12, -5, 0, 4, 0, -5], step: 0.18, wave: 'triangle', gap: 0.9 },
  { name: 'Tower', notes: [7, -99, 7, -99, 12, 7], step: 0.12, wave: 'square', gap: 1.3, len: 0.08 },
  { name: 'Aurora', notes: [12, 16, 19, 24, 19, 16], step: 0.15, wave: 'sine', gap: 1.0, bell: true },
  { name: 'Ferry', notes: [0, 0, 7, 7, 9, 9, 7], step: 0.14, wave: 'triangle', gap: 1.0 },
  { name: 'Morse', notes: [12, -99, 12, 12, -99, 12], step: 0.07, wave: 'sine', gap: 1.5 },
  { name: 'Lighthouse', notes: [-5, 7, -5, 7], step: 0.3, wave: 'sine', gap: 0.8, bell: true },
  { name: 'Takeoff', notes: [0, 3, 7, 10, 12, 15, 19], step: 0.09, wave: 'triangle', gap: 1.2 },
  { name: 'Waves', notes: [4, 7, 4, 2, 4, 7, 11], step: 0.16, wave: 'sine', gap: 1.0 },
  { name: 'Brass', notes: [0, 7, 12, 7, 0], step: 0.15, wave: 'sawtooth', gap: 1.2, len: 0.12 },
  { name: 'Snowfall', notes: [24, 19, 16, 12, 7], step: 0.17, wave: 'sine', gap: 1.1, bell: true },
  { name: 'Digital', notes: [12, 24, 12, 24, 12, 24], step: 0.06, wave: 'square', gap: 1.5, len: 0.04 },
  { name: 'Lullaby', notes: [7, 4, 0, 4, 7, 4], step: 0.22, wave: 'sine', gap: 0.9, bell: true },
];
const PH_BACK: PhTune[] = [
  { name: 'Europe', notes: [3], step: 1.0, wave: 'sine', gap: 3.0, len: 1.0 },
  { name: 'UK double', notes: [3, -99, 3], step: 0.4, wave: 'sine', gap: 2.0, len: 0.4 },
  { name: 'North America', notes: [-2], step: 2.0, wave: 'sine', gap: 4.0, len: 2.0 },
  { name: 'Nordic', notes: [3], step: 1.0, wave: 'sine', gap: 4.0, len: 1.0 },
  { name: 'Soft chime', notes: [7, 12], step: 0.5, wave: 'sine', gap: 2.5, bell: true },
  { name: 'Gentle piano', notes: [0, 4, 7], step: 0.4, wave: 'triangle', gap: 2.2 },
  { name: 'Hum', notes: [-14], step: 1.5, wave: 'triangle', gap: 2.5, len: 1.5 },
  { name: 'Two-tone', notes: [3, 0], step: 0.5, wave: 'sine', gap: 2.5, len: 0.5 },
  { name: 'Cabin call', notes: [12, 7], step: 0.55, wave: 'sine', gap: 2.6, bell: true },
  { name: 'Distant', notes: [-2, -2], step: 0.7, wave: 'sine', gap: 3.0, len: 0.6 },
  { name: 'Calm harp', notes: [0, 7, 12, 16], step: 0.3, wave: 'sine', gap: 2.0, bell: true },
  { name: 'Warm pad', notes: [-5, 0], step: 1.0, wave: 'triangle', gap: 2.4, len: 1.0 },
  { name: 'Pulse', notes: [3, -99, 3, -99, 3], step: 0.2, wave: 'sine', gap: 2.6, len: 0.18 },
  { name: 'Airport', notes: [4, 0, 7], step: 0.45, wave: 'sine', gap: 2.3, bell: true },
  { name: 'Old line', notes: [-9], step: 1.2, wave: 'square', gap: 3.0, len: 1.2 },
  { name: 'Glass', notes: [24, 19], step: 0.5, wave: 'sine', gap: 2.6, bell: true },
  { name: 'Waltz', notes: [0, 4, 4], step: 0.45, wave: 'triangle', gap: 2.0 },
  { name: 'Sea breeze', notes: [2, 5, 9], step: 0.5, wave: 'sine', gap: 2.4, bell: true },
  { name: 'Low hum', notes: [-17, -17], step: 0.8, wave: 'sine', gap: 3.0, len: 0.8 },
  { name: 'Bright', notes: [12, 16, 19], step: 0.35, wave: 'sine', gap: 2.2, bell: true },
];
const PH_MSG: PhTune[] = [
  { name: 'Note', notes: [12, 19], step: 0.1, wave: 'sine', gap: 0, bell: true },
  { name: 'Ding', notes: [16], step: 0.4, wave: 'sine', gap: 0, bell: true },
  { name: 'Pop', notes: [7, 12], step: 0.06, wave: 'triangle', gap: 0 },
  { name: 'Tri-tone', notes: [12, 7, 16], step: 0.12, wave: 'sine', gap: 0, bell: true },
  { name: 'Bubble', notes: [0, 7, 12, 19], step: 0.05, wave: 'sine', gap: 0 },
  { name: 'Chord', notes: [0, 4, 7], step: 0.03, wave: 'sine', gap: 0, bell: true, len: 0.5 },
  { name: 'Knock', notes: [-12, -12], step: 0.12, wave: 'triangle', gap: 0, len: 0.06 },
  { name: 'Glass', notes: [24, 28], step: 0.1, wave: 'sine', gap: 0, bell: true },
  { name: 'Swift', notes: [19, 12], step: 0.07, wave: 'sine', gap: 0 },
  { name: 'Cabin', notes: [16, 9], step: 0.25, wave: 'sine', gap: 0, bell: true },
  { name: 'Drop', notes: [12, 0], step: 0.08, wave: 'sine', gap: 0 },
  { name: 'Morse', notes: [12, 12, 12], step: 0.06, wave: 'square', gap: 0, len: 0.04 },
  { name: 'Harp', notes: [0, 4, 7, 12, 16], step: 0.05, wave: 'sine', gap: 0, bell: true },
  { name: 'Retro', notes: [12, 16, 19], step: 0.07, wave: 'square', gap: 0, len: 0.05 },
  { name: 'Hello', notes: [7, 12, 7], step: 0.1, wave: 'triangle', gap: 0 },
  { name: 'Spark', notes: [28], step: 0.2, wave: 'sine', gap: 0, bell: true },
  { name: 'Wood', notes: [5, 12], step: 0.08, wave: 'triangle', gap: 0, len: 0.05 },
  { name: 'Bloom', notes: [0, 7, 14], step: 0.09, wave: 'sine', gap: 0, bell: true },
  { name: 'Ping', notes: [19], step: 0.15, wave: 'sine', gap: 0 },
  { name: 'Quiet', notes: [12], step: 0.2, wave: 'sine', gap: 0, len: 0.12 },
];
function phTunes(kind: string): PhTune[] { return kind === 'back' ? PH_BACK : kind === 'msg' ? PH_MSG : PH_RING; }
/** Plays one round of a tune; its length in seconds back (for repeating). */
function phTonePlay(kind: string, i: number, vol = 0.22): number {
  const L = phTunes(kind), t = L[((i | 0) % L.length + L.length) % L.length]; let ctx: AudioContext | null = null;
  try { ctx = audio(); } catch (e) { ctx = null; } if (!ctx || !t) return 1;
  const s0 = ctx.currentTime + 0.02;
  t.notes.forEach((n, k) => {
    if (n <= -99) return;
    const f = 440 * Math.pow(2, n / 12), at = s0 + k * t.step, d = t.len || t.step * 0.9;
    tone(ctx as AudioContext, f, at, t.bell ? d * 2.2 : d, vol, t.wave);
    if (t.bell) tone(ctx as AudioContext, f * 2.76, at, d * 0.8, vol * 0.25, 'sine');
  });
  return t.notes.length * t.step + (t.len || 0) + t.gap;
}
/** The voicemail's beep. */
function phBeep() { try { const ctx = audio(); if (ctx) tone(ctx, 1000, ctx.currentTime + 0.02, 0.45, 0.25, 'sine'); } catch (e) { /* no sound */ } }
/** The tone chosen on your phone (the phone's own settings). */
function phToneOf(kind: 'ring' | 'back' | 'msg'): number {
  const L: any = Lap; const was = { phone: L.phone, tablet: L.tablet, ops: L.ops }; L.phone = true; L.tablet = false; L.ops = false;
  let v = 0; try { const D = lapDev(); v = +(kind === 'ring' ? D.ringTone : kind === 'back' ? D.backTone : D.msgTone) || 0; } catch (e) { v = 0; }
  L.phone = was.phone; L.tablet = was.tablet; L.ops = was.ops; return v;
}
/** A tune round and round until stop() (the ringing, the calling tune). */
function phToneLoop(kind: 'ring' | 'back'): { stop: () => void } {
  let on = true, h = 0; const go = () => { if (!on) return; const len = phTonePlay(kind, phToneOf(kind), kind === 'back' ? 0.12 : 0.22); h = setTimeout(go, len * 1000) as any; };
  go(); return { stop: () => { on = false; clearTimeout(h); } };
}
/** The phone's tone settings (ringtone, calling tune, message tone), each with a button to hear it. */
function phToneRowsHtml(D: any): string {
  const row = (k: string, key: string, title: string, sub: string) => `<div class="ch-row"><span><b>${title}</b><small>${sub}</small></span><div class="seg"><select class="ph-tone" data-k="${key}" aria-label="${title}">${phTunes(k).map((t, i) => `<option value="${i}" ${(+D[key] || 0) === i ? 'selected' : ''}>${esc(t.name)}</option>`).join('')}</select><button data-act="ph-tone-try" data-kind="${k}" data-k="${key}">${icon('play')}Play</button></div></div>`;
  return row('ring', 'ringTone', 'Ringtone', 'When someone calls you.') + row('back', 'backTone', 'Calling tune', 'What you hear while it rings at theirs.') + row('msg', 'msgTone', 'Message tone', 'When a message comes in.');
}
ACTIONS['ph-tone-try'] = el => { const kind = el.getAttribute('data-kind') || 'ring', key = el.getAttribute('data-k') || ''; const sel = (el.parentElement as HTMLElement).querySelector('select') as HTMLSelectElement; phTonePlay(kind, sel ? +sel.value : +(lapDev() as any)[key] || 0); };
document.addEventListener('change', e => {
  const t = e.target as HTMLSelectElement; if (!t || !t.classList || !t.classList.contains('ph-tone')) return;
  const D = lapDev(), k = t.getAttribute('data-k') || ''; (D as any)[k] = +t.value; Store.soon();
  phTonePlay(k === 'backTone' ? 'back' : k === 'msgTone' ? 'msg' : 'ring', +t.value);
}, true);
