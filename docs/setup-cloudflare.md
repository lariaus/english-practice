# Cloudflare Worker setup log

Commands run to set up the Cloudflare Worker + KV backend.

## 1. Create a Cloudflare account

Manual, in the browser: `https://dash.cloudflare.com/sign-up/workers-and-pages`.

## 2. Install dependencies

```sh
npm install
```

(`wrangler` is a `devDependency` in the root `package.json`.)

## 3. Node version requirement

Wrangler 4.x requires Node.js ≥22.

```sh
nvm install 22
nvm use 22
nvm alias default 22
```

## 4. Log in

```sh
npx wrangler login
```

Verify:

```sh
npx wrangler whoami
```

**Gotcha**: if this errors with `request_forbidden` / "No CSRF value
available in the session cookie," the browser it auto-opened in probably
isn't logged into the Cloudflare dashboard. Copy the URL it also prints as
plain text and paste it into the browser that is.

## 5. Create the KV namespace

```sh
npx wrangler kv namespace create HISTORY --config cloudflare-worker/wrangler.jsonc
```

Copy the printed `id` into `cloudflare-worker/wrangler.jsonc`'s
`kv_namespaces[0].id`.

## 6. Deploy

```sh
npx wrangler deploy --config cloudflare-worker/wrangler.jsonc
```

Prints the live Worker URL.
