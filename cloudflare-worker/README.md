# Cloudflare Worker (sync backend)

Small Worker + KV backend for cross-device sync. First use case: the "last 5
videos" history (`src/engine/ytHistory.js`), moving it from per-browser
`localStorage` to one shared KV entry.

No auth check at all - the Worker's URL itself is never committed to this
repo (entered manually per device in the app's own settings screen
instead), so the URL is the only thing gating access. See
`docs/setup-cloudflare.md` for the reasoning behind this.

## One-time setup

Run these from the repo root (`wrangler` is a root `devDependency`, already
covered by `npm install`). See `docs/setup-cloudflare.md` for the full
account/login setup - this is just the commands specific to this Worker.

1. Create the KV namespace:

   ```sh
   npx wrangler kv namespace create HISTORY --config cloudflare-worker/wrangler.jsonc
   ```

   Copy the printed `id` into `cloudflare-worker/wrangler.jsonc`'s
   `kv_namespaces[0].id`.

2. Deploy:

   ```sh
   npx wrangler deploy --config cloudflare-worker/wrangler.jsonc
   ```

   This prints the live URL (`https://english-practice-sync.<subdomain>.workers.dev`)
   - enter it into the app's settings screen on each device, it's never
   stored in source.

## Local dev

```sh
npx wrangler dev --config cloudflare-worker/wrangler.jsonc
```

Runs the Worker locally against an isolated local KV copy (not the real
remote data) unless `--remote` is passed.

## API

No auth header required - see the note above.

- `GET /history` - returns the history array (newest first, max 5), each entry
  shaped `{ videoId, url, title, author, duration, currentPosition }`.
- `POST /history` - body `{ videoId, url, title, author?, duration?, currentPosition? }`;
  moves it to the front (no duplicates), trims to 5, returns the updated array.
  `currentPosition` is protected: omit it (as the client does when a video
  starts) to leave whatever was last recorded untouched; send a real number
  (as the client does when a session ends - Back, or the page closing/
  reloading) to actually update it. `author`/`duration` are always sent
  fresh and just overwrite outright.
