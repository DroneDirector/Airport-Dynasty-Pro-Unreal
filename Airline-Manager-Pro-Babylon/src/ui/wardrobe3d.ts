// The PC edition's wardrobe menu (owner, 2026-11-12), drawn over Unreal's room in the title screen's own graphics: the
// categories as the title screen's menu bars on the left, and attached at their right a glass window showing what the
// open category holds, as tiles; you stand in the room to the right of it all, where Unreal's camera frames you.
// Unreal opens it with window.__ueWardrobe(json) ({cat, cats: [...], items: [{label, who, on, note}], real}) and closes
// it with window.__ueWardrobeClose(); the page tells Unreal what you click through ue.adp.wardrobe(action, arg):
// 'category' (a category's name), 'wear' (an outfit's name), 'close'.
import { ue } from '../core/uelink';
import { icon } from './icons';

const Wd: any = { root: null as HTMLElement | null, key$: null as any, data: null as any };
const ICON: Record<string, string> = { Outfits: 'grid', Jackets: 'star', Shirts: 'save', Trousers: 'folder', Shoes: 'mountain', Hats: 'plus', Sunglasses: 'cam', Ties: 'play' };
const esc = (s: string) => String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c] as string));

function tell(action: string, arg = '') { const u = ue(); if (u && u.wardrobe) u.wardrobe(action, arg); }

function render() {
  const d = Wd.data, r = Wd.root; if (!d || !r) return;
  const cats: string[] = d.cats || [];
  r.innerHTML = `<div class="wd-body"><div class="wd-left">
      <div class="wd-k">Your office</div><div class="wd-t">Wardrobe</div>
      <div class="wd-k wd-k2">Categories</div>
      <nav class="ts-nav">${cats.map(c => `<button class="ts-btn ${c === d.cat ? 'on' : ''}" data-cat="${esc(c)}"><span class="ts-bi">${icon(ICON[c] || 'grid')}</span><span class="ts-bt"><b>${esc(c)}</b></span><i class="ts-sheen"></i></button>`).join('')}
        <button class="ts-btn wd-close" data-close="1"><span class="ts-bi">${icon('close')}</span><span class="ts-bt"><b>Close</b><small>E or Esc</small></span><i class="ts-sheen"></i></button>
      </nav>
    </div>
    <div class="wd-win"><small>What you can wear</small><b>${esc(d.cat || '')}</b>
      ${(d.items || []).length ? `<div class="wd-grid">${(d.items as any[]).map(it => `<button class="wd-tile ${it.on ? 'on' : ''} ${it.who ? '' : 'demo'}" ${it.who ? `data-who="${esc(it.who)}"` : ''}><b>${esc(it.label)}</b><small>${esc(it.note || '')}</small></button>`).join('')}</div>` : `<div class="wd-empty">empty</div>`}
    </div></div>`;
}

function open(json: string) {
  try { Wd.data = typeof json === 'string' ? JSON.parse(json) : json; } catch (e) { Wd.data = { cat: 'Outfits', cats: [], items: [] }; }
  if (!Wd.root) {
    document.documentElement.classList.add('ue-wd');
    const root = document.createElement('div'); root.className = 'wd'; document.body.appendChild(root); Wd.root = root;
    root.addEventListener('click', (e: Event) => {
      const t = e.target as HTMLElement;
      const c = t.closest('[data-cat]') as HTMLElement | null; if (c) { tell('category', c.getAttribute('data-cat') || ''); return; }
      const w = t.closest('[data-who]') as HTMLElement | null; if (w) { tell('wear', w.getAttribute('data-who') || ''); return; }
      if (t.closest('[data-close]')) { tell('close'); return; }
    });
    Wd.key$ = (e: KeyboardEvent) => { if (!Wd.root) return; if (e.key === 'Escape' || e.key === 'e' || e.key === 'E') { e.preventDefault(); e.stopImmediatePropagation(); tell('close'); } };
    window.addEventListener('keydown', Wd.key$, true);
    requestAnimationFrame(() => root.classList.add('in'));
  }
  render();
}

function close() {
  const root = Wd.root; if (!root) return;
  Wd.root = null; Wd.data = null; window.removeEventListener('keydown', Wd.key$, true);
  root.classList.remove('in'); root.classList.add('out');
  setTimeout(() => { root.remove(); document.documentElement.classList.remove('ue-wd'); }, 380);
}

(window as any).__ueWardrobe = open;
(window as any).__ueWardrobeClose = close;
