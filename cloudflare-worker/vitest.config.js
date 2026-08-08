// Runs tests inside a real Workers runtime + emulated KV (Miniflare),
// never against the real deployed Worker/KV - see docs/flashcards-spec.md's
// "Testing" section for why.
import { fileURLToPath } from 'node:url'
import { defineConfig } from 'vitest/config'
import { cloudflareTest } from '@cloudflare/vitest-pool-workers'

const wranglerConfigPath = fileURLToPath(new URL('./wrangler.jsonc', import.meta.url))

export default defineConfig({
  test: {
    // Scoped explicitly - without this, vitest's default discovery also
    // picks up src/**/*.test.js (the frontend engine tests), running them
    // redundantly inside the Workers runtime pool instead of vitest.config.js's
    // plain one at the repo root.
    include: ['cloudflare-worker/test/**/*.test.js'],
  },
  plugins: [cloudflareTest({ wrangler: { configPath: wranglerConfigPath } })],
})
