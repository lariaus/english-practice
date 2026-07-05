# English Practice

Live at: <https://lariaus.github.io/english-practice/>

A small collection of self-practice tools for English pronunciation. Starts
with "Recorder Loop", a hands-free record → playback self-monitoring loop.

## Test locally

```sh
npm install
npm run build
python3 -m http.server 8000 --directory dist
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
