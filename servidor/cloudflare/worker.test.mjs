import { test } from 'node:test';
import assert from 'node:assert/strict';
import worker, { validOrigin } from './worker.mjs';

const origin = 'https://capri-fixture.trycloudflare.com';
const stable = 'https://capri.example.workers.dev';
const env = { REGISTER_SECRET: 'registration-fixture', FORWARD_SECRET: 'forwarding-fixture',
  CONNECTION: { async get() { return origin; }, async put() {} } };

test('origin permits only HTTPS quick tunnels without credentials or paths', () => {
  assert.ok(validOrigin(origin));
  for (const value of ['http://127.0.0.1', 'https://localhost', 'https://attacker.test',
    'https://user:pass@capri-fixture.trycloudflare.com', origin + '/private', origin + '?a=b', origin + ':8080'])
    assert.equal(validOrigin(value), false);
});
test('registration rejects missing credentials before reaching storage', async () => {
  const result = await worker.fetch(new Request(stable + '/register', { method: 'POST', body: '{}' }), env);
  assert.equal(result.status, 401);
});
test('private files and arbitrary paths are not proxied', async () => {
  for (const path of ['/hillblack.db', '/credentials.dat', '/admin-login.dat', '/health?key=hidden'])
    assert.equal((await worker.fetch(new Request(stable + path), env)).status, 404);
});
test('authentication body is bounded', async () => {
  const request = new Request(stable + '/validate', { method: 'POST',
    headers: { 'Content-Type': 'application/json', 'CF-Connecting-IP': '198.51.100.4' }, body: 'a'.repeat(4097) });
  assert.equal((await worker.fetch(request, env)).status, 400);
});
test('proxy replaces spoofed forwarding headers and does not cache authentication', async () => {
  const old = globalThis.fetch;
  try {
    globalThis.fetch = async (url, options) => {
      assert.equal(url, origin + '/validate');
      assert.equal(options.headers.get('X-Capri-Client-IP'), '198.51.100.4');
      assert.equal(options.headers.get('X-Capri-Gateway'), env.FORWARD_SECRET);
      assert.equal(options.headers.get('Authorization'), null);
      assert.equal(new TextDecoder().decode(options.body), '{"key":"fixture"}');
      return Response.json({ valid: false });
    };
    const request = new Request(stable + '/validate', { method: 'POST', body: '{"key":"fixture"}',
      headers: { 'Content-Type': 'application/json', 'CF-Connecting-IP': '198.51.100.4',
        'X-Capri-Client-IP': '198.51.100.99', 'X-Capri-Gateway': 'forged', Authorization: 'Bearer forged' } });
    const response = await worker.fetch(request, env);
    assert.equal(response.status, 200);
    assert.equal(response.headers.get('Cache-Control'), 'no-store');
    assert.deepEqual(await response.json(), { valid: false });
  } finally { globalThis.fetch = old; }
});
test('upstream redirects never forward credentials', async () => {
  const old = globalThis.fetch;
  try {
    globalThis.fetch = async () => new Response(null, { status: 302, headers: { Location: 'https://attacker.test' } });
    assert.equal((await worker.fetch(new Request(stable + '/health', {
      headers: { 'CF-Connecting-IP': '198.51.100.4' } }), env)).status, 502);
  } finally { globalThis.fetch = old; }
});
