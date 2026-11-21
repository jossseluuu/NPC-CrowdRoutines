// Resident / building inspector panel.
import { t, ui, fmtHour, esc, weekday } from './i18n.js';

const f2 = (v) => Number(v).toFixed(2);
const NEED_COLOR = { stress: '#ff9f5a', fear: '#ff5a68', health: '#ff9f9f' };

export class Inspector {
  constructor(el, app) {
    this.el = el;
    this.app = app;
    this.tab = 'summary';
    this.data = null;
    this.kind = null;
  }

  close() { this.el.classList.add('hidden'); this.data = null; this.kind = null; }

  showResident(d, catalog) {
    this.kind = 'resident';
    this.data = d;
    this.catalog = catalog;
    this.render();
  }

  showBuilding(d) {
    this.kind = 'building';
    this.data = d;
    this.render();
  }

  render() {
    const d = this.data;
    if (!d) return;
    this.el.classList.remove('hidden');
    if (this.kind === 'building') return this.renderBuilding(d);
    const following = this.app.following === d.id;
    const tabs = ['summary', 'agenda', 'history', 'relations', 'actions', 'why'];
    this.el.innerHTML = `
      <div class="ihead">
        <div class="grow"><div class="label muted">${esc(ui('inspector'))} #${d.id}</div>
          <div class="name">${esc(d.name)}</div>
          <div class="state">${esc(t('activity', d.activity))} › ${esc(t('substate', d.substate))} <span class="muted">· ${esc(d.since_label)}</span></div></div>
        <button class="btn small" data-act="follow">${esc(ui(following ? 'unfollow' : 'follow'))}</button>
        <button class="btn small" data-act="close" title="Esc">×</button>
      </div>
      <div class="itabs">${tabs.map((k) => `<button data-tab="${k}" class="${this.tab === k ? 'active' : ''}">${esc(ui('tab_' + k))}</button>`).join('')}</div>
      <div class="ibody">${this['tab_' + this.tab](d)}</div>`;
    this.el.querySelector('[data-act=close]').onclick = () => this.app.deselect();
    this.el.querySelector('[data-act=follow]').onclick = () => { this.app.following = following ? null : d.id; this.render(); };
    this.el.querySelectorAll('.itabs button').forEach((b) => (b.onclick = () => { this.tab = b.dataset.tab; this.render(); }));
    this.el.querySelectorAll('[data-resident]').forEach((r) => (r.onclick = () => this.app.select(Number(r.dataset.resident), true)));
    this.el.querySelectorAll('[data-building]').forEach((r) => (r.onclick = () => this.app.selectBuilding(Number(r.dataset.building), true)));
  }

  place(p) {
    if (!p) return `<i class="muted">${esc(ui('street'))}</i>`;
    return `<a href="#" data-building="${p.id}">${esc(t('building', p.type))} #${p.id}</a> <span class="muted">· ${esc(p.district)}</span>`;
  }

  meters(obj, section, invertGood) {
    return Object.entries(obj).map(([k, v]) => `
      <div class="meter"><span>${esc(t(section, k))}</span><div class="track"><i style="width:${Math.round(v * 100)}%;background:${section === 'need' ? NEED_COLOR[k] || '#65d6d2' : '#89b9ef'}"></i></div><span class="num">${Math.round(v * 100)}</span></div>`).join('');
  }

  tab_summary(d) {
    const days = d.schedule.workdays.map((w, i) => (w ? weekday(i).slice(0, 2) : '')).filter(Boolean).join(' ') || '—';
    const trip = d.trip ? `<div class="kv"><span>${esc(ui('trip'))}</span><b>${this.place(d.trip.target)} · ${esc(t('activity', d.trip.pending))} (${d.trip.legs.map((l) => esc(l)).join(' → ')})</b></div>` : '';
    return `
      <div class="kv">
        <span>${esc(ui('age'))}</span><b>${d.age} ${esc(ui('years'))} · ${esc(ui(d.sex))}</b>
        <span>${esc(ui('profile'))}</span><b>${esc(t('profile', d.profile))}</b>
        <span>${esc(ui('archetypes'))}</span><b>${esc(t('archetype', d.archetype))} <span class="muted">+ ${esc(t('archetype', d.archetype2))}</span></b>
        <span>${esc(ui('location'))}</span><b>${this.place(d.location)}</b>
        <span>${esc(ui('home'))}</span><b>${this.place(d.home)}</b>
        <span>${esc(ui('workplace'))}</span><b>${d.work ? this.place(d.work) : '—'}</b>
        <span>${esc(ui('commute'))}</span><b>${esc(t('mode', d.commute))}</b>
        <span>${esc(ui('schedule'))}</span><b>${esc(ui('wake'))} ${fmtHour(d.schedule.wake)} · ${esc(ui('sleep'))} ${fmtHour(d.schedule.sleep)}</b>
        <span>${esc(ui('workdays'))}</span><b>${esc(days)}${d.schedule.work_hours ? ` · ${fmtHour(d.schedule.work_start)} (${d.schedule.work_hours.toFixed(1)} h)` : ''}</b>
        <span>${esc(ui('distance'))}</span><b>${Number(d.distance_km).toFixed(1)} km</b>
        ${d.partner_name ? `<span>☺</span><b><a href="#" data-resident="${d.partner}">${esc(d.partner_name)}</a></b>` : ''}
        ${d.group ? `<span>${esc(ui('groups'))}</span><b>${d.group.map((g) => `<a href="#" data-resident="${g}">#${g}</a>`).join(' ')}</b>` : ''}
        ${d.known_events.length ? `<span>${esc(ui('known_events'))}</span><b>${d.known_events.map((e) => '#' + e).join(' ')}</b>` : ''}
      </div>
      ${trip}
      <div class="sectitle">${esc(ui('needs'))}</div>${this.meters(d.needs, 'need')}
      <div class="sectitle">${esc(ui('traits'))}</div>${this.meters(d.traits, 'trait')}`;
  }

  tab_agenda(d) {
    const hour = this.app.state ? this.app.state.clock.hour : 0;
    const color = (a) => (this.catalog.activities.find((x) => x.id === a) || {}).color || '#888';
    const bars = d.agenda.map((it) => {
      const s = Math.max(0, it.start), e = Math.min(24, it.end);
      if (e <= 0 || s >= 24) return '';
      return `<i style="left:${(s / 24) * 100}%;width:${((e - s) / 24) * 100}%;background:${color(it.activity)}" title="${esc(t('activity', it.activity))}"></i>`;
    }).join('');
    const rows = d.agenda.map((it) => {
      const now = hour >= it.start && hour < it.end;
      return `<div class="slot ${now ? 'now' : ''}"><span>${fmtHour(it.start)}–${fmtHour(it.end)}</span>
        <span><b style="color:${color(it.activity)}">■</b> ${esc(t('activity', it.activity))}<span class="tag">${esc(ui(it.hard ? 'obligation' : 'plan'))}</span>${now ? `<span class="tag">${esc(ui('agenda_now'))}</span>` : ''}
        ${it.place ? `<br><small class="muted">${this.place(it.place)}</small>` : ''}</span></div>`;
    }).join('');
    return `<div class="dayline">${bars}<span class="nowmark" style="left:${(hour / 24) * 100}%"></span></div><div class="agenda">${rows}</div>`;
  }

  tab_history(d) {
    return `<div class="hist">${d.history.map((h) => `<div class="h"><small>${esc(h.label)}</small><span>${esc(t('activity', h.activity))} › ${esc(t('substate', h.substate))}${h.place ? ` <small>· ${esc(t('building', h.place.type))} #${h.place.id}</small>` : ''}</span></div>`).join('')}</div>`;
  }

  tab_relations(d) {
    if (!d.relations.length) return `<div class="empty">—</div>`;
    return d.relations.map((r) => `<div class="rel" data-resident="${r.id}"><span><b>${esc(r.name)}</b> <small>· ${esc(t('relation', r.kind))}</small></span><small>${Math.round(r.bond * 100)}%</small>
      <small>${esc(t('activity', r.activity))} › ${esc(t('substate', r.substate))}</small></div>`).join('');
  }

  tab_actions(d) {
    if (!d.actions.length) return `<div class="empty">—</div>`;
    return `<div class="hist">${d.actions.map((a) => {
      let extra = '';
      if (a.other_name) extra = ` <a href="#" data-resident="${a.other}">${esc(a.other_name)}</a>`;
      if (a.place) extra = ' ' + this.place(a.place);
      if (a.event_type) extra += `${a.kind === 'told_about' ? ' →' : ''} ${esc(t('event', a.event_type))} #${a.event}`;
      if (a.line) extra += ` ${esc(a.line)}`;
      return `<div class="h"><small>${esc(a.label)}</small><span>${esc(t('action', a.kind))}${extra}</span></div>`;
    }).join('')}</div>`;
  }

  tab_why(d) {
    const rows = d.explain.filter((e) => e.score !== null).sort((a, b) => b.score - a.score);
    const max = Math.max(1, ...rows.map((r) => Math.abs(r.score)));
    const color = (a) => (this.catalog.activities.find((x) => x.id === a) || {}).color || '#888';
    return `<div class="help">${esc(ui('why_help'))}</div><div class="why">${rows.map((r, k) => {
      const w = (Math.abs(r.score) / max) * 50;
      const left = r.score >= 0 ? 50 : 50 - w;
      return `<div class="w ${k === 0 ? 'best' : ''}"><span>${esc(t('activity', r.activity))}</span>
        <div class="track"><i style="left:${left}%;width:${w}%;background:${color(r.activity)}"></i></div><span class="num">${r.score.toFixed(2)}</span>
        <span class="terms">${esc(ui('term_needs'))} ${f2(r.needs)} · ${esc(ui('term_schedule'))} ${f2(r.schedule)} · ${esc(ui('term_timing'))} ${f2(r.timing)} · ${esc(ui('term_travel'))} −${f2(Math.abs(r.travel))} · ×${f2(r.personality)}</span></div>`;
    }).join('')}</div>`;
  }

  renderBuilding(b) {
    this.el.innerHTML = `
      <div class="ihead"><div class="grow"><div class="label muted">${esc(ui('building_inspector'))} #${b.id}</div>
        <div class="name">${esc(t('building', b.type))}</div><div class="state">${esc(b.district)}</div></div>
        <button class="btn small" data-act="close">×</button></div>
      <div class="ibody">
        <div class="kv">
          <span>${esc(ui('open'))}</span><b>${b.closed_by_event ? esc(ui('closed_by_event')) : esc(ui(b.open ? 'open' : 'closed'))}</b>
          <span>${esc(ui('occupancy'))}</span><b>${b.occupancy}</b>
          <span>${esc(ui('floors'))}</span><b>${b.floors}</b>
          ${b.households ? `<span>${esc(ui('households'))}</span><b>${b.households}</b><span>${esc(ui('residents_here'))}</span><b>${b.residents}</b>` : ''}
          ${b.jobs ? `<span>${esc(ui('jobs'))}</span><b>${b.jobs}</b><span>${esc(ui('workers_here'))}</span><b>${b.workers}</b>` : ''}
          ${b.students ? `<span>${esc(ui('students'))}</span><b>${b.students}</b>` : ''}
        </div>
        <div class="sectitle">${esc(ui('inside'))}</div>
        ${b.inside.length ? b.inside.map((p) => `<div class="rel" data-resident="${p.id}"><span>${esc(p.name)}</span><small>${esc(t('substate', p.substate))}</small></div>`).join('') : '<div class="empty">—</div>'}
      </div>`;
    this.el.querySelector('[data-act=close]').onclick = () => this.app.deselect();
    this.el.querySelectorAll('[data-resident]').forEach((r) => (r.onclick = () => this.app.select(Number(r.dataset.resident), true)));
  }
}
