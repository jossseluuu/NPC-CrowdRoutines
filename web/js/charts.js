// Minimal canvas time-series charts (line and stacked area).
import { fmtHour, esc } from './i18n.js';

export class Chart {
  constructor(container, title, series, opts = {}) {
    this.series = series; // [{key, label, color, scale?}]
    this.opts = opts;
    this.el = document.createElement('div');
    this.el.className = 'chart';
    this.el.innerHTML = `<div class="ctitle">${esc(title)}</div><canvas></canvas><div class="clegend"></div>`;
    container.appendChild(this.el);
    this.canvas = this.el.querySelector('canvas');
    this.el.querySelector('.clegend').innerHTML = series.map((s) => `<span><i style="background:${s.color}"></i>${esc(s.label)}</span>`).join('');
  }

  draw(data) {
    const cv = this.canvas, dpr = window.devicePixelRatio || 1;
    const w = cv.clientWidth, h = cv.clientHeight;
    if (!w || !h) return;
    cv.width = w * dpr; cv.height = h * dpr;
    const c = cv.getContext('2d');
    c.setTransform(dpr, 0, 0, dpr, 0, 0);
    c.clearRect(0, 0, w, h);
    const t = data.t || [];
    if (t.length < 2) return;
    const pad = { l: 34, r: 6, t: 6, b: 16 };
    const values = this.series.map((s) => (typeof s.values === 'function' ? s.values(data) : (data[s.key] || [])).map((v) => v * (s.scale || 1)));
    let max = this.opts.max || 0;
    if (!max) {
      if (this.opts.stacked) for (let k = 0; k < t.length; k++) max = Math.max(max, values.reduce((a, v) => a + (v[k] || 0), 0));
      else for (const v of values) for (const x of v) max = Math.max(max, x);
    }
    if (max <= 0) max = 1;
    const X = (k) => pad.l + ((t[k] - t[0]) / (t[t.length - 1] - t[0] || 1)) * (w - pad.l - pad.r);
    const Y = (v) => h - pad.b - (v / max) * (h - pad.t - pad.b);
    c.strokeStyle = '#1f2b36'; c.lineWidth = 1; c.fillStyle = '#7f8e9a'; c.font = '9px Inter, sans-serif';
    for (let g = 0; g <= 2; g++) {
      const v = (max * g) / 2, y = Y(v);
      c.beginPath(); c.moveTo(pad.l, y); c.lineTo(w - pad.r, y); c.stroke();
      c.fillText(this.opts.format ? this.opts.format(v) : (v === 0 || v >= 100 || Number.isInteger(v) ? String(Math.round(v)) : v.toFixed(v < 1 ? 2 : 1)), 2, y + 3);
    }
    const ticks = 4;
    for (let g = 0; g <= ticks; g++) {
      const k = Math.round((g / ticks) * (t.length - 1));
      const sec = t[k], day = Math.floor(sec / 86400);
      c.fillText(`d${day} ${fmtHour((sec % 86400) / 3600)}`, Math.min(w - 52, X(k) - 18), h - 4);
    }
    if (this.opts.stacked) {
      const base = new Array(t.length).fill(0);
      values.forEach((v, si) => {
        c.beginPath();
        for (let k = 0; k < t.length; k++) { const y = Y(base[k] + (v[k] || 0)); k ? c.lineTo(X(k), y) : c.moveTo(X(k), y); }
        for (let k = t.length - 1; k >= 0; k--) c.lineTo(X(k), Y(base[k]));
        c.closePath();
        c.fillStyle = this.series[si].color + 'cc';
        c.fill();
        for (let k = 0; k < t.length; k++) base[k] += v[k] || 0;
      });
    } else {
      values.forEach((v, si) => {
        c.strokeStyle = this.series[si].color; c.lineWidth = 1.5;
        c.beginPath();
        for (let k = 0; k < t.length; k++) { const y = Y(v[k] || 0); k ? c.lineTo(X(k), y) : c.moveTo(X(k), y); }
        c.stroke();
      });
    }
  }
}
