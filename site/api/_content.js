// What the gateway does to a post from the CMS before a device sees it. Pure
// functions, no network: the handler in c.js fetches, this file decides.
//
// A device has a 466 x 466 round glass, a JPEG decoder that only reads
// baseline files, two typefaces with a known set of glyphs and room for a
// few thousand characters. Everything here cuts the CMS's rich content down
// to that.

const crypto = require('node:crypto');

const BUCKET_SECONDS = 7 * 24 * 60 * 60;
const FILE = /^([a-z0-9][a-z0-9-]{0,80})(?:\.([A-Za-z]{2}(?:-[A-Za-z]{2})?))?\.(jpg|json)$/;
const LOCALES = new Set(['pt-BR', 'en', 'es']);

const ART_SIDE = 466;
// Below this the picture is a blur on the glass; the device then shows the
// celebration in type alone.
const ART_MIN_SOURCE = 320;
const SUMMARY_CHARS = 240;
const BODY_CHARS = 3200;

// ---------------------------------------------------------------- requests

// "celebration-martin-of-tours.jpg" or "celebration-martin-of-tours.pt-BR.json"
function parseFile(file) {
  const match = FILE.exec(String(file || ''));
  if (!match) return null;
  const [, slug, locale, kind] = match;
  if (kind === 'jpg' && locale) return null;
  if (kind === 'json' && !LOCALES.has(locale || '')) return null;
  return { file: match[0], slug, locale: locale || null, kind };
}

function bucketAt(nowMs) {
  return Math.floor(nowMs / 1000 / BUCKET_SECONDS);
}

// The same formula as Devices::ContentLinks in the API: the first 32 hex
// digits of HMAC-SHA256(secret, "<file>|<bucket>").
function sign(secret, file, bucket) {
  return crypto.createHmac('sha256', secret).update(`${file}|${bucket}`).digest('hex').slice(0, 32);
}

// True when `s` is the signature the API would have minted for this file in
// the current week, the one before or (clocks differ) the one after.
function verify({ secret, file, t, s, nowMs }) {
  if (!secret || typeof t !== 'string' || typeof s !== 'string') return false;
  if (!/^\d{1,8}$/.test(t) || !/^[0-9a-f]{32}$/.test(s)) return false;
  const bucket = Number(t);
  if (Math.abs(bucket - bucketAt(nowMs)) > 1) return false;
  const expected = Buffer.from(sign(secret, file, bucket));
  const given = Buffer.from(s);
  return expected.length === given.length && crypto.timingSafeEqual(expected, given);
}

// ------------------------------------------------------------------ picture

// The square a round glass shows. Portraits keep their top (faces sit in the
// upper third of a painting); landscapes keep their middle.
function cropSquare(width, height) {
  const side = Math.min(width, height);
  const left = Math.floor((width - side) / 2);
  const top = height > width ? Math.min(Math.floor(height * 0.03), height - side) : Math.floor((height - side) / 2);
  return { left, top, side };
}

// Of the sizes the CMS keeps of a picture, the smallest one that still fills
// the glass; failing that, the largest there is.
function pickSource(image) {
  if (!image || !image.url) return null;
  const candidates = [image, ...Object.values(image.formats || {})]
    .filter((f) => f && f.url && f.width && f.height)
    .map((f) => ({ url: f.url, width: f.width, height: f.height, short: Math.min(f.width, f.height) }))
    .sort((a, b) => a.short - b.short);
  if (!candidates.length) return null;
  return candidates.find((f) => f.short >= ART_SIDE) || candidates[candidates.length - 1];
}

// --------------------------------------------------------------------- text

const ENTITIES = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' ', ndash: '–', mdash: '—', hellip: '…', laquo: '«', raquo: '»', ordm: 'º', ordf: 'ª' };

function decodeEntities(text) {
  return text.replace(/&(#x[0-9a-f]+|#\d+|[a-z]+);/gi, (whole, name) => {
    if (name[0] === '#') {
      const code = name[1].toLowerCase() === 'x' ? parseInt(name.slice(2), 16) : parseInt(name.slice(1), 10);
      return Number.isFinite(code) && code > 0 && code < 0x110000 ? String.fromCodePoint(code) : '';
    }
    const known = ENTITIES[name.toLowerCase()];
    return known === undefined ? whole : known;
  });
}

// The glyphs the firmware's fonts were generated with (compass/tools/make_fonts.sh).
function inFont(code) {
  return (
    (code >= 0x20 && code <= 0x7e) ||
    (code >= 0xa1 && code <= 0xff) ||
    code === 0x2013 || code === 0x2014 ||
    (code >= 0x2018 && code <= 0x201d) ||
    code === 0x2022 || code === 0x2026 || code === 0x2039 || code === 0x203a || code === 0x2212
  );
}

const SUBSTITUTES = {
  ' ': ' ', ' ': ' ', ' ': ' ', ' ': ' ', ' ': ' ', ' ': ' ',
  '‐': '-', '‑': '-', '‒': '-', '―': '—',
  '′': "'", '″': '"', '„': '"', '‟': '"',
  'ł': 'l', 'Ł': 'L', 'đ': 'd', 'Đ': 'D', 'œ': 'oe', 'Œ': 'OE',
  '→': '->', '←': '<-', '†': '+', '­': '', '​': '', '﻿': ''
};

// Anything the device cannot draw is replaced by its nearest plain form, or
// dropped: an unknown glyph shows as an empty box on the glass.
function forDevice(text) {
  let out = '';
  for (const char of String(text)) {
    const code = char.codePointAt(0);
    if (inFont(code)) { out += char; continue; }
    if (char === '\n') { out += '\n'; continue; }
    if (Object.prototype.hasOwnProperty.call(SUBSTITUTES, char)) { out += SUBSTITUTES[char]; continue; }
    // An accented letter outside Latin-1 keeps its base letter.
    const base = char.normalize('NFKD').replace(/[̀-ͯ]/g, '');
    for (const part of base) {
      if (inFont(part.codePointAt(0))) out += part;
    }
  }
  return out;
}

function tidy(text) {
  return forDevice(decodeEntities(text))
    .replace(/\[\d+\]/g, '') // footnote marks left by the sources: "[1][2]"
    .replace(/[ \t\r\n]+/g, ' ')
    .replace(/ ([,.;:!?])/g, '$1')
    .trim();
}

function stripTags(html) {
  return String(html || '').replace(/<br\s*\/?>/gi, ' ').replace(/<[^>]*>/g, '');
}

// The post as a list of headings and paragraphs, in reading order.
function blocks(html) {
  const out = [];
  const pattern = /<(h[1-6]|p|li|blockquote)\b[^>]*>([\s\S]*?)<\/\1>/gi;
  let match;
  let found = false;
  while ((match = pattern.exec(String(html || ''))) !== null) {
    found = true;
    const text = tidy(stripTags(match[2]));
    if (!text) continue;
    const tag = match[1].toLowerCase();
    if (tag[0] === 'h') out.push({ h: text });
    else out.push({ p: tag === 'li' ? `• ${text}` : text });
  }
  if (!found) {
    const text = tidy(stripTags(html));
    if (text) out.push({ p: text });
  }
  return out;
}

// One or two sentences that fit the glass without scrolling.
function summarize(text, limit = SUMMARY_CHARS) {
  const clean = tidy(stripTags(text));
  if (clean.length <= limit) return clean;
  const head = clean.slice(0, limit + 1);
  let end = -1;
  const sentence = /[.!?](?=\s|$)/g;
  let match;
  while ((match = sentence.exec(head)) !== null) {
    if (match.index + 1 >= 80) end = match.index + 1;
  }
  if (end > 0) return head.slice(0, end);
  const space = head.slice(0, limit).lastIndexOf(' ');
  return `${head.slice(0, space > 0 ? space : limit).replace(/[,;: ]+$/, '')}…`;
}

// Keeps whole paragraphs up to `limit` characters and says whether it left
// some out. A heading with nothing under it is dropped.
function clip(list, limit = BODY_CHARS) {
  const out = [];
  let used = 0;
  let more = false;
  for (const block of list) {
    const length = (block.h || block.p).length;
    if (used + length > limit && out.length) { more = true; break; }
    out.push(block);
    used += length;
  }
  while (out.length && out[out.length - 1].h) out.pop();
  return { body: out, more };
}

// What the device stores and shows for one post.
function textFor(post, { slug, locale }) {
  const sources = [];
  if (post.content_type === 'tabs' && Array.isArray(post.tabs)) {
    [...post.tabs].sort((a, b) => (a.order || 0) - (b.order || 0)).forEach((tab) => {
      if (tab.title) sources.push(`<h2>${tab.title}</h2>`);
      sources.push(tab.content || '');
    });
  } else {
    sources.push(post.simple_content || '');
  }
  const { body, more } = clip(blocks(sources.join('\n')));
  const summary = summarize(post.excerpt || (body.find((b) => b.p) || {}).p || '');
  return { v: 1, slug, locale, title: tidy(post.title || ''), summary, body, more };
}

module.exports = {
  ART_SIDE, ART_MIN_SOURCE, BUCKET_SECONDS, LOCALES,
  parseFile, bucketAt, sign, verify, cropSquare, pickSource,
  forDevice, tidy, blocks, summarize, clip, textFor
};
