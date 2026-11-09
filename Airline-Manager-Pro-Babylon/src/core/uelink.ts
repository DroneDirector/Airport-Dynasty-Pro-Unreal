// The PC version: these menus run inside Unreal Engine's web view, and Unreal is the 3D game. Unreal puts its link in
// window.ue.adp (open the terminal or the airport site in Unreal's own 3D, the airline's state for its interface) and
// says on the page's address which PC version and engine it is (?pc=0.10.0&ue=5.7). On Android none of this exists and
// nothing here does anything.
//  - The gamepad: Unreal reads the controller (Xbox, PlayStation, any) and hands its buttons and sticks to the page as
//    a standard gamepad, so the menus' own controller code (the pointer, the d-pad jumps, Cross / A to click, Circle /
//    B back) works exactly as on the phone. A gamepad the page sees itself comes first.
//  - The airline: every few seconds the game's departures and arrivals at your airport, and the airline's name and
//    money, go to Unreal (its top bar, the airport's announcements).
const q = new URLSearchParams(location.search);
/** The PC version's number, when these menus run inside Unreal (empty on Android). */
export const UE_PC = q.get('pc') || '';
/** The Unreal Engine version (5.7…). */
export const UE_ENGINE = q.get('ue') || '';
/** Unreal's link, or null. */
export function ue(): any { const u = (window as any).ue; return u && u.adp ? u.adp : null; }

let pad: any = null;
function wins(): any[] { const l: any[] = [window]; for (const f of Array.from(document.querySelectorAll('iframe'))) { try { if ((f as HTMLIFrameElement).contentWindow) l.push((f as HTMLIFrameElement).contentWindow); } catch (e) { /* not ours */ } } return l; }
/** Unreal's gamepad shows up in this window's navigator.getGamepads() (the page's own pads first). */
export function uePadInstall(w: any) {
  try {
    if (!w || w.__uePadOn) return; w.__uePadOn = true;
    const nav = w.navigator, real = nav.getGamepads ? nav.getGamepads.bind(nav) : () => [];
    Object.defineProperty(nav, 'getGamepads', { configurable: true, value: () => {
      let r: any[] = []; try { r = Array.from(real() || []); } catch (e) { r = []; }
      return r.some(Boolean) || !pad ? r : [pad];
    } });
  } catch (e) { /* this window keeps its own */ }
}
if (UE_PC) {
  console.log('[ue] menus ready, link ' + (ue() ? 'there' : 'missing'));
  uePadInstall(window);
  // called by Unreal when the controller changes: its name, the four stick axes and the 17 buttons (0..1)
  (window as any).__uePad = (id: string, axes: number[], buttons: number[]) => {
    const first = !pad;
    pad = { id, index: 0, connected: true, mapping: 'standard', timestamp: performance.now(), axes: axes.slice(),
      buttons: buttons.map(v => ({ pressed: v > 0.5, touched: v > 0.05, value: v })), vibrationActuator: null };
    if (first) for (const w of wins()) { try { const e = new w.Event('gamepadconnected'); e.gamepad = pad; w.dispatchEvent(e); } catch (e) { /* later */ } }
  };
}
