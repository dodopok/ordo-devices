const test = require('node:test');
const assert = require('node:assert/strict');
const crypto = require('node:crypto');

const content = require('../api/_content.js');

const WEEK_MS = content.BUCKET_SECONDS * 1000;

test('file names: a picture has no locale, a text has one the CMS knows', () => {
  assert.deepEqual(content.parseFile('celebration-martin-of-tours.jpg'), {
    file: 'celebration-martin-of-tours.jpg', slug: 'celebration-martin-of-tours', locale: null, kind: 'jpg'
  });
  assert.equal(content.parseFile('celebration-martin-of-tours.pt-BR.json').locale, 'pt-BR');
  assert.equal(content.parseFile('celebration-martin-of-tours.en.json').locale, 'en');
  for (const bad of [
    '', 'x.png', '../etc/passwd.jpg', 'Celebration.jpg', 'a/b.jpg', 'a.jpg.jpg', 'a.json', 'a.fr.json',
    'a.pt-BR.jpg', '-a.jpg', `${'a'.repeat(90)}.jpg`, 'a b.jpg', 'a%2Fb.jpg'
  ]) {
    assert.equal(content.parseFile(bad), null, bad);
  }
});

test('signature: the formula the API uses, and only for the weeks around now', () => {
  const secret = 'shared-secret';
  const file = 'celebration-martin-of-tours.jpg';
  const now = 2954 * WEEK_MS + 12345;
  const s = crypto.createHmac('sha256', secret).update(`${file}|2954`).digest('hex').slice(0, 32);
  assert.equal(content.sign(secret, file, 2954), s);
  assert.equal(content.bucketAt(now), 2954);

  assert.equal(content.verify({ secret, file, t: '2954', s, nowMs: now }), true);
  // Minted last week, or by a clock a moment ahead: still good.
  assert.equal(content.verify({ secret, file, t: '2954', s, nowMs: now + WEEK_MS }), true);
  assert.equal(content.verify({ secret, file, t: '2954', s, nowMs: now - WEEK_MS }), true);
  // Two weeks old: not any more.
  assert.equal(content.verify({ secret, file, t: '2954', s, nowMs: now + 2 * WEEK_MS }), false);

  assert.equal(content.verify({ secret, file: 'celebration-other.jpg', t: '2954', s, nowMs: now }), false);
  assert.equal(content.verify({ secret: 'another', file, t: '2954', s, nowMs: now }), false);
  assert.equal(content.verify({ secret, file, t: '2955', s, nowMs: now }), false);
  assert.equal(content.verify({ secret: '', file, t: '2954', s, nowMs: now }), false);
  assert.equal(content.verify({ secret, file, t: '2954', s: s.toUpperCase(), nowMs: now }), false);
  assert.equal(content.verify({ secret, file, t: '2954', s: s.slice(0, 31), nowMs: now }), false);
  assert.equal(content.verify({ secret, file, t: ['2954'], s, nowMs: now }), false);
  assert.equal(content.verify({ secret, file, t: undefined, s: undefined, nowMs: now }), false);
});

test('signature: the very values the API mints', () => {
  // From estevao-api's spec/requests/api/v1/device/today_spec.rb: the same
  // secret, file and week must give the same digits in Ruby and here.
  const secret = 'a-secret-shared-with-the-gateway';
  assert.equal(content.bucketAt(Date.UTC(2026, 10, 11, 15, 0, 0)), 2966);
  assert.equal(content.sign(secret, 'celebration-martin-of-tours.jpg', 2966), '2889b06f31a72fa01c1cdb4d54729085');
  assert.equal(content.sign(secret, 'celebration-martin-of-tours.pt-BR.json', 2966), '077a169374804c16ab166af006fe4aca');
  assert.equal(content.sign(secret, 'celebration-martin-of-tours.en.json', 2966), '3dc7644b3a2c901e3eec824a5451a540');
});

test('crop: portraits keep their top, landscapes their middle', () => {
  assert.deepEqual(content.cropSquare(960, 1528), { left: 0, top: 45, side: 960 });
  assert.deepEqual(content.cropSquare(1280, 960), { left: 160, top: 0, side: 960 });
  assert.deepEqual(content.cropSquare(500, 500), { left: 0, top: 0, side: 500 });
  // A portrait barely taller than wide cannot start lower than it has room for.
  assert.deepEqual(content.cropSquare(1000, 1010), { left: 0, top: 10, side: 1000 });
});

test('source: the smallest size that fills the glass', () => {
  const image = {
    url: '/uploads/a.jpeg', width: 960, height: 1528,
    formats: {
      thumbnail: { url: '/uploads/thumbnail_a.jpeg', width: 98, height: 156 },
      small: { url: '/uploads/small_a.jpeg', width: 314, height: 500 },
      medium: { url: '/uploads/medium_a.jpeg', width: 471, height: 750 },
      large: { url: '/uploads/large_a.jpeg', width: 628, height: 1000 }
    }
  };
  assert.equal(content.pickSource(image).url, '/uploads/medium_a.jpeg');
  // Nothing fills it: the largest there is, and the caller judges its size.
  assert.equal(content.pickSource({ url: '/uploads/b.webp', width: 235, height: 325, formats: { thumbnail: { url: '/uploads/t_b.webp', width: 113, height: 156 } } }).short, 235);
  assert.equal(content.pickSource(null), null);
  assert.equal(content.pickSource({}), null);
});

test('text: only glyphs the device has', () => {
  assert.equal(content.forDevice('São Martinho — “monge” e bispo…'), 'São Martinho — “monge” e bispo…');
  // ł, ő and ř are not in the fonts and lose their marks; á is, and stays.
  assert.equal(content.forDevice('Wojtyła, Erdős, Dvořák'), 'Wojtyla, Erdos, Dvorák');
  assert.equal(content.forDevice('a b c‑d'), 'a b c-d');
  assert.equal(content.forDevice('Χριστός 聖 ok'), '  ok');
  assert.equal(content.tidy('Leão  Magno[1][2] ,  papa &amp; doutor&nbsp;da Igreja .'), 'Leão Magno, papa & doutor da Igreja.');
});

test('text: headings and paragraphs, in order, without markup', () => {
  const html = '<h2>Vida e Formação</h2><p>Nascido por volta do ano <strong>400</strong> na <em>Toscana</em>[1][2].</p>' +
    '<ul><li>Primeiro</li><li>Segundo</li></ul><h3>Legado</h3><p></p><p>Doutor da Igreja.</p>';
  assert.deepEqual(content.blocks(html), [
    { h: 'Vida e Formação' },
    { p: 'Nascido por volta do ano 400 na Toscana.' },
    { p: '• Primeiro' },
    { p: '• Segundo' },
    { h: 'Legado' },
    { p: 'Doutor da Igreja.' }
  ]);
  assert.deepEqual(content.blocks('texto solto, sem parágrafo'), [{ p: 'texto solto, sem parágrafo' }]);
});

test('summary: whole sentences that fit the glass', () => {
  const short = 'André foi o primeiro discípulo a seguir Jesus Cristo.';
  assert.equal(content.summarize(`<p>${short}</p>`), short);
  const two = `${'A'.repeat(120)} fim da primeira. ${'B'.repeat(150)} fim da segunda.`;
  assert.equal(content.summarize(two), `${'A'.repeat(120)} fim da primeira.`);
  const one = `${'palavra '.repeat(40)}fim.`;
  const cut = content.summarize(one);
  assert.ok(cut.length <= 241 && cut.endsWith('…') && !cut.endsWith(' …'), cut);
});

test('body: whole paragraphs up to the limit, no heading left dangling', () => {
  const list = [{ h: 'Um' }, { p: 'a'.repeat(50) }, { h: 'Dois' }, { p: 'b'.repeat(80) }];
  assert.deepEqual(content.clip(list, 120), { body: [{ h: 'Um' }, { p: 'a'.repeat(50) }], more: true });
  assert.deepEqual(content.clip(list, 1000), { body: list, more: false });
});

test('post: tabs become headings, and the summary falls back to the first paragraph', () => {
  const post = {
    title: 'Santo André - O Primeiro Discípulo', content_type: 'tabs', excerpt: '',
    tabs: [
      { title: 'Legado', order: 2, content: '<p>Padroeiro da Escócia.</p>' },
      { title: 'Vida', order: 1, content: '<p>Pescador da Galileia.</p>' }
    ]
  };
  assert.deepEqual(content.textFor(post, { slug: 'celebration-andrew-the-apostle', locale: 'pt-BR' }), {
    v: 1, slug: 'celebration-andrew-the-apostle', locale: 'pt-BR', title: 'Santo André - O Primeiro Discípulo',
    summary: 'Pescador da Galileia.',
    body: [{ h: 'Vida' }, { p: 'Pescador da Galileia.' }, { h: 'Legado' }, { p: 'Padroeiro da Escócia.' }],
    more: false
  });
});
