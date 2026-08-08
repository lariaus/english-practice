// Proves the Miniflare/vitest-pool-workers harness itself actually runs a
// real request through the Worker, before any real Flashcards coverage is
// written on top of it - see docs/flashcards-spec.md's "Testing" section.
import { SELF } from 'cloudflare:test'
import { describe, expect, it } from 'vitest'

describe('worker harness', () => {
  it('404s on an unknown route', async () => {
    const response = await SELF.fetch('https://example.com/does-not-exist')
    expect(response.status).toBe(404)
  })
})
