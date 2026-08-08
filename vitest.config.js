// Plain vitest config for frontend engine code (src/engine/*Engine.js etc.)
// - no Vue mounting, no DOM, no Workers runtime needed, so this stays
// completely separate from cloudflare-worker/vitest.config.js's
// Miniflare-backed setup (run via `npm run worker:test`, not this one).
import { defineConfig } from 'vitest/config'

export default defineConfig({
  test: {
    include: ['src/**/*.test.js'],
  },
})
