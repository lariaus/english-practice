# English Practice

Live at: <https://lariaus.github.io/english-practice/>

A small collection of self-practice tools for English pronunciation. Starts
with "Recorder Loop", a hands-free record → playback self-monitoring loop.

## Setup

Install all dependencies for frontend / tunnelling / cloudfare:

```sh
npm install
npx wrangler login
brew install cloudflared
cmake -S native-server -B native-server/build -DCMAKE_BUILD_TYPE=Release
```

## Run locally

Build and run frontend and server:

```sh
# Only needed if cloudflare-worker/src/**.js changed since your last deploy:
npx wrangler deploy --config cloudflare-worker/wrangler.jsonc

npm run build
cmake --build native-server/build -j
native-server/build/native_server_cli/native_server_cli --dir "$(pwd)/dist" --port 8000
```

Open `http://localhost:8000`.

## Run MacOS / iOS App

Open `english-practice-app/English Practice.xcodeproj` in Xcode and run it.

## Run with HTTPS Server

Use Cloudfare tunneling to test on ios (requires HTTPS):

```sh
cloudflared tunnel --url http://localhost:8000
```

## Testing

Run everything at once (exits 0 only if all suites pass):

```sh
./scripts/run-all-tests
```

Run vite tests:

```sh
npm test
```

Run cloudflare worker tests:

```sh
npm run worker:test
```

Run native-server tests:

```sh
cmake --build native-server/build -j
ctest --test-dir native-server/build --output-on-failure
```

Run english-practice tests:

```sh
xcodebuild -quiet test -project "english-practice-app/English Practice.xcodeproj" -scheme "English Practice" -destination "platform=macOS"
```

## Get Sync Server URL

Run this command to retrieve the Cloudflare Worker URL:

```sh
./scripts/get-cloudfare-url
```

## Data storage location

`native-server` persists small app data (e.g. the sync server URL) under
`.app_data` (CLI, gitignored, override with `--data-dir`) or an internal
app-managed directory (Mac/iOS app).

## Deploy (Deprecated)

Pushing to `main` auto-builds and publishes via GitHub Actions (see
`.github/workflows/deploy.yml`). Live at:
<https://lariaus.github.io/english-practice/>

## Export

Get the whole repo's source as a `.zip`:

```sh
git archive --format=zip -o english-practice.zip HEAD
```
