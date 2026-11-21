// Map renderer: static city layer (cached per view), dynamic overlays and residents.
import { FLAG } from './api.js';

const ROAD_WIDTH = { street: 7, avenue: 11, path: 3, bridge: 10, promenade: 8 };
const ROAD_COLOR = { street: '#3a4650', avenue: '#4b5964', path: '#4a5747', bridge: '#69707a', promenade: '#56656c' };
const PROFILE_PALETTE = ['#a9dc79', '#eea66c', '#ffca70', '#8ea0ff', '#ff9f9f', '#b48cff', '#89b9ef', '#ff6e75', '#5fd39a',
  '#b89af4', '#d7a8ff', '#c9b38a', '#f09abc', '#65d6d2', '#ffd166', '#8998a8'];
const STRESS_COLORS = ['#58c7b0', '#9ad37a', '#ffd166', '#ff9f5a', '#ff5a68'];
const AGE_COLORS = ['#7ad1ff', '#a9dc79', '#ffca70', '#d7a8ff'];
const SEX_COLORS = ['#67b8f0', '#f08fb6', '#c8e07a'];
const MODE_COLORS = ['#a9dc79', '#65d6d2', '#ffd166', '#ff8a7a'];
const WEATHER_TINT = { rain: 'rgba(60,110,180,0.10)', storm: 'rgba(30,40,90,0.22)', heat: 'rgba(255,140,40,0.08)' };

function shade(hex, f) {
  const n = parseInt(hex.slice(1), 16);
  let r = (n >> 16) & 255, g = (n >> 8) & 255, b = n & 255;
  if (f >= 0) { r += (255 - r) * f; g += (255 - g) * f; b += (255 - b) * f; } else { r *= 1 + f; g *= 1 + f; b *= 1 + f; }
  return `rgb(${r | 0},${g | 0},${b | 0})`;
}

export class MapView {
  constructor(canvas, minimap) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.mini = minimap;
    this.mctx = minimap.getContext('2d');
    this.view = { scale: 0.5, ox: 0, oy: 0 };
    this.layers = { districts: true, buildings: true, roads: true, traffic: false, density: false, events: true, buses: true, indoor: false, labels: true };
    this.colorMode = 'activity';
    this.static = document.createElement('canvas');
    this.staticDirty = true;
    this.map = null;
    this.dpr = window.devicePixelRatio || 1;
    new ResizeObserver(() => this.resize()).observe(canvas.parentElement);
    this.resize();
  }

  setCatalog(catalog) {
    this.catalog = catalog;
    this.activityColor = catalog.activities.map((a) => a.color);
    this.substateColor = [];
    for (const a of catalog.activities)
      a.substates.forEach((s, k) => { this.substateColor[s.index] = shade(a.color, (k / Math.max(1, a.substates.length - 1)) * 0.7 - 0.35); });
    this.buildingColor = Object.fromEntries(catalog.buildings.map((b) => [b.id, b.color]));
    this.districtColor = Object.fromEntries(catalog.districts.map((d) => [d.id, d.color]));
    this.busColors = [];
  }

  setResidents(res) { this.residents = res; }

  setMap(map) {
    this.map = map;
    this.nodes = map.nodes;
    this.lights = new Set(map.lights);
    this.busColors = map.bus_lines.map((l) => l.color);
    this.streetLabels = [];
    // One label per street, on its longest segment.
    const best = new Map();
    map.edges.forEach((e) => {
      if (e[3] < 0) return;
      const a = map.nodes[e[0]], b = map.nodes[e[1]];
      const len = Math.hypot(b[0] - a[0], b[1] - a[1]);
      if (!best.has(e[3]) || best.get(e[3]).len < len) best.set(e[3], { len, a, b });
    });
    for (const [street, s] of best) this.streetLabels.push({ name: map.street_names[street], ...s });
    this.fit();
    this.renderMinimap();
  }

  resize() {
    const r = this.canvas.parentElement.getBoundingClientRect();
    this.dpr = window.devicePixelRatio || 1;
    this.w = Math.max(100, r.width);
    this.h = Math.max(100, r.height);
    this.canvas.width = this.w * this.dpr;
    this.canvas.height = this.h * this.dpr;
    this.static.width = this.canvas.width;
    this.static.height = this.canvas.height;
    this.staticDirty = true;
  }

  fit() {
    if (!this.map) return;
    const s = Math.min(this.w / (this.map.width + 40), this.h / (this.map.height + 40));
    this.view.scale = s;
    this.view.ox = (this.w - this.map.width * s) / 2;
    this.view.oy = (this.h - this.map.height * s) / 2;
    this.staticDirty = true;
  }

  zoomAt(factor, sx, sy) {
    const v = this.view;
    const ns = Math.min(40, Math.max(0.05, v.scale * factor));
    const wx = (sx - v.ox) / v.scale, wy = (sy - v.oy) / v.scale;
    v.scale = ns;
    v.ox = sx - wx * ns;
    v.oy = sy - wy * ns;
    this.staticDirty = true;
  }
  pan(dx, dy) { this.view.ox += dx; this.view.oy += dy; this.staticDirty = true; }
  centerOn(wx, wy) { this.view.ox = this.w / 2 - wx * this.view.scale; this.view.oy = this.h / 2 - wy * this.view.scale; this.staticDirty = true; }
  toWorld(sx, sy) { return [(sx - this.view.ox) / this.view.scale, (sy - this.view.oy) / this.view.scale]; }
  toScreen(wx, wy) { return [wx * this.view.scale + this.view.ox, wy * this.view.scale + this.view.oy]; }

  // ---------------------------------------------------------------------------------------------
  renderStatic() {
    const c = this.static.getContext('2d');
    const { scale, ox, oy } = this.view;
    const m = this.map;
    c.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
    c.fillStyle = '#121b1f';
    c.fillRect(0, 0, this.w, this.h);
    if (!m) return;
    c.setTransform(this.dpr * scale, 0, 0, this.dpr * scale, this.dpr * ox, this.dpr * oy);
    c.fillStyle = '#1a2624';
    c.fillRect(0, 0, m.width, m.height);
    const poly = (pts) => { c.beginPath(); c.moveTo(pts[0][0], pts[0][1]); for (let i = 1; i < pts.length; i++) c.lineTo(pts[i][0], pts[i][1]); c.closePath(); };
    for (const w of m.water) { c.fillStyle = w.kind === 'sea' ? '#14303f' : '#163445'; poly(w.polygon); c.fill(); }
    if (this.layers.districts) for (const b of m.blocks) { c.fillStyle = this.districtColor[b.kind] || '#222'; poly(b.polygon); c.fill(); }
    if (this.layers.roads) {
      c.lineCap = 'round';
      for (const kind of ['path', 'street', 'promenade', 'bridge', 'avenue']) {
        c.strokeStyle = ROAD_COLOR[kind];
        c.lineWidth = ROAD_WIDTH[kind];
        if (kind === 'path') c.setLineDash([3, 3]); else c.setLineDash([]);
        c.beginPath();
        for (const e of m.edges) {
          if (e[2] !== kind) continue;
          const a = m.nodes[e[0]], b = m.nodes[e[1]];
          c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]);
        }
        c.stroke();
      }
      c.setLineDash([]);
      if (scale > 1.2) {
        c.fillStyle = '#ffd16688';
        for (const n of m.lights) { const p = m.nodes[n]; c.beginPath(); c.arc(p[0], p[1], 1.6, 0, 6.283); c.fill(); }
      }
    }
    if (this.layers.buildings) {
      for (const b of m.buildings) {
        const col = this.buildingColor[b.type] || '#555';
        c.fillStyle = b.floors === 0 ? col : col;
        poly(b.polygon); c.fill();
        if (b.floors > 0 && scale > 0.8) { c.strokeStyle = '#ffffff10'; c.lineWidth = 0.6; c.stroke(); }
      }
    }
    if (this.layers.buses) {
      c.lineWidth = Math.max(1.2, 2 / scale);
      m.bus_lines.forEach((l) => {
        c.strokeStyle = l.color + '55';
        c.beginPath();
        l.path.forEach((n, i) => { const p = m.nodes[n]; i ? c.lineTo(p[0], p[1]) : c.moveTo(p[0], p[1]); });
        c.stroke();
        c.fillStyle = l.color;
        for (const n of l.stops) { const p = m.nodes[n]; c.fillRect(p[0] - 2, p[1] - 2, 4, 4); }
      });
    }
    // Labels in screen space.
    if (this.layers.labels) {
      c.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
      c.textAlign = 'center';
      c.font = '600 12px Inter, Segoe UI, sans-serif';
      for (const d of m.districts) {
        const [sx, sy] = this.toScreen(d.center[0], d.center[1]);
        c.fillStyle = '#0008'; c.fillText(d.name, sx + 1, sy + 1);
        c.fillStyle = '#d8e4ea'; c.fillText(d.name, sx, sy);
      }
      if (scale > 1.1) {
        c.font = '10px Inter, Segoe UI, sans-serif';
        c.fillStyle = '#b9c6cc';
        for (const s of this.streetLabels) {
          if (s.len * scale < 90) continue;
          const [x1, y1] = this.toScreen(s.a[0], s.a[1]), [x2, y2] = this.toScreen(s.b[0], s.b[1]);
          let ang = Math.atan2(y2 - y1, x2 - x1);
          if (ang > Math.PI / 2 || ang < -Math.PI / 2) ang += Math.PI;
          c.save(); c.translate((x1 + x2) / 2, (y1 + y2) / 2); c.rotate(ang); c.fillText(s.name, 0, -3); c.restore();
        }
      }
    }
    this.staticDirty = false;
  }

  renderMinimap() {
    const m = this.map, c = this.mctx;
    if (!m) return;
    const s = Math.min(this.mini.width / m.width, this.mini.height / m.height);
    this.miniScale = s;
    this.miniImg = document.createElement('canvas');
    this.miniImg.width = this.mini.width; this.miniImg.height = this.mini.height;
    const g = this.miniImg.getContext('2d');
    g.fillStyle = '#0b1216'; g.fillRect(0, 0, this.mini.width, this.mini.height);
    g.setTransform(s, 0, 0, s, 0, 0);
    for (const w of m.water) { g.fillStyle = '#163445'; g.beginPath(); w.polygon.forEach((p, i) => i ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1])); g.fill(); }
    for (const b of m.blocks) { g.fillStyle = this.districtColor[b.kind] || '#222'; g.beginPath(); b.polygon.forEach((p, i) => i ? g.lineTo(p[0], p[1]) : g.moveTo(p[0], p[1])); g.fill(); }
    g.strokeStyle = '#4b5964'; g.lineWidth = 6;
    g.beginPath();
    for (const e of m.edges) { const a = m.nodes[e[0]], b = m.nodes[e[1]]; g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); }
    g.stroke();
    c.drawImage(this.miniImg, 0, 0);
  }

  // ---------------------------------------------------------------------------------------------
  agentColor(i, f) {
    switch (this.colorMode) {
      case 'substate': return this.substateColor[f.sub[i]] || '#ccc';
      case 'stress': return STRESS_COLORS[Math.min(4, (f.stress[i] / 52) | 0)];
      case 'profile': return this.residents ? PROFILE_PALETTE[this.residents.profile[i] % 16] : '#ccc';
      case 'age': { const a = this.residents ? this.residents.age[i] : 30; return AGE_COLORS[a < 18 ? 0 : a < 35 ? 1 : a < 65 ? 2 : 3]; }
      case 'sex': return this.residents ? SEX_COLORS[this.residents.sex[i]] : '#ccc';
      case 'mode': return this.residents ? MODE_COLORS[this.residents.mode[i]] : '#ccc';
      default: return this.activityColor[f.act[i]] || '#ccc';
    }
  }

  legendEntries(t) {
    switch (this.colorMode) {
      case 'stress': return STRESS_COLORS.map((c, k) => [c, `${k * 20}-${k * 20 + 20}%`]);
      case 'profile': return (this.catalog?.profiles || []).map((p, k) => [PROFILE_PALETTE[k % 16], t('profile', p)]);
      case 'age': return [[AGE_COLORS[0], '<18'], [AGE_COLORS[1], '18-34'], [AGE_COLORS[2], '35-64'], [AGE_COLORS[3], '65+']];
      case 'sex': return [[SEX_COLORS[0], t('ui', 'male')], [SEX_COLORS[1], t('ui', 'female')], [SEX_COLORS[2], t('ui', 'other')]];
      case 'mode': return ['walk', 'bike', 'bus', 'car'].map((m, k) => [MODE_COLORS[k], t('mode', m)]);
      default: return (this.catalog?.activities || []).map((a) => [a.color, t('activity', a.id)]);
    }
  }

  /** Draws everything. prev/cur: frames, alpha: interpolation factor. */
  draw(prev, cur, alpha, o) {
    const c = this.ctx;
    if (this.staticDirty) this.renderStatic();
    c.setTransform(1, 0, 0, 1, 0, 0);
    c.drawImage(this.static, 0, 0);
    if (!this.map || !cur) return;
    const { scale, ox, oy } = this.view;
    c.setTransform(this.dpr * scale, 0, 0, this.dpr * scale, this.dpr * ox, this.dpr * oy);
    const m = this.map;
    // Traffic: load per road segment (255 = closed by an event).
    if (this.layers.traffic && cur.edgeLoad.length === m.edges.length) {
      c.lineCap = 'round';
      for (let e = 0; e < m.edges.length; e++) {
        const code = cur.edgeLoad[e];
        if (code === 0) continue;
        const ed = m.edges[e], a = m.nodes[ed[0]], b = m.nodes[ed[1]];
        c.strokeStyle = code === 255 ? '#ff3d4fcc' : code < 35 ? '#5fd39a99' : code < 80 ? '#ffd166bb' : '#ff6e75cc';
        c.lineWidth = code === 255 ? 6 : 3.5;
        c.beginPath(); c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]); c.stroke();
      }
    } else if (cur.edgeLoad.length === m.edges.length) {
      // Closed roads are always shown.
      c.strokeStyle = '#ff3d4fcc'; c.lineWidth = 5; c.setLineDash([6, 4]);
      c.beginPath();
      for (let e = 0; e < m.edges.length; e++) {
        if (cur.edgeLoad[e] !== 255) continue;
        const ed = m.edges[e], a = m.nodes[ed[0]], b = m.nodes[ed[1]];
        c.moveTo(a[0], a[1]); c.lineTo(b[0], b[1]);
      }
      c.stroke(); c.setLineDash([]);
    }
    const n = cur.n;
    const lerpOk = prev && prev.n === n && alpha < 1;
    const px = (i) => (lerpOk ? prev.x[i] + (cur.x[i] - prev.x[i]) * alpha : cur.x[i]);
    const py = (i) => (lerpOk ? prev.y[i] + (cur.y[i] - prev.y[i]) * alpha : cur.y[i]);
    // Density heatmap of people on the street.
    if (this.layers.density) {
      const cell = 25, gw = Math.ceil(m.width / cell), gh = Math.ceil(m.height / cell);
      const grid = new Uint16Array(gw * gh);
      for (let i = 0; i < n; i++) {
        if (!(cur.flags[i] & FLAG.OUTDOOR)) continue;
        const gx = Math.min(gw - 1, Math.max(0, (cur.x[i] / cell) | 0)), gy = Math.min(gh - 1, Math.max(0, (cur.y[i] / cell) | 0));
        grid[gy * gw + gx]++;
      }
      for (let k = 0; k < grid.length; k++) {
        if (!grid[k]) continue;
        const v = Math.min(1, grid[k] / 25);
        c.fillStyle = `rgba(255,${(200 - v * 170) | 0},80,${0.15 + v * 0.5})`;
        c.fillRect((k % gw) * cell, ((k / gw) | 0) * cell, cell, cell);
      }
    }
    // Events.
    if (this.layers.events && o.events) {
      for (const e of o.events) {
        if (e.citywide) continue;
        const col = o.eventColor(e.type);
        c.beginPath(); c.arc(e.x, e.y, e.radius, 0, 6.283);
        c.fillStyle = col + (e.phase === 'active' ? '26' : '10'); c.fill();
        c.strokeStyle = col + (e.phase === 'active' ? 'cc' : '55'); c.lineWidth = 2 / scale; c.stroke();
      }
    }
    // Residents, batched by colour.
    const size = Math.max(2.4, Math.min(5, 3.2 * Math.sqrt(scale))) / scale;
    const bits = o.filterBits;
    const buckets = new Map();
    const dim = [];
    for (let i = 0; i < n; i++) {
      const fl = cur.flags[i];
      if (fl & FLAG.BUS) continue; // drawn as the bus
      const outdoor = fl & FLAG.OUTDOOR;
      if (!outdoor && !this.layers.indoor) continue;
      if (bits && !(bits[i >> 3] & (1 << (i & 7)))) { dim.push(i); continue; }
      const col = this.agentColor(i, cur);
      let b = buckets.get(col);
      if (!b) { b = []; buckets.set(col, b); }
      b.push(i);
    }
    if (dim.length) {
      c.fillStyle = '#7b8a9433';
      for (const i of dim) c.fillRect(px(i) - size / 2, py(i) - size / 2, size, size);
    }
    for (const [col, list] of buckets) {
      c.fillStyle = col;
      for (const i of list) {
        const fl = cur.flags[i];
        let s = size;
        if (!(fl & FLAG.OUTDOOR)) { c.globalAlpha = 0.45; s = size * 0.75; }
        else if (fl & FLAG.CAR) s = size * 1.7;
        c.fillRect(px(i) - s / 2, py(i) - s / 2, s, s);
        c.globalAlpha = 1;
      }
    }
    // Buses.
    if (this.layers.buses) {
      const bw = Math.max(9, 7 * Math.sqrt(scale)) / scale;
      for (const b of cur.buses) {
        c.fillStyle = this.busColors[b.line] || '#ffd43b';
        c.fillRect(b.x - bw / 2, b.y - bw / 3, bw, bw / 1.5);
        c.strokeStyle = '#0a0e13'; c.lineWidth = 1 / scale; c.strokeRect(b.x - bw / 2, b.y - bw / 3, bw, bw / 1.5);
      }
    }
    // Selection and route.
    if (o.route && o.route.length > 1) {
      c.strokeStyle = '#ffffffaa'; c.lineWidth = 2 / scale; c.setLineDash([5 / scale, 4 / scale]);
      c.beginPath(); o.route.forEach((p, k) => (k ? c.lineTo(p[0], p[1]) : c.moveTo(p[0], p[1]))); c.stroke(); c.setLineDash([]);
    }
    if (o.selected != null && o.selected < n) {
      const x = px(o.selected), y = py(o.selected);
      c.strokeStyle = '#fff'; c.lineWidth = 2 / scale;
      c.beginPath(); c.arc(x, y, 9 / scale, 0, 6.283); c.stroke();
      if (o.related) {
        c.strokeStyle = '#dc9ce855'; c.lineWidth = 1 / scale;
        for (const j of o.related) if (j < n) { c.beginPath(); c.moveTo(x, y); c.lineTo(px(j), py(j)); c.stroke(); }
      }
    }
    if (o.building) {
      c.strokeStyle = '#65d6d2'; c.lineWidth = 2.5 / scale;
      c.beginPath(); o.building.forEach((p, k) => (k ? c.lineTo(p[0], p[1]) : c.moveTo(p[0], p[1]))); c.closePath(); c.stroke();
    }
    if (o.armed && o.mouse) {
      c.strokeStyle = '#ffad66'; c.lineWidth = 2 / scale; c.setLineDash([6 / scale, 4 / scale]);
      c.beginPath(); c.arc(o.mouse[0], o.mouse[1], o.armedRadius || 60, 0, 6.283); c.stroke(); c.setLineDash([]);
    }
    // Weather tint.
    c.setTransform(1, 0, 0, 1, 0, 0);
    if (WEATHER_TINT[o.weather]) { c.fillStyle = WEATHER_TINT[o.weather]; c.fillRect(0, 0, this.canvas.width, this.canvas.height); }
    // Minimap viewport.
    if (this.miniImg) {
      const mc = this.mctx, s = this.miniScale;
      mc.drawImage(this.miniImg, 0, 0);
      const [wx0, wy0] = this.toWorld(0, 0), [wx1, wy1] = this.toWorld(this.w, this.h);
      mc.strokeStyle = '#65d6d2'; mc.lineWidth = 1.5;
      mc.strokeRect(wx0 * s, wy0 * s, (wx1 - wx0) * s, (wy1 - wy0) * s);
    }
  }

  pickAgent(sx, sy, frame) {
    if (!frame) return null;
    let best = null, bd = 12 * 12;
    for (let i = 0; i < frame.n; i++) {
      if (!(frame.flags[i] & FLAG.OUTDOOR) && !this.layers.indoor) continue;
      const [x, y] = this.toScreen(frame.x[i], frame.y[i]);
      const d = (x - sx) ** 2 + (y - sy) ** 2;
      if (d < bd) { bd = d; best = i; }
    }
    return best;
  }

  pickBuilding(wx, wy) {
    if (!this.map) return null;
    const bs = this.map.buildings;
    for (let k = bs.length - 1; k >= 0; k--) {
      const p = bs[k].polygon;
      let inside = false;
      for (let i = 0, j = p.length - 1; i < p.length; j = i++)
        if ((p[i][1] > wy) !== (p[j][1] > wy) && wx < ((p[j][0] - p[i][0]) * (wy - p[i][1])) / (p[j][1] - p[i][1]) + p[i][0]) inside = !inside;
      if (inside) return k;
    }
    return null;
  }

  minimapToWorld(mx, my) { return [mx / this.miniScale, my / this.miniScale]; }
}
