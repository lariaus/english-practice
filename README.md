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
