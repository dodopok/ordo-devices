// GET /c/<slug>.jpg                the celebration's picture, 466 x 466, baseline JPEG
// GET /c/<slug>.<locale>.json      its text: summary and body, in the device's glyphs
//
// The picture and the text live in the CMS and stay there. This function is
// the only thing that talks to it on a device's behalf: it holds the CMS
// token, reads one post, and cuts it down to what a round 466 px glass can
// take. Nothing is stored here; Vercel's CDN keeps the answer.
//
// It is not an open door. Every path is signed by the Ordo API, which only
// hands paths to linked devices of Ordo+ accounts (see docs/DEVICES.md in
// estevao-api): `t` is the week the path was minted in and `s` the HMAC of the
// file name and that week. Without the shared secret this answers nothing.

const sharp = require('sharp');
const content = require('./_content.js');

const CMS = (process.env.CMS_BASE_URL || 'https://cms.caminhoanglicano.com.br').replace(/\/+$/, '');
const CMS_TIMEOUT_MS = 8000;
const MAX_SOURCE_BYTES = 6 * 1024 * 1024;

const CACHE_OK = 'public, max-age=3600, s-maxage=86400, stale-while-revalidate=604800';
const CACHE_MISSING = 'public, max-age=600, s-maxage=3600';

function reply(res, status, body, headers = {}) {
  res.statusCode = status;
  for (const [key, value] of Object.entries(headers)) res.setHeader(key, value);
  if (!res.getHeader('Cache-Control')) res.setHeader('Cache-Control', 'no-store');
  if (typeof body === 'string' && !res.getHeader('Content-Type')) res.setHeader('Content-Type', 'text/plain; charset=utf-8');
  res.setHeader('Content-Length', Buffer.byteLength(body));
  res.end(body);
}

async function cmsGet(path, { binary = false } = {}) {
  const headers = {};
  // The token is for the API of the CMS; its uploads are public files.
  if (!binary && process.env.CMS_TOKEN) headers.Authorization = `Bearer ${process.env.CMS_TOKEN}`;
  const response = await fetch(`${CMS}${path}`, { headers, signal: AbortSignal.timeout(CMS_TIMEOUT_MS) });
  if (!response.ok) throw Object.assign(new Error(`CMS answered ${response.status}`), { status: response.status });
  if (!binary) return response.json();
  const length = Number(response.headers.get('content-length') || 0);
  if (length > MAX_SOURCE_BYTES) throw new Error('picture too large');
  const bytes = Buffer.from(await response.arrayBuffer());
  if (bytes.length > MAX_SOURCE_BYTES) throw new Error('picture too large');
  return bytes;
}

async function findPost(slug, locale, { withBody }) {
  const params = new URLSearchParams();
  params.set('filters[slug][$eq]', slug);
  params.set('locale', locale);
  params.set('pagination[pageSize]', '1');
  params.set('populate[featured_image][populate]', '*');
  if (withBody) params.set('populate[tabs]', '*');
  const data = await cmsGet(`/api/posts?${params.toString()}`);
  return Array.isArray(data.data) && data.data.length ? data.data[0] : null;
}

async function art(slug) {
  // The picture is the same in every language; the Portuguese post is the
  // one every celebration has.
  const post = await findPost(slug, 'pt-BR', { withBody: false });
  const image = post && post.featured_image && post.featured_image.image;
  const source = content.pickSource(image);
  if (!source) return { status: 404, reason: 'no-picture' };
  if (source.short < content.ART_MIN_SOURCE) return { status: 404, reason: 'picture-too-small' };
  // Only ever a path inside the CMS's own uploads.
  if (!/^\/uploads\/[A-Za-z0-9._-]+$/.test(source.url)) return { status: 404, reason: 'picture-elsewhere' };

  const original = await cmsGet(source.url, { binary: true });
  const meta = await sharp(original, { failOn: 'none' }).metadata();
  const { left, top, side } = content.cropSquare(meta.width, meta.height);
  const jpeg = await sharp(original, { failOn: 'none' })
    .extract({ left, top, width: side, height: side })
    .resize(content.ART_SIDE, content.ART_SIDE, { kernel: 'lanczos3' })
    .flatten({ background: '#000000' })
    // Baseline, 4:2:0, no progressive scans: the decoder on the device reads
    // nothing else.
    .jpeg({ quality: 76, progressive: false, chromaSubsampling: '4:2:0', mozjpeg: false, optimiseCoding: true })
    .toBuffer();
  return { status: 200, body: jpeg, type: 'image/jpeg' };
}

async function text(slug, locale) {
  let post = await findPost(slug, locale, { withBody: true });
  let served = locale;
  if (!post && locale !== 'pt-BR') {
    post = await findPost(slug, 'pt-BR', { withBody: true });
    served = 'pt-BR';
  }
  if (!post) return { status: 404, reason: 'no-post' };
  const body = JSON.stringify(content.textFor(post, { slug, locale: served }));
  return { status: 200, body, type: 'application/json; charset=utf-8' };
}

module.exports = async function handler(req, res) {
  if (req.method !== 'GET' && req.method !== 'HEAD') {
    return reply(res, 405, 'method not allowed', { Allow: 'GET, HEAD' });
  }
  const secret = process.env.DEVICE_CONTENT_SECRET;
  if (!secret) return reply(res, 503, 'not configured');

  const query = req.query || {};
  // Nothing but the three known parameters: a stray one would make a new
  // cache entry, and a new run of this function, out of every request.
  if (Object.keys(query).some((key) => !['file', 't', 's'].includes(key))) return reply(res, 400, 'bad request');
  const wanted = content.parseFile(query.file);
  if (!wanted) return reply(res, 400, 'bad request');
  if (!content.verify({ secret, file: wanted.file, t: query.t, s: query.s, nowMs: Date.now() })) {
    return reply(res, 403, 'forbidden');
  }

  try {
    const result = wanted.kind === 'jpg' ? await art(wanted.slug) : await text(wanted.slug, wanted.locale);
    if (result.status !== 200) {
      return reply(res, result.status, result.reason, { 'Cache-Control': CACHE_MISSING, 'X-Ordo-Reason': result.reason });
    }
    return reply(res, 200, result.body, { 'Content-Type': result.type, 'Cache-Control': CACHE_OK });
  } catch (error) {
    // The CMS is down or slow: say so without caching it, the device tries
    // again at its next sync.
    console.error('content gateway:', error && error.message);
    return reply(res, 502, 'upstream');
  }
};
