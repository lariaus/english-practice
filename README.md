# English Practice

Live at: <https://lariaus.github.io/english-practice/>

A small collection of self-practice tools for English pronunciation. Starts
with "Recorder Loop", a hands-free record → playback self-monitoring loop.

## Test locally

Build the frontend:

```sh
npm install
npx wrangler login

npm run build
```

Serve files statically:

```sh
cmake -S native-server -B native-server/build -DCMAKE_BUILD_TYPE=Release
cmake --build native-server/build -j
native-server/build/native_server_cli/native_server_cli --dir "$(pwd)/dist" --port 8000
```

The original one-liner still works too, until the migration removes it:

```sh
python3 -m http.server 8000 --directory dist
```

Run the companion server:

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install -r native-exp-server/requirements.txt
python3 native-exp-server/server.py
```

Open `http://localhost:8000`.

## Test locally on iPhone

Mic access needs HTTPS, so use a tunnel:

```sh
brew install cloudflared   # once
cloudflared tunnel --url http://localhost:8000
```

Open the printed `https://*.trycloudflare.com` URL in Safari on the iPhone.

## Deploy

Pushing to `main` auto-builds and publishes via GitHub Actions (see
`.github/workflows/deploy.yml`). Live at:
<https://lariaus.github.io/english-practice/>

## Export

Get the whole repo's source as a `.zip`:

```sh
git archive --format=zip -o english-practice.zip HEAD
```
