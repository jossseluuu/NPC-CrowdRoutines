// Thin HTTP client for the C++ engine (see docs/reference/api.md).

async function request(path, body) {
  const opts = body === undefined ? {} : { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) };
  const res = await fetch(path, opts);
  const text = await res.text();
  let data;
  try { data = text ? JSON.parse(text) : {}; } catch { data = { error: text }; }
  if (!res.ok) throw new Error(data.error || res.statusText);
  return data;
}

/** Decodes the binary frame produced by Simulation::frame_binary(). */
export function parseFrame(buf) {
  const dv = new DataView(buf);
  if (dv.getUint8(0) !== 78 || dv.getUint8(3) !== 70) throw new Error('bad frame');
  let o = 8;
  const t = dv.getFloat64(o, true); o += 8;
  const n = dv.getUint32(o, true); o += 4;
  const x = new Float32Array(n), y = new Float32Array(n);
  const act = new Uint8Array(n), sub = new Uint8Array(n), flags = new Uint8Array(n), stress = new Uint8Array(n);
  for (let i = 0; i < n; i++, o += 12) {
    x[i] = dv.getFloat32(o, true);
    y[i] = dv.getFloat32(o + 4, true);
    act[i] = dv.getUint8(o + 8);
    sub[i] = dv.getUint8(o + 9);
    flags[i] = dv.getUint8(o + 10);
    stress[i] = dv.getUint8(o + 11);
  }
  const nb = dv.getUint32(o, true); o += 4;
  const buses = [];
  for (let b = 0; b < nb; b++, o += 12)
    buses.push({ x: dv.getFloat32(o, true), y: dv.getFloat32(o + 4, true), line: dv.getUint16(o + 8, true), passengers: dv.getUint16(o + 10, true) });
  const ne = dv.getUint32(o, true); o += 4;
  const edgeLoad = new Uint8Array(buf, o, ne);
  return { t, n, x, y, act, sub, flags, stress, buses, edgeLoad, received: performance.now() };
}

export const FLAG = { OUTDOOR: 1, MOVING: 2, GROUP: 4, EMERGENCY: 8, CAR: 16, BUS: 32, BIKE: 64 };

export const api = {
  get: (path) => request(path),
  post: (path, body) => request(path, body ?? {}),
  async frame() {
    const res = await fetch('/api/frame');
    if (!res.ok) throw new Error('frame ' + res.status);
    return parseFrame(await res.arrayBuffer());
  },
  async query(filter) {
    const res = await fetch('/api/query', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(filter) });
    if (!res.ok) throw new Error((await res.json()).error);
    return { bits: new Uint8Array(await res.arrayBuffer()), matches: Number(res.headers.get('X-Matches') || 0) };
  },
  async download(path, body, filename) {
    const res = await fetch(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
    const blob = await res.blob();
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; a.download = filename; a.click();
    URL.revokeObjectURL(url);
  },
};
