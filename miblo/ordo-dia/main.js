// Ordo: the liturgical day of the person's Ordo account on the Miblo Desk. A page Desk app
// (docs: https://miblo.ai/sdk): run(ctx) is called every manifest.intervalMs and returns
// `{ data }`; page.html paints it (the year ring, the day in its colour, the readings, the four
// offices).
//
// The app links to the account the way any keyboardless device does (estevao-api
// docs/DEVICES.md, RFC 8628): it asks the API for a short code, the person types it in the Ordo
// app (Settings > Devices), and the app collects a token of its own. That token opens exactly one
// read-only route, GET /api/v1/device/today, and lives in ctx.data (the Miblo's sealed store):
// never on screen, never in the log.
//
// Nothing here writes to the account. Completing an office stays in the Ordo app; this screen
// reflects it within refresh.interval_seconds.

// The API and, when it does not answer, its second host. Both are in manifest "network".
const API_BASES = ['https://api.caminhoanglicano.com.br/api/v1', 'https://api.oficio.app/api/v1'];
const CLIENT_ID = 'ordo-miblo';
export const APP_VERSION = '0.1.1';

// How often the day is read before the API says (refresh.interval_seconds), and the back-off
// after a failed call.
export const REFRESH_FALLBACK_MS = 5 * 60_000;
const RETRY_MIN_MS = 30_000;
const RETRY_MAX_MS = 10 * 60_000;
const RATE_LIMITED_MS = 15 * 60_000;
// A new code is asked this long after one that could not be obtained.
const LINK_RETRY_MS = 30_000;
const REQUEST_TIMEOUT_MS = 15_000;

// Where the token lives between runs (ctx.data, a sealed store the Hub keeps per app).
const STORE = { collection: 'link', doc: 'device' };

const OFFICES = ['morning', 'midday', 'evening', 'compline'];

const T = {
  pt: {
    title: 'Oração da Manhã', titleEvening: 'Oração da Tarde',
    remind: (streak) => (streak > 1 ? `Ainda não orada hoje. ${streak} dias seguidos: não deixe passar.` : 'Ainda não orada hoje. Abra o Ordo quando puder.'),
    noApi: 'Sem resposta da API', noApiHint: 'tentando de novo em instantes',
    unknownClient: 'App não cadastrado na API', unknownClientHint: 'o cliente ordo-miblo não existe nesta API do Ordo',
    clientDisabled: 'App desativado', clientDisabledHint: 'este app foi desativado na API do Ordo',
    denied: 'Não foi possível vincular', deniedHint: 'confira no app se a conta tem Ordo+ e menos de 5 aparelhos',
    noStore: 'o vínculo será pedido de novo quando o Hub reiniciar',
  },
  en: {
    title: 'Morning Prayer', titleEvening: 'Evening Prayer',
    remind: (streak) => (streak > 1 ? `Not prayed yet today. ${streak} days in a row: keep it going.` : 'Not prayed yet today. Open Ordo when you can.'),
    noApi: 'No answer from the API', noApiHint: 'trying again shortly',
    unknownClient: 'App not registered on the API', unknownClientHint: 'the ordo-miblo client does not exist on this Ordo API',
    clientDisabled: 'App disabled', clientDisabledHint: 'this app was disabled on the Ordo API',
    denied: 'Could not link', deniedHint: 'check in the app that the account has Ordo+ and fewer than 5 devices',
    noStore: 'the link will be asked again when the Hub restarts',
  },
};
const tr = (lang) => T[lang] ?? T.en;

// ------------------------------------------------------------------ HTTP

// One request against the API, trying the second host when the first does not answer at all
// (a DNS failure, a timeout). An HTTP answer, whatever its status, ends the attempt: a 401 from
// the first host is the API's answer, not a reason to ask the second.
// -> { status, headers, json | null }
async function apiRequest(path, { method = 'GET', headers = {}, body } = {}) {
  let lastError = null;
  for (const base of API_BASES) {
    try {
      const res = await fetch(`${base}${path}`, {
        method,
        headers: {
          accept: 'application/json',
          'user-agent': `OrdoMiblo/${APP_VERSION}`,
          ...(body !== undefined ? { 'content-type': 'application/json' } : {}),
          ...headers,
        },
        ...(body !== undefined ? { body: JSON.stringify(body) } : {}),
        signal: AbortSignal.timeout(REQUEST_TIMEOUT_MS),
      });
      let json = null;
      if (res.status !== 304 && res.status !== 204) {
        try { json = await res.json(); } catch { json = null; }
      }
      return { status: res.status, headers: res.headers, json };
    } catch (e) {
      lastError = e;
    }
  }
  throw lastError ?? new Error('no API host answered');
}

// ----------------------------------------------------------------- store

async function loadLink(ctx) {
  if (!ctx.data?.get) return null;
  try {
    const v = await ctx.data.get(STORE.collection, STORE.doc);
    return v && typeof v.token === 'string' && v.token ? v : null;
  } catch {
    return null;
  }
}

async function saveLink(ctx, link) {
  if (!ctx.data?.put) return false;
  try {
    await ctx.data.put(STORE.collection, STORE.doc, link);
    return true;
  } catch (e) {
    ctx.log?.(`link not saved (${e?.message ?? e})`);
    return false;
  }
}

async function forgetLink(ctx) {
  if (!ctx.data?.delete) return;
  try { await ctx.data.delete(STORE.collection, STORE.doc); } catch { /* nothing to forget */ }
}

// --------------------------------------------------------------- linking

// The link screen's data: the code to type, where, and how long it is still good for.
function linkData(ctx, st, note, hint) {
  const l = st.link;
  return {
    mode: 'link',
    lang: ctx.lang,
    code: l?.userCode ?? null,
    // Seconds the code is still valid for, rounded to the run so the page does not redraw every tick.
    expiresIn: l ? Math.max(0, Math.round((l.expiresAt - ctx.now()) / 30_000) * 30) : null,
    note: note ?? null,
    hint: hint ?? null,
  };
}

async function stepLinking(ctx, st) {
  const t = tr(ctx.lang);
  const now = ctx.now();
  if (!st.link) {
    if (st.linkNextAt && now < st.linkNextAt) return linkData(ctx, st, st.linkNote, st.linkHint);
    try {
      const r = await apiRequest('/device_links', { method: 'POST', body: { client_id: CLIENT_ID } });
      if (r.status === 201 || r.status === 200) {
        const j = r.json ?? {};
        st.link = {
          deviceCode: String(j.device_code ?? ''),
          userCode: String(j.user_code ?? ''),
          expiresAt: now + (Number(j.expires_in) || 600) * 1000,
          intervalMs: Math.max(5, Number(j.interval) || 5) * 1000,
          nextPollAt: now + (Math.max(5, Number(j.interval) || 5) * 1000),
        };
        st.linkNote = st.linkHint = null;
        ctx.log?.(`link code ${st.link.userCode}`);
        return linkData(ctx, st);
      }
      // 400 UNKNOWN_DEVICE_CLIENT or 403 CLIENT_DISABLED: nothing this app can do but say so.
      const code = String(r.json?.code ?? '');
      if (code === 'UNKNOWN_DEVICE_CLIENT') { st.linkNote = t.unknownClient; st.linkHint = t.unknownClientHint; }
      else if (code === 'CLIENT_DISABLED') { st.linkNote = t.clientDisabled; st.linkHint = t.clientDisabledHint; }
      else { st.linkNote = t.noApi; st.linkHint = code ? `${t.noApiHint} (${r.status} ${code})` : `${t.noApiHint} (${r.status})`; }
      ctx.log?.(`link code not obtained: ${r.status} ${code}`);
    } catch {
      st.linkNote = t.noApi;
      st.linkHint = t.noApiHint;
    }
    st.linkNextAt = now + LINK_RETRY_MS;
    return linkData(ctx, st, st.linkNote, st.linkHint);
  }

  const l = st.link;
  if (now >= l.expiresAt) {
    st.link = null;
    st.linkNextAt = 0;
    return linkData(ctx, st);
  }
  if (now < l.nextPollAt) return linkData(ctx, st);
  l.nextPollAt = now + l.intervalMs;

  let r;
  try {
    r = await apiRequest('/device_links/token', { method: 'POST', body: { device_code: l.deviceCode } });
  } catch {
    l.nextPollAt = now + RETRY_MIN_MS;
    return linkData(ctx, st);
  }
  if (r.status === 200 && typeof r.json?.device_token === 'string') {
    const link = { token: r.json.device_token, name: r.json.device?.name ?? null, linkedAt: new Date(now).toISOString() };
    st.link = null;
    st.token = link.token;
    st.etag = null;
    st.today = null;
    st.nextFetchAt = 0;
    const saved = await saveLink(ctx, link);
    ctx.log?.(saved ? 'linked' : `linked; ${t.noStore}`);
    return stepLinked(ctx, st);
  }
  switch (r.json?.error) {
    case 'authorization_pending':
      break;
    case 'slow_down':
      l.intervalMs += 5000;
      l.nextPollAt = now + l.intervalMs;
      break;
    case 'access_denied':
      st.link = null;
      st.linkNote = t.denied;
      st.linkHint = t.deniedHint;
      st.linkNextAt = now + LINK_RETRY_MS;
      break;
    case 'expired_token':
    case 'invalid_grant':
    default:
      // The code is gone either way: a fresh one on the next run.
      st.link = null;
      st.linkNextAt = 0;
      break;
  }
  return linkData(ctx, st, st.linkNote, st.linkHint);
}

// ---------------------------------------------------------------- linked

const celebrationName = (day) => {
  const c = day?.celebration;
  if (typeof c === 'string') return c || null;
  return c && typeof c.name === 'string' && c.name ? c.name : null;
};

const daysBetween = (a, b) => Math.round((Date.parse(`${b}T00:00:00Z`) - Date.parse(`${a}T00:00:00Z`)) / 86_400_000);

// The payload of /device/today as the page draws it: smaller than the payload (16 KB is the
// page's limit), dates turned into fractions of the ring, and only what the screen shows.
export function pageData(today, { lang, showReadings, offline }) {
  const day = today.day ?? {};
  const ring = today.year_ring ?? {};
  const span = ring.from && ring.to ? Math.max(1, daysBetween(ring.from, ring.to) + 1) : 365;
  const frac = (date) => Math.min(1, Math.max(0, daysBetween(ring.from, date) / span));
  const offices = today.offices ?? {};
  const done = Object.fromEntries(OFFICES.map((o) => [o, offices[o]?.done === true]));
  const next = OFFICES.find((o) => !done[o]) ?? null;
  const readings = today.readings ?? {};
  const refs = (list) => (Array.isArray(list) ? list.slice(0, 6).map((r) => ({ slot: String(r.slot ?? ''), ref: String(r.reference ?? '') })) : []);
  return {
    mode: 'day',
    // The owner's language (the API localizes names and weekdays in it), else the Hub's.
    lang: /^[a-z]{2}/i.test(String(today.language ?? '')) ? String(today.language).slice(0, 2).toLowerCase() : lang,
    date: today.date ?? null,
    weekday: day.weekday ?? null,
    // `celebration` is null or { name, type }: a feast or a commemoration
    // names the day and the week goes under it.
    title: celebrationName(day) ?? day.title ?? null,
    subtitle: celebrationName(day) ? day.title ?? null : null,
    season: day.season ?? null,
    color: day.color ?? null,
    hex: day.color_hex ?? null,
    isSunday: day.is_sunday === true,
    fastDay: day.fast_day === true,
    offices: done,
    next,
    streak: Number(today.streak?.current) || 0,
    readings: showReadings ? { daily: refs(readings.daily), morning: refs(readings.morning), evening: refs(readings.evening) } : null,
    ring: ring.from && ring.to ? {
      progress: Number(ring.progress) || 0,
      segments: (Array.isArray(ring.segments) ? ring.segments : []).map((s) => ({ from: frac(s.from), to: frac(s.to), hex: s.color_hex ?? null, season: s.season ?? null })),
      feasts: (Array.isArray(ring.feasts) ? ring.feasts : []).map((f) => ({ at: frac(f.date), hex: f.color_hex ?? null })),
    } : null,
    offline: offline ?? null,
  };
}

function noticeData(ctx, st, mode) {
  return { mode, lang: ctx.lang, offline: null };
}

// The reminders: at the time the person set, an office still not prayed asks for a line on the
// Miblo, once a day each.
function reminders(ctx, st, today) {
  const out = [];
  const date = today?.date;
  if (!date) return out;
  if (st.remindedOn !== date) { st.remindedOn = date; st.reminded = {}; }
  const now = new Date(ctx.now());
  const minutes = now.getHours() * 60 + now.getMinutes();
  const t = tr(ctx.lang);
  const streak = Number(today.streak?.current) || 0;
  for (const [key, office, title] of [['manha', 'morning', t.title], ['tarde', 'evening', t.titleEvening]]) {
    const at = String(ctx.settings?.[key] ?? '').trim();
    const m = /^(\d{1,2}):(\d{2})$/.exec(at);
    if (!m || st.reminded[key]) continue;
    if (minutes < Number(m[1]) * 60 + Number(m[2])) continue;
    st.reminded[key] = true;
    if (today.offices?.[office]?.done === true) continue;
    out.push({ type: key, title, text: t.remind(streak), key: `${key}-${date}` });
  }
  return out;
}

async function stepLinked(ctx, st) {
  const now = ctx.now();
  const due = !st.nextFetchAt || now >= st.nextFetchAt || (st.dayEndsAt && now >= st.dayEndsAt);
  if (due) {
    // The owner's midnight has come: the timer is spent. Before that it is
    // kept, because a 304 (nothing changed) carries no new countdown.
    if (st.dayEndsAt && now >= st.dayEndsAt) st.dayEndsAt = 0;
    let r;
    try {
      r = await apiRequest('/device/today', {
        headers: {
          'x-ordo-device-token': st.token,
          'x-ordo-device-firmware': APP_VERSION,
          ...(st.etag ? { 'if-none-match': st.etag } : {}),
        },
      });
    } catch (e) {
      st.offlineSince ??= now;
      st.retryMs = st.retryMs ? Math.min(st.retryMs * 2, RETRY_MAX_MS) : RETRY_MIN_MS;
      st.nextFetchAt = now + st.retryMs;
      ctx.log?.(`today not fetched (${e?.message ?? e})`);
    }
    if (r) {
      switch (r.status) {
        case 200: {
          const today = r.json;
          if (today && typeof today === 'object' && today.day) {
            const before = st.today ? OFFICES.filter((o) => st.today.offices?.[o]?.done).length : null;
            const after = OFFICES.filter((o) => today.offices?.[o]?.done).length;
            st.today = today;
            st.etag = r.headers.get('etag');
            st.mode = 'day';
            st.offlineSince = null;
            st.retryMs = 0;
            const interval = (Number(today.refresh?.interval_seconds) || REFRESH_FALLBACK_MS / 1000) * 1000;
            st.nextFetchAt = now + interval;
            const ends = Number(today.refresh?.day_ends_in_seconds);
            if (ends > 0) st.dayEndsAt = now + (ends + 2) * 1000;
            // An office just completed: a glance at the screen (the Desk's peek), once.
            st.peek = before !== null && after > before && after <= 4 ? 5 : false;
          } else {
            st.retryMs = st.retryMs ? Math.min(st.retryMs * 2, RETRY_MAX_MS) : RETRY_MIN_MS;
            st.nextFetchAt = now + st.retryMs;
          }
          break;
        }
        case 304:
          st.mode = 'day';
          st.offlineSince = null;
          st.retryMs = 0;
          st.nextFetchAt = now + ((Number(st.today?.refresh?.interval_seconds) || REFRESH_FALLBACK_MS / 1000) * 1000);
          break;
        case 401:
          // Unlinked in the app, or idle for 90 days: back to the code, the token forgotten.
          ctx.log?.('token refused; linking again');
          st.token = null;
          st.today = null;
          st.etag = null;
          st.mode = null;
          await forgetLink(ctx);
          return stepLinking(ctx, st);
        case 403:
          st.mode = r.json?.code === 'CLIENT_DISABLED' ? 'disabled' : 'premium';
          st.nextFetchAt = now + REFRESH_FALLBACK_MS;
          break;
        case 429:
          st.nextFetchAt = now + RATE_LIMITED_MS;
          break;
        default:
          st.offlineSince ??= now;
          st.retryMs = st.retryMs ? Math.min(st.retryMs * 2, RETRY_MAX_MS) : RETRY_MIN_MS;
          st.nextFetchAt = now + st.retryMs;
          break;
      }
    }
  }

  if (st.mode === 'premium' || st.mode === 'disabled') return { data: noticeData(ctx, st, st.mode) };
  if (!st.today) {
    return { data: { mode: 'waiting', lang: ctx.lang, offline: st.offlineSince ? ctx.timeOf(st.offlineSince) : null } };
  }
  const data = pageData(st.today, {
    lang: ctx.lang,
    showReadings: ctx.settings?.leituras !== false,
    offline: st.offlineSince ? ctx.timeOf(st.offlineSince) : null,
  });
  const alerts = reminders(ctx, st, st.today);
  const peek = st.peek;
  st.peek = false;
  return { data, ...(alerts.length ? { alerts } : {}), ...(peek ? { peek } : {}) };
}

// `ctx`: { settings: { leituras, manha, tarde }, state, data, lang, timeOf(ms), now(), log }.
export async function run(ctx) {
  const st = ctx.state;
  if (!st.ready) {
    st.ready = true;
    const link = await loadLink(ctx);
    st.token = link?.token ?? null;
    if (!ctx.data) ctx.log?.(`no ctx.data: ${tr(ctx.lang).noStore}`);
  }
  if (!st.token) return { data: await stepLinking(ctx, st) };
  return stepLinked(ctx, st);
}
