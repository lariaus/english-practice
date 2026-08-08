# Cloudflare Worker (sync backend)

Small Worker + KV backend for cross-device sync. First use case: the "last 5
videos" history (`src/engine/ytHistory.js`), moving it from per-browser
`localStorage` to one shared KV entry. Second use case: Flashcards (see
`docs/flashcards-spec.md`) - one KV key per set, plus a small index key
listing set names, in its own `FLASHCARDS` namespace (kept separate from
`HISTORY` so the two features' KV write-rate limits never compete).

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

   Flashcards needs its own second namespace, done the same way:

   ```sh
   npx wrangler kv namespace create FLASHCARDS --config cloudflare-worker/wrangler.jsonc
   ```

   Copy the printed `id` into a second entry in `kv_namespaces` with
   `"binding": "FLASHCARDS"`.

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

### Flashcards

Full data model/algorithm reference: `docs/flashcards-spec.md` and
`docs/anki-algorithm.md`. Every route below 404s on an unknown `:name`/
`:uid` except where noted; `create_set` 409s on a name collision.

- `GET /flashcards/sets` -> `list[str]` of set names, in creation order.
- `POST /flashcards/sets` - body `{ name }` -> the created
  `{ name, cards: [], next_uid: 1 }`.
- `GET /flashcards/sets/:name` -> the full `FlashcardsSet`.
- `DELETE /flashcards/sets/:name` -> `{ status: 'ok' }`.
- `POST /flashcards/sets/:name/cards` - body `{ front, back }` -> the new
  card's server-assigned `uid` (a bare number).
- `PUT /flashcards/sets/:name/cards/:uid` - body `{ front, back }` (content
  fields only - scheduling fields are always server-owned) ->
  `{ status: 'ok' }`.
- `DELETE /flashcards/sets/:name/cards/:uid` -> `{ status: 'ok' }`.
- `POST /flashcards/sets/:name/reset` -> the updated `FlashcardsSet`. Puts
  every card's scheduling fields back to NEW defaults, leaving
  `front`/`back`/`uid` (and the set's `next_uid`) untouched.
- `POST /flashcards/sets/:name/learned` - body
  `{ today, learned: [{ uid, grade: 'GOOD'|'EASY' }] }` -> `{ status: 'ok' }`.
  Errors the whole batch (applying nothing) if any `uid` is unknown.
- `GET /flashcards/sets/:name/review?today=YYYY-MM-DD` -> `list[int]` of
  due card uids.
- `POST /flashcards/sets/:name/review` - body
  `{ today, results: [{ card_uid, grade: 'HARD'|'GOOD'|'EASY', lapsed }] }`
  -> `{ status: 'ok' }`. **Does not 404 on an unknown/not-currently-due
  `card_uid`** - it's silently skipped instead, which is what makes
  resubmitting the same batch after a dropped response safe (no double-
  applied scheduling math).
- `GET /flashcards/sets/:name/csv` -> raw CSV text (`Content-Type: text/csv`,
  not JSON) - header `Front,Back` plus one row per card, all scheduling
  fields dropped.
- `POST /flashcards/sets/:name/csv` - body is the **raw CSV text itself**
  (`Content-Type: text/csv`, not JSON) -> the updated `FlashcardsSet`. 400s
  with a message if the header doesn't contain `Front`/`Back` columns
  (case-insensitive, any order/position -
  extra columns are ignored); a row with an empty Front or Back is silently
  skipped rather than failing the whole import.

`today`/`due` are plain calendar-date strings (`"2026-01-28"`), never a
timestamp - see `docs/flashcards-spec.md`'s "Dates, not timestamps"
section for why.
