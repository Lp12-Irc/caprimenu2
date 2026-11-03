const encoder = new TextEncoder();
const json = (value, status = 200) => Response.json(value, {
  status, headers: { 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff' }
});

export function validOrigin(value) {
  if (typeof value !== 'string') return false;
  try {
    const u = new URL(value);
    return u.protocol === 'https:' && /^[a-z0-9-]+\.trycloudflare\.com$/.test(u.hostname)
      && !u.username && !u.password && !u.port && u.pathname === '/' && !u.search && !u.hash;
  } catch { return false; }
}

async function equalSecret(provided, expected) {
  if (!expected || provided.length > 256) return false;
  const key = await crypto.subtle.importKey('raw', encoder.encode('capri-token-comparison-v1'),
    { name: 'HMAC', hash: 'SHA-256' }, false, ['sign', 'verify']);
  const signature = await crypto.subtle.sign('HMAC', key, encoder.encode(expected));
  return crypto.subtle.verify('HMAC', key, signature, encoder.encode(provided));
}

async function boundedBody(request, limit) {
  if (Number(request.headers.get('Content-Length') || 0) > limit) throw Error('body_limit');
  if (!request.body) return new Uint8Array();
  const reader = request.body.getReader();
  const chunks = []; let size = 0;
  try {
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      size += value.byteLength;
      if (size > limit) { await reader.cancel(); throw Error('body_limit'); }
      chunks.push(value);
    }
  } finally { reader.releaseLock(); }
  const body = new Uint8Array(size); let offset = 0;
  for (const chunk of chunks) { body.set(chunk, offset); offset += chunk.byteLength; }
  return body;
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.search) return json({ error: 'not_found' }, 404);
    try {
      if (url.pathname === '/register' && request.method === 'POST') {
        const token = (request.headers.get('Authorization') || '').replace(/^Bearer /, '');
        if (!(await equalSecret(token, env.REGISTER_SECRET))) return json({ error: 'unauthorized' }, 401);
        const payload = JSON.parse(new TextDecoder().decode(await boundedBody(request, 512)));
        if (!validOrigin(payload.origin)) return json({ error: 'invalid_origin' }, 400);
        const origin = new URL(payload.origin).origin;
        const check = await fetch(origin + '/health', { redirect: 'manual', signal: AbortSignal.timeout(8000) });
        if (check.status !== 200 || !check.headers.get('content-type')?.includes('application/json'))
          return json({ error: 'origin_unavailable' }, 503);
        const data = JSON.parse(new TextDecoder().decode(await boundedBody(check, 1024)));
        if (data.service !== 'capri-license-api' || data.status !== 'ok') return json({ error: 'invalid_service' }, 400);
        // Repeated starts do not consume another KV write when the tunnel is unchanged.
        if (await env.CONNECTION.get('origin') !== origin) await env.CONNECTION.put('origin', origin);
        return json({ ok: true });
      }
      const get = request.method === 'GET' && (['/health', '/admin/release'].includes(url.pathname)
        || /^\/packages\/[a-f0-9]{64}\.zip$/.test(url.pathname));
      const post = request.method === 'POST' && ['/validate', '/admin/login', '/admin/action'].includes(url.pathname);
      if (!get && !post) return json({ error: 'not_found' }, 404);
      if (post && request.headers.get('Content-Type')?.split(';')[0].trim() !== 'application/json')
        return json({ error: 'invalid_request' }, 400);
      const origin = await env.CONNECTION.get('origin', { cacheTtl: 30 });
      if (!validOrigin(origin)) return json({ valid: false, reason: 'service_unavailable' }, 503);
      const headers = new Headers();
      headers.set('Accept', 'application/json');
      if (post) headers.set('Content-Type', 'application/json');
      // Only edge-observed IPs are forwarded; visitor-supplied gateway headers are discarded.
      const ip = request.headers.get('CF-Connecting-IP');
      if (!ip || !env.FORWARD_SECRET) return json({ error: 'invalid_connection' }, 503);
      headers.set('X-Capri-Client-IP', ip);
      headers.set('X-Capri-Gateway', env.FORWARD_SECRET);
      if (url.pathname === '/admin/action' && request.headers.has('Authorization'))
        headers.set('Authorization', request.headers.get('Authorization'));
      const body = post ? await boundedBody(request, 4096) : undefined;
      const upstream = await fetch(origin + url.pathname, {
        method: request.method, headers, body, redirect: 'manual',
        signal: AbortSignal.timeout(url.pathname.startsWith('/packages/') ? 120000 : 10000)
      });
      if (upstream.status >= 300 && upstream.status < 400) return json({ error: 'upstream_redirect' }, 502);
      const responseHeaders = new Headers({ 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff' });
      for (const name of ['Content-Type', 'Content-Length', 'Content-Disposition'])
        if (upstream.headers.has(name)) responseHeaders.set(name, upstream.headers.get(name));
      // Package bytes are streamed unchanged; existing SHA-256 verification stays effective.
      return new Response(upstream.body, { status: upstream.status, headers: responseHeaders });
    } catch (error) {
      if (error instanceof SyntaxError || error.message === 'body_limit') return json({ error: 'invalid_request' }, 400);
      console.error(JSON.stringify({ event: 'origin_request_failed', kind: error.name }));
      return json({ valid: false, reason: 'service_unavailable', message: 'Servidor do administrador indisponivel.' }, 503);
    }
  }
};
