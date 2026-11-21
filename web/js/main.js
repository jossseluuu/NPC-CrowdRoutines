// Web client controller. All simulation logic lives in the C++ engine; this file only displays
// state and sends commands (see docs/reference/api.md).
import { api } from './api.js';
import { loadLanguage, t, ui, weekday, fmtHour, esc, lang, applyStatic } from './i18n.js';
import { MapView } from './render.js';
import { Inspector } from './inspector.js';
import { Chart } from './charts.js';

const $ = (id) => document.getElementById(id);
const SPEEDS = [[1, '1×'], [10, '10×'], [60, '1m/s'], [300, '5m/s'], [900, '15m/s'], [3600, '1h/s']];
const WEATHER_ICON = { clear: '☀', rain: '☂', storm: '⛈', heat: '♨' };
const CATEGORY_ORDER = ['traffic', 'weather', 'hazard', 'social', 'public'];

const app = {
  info: null, catalog: null, map: null, state: null, residents: null,
  prev: null, cur: null, frameInterval: 100,
  selected: null, selectedBuilding: null, following: null, detail: null,
  rules: [], treeSel: new Set(), filterBits: null, matches: null,
  armed: null, mouse: null, online: true, charts: [], series: null, lastSeriesT: -1,
};

const view = new MapView($('map'), $('minimap'));
const inspector = new Inspector($('inspector'), app);

// ---------------------------------------------------------------------------------------------
function toast(msg, error = false) {
  const el = $('toast');
  el.textContent = msg;
  el.classList.toggle('error', error);
  el.classList.remove('hidden');
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => el.classList.add('hidden'), 3500);
}

async function guard(fn) {
  try { return await fn(); } catch (e) { toast(e.message, true); return null; }
}

// ---------------------------------------------------------------------------------------------
async function loadWorld() {
  [app.info, app.map, app.residents] = await Promise.all([api.get('/api/info'), api.get('/api/map'), api.get('/api/residents')]);
  view.setResidents(app.residents);
  view.setMap(app.map);
  $('mapName').textContent = `${app.map.name} · ${app.info.population.toLocaleString()} ${ui('citizens')}`;
  app.prev = app.cur = null;
  app.series = null;
  app.lastSeriesT = -1;
  deselect();
  fillScenarioLists();
  buildFilterValue();
}

async function init() {
  await loadLanguage(lang);
  app.catalog = await api.get('/api/catalog');
  view.setCatalog(app.catalog);
  await loadWorld();
  buildStaticUi();
  frameLoop();
  stateLoop();
  slowLoop();
  requestAnimationFrame(render);
  applyHash();
}

/** Deep links: #select=12&building=40&tab=charts&zoom=2.5&center=400,300&color=stress&layers=traffic,density&follow=1&lang=en */
async function applyHash() {
  const h = new URLSearchParams(location.hash.slice(1));
  if (!h.toString()) return;
  if (h.get('lang')) { await loadLanguage(h.get('lang')); rebuildTexts(); }
  if (h.get('tab')) document.querySelector(`#tabs [data-tab="${h.get('tab')}"]`)?.click();
  if (h.get('color')) { view.colorMode = h.get('color'); $('colorMode').value = view.colorMode; renderLegend(); }
  if (h.get('layers')) for (const l of h.get('layers').split(',')) { view.layers[l] = true; view.staticDirty = true; }
  if (h.get('center')) { const [x, y] = h.get('center').split(',').map(Number); view.centerOn(x, y); }
  if (h.get('zoom')) view.zoomAt(Number(h.get('zoom')) / view.view.scale, view.w / 2, view.h / 2);
  if (h.get('select')) { await select(Number(h.get('select')), !h.get('center')); if (h.get('itab')) { inspector.tab = h.get('itab'); inspector.render(); } }
  if (h.get('follow')) app.following = app.selected;
  if (h.get('building')) await selectBuilding(Number(h.get('building')), true);
  if (h.get('expand')) $('expandAll').click();
}
window.addEventListener('hashchange', applyHash);

function buildStaticUi() {
  applyStatic();
  // Languages
  $('langSelect').innerHTML = app.info.languages.map((l) => `<option value="${l}" ${l === lang ? 'selected' : ''}>${l.toUpperCase()}</option>`).join('');
  $('langSelect').onchange = async (e) => { await loadLanguage(e.target.value); rebuildTexts(); };
  // Speeds
  $('speeds').innerHTML = SPEEDS.map(([v, l], k) => `<button class="iconbtn" data-speed="${v}" title="${k + 1}">${l}</button>`).join('');
  $('speeds').querySelectorAll('button').forEach((b) => (b.onclick = () => setSpeed(Number(b.dataset.speed))));
  $('playBtn').onclick = () => control({ action: 'toggle' });
  $('stepBtn').onclick = () => control({ action: 'step', ticks: 60 });
  // Layers & colours
  const layerKeys = Object.keys(view.layers);
  $('layerList').innerHTML = layerKeys.map((k) => `<label class="check"><input type="checkbox" data-layer="${k}" ${view.layers[k] ? 'checked' : ''}> <span data-i18n="layer_${k}"></span></label>`).join('');
  $('layerList').querySelectorAll('input').forEach((i) => (i.onchange = () => { view.layers[i.dataset.layer] = i.checked; view.staticDirty = true; }));
  const modes = ['activity', 'substate', 'stress', 'profile', 'age', 'sex', 'mode'];
  $('colorMode').innerHTML = modes.map((m) => `<option value="${m}" data-i18n="color_${m}"></option>`).join('');
  $('colorMode').onchange = (e) => { view.colorMode = e.target.value; renderLegend(); };
  $('zoomIn').onclick = () => view.zoomAt(1.4, view.w / 2, view.h / 2);
  $('zoomOut').onclick = () => view.zoomAt(1 / 1.4, view.w / 2, view.h / 2);
  $('zoomFit').onclick = () => view.fit();
  // Tabs
  $('tabs').querySelectorAll('button').forEach((b) => (b.onclick = () => {
    $('tabs').querySelectorAll('button').forEach((x) => x.classList.toggle('active', x === b));
    document.querySelectorAll('.tab').forEach((x) => x.classList.toggle('active', x.id === 'tab-' + b.dataset.tab));
    if (b.dataset.tab === 'charts') refreshCharts(true);
  }));
  $('expandAll').onclick = () => document.querySelectorAll('#tree .node').forEach((n) => n.classList.add('open'));
  $('collapseAll').onclick = () => document.querySelectorAll('#tree .node').forEach((n) => n.classList.remove('open'));
  buildTree();
  buildFilterUi();
  buildEventUi();
  buildScenarioUi();
  buildCharts();
  renderLegend();
  bindMapInput();
  applyStatic();
}

function rebuildTexts() {
  applyStatic();
  buildTree();
  buildFilterUi();
  buildEventUi();
  buildScenarioUi();
  buildCharts();
  renderLegend();
  renderRules();
  if (app.state) updateState(app.state);
  if (app.detail) inspector.render();
  $('mapName').textContent = `${app.map.name} · ${app.info.population.toLocaleString()} ${ui('citizens')}`;
}

function renderLegend() {
  $('legend').innerHTML = view.legendEntries(t).map(([c, l]) => `<span><i class="key" style="background:${c}"></i>${esc(l)}</span>`).join('');
}

// ---------------------------------------------------------------------------------------------
async function control(body) {
  const st = await guard(() => api.post('/api/control', body));
  if (st) updateRunner(st);
}
function setSpeed(v) { control({ action: 'speed', value: v }); }

function updateRunner(r) {
  $('playBtn').textContent = r.running ? '❚❚' : '▶';
  $('playBtn').title = ui(r.running ? 'pause' : 'play');
  $('statusText').textContent = ui(r.running ? 'running' : 'paused');
  $('statusDot').style.background = r.running ? 'var(--lime)' : 'var(--orange)';
  $('speeds').querySelectorAll('button').forEach((b) => b.classList.toggle('active', Number(b.dataset.speed) === r.speed));
  $('achieved').textContent = r.running ? `×${Math.round(r.achieved)}` : '—';
  $('tickP50').textContent = r.tick_ms_p50.toFixed(2);
  $('tickP95').textContent = r.tick_ms_p95.toFixed(2);
}

// ---------------------------------------------------------------------------------------------
async function frameLoop() {
  while (true) {
    const t0 = performance.now();
    try {
      const f = await api.frame();
      if (!app.online) { app.online = true; $('offline').classList.add('hidden'); }
      if (app.cur && f.n === app.cur.n) app.prev = app.cur; else app.prev = null;
      app.frameInterval = app.cur ? Math.max(50, Math.min(400, f.received - app.cur.received)) : 100;
      app.cur = f;
    } catch {
      if (app.online) { app.online = false; $('offline').classList.remove('hidden'); }
      await new Promise((r) => setTimeout(r, 1000));
    }
    await new Promise((r) => setTimeout(r, Math.max(10, 100 - (performance.now() - t0))));
  }
}

async function stateLoop() {
  while (true) {
    try {
      const st = await api.get('/api/state');
      app.state = st;
      updateState(st);
    } catch { /* offline handled by frame loop */ }
    await new Promise((r) => setTimeout(r, 450));
  }
}

async function slowLoop() {
  while (true) {
    try {
      await refreshFilter();
      if (app.selected != null) {
        app.detail = await api.get('/api/agent?id=' + app.selected);
        inspector.showResident(app.detail, app.catalog);
      } else if (app.selectedBuilding != null) {
        inspector.showBuilding(await api.get('/api/building?id=' + app.selectedBuilding));
      }
      if (document.getElementById('tab-charts').classList.contains('active')) await refreshCharts(false);
    } catch { /* ignore */ }
    await new Promise((r) => setTimeout(r, 1000));
  }
}

function render() {
  const now = performance.now();
  const alpha = app.cur && app.prev ? Math.min(1, (now - app.cur.received) / app.frameInterval) : 1;
  if (app.following != null && app.cur && app.following < app.cur.n) {
    const i = app.following;
    const x = app.prev ? app.prev.x[i] + (app.cur.x[i] - app.prev.x[i]) * alpha : app.cur.x[i];
    const y = app.prev ? app.prev.y[i] + (app.cur.y[i] - app.prev.y[i]) * alpha : app.cur.y[i];
    view.centerOn(x, y);
  }
  const related = app.detail && app.detail.relations ? app.detail.relations.map((r) => r.id) : null;
  view.draw(app.prev, app.cur, alpha, {
    events: app.state ? app.state.events : [],
    eventColor: (type) => (app.catalog.events.find((e) => e.id === type) || {}).color || '#ffad66',
    filterBits: app.filterBits,
    selected: app.selected,
    related,
    route: app.detail && app.detail.trip ? [[app.detail.x, app.detail.y], ...app.detail.trip.path] : null,
    building: app.selectedBuilding != null ? app.map.buildings[app.selectedBuilding].polygon : null,
    armed: app.armed, mouse: app.mouse, armedRadius: Number($('evRadius').value) || 60,
    weather: app.state ? app.state.weather : 'clear',
  });
  requestAnimationFrame(render);
}

// ---------------------------------------------------------------------------------------------
function updateState(st) {
  const c = st.clock;
  $('clockTime').textContent = fmtHour(c.hour);
  $('clockWeekday').textContent = weekday(c.weekday);
  $('clockDay').textContent = `${ui('day')} ${c.day}`;
  $('weather').textContent = WEATHER_ICON[st.weather] || '☀';
  $('weather').title = t('weather', st.weather);
  if (st.runner) updateRunner(st.runner);
  const s = st.stats;
  const stats = [
    ['residents', st.population.toLocaleString()], ['outdoors', s.outdoor.toLocaleString()], ['moving', s.moving.toLocaleString()],
    ['groups', s.groups], ['emergencies', s.emergency], ['bus_riders', s.bus_riders],
    ['mean_stress', Math.round(s.stress * 100) + '%'], ['congestion', s.congestion != null ? Math.round(s.congestion * 100) + '%' : '—'],
    ['car_speed', s.car_speed != null ? Math.round(s.car_speed) + ' km/h' : '—'], ['walk_speed', s.walk_speed != null ? s.walk_speed.toFixed(1) + ' km/h' : '—'],
  ];
  $('stats').innerHTML = stats.map(([k, v]) => `<div class="stat"><div class="label">${esc(ui(k))}</div><div class="value">${esc(v)}</div></div>`).join('');
  updateTree(st);
  updateEvents(st);
  $('chaos').value = st.chaos;
  $('chaosOut').textContent = st.chaos;
  $('randomEvents').checked = st.random_events;
}

// ---- Population tree: activity > substate, with counts and checkboxes that filter the map.
function buildTree() {
  const tree = $('tree');
  const open = new Set([...tree.querySelectorAll('.node.open')].map((n) => n.dataset.act));
  tree.innerHTML = app.catalog.activities.map((a, ai) => `
    <div class="node ${open.has(String(ai)) ? 'open' : ''}" data-act="${ai}">
      <div class="row" data-toggle="${ai}"><span class="caret">▸</span><input type="checkbox" data-sel="activity:${a.id}" ${app.treeSel.has('activity:' + a.id) ? 'checked' : ''}>
        <span><i class="swatch" style="display:inline-block;background:${a.color}"></i> ${esc(a.icon)} ${esc(t('activity', a.id))}</span>
        <div class="bar"><i style="width:0;background:${a.color}"></i></div><span class="count" data-count="a${ai}">0</span></div>
      <div class="children">${a.substates.map((s) => `
        <div class="row"><span></span><input type="checkbox" data-sel="substate:${s.id}" ${app.treeSel.has('substate:' + s.id) ? 'checked' : ''}>
          <span>${esc(t('substate', s.id))}</span><div class="bar"><i style="width:0;background:${a.color}99"></i></div><span class="count" data-count="s${s.index}">0</span></div>`).join('')}
      </div>
    </div>`).join('');
  tree.querySelectorAll('[data-toggle]').forEach((r) => (r.onclick = (e) => {
    if (e.target.tagName === 'INPUT') return;
    r.parentElement.classList.toggle('open');
  }));
  tree.querySelectorAll('input[data-sel]').forEach((i) => (i.onchange = () => {
    if (i.checked) app.treeSel.add(i.dataset.sel); else app.treeSel.delete(i.dataset.sel);
    refreshFilter();
  }));
  if (app.state) updateTree(app.state);
}

function updateTree(st) {
  const total = Math.max(1, st.population);
  const tree = $('tree');
  st.activities.forEach((n, ai) => {
    const el = tree.querySelector(`[data-count="a${ai}"]`);
    if (!el) return;
    el.textContent = `${n.toLocaleString()} · ${Math.round((n / total) * 100)}%`;
    el.previousElementSibling.firstElementChild.style.width = `${(n / total) * 100}%`;
  });
  st.substates.forEach((n, si) => {
    const el = tree.querySelector(`[data-count="s${si}"]`);
    if (!el) return;
    el.textContent = n.toLocaleString();
    el.previousElementSibling.firstElementChild.style.width = `${(n / total) * 100}%`;
  });
}

// ---- Filters (evaluated in C++ via /api/query).
const FIELDS = {
  activity: { type: 'select', values: () => app.catalog.activities.map((a) => [a.id, t('activity', a.id)]) },
  substate: { type: 'select', values: () => app.catalog.activities.flatMap((a) => a.substates.map((s) => [s.id, `${t('activity', a.id)} › ${t('substate', s.id)}`])) },
  profile: { type: 'select', values: () => app.catalog.profiles.map((p) => [p, t('profile', p)]) },
  archetype: { type: 'select', values: () => app.catalog.archetypes.map((p) => [p, t('archetype', p)]).sort((a, b) => a[1].localeCompare(b[1])) },
  district: { type: 'select', values: () => app.map.districts.map((d, k) => [k, d.name]) },
  sex: { type: 'select', values: () => [['male', ui('male')], ['female', ui('female')], ['other', ui('other')]] },
  age: { type: 'range', min: 0, max: 100 },
  need: { type: 'named', names: () => app.catalog.needs.map((n) => [n, t('need', n)]), min: 0, max: 1, step: 0.05 },
  trait: { type: 'named', names: () => app.catalog.traits.map((n) => [n, t('trait', n)]), min: 0, max: 1, step: 0.05 },
  mode: { type: 'select', values: () => ['walk', 'bike', 'bus', 'car'].map((m) => [m, t('mode', m)]) },
  place: { type: 'select', values: () => [...new Set(app.catalog.buildings.map((b) => b.category))].map((c) => [c, t('building', app.catalog.buildings.find((b) => b.category === c).id)]) },
  work: { type: 'select', values: () => [...new Set(app.catalog.buildings.map((b) => b.category))].map((c) => [c, t('building', app.catalog.buildings.find((b) => b.category === c).id)]) },
  outdoors: { type: 'bool' }, moving: { type: 'bool' }, group: { type: 'bool' }, emergency: { type: 'bool' },
  relation: { type: 'select', values: () => ['family', 'partner', 'friend', 'coworker', 'classmate', 'neighbour', 'rival'].map((r) => [r, t('relation', r)]) },
  knows_event: { type: 'fixed', value: 'any' },
  distance_km: { type: 'range', min: 0, max: 100, step: 0.5 },
  relations: { type: 'range', min: 0, max: 60 },
};

function buildFilterUi() {
  $('filterField').innerHTML = Object.keys(FIELDS).map((f) => `<option value="${f}">${esc(ui('field_' + f))}</option>`).join('');
  $('filterField').onchange = buildFilterValue;
  $('filterLogic').onchange = refreshFilter;
  $('addRule').onclick = addRule;
  $('clearRules').onclick = () => { app.rules = []; app.treeSel.clear(); buildTree(); renderRules(); refreshFilter(); };
  $('exportCsv').onclick = () => api.download('/api/export', currentFilter() || { rules: [] }, 'residents.csv');
  buildFilterValue();
}

function buildFilterValue() {
  if (!app.catalog) return;
  const f = FIELDS[$('filterField').value];
  const el = $('filterValue');
  if (!f) return;
  if (f.type === 'select') el.innerHTML = `<select class="select" id="fv">${f.values().map(([v, l]) => `<option value="${esc(v)}">${esc(l)}</option>`).join('')}</select>`;
  else if (f.type === 'bool') el.innerHTML = `<select class="select" id="fv"><option value="1">${esc(ui('yes'))}</option><option value="0">${esc(ui('no'))}</option></select>`;
  else if (f.type === 'fixed') el.innerHTML = '';
  else {
    const names = f.type === 'named' ? `<select class="select" id="fname">${f.names().map(([v, l]) => `<option value="${v}">${esc(l)}</option>`).join('')}</select>` : '';
    el.innerHTML = `${names}<div class="grid2"><label class="field small"><span>${esc(ui('min'))}</span><input class="select" id="fmin" type="number" min="${f.min}" max="${f.max}" step="${f.step || 1}"></label>
      <label class="field small"><span>${esc(ui('max'))}</span><input class="select" id="fmax" type="number" min="${f.min}" max="${f.max}" step="${f.step || 1}"></label></div>`;
  }
}

function addRule() {
  const field = $('filterField').value, f = FIELDS[field];
  const rule = { field };
  if (f.type === 'select') rule.value = field === 'district' ? Number($('fv').value) : $('fv').value;
  else if (f.type === 'bool') rule.value = $('fv').value === '1';
  else if (f.type === 'fixed') rule.value = f.value;
  else {
    if (f.type === 'named') rule.name = $('fname').value;
    if ($('fmin').value !== '') rule.min = Number($('fmin').value);
    if ($('fmax').value !== '') rule.max = Number($('fmax').value);
  }
  if ($('filterNegate').checked) rule.negate = true;
  app.rules.push(rule);
  renderRules();
  refreshFilter();
}

function ruleLabel(r) {
  const f = FIELDS[r.field];
  let v = '';
  if (f.type === 'select') v = (f.values().find(([k]) => String(k) === String(r.value)) || [0, r.value])[1];
  else if (f.type === 'bool') v = ui(r.value ? 'yes' : 'no');
  else if (f.type !== 'fixed') v = `${r.name ? t(r.field, r.name) + ' ' : ''}${r.min ?? '…'} – ${r.max ?? '…'}`;
  return `${r.negate ? ui('negate') + ' ' : ''}${ui('field_' + r.field)}${v ? ': ' + v : ''}`;
}

function renderRules() {
  $('rules').innerHTML = app.rules.map((r, k) => `<div class="chip"><span>${esc(ruleLabel(r))}</span><button data-rm="${k}">×</button></div>`).join('');
  $('rules').querySelectorAll('[data-rm]').forEach((b) => (b.onclick = () => { app.rules.splice(Number(b.dataset.rm), 1); renderRules(); refreshFilter(); }));
}

function currentFilter() {
  const rules = [...app.rules];
  const logic = $('filterLogic').value;
  const tree = [...app.treeSel].map((s) => { const [field, value] = s.split(':'); return { field, value }; });
  if (!rules.length && !tree.length) return null;
  if (tree.length) {
    if (!rules.length) return { logic: 'any', rules: tree };
    return { logic: 'all', rules: [{ [logic]: rules }, { any: tree }] };
  }
  return { logic, rules };
}

async function refreshFilter() {
  const filter = currentFilter();
  if (!filter) {
    app.filterBits = null;
    $('filterCount').textContent = '—';
    $('matchTag').classList.add('hidden');
    return;
  }
  const r = await api.query(filter);
  app.filterBits = r.bits;
  $('filterCount').textContent = `${r.matches.toLocaleString()} ${ui('match_tag')}`;
  $('matchTag').textContent = `${r.matches.toLocaleString()} ${ui('match_tag')}`;
  $('matchTag').classList.remove('hidden');
}

// ---- Events
function buildEventUi() {
  const groups = {};
  for (const e of app.catalog.events) (groups[e.category] ||= []).push(e);
  $('eventButtons').innerHTML = CATEGORY_ORDER.filter((c) => groups[c]).map((c) => `
    <div class="evgroup"><div class="gtitle">${esc(ui('cat_' + c))}</div><div class="evbuttons">
      ${groups[c].map((e) => `<button class="evbtn" data-event="${e.id}"><span class="icon" style="background:${e.color}22;color:${e.color}">${esc(e.icon)}</span>${esc(t('event', e.id))}</button>`).join('')}
    </div></div>`).join('');
  $('eventButtons').querySelectorAll('[data-event]').forEach((b) => (b.onclick = () => {
    const type = b.dataset.event;
    const ev = app.catalog.events.find((e) => e.id === type);
    if ($('placeOnMap').checked && !ev.citywide) {
      app.armed = type;
      $('armedTag').textContent = ui('click_to_place') + t('event', type);
      $('armedTag').classList.remove('hidden');
      $('map').classList.add('armed');
    } else injectEvent(type);
  }));
  $('chaos').oninput = (e) => { $('chaosOut').textContent = e.target.value; };
  $('chaos').onchange = (e) => api.post('/api/command', { type: 'set_chaos', value: Number(e.target.value) });
  $('randomEvents').onchange = (e) => api.post('/api/command', { type: 'set_random_events', value: e.target.checked });
}

function injectEvent(type, pos) {
  const cmd = { type: 'inject_event', event: type };
  if (pos) { cmd.x = Math.round(pos[0]); cmd.y = Math.round(pos[1]); }
  for (const [id, key] of [['evSeverity', 'severity'], ['evRadius', 'radius'], ['evDuration', 'duration']])
    if ($(id).value !== '') cmd[key] = Number($(id).value);
  guard(() => api.post('/api/command', cmd)).then((r) => r && toast(`${t('event', type)} ✓`));
}

function disarm() {
  app.armed = null;
  $('armedTag').classList.add('hidden');
  $('map').classList.remove('armed');
}

function eventRow(e, active) {
  const meta = [e.citywide ? ui('citywide') : `r ${e.radius} m`, `${e.perceived} ${ui('perceived')}`, `${e.reacted} ${ui('reacted')}`];
  if (e.crowd_target) meta.push(`${e.crowd}/${e.crowd_target} ${ui('crowd')}`);
  const color = (app.catalog.events.find((x) => x.id === e.type) || {}).color || '#ffad66';
  return `<div class="ev" data-x="${e.x}" data-y="${e.y}" data-city="${e.citywide ? 1 : 0}"><b style="color:${color}">●</b>
    <div class="grow">${esc(t('event', e.type))} <span class="meta">#${e.id} · ${esc(e.start_label)}${e.manual ? ' · ✋' : ''}</span><div class="meta">${esc(meta.join(' · '))}</div></div>
    ${active && e.phase === 'active' ? `<button class="btn small" data-end="${e.id}">${esc(ui('end_event'))}</button>` : ''}</div>`;
}

function updateEvents(st) {
  $('activeEvents').innerHTML = st.events.length ? st.events.map((e) => eventRow(e, true)).join('') : `<div class="empty">${esc(ui('no_events'))}</div>`;
  $('eventHistory').innerHTML = st.event_history.map((e) => eventRow(e, false)).join('');
  document.querySelectorAll('.eventlist .ev').forEach((r) => (r.onclick = (ev) => {
    if (ev.target.dataset.end) return;
    if (r.dataset.city === '0') { view.centerOn(Number(r.dataset.x), Number(r.dataset.y)); if (view.view.scale < 1) view.zoomAt(1.6 / view.view.scale, view.w / 2, view.h / 2); }
  }));
  document.querySelectorAll('[data-end]').forEach((b) => (b.onclick = () => api.post('/api/command', { type: 'end_event', id: Number(b.dataset.end) })));
}

// ---- Charts
function buildCharts() {
  const box = $('charts');
  box.innerHTML = '';
  const acts = app.catalog.activities;
  app.charts = [
    new Chart(box, ui('chart_activities'), acts.map((a, k) => ({ label: t('activity', a.id), color: a.color, values: (d) => d.activity[k] || [] })), { stacked: true }),
    new Chart(box, ui('chart_stress'), [{ key: 'stress', label: t('need', 'stress'), color: '#ff9f5a' }, { key: 'fear', label: t('need', 'fear'), color: '#ff5a68' }], { max: 1, format: (v) => Math.round(v * 100) + '%' }),
    new Chart(box, ui('chart_needs'), ['energy', 'hunger', 'social', 'fun'].map((n, k) => ({ key: n, label: t('need', n), color: ['#8ea0ff', '#ffca70', '#dc9ce8', '#65d6d2'][k] })), { max: 1, format: (v) => Math.round(v * 100) + '%' }),
    new Chart(box, ui('chart_traffic'), [{ key: 'driving', label: t('substate', 'driving'), color: '#ff8a7a' }, { key: 'bus', label: t('substate', 'on_bus'), color: '#ffd166' },
      { key: 'congestion', label: ui('congestion') + ' ×100', color: '#ff5a68', scale: 100 }]),
    new Chart(box, ui('chart_movement'), [{ key: 'walking', label: t('substate', 'walking'), color: '#62d0c8' }, { key: 'cycling', label: t('substate', 'cycling'), color: '#5fd39a' },
      { key: 'outdoor', label: ui('outdoors'), color: '#89b9ef' }, { key: 'groups', label: ui('groups'), color: '#dc9ce8' }]),
    new Chart(box, ui('chart_events'), [{ key: 'events', label: ui('active_events'), color: '#ffad66' }]),
  ];
  if (app.series) app.charts.forEach((c) => c.draw(app.series));
}

async function refreshCharts(force) {
  if (force || !app.series) { app.series = await api.get('/api/timeseries?max=900'); }
  else {
    const t0 = app.series.t.length ? app.series.t[app.series.t.length - 1] : -1;
    const add = await api.get('/api/timeseries?since=' + t0 + '&max=900');
    if (add.t.length) {
      for (const [k, v] of Object.entries(add)) {
        if (k === 'activity') v.forEach((col, i) => app.series.activity[i].push(...col));
        else app.series[k].push(...v);
      }
      if (app.series.t.length > 2400) app.series = await api.get('/api/timeseries?max=900'); // re-decimate
    }
  }
  app.charts.forEach((c) => c.draw(app.series));
}

// ---- Scenarios, runs and maps
function buildScenarioUi() {
  $('ncTemplate').innerHTML = app.info.templates.map((tp) => `<option value="${tp}">${esc(t('template', tp))}</option>`).join('');
  $('ncTemplate').value = app.info.map.template;
  $('ncSize').innerHTML = app.info.sizes.map((s) => `<option value="${s.id}">${esc(t('size', s.id))} (${s.width}×${s.height} m, ~${s.population.toLocaleString()})</option>`).join('');
  $('ncSize').value = app.info.map.size;
  $('ncWeekday').innerHTML = [0, 1, 2, 3, 4, 5, 6].map((d) => `<option value="${d}">${esc(weekday(d))}</option>`).join('');
  $('ncGenerate').onclick = newCity;
  $('saveScenario').onclick = async () => {
    const r = await guard(() => api.post('/api/scenario/save', { name: $('scenarioName').value || 'escenario' }));
    if (r) { toast(`${ui('saved_as')} ${r.file}`); app.info = await api.get('/api/info'); fillScenarioLists(); }
  };
  $('saveRun').onclick = async () => {
    const r = await guard(() => api.post('/api/run/save', { name: $('runName').value || 'partida' }));
    if (r) { toast(`${ui('saved_as')} ${r.file}`); app.info = await api.get('/api/info'); fillScenarioLists(); }
  };
  $('uploadMap').onchange = async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    let map;
    try { map = JSON.parse((await file.text()).replace(/^\s*\/\/.*$/gm, '')); } catch (err) { toast(err.message, true); return; }
    const r = await guard(() => api.post('/api/map/upload', { map }));
    if (r) { toast(`${ui('map_uploaded')}: ${r.file} (${r.buildings})`); app.info = await api.get('/api/info'); fillScenarioLists(); $('ncMap').value = r.file; }
    e.target.value = '';
  };
  fillScenarioLists();
}

function fillScenarioLists() {
  if (!app.info) return;
  $('ncMap').innerHTML = `<option value="">${esc(ui('none'))}</option>` + app.info.custom_maps.map((m) => `<option value="${esc(m.file)}">${esc(m.name)}</option>`).join('');
  $('scenarioList').innerHTML = app.info.scenarios.map((s) => `<div class="file"><span>${esc(s.name)}<br><small>${esc(s.description || s.file)}</small></span>
    <button class="btn small" data-scenario="${esc(s.file)}">${esc(ui('load'))}</button></div>`).join('') || '<div class="empty">—</div>';
  $('runList').innerHTML = app.info.runs.map((r) => `<div class="file"><span>${esc(r.name)}<br><small>${esc(r.end_time || '')} ${esc(r.scenario || '')}</small></span>
    <button class="btn small" data-run="${esc(r.file)}">${esc(ui('load'))}</button><button class="btn small" data-replay="${esc(r.file)}">${esc(ui('replay'))}</button></div>`).join('') || '<div class="empty">—</div>';
  document.querySelectorAll('[data-scenario]').forEach((b) => (b.onclick = () => rebuild('/api/scenario/load', { file: b.dataset.scenario })));
  document.querySelectorAll('[data-run]').forEach((b) => (b.onclick = () => rebuild('/api/run/load', { file: b.dataset.run, replay: false })));
  document.querySelectorAll('[data-replay]').forEach((b) => (b.onclick = async () => {
    const r = await rebuild('/api/run/load', { file: b.dataset.replay, replay: true });
    if (r) toast(ui(r.checksum_ok ? 'replay_ok' : 'replay_fail'), !r.checksum_ok);
  }));
}

async function rebuild(path, body) {
  toast(ui('loading'));
  const r = await guard(() => api.post(path, body));
  if (r) { await loadWorld(); buildScenarioUi(); refreshCharts(true); }
  return r;
}

function newCity() {
  const body = {
    map_template: $('ncTemplate').value, map_size: $('ncSize').value, seed: Number($('ncSeed').value) || 1,
    start_hour: Number($('ncHour').value) || 7, start_weekday: Number($('ncWeekday').value) || 0,
    chaos: Number($('chaos').value), random_events: $('randomEvents').checked,
  };
  if ($('ncPopulation').value) body.population = Number($('ncPopulation').value);
  if ($('ncMap').value) body.map_file = $('ncMap').value;
  rebuild('/api/reset', body);
}

// ---------------------------------------------------------------------------------------------
async function select(id, center = false) {
  app.selected = id;
  app.selectedBuilding = null;
  app.detail = await guard(() => api.get('/api/agent?id=' + id));
  if (app.detail) {
    inspector.showResident(app.detail, app.catalog);
    if (center) view.centerOn(app.detail.x, app.detail.y);
  }
}
async function selectBuilding(id, center = false) {
  app.selected = null;
  app.detail = null;
  app.following = null;
  app.selectedBuilding = id;
  const b = await guard(() => api.get('/api/building?id=' + id));
  if (b) { inspector.showBuilding(b); if (center) view.centerOn(app.map.buildings[id].polygon[0][0], app.map.buildings[id].polygon[0][1]); }
}
function deselect() {
  app.selected = null; app.selectedBuilding = null; app.detail = null; app.following = null;
  inspector.close();
}
app.select = select;
app.selectBuilding = selectBuilding;
app.deselect = deselect;

function bindMapInput() {
  const cv = $('map');
  let drag = null;
  cv.onmousedown = (e) => { drag = { x: e.clientX, y: e.clientY, moved: false }; };
  window.onmouseup = (e) => {
    if (!drag) return;
    const moved = drag.moved;
    drag = null;
    cv.classList.remove('dragging');
    if (moved) return;
    const r = cv.getBoundingClientRect();
    const sx = e.clientX - r.left, sy = e.clientY - r.top;
    if (e.target !== cv) return;
    const [wx, wy] = view.toWorld(sx, sy);
    if (app.armed) { injectEvent(app.armed, [wx, wy]); disarm(); return; }
    const agent = view.pickAgent(sx, sy, app.cur);
    if (agent != null) { select(agent); return; }
    const b = view.pickBuilding(wx, wy);
    if (b != null) selectBuilding(b); else deselect();
  };
  window.onmousemove = (e) => {
    const r = cv.getBoundingClientRect();
    app.mouse = view.toWorld(e.clientX - r.left, e.clientY - r.top);
    if (!drag) return;
    const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
    if (Math.abs(dx) + Math.abs(dy) > 3) { drag.moved = true; cv.classList.add('dragging'); app.following = null; }
    if (drag.moved) { view.pan(dx, dy); drag.x = e.clientX; drag.y = e.clientY; }
  };
  cv.addEventListener('wheel', (e) => {
    e.preventDefault();
    const r = cv.getBoundingClientRect();
    view.zoomAt(e.deltaY < 0 ? 1.18 : 1 / 1.18, e.clientX - r.left, e.clientY - r.top);
  }, { passive: false });
  cv.ondblclick = (e) => { const r = cv.getBoundingClientRect(); view.zoomAt(1.8, e.clientX - r.left, e.clientY - r.top); };
  $('minimap').onclick = (e) => { const r = $('minimap').getBoundingClientRect(); const [x, y] = view.minimapToWorld(e.clientX - r.left, e.clientY - r.top); view.centerOn(x, y); };
  document.addEventListener('keydown', (e) => {
    if (e.target.matches('input, select, textarea')) return;
    if (e.code === 'Space') { e.preventDefault(); control({ action: 'toggle' }); }
    else if (e.key >= '1' && e.key <= '6') setSpeed(SPEEDS[Number(e.key) - 1][0]);
    else if (e.key === '+' || e.key === '=') view.zoomAt(1.3, view.w / 2, view.h / 2);
    else if (e.key === '-') view.zoomAt(1 / 1.3, view.w / 2, view.h / 2);
    else if (e.key === 'f' || e.key === 'F') { if (app.selected != null) { app.following = app.following == null ? app.selected : null; inspector.render(); } }
    else if (e.key === 'Escape') { if (app.armed) disarm(); else deselect(); }
  });
}

init().catch((e) => { console.error(e); $('offline').classList.remove('hidden'); });
