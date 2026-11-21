// Translations come from data/i18n/<lang>.json, served by the engine.
import { api } from './api.js';

let dict = {};
export let lang = localStorage.getItem('npc.lang') || (navigator.language || 'es').slice(0, 2);

export async function loadLanguage(code) {
  try { dict = await api.get('/api/i18n?lang=' + code); lang = code; }
  catch { dict = await api.get('/api/i18n?lang=es'); lang = 'es'; }
  localStorage.setItem('npc.lang', lang);
  document.documentElement.lang = lang;
  applyStatic();
}

/** t('activity', 'working') -> "Trabajando" (falls back to the id). */
export function t(section, id) {
  const s = dict[section];
  if (s && typeof s === 'object' && s[id] !== undefined) return s[id];
  return String(id).replace(/_/g, ' ');
}
export const ui = (key) => t('ui', key);
export const weekday = (d) => (dict.weekday || [])[d] ?? String(d);
export const languageName = () => dict.name || lang;

export function applyStatic(root = document) {
  root.querySelectorAll('[data-i18n]').forEach((el) => { el.textContent = ui(el.dataset.i18n); });
  root.querySelectorAll('[data-i18n-title]').forEach((el) => { el.title = ui(el.dataset.i18nTitle); });
  root.querySelectorAll('[data-i18n-placeholder]').forEach((el) => { el.placeholder = ui(el.dataset.i18nPlaceholder); });
}

export function fmtHour(h) {
  h = ((h % 24) + 24) % 24;
  const hh = Math.floor(h), mm = Math.floor((h - hh) * 60);
  return `${String(hh).padStart(2, '0')}:${String(mm).padStart(2, '0')}`;
}
export const pct = (v) => `${Math.round(v * 100)}%`;
export const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
