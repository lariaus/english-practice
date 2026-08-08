// CSV import/export for Flashcards - see docs/flashcards-spec.md's "CSV
// import/export" section. Pure functions only, no KV/HTTP - covered by
// plain unit tests, no Miniflare needed for this part (the route handlers
// that call these are what get Miniflare-tested).

// A real CSV parser, not a naive `.split(',')` - a field can contain a
// comma, a double quote, or a newline as long as it's wrapped in double
// quotes (with embedded quotes doubled, e.g. `"She said ""hi"""`), which a
// naive split would corrupt.
function parseCsvRows(text) {
  const rows = []
  let row = []
  let field = ''
  let inQuotes = false
  let i = 0

  while (i < text.length) {
    const char = text[i]

    if (inQuotes) {
      if (char === '"') {
        if (text[i + 1] === '"') {
          field += '"'
          i += 2
          continue
        }
        inQuotes = false
        i += 1
        continue
      }
      field += char
      i += 1
      continue
    }

    if (char === '"') {
      inQuotes = true
      i += 1
      continue
    }
    if (char === ',') {
      row.push(field)
      field = ''
      i += 1
      continue
    }
    if (char === '\r') {
      i += 1
      continue
    }
    if (char === '\n') {
      row.push(field)
      rows.push(row)
      row = []
      field = ''
      i += 1
      continue
    }
    field += char
    i += 1
  }

  // Final field/row if the text doesn't end with a trailing newline.
  if (field !== '' || row.length > 0) {
    row.push(field)
    rows.push(row)
  }

  // Drop fully-blank lines (a lone empty field from a stray trailing
  // newline) rather than treating them as a real, if empty, row.
  return rows.filter((r) => !(r.length === 1 && r[0] === ''))
}

// Matches "Front"/"Back" columns by header name (case-insensitive, any
// position) - extra columns (e.g. a trailing empty one from a spreadsheet
// export) are simply ignored. Throws if the header doesn't have both
// columns at all; a row missing either value is silently skipped rather
// than failing the whole import (a stray blank line shouldn't block
// everything else).
export function parseCsv(text) {
  const rows = parseCsvRows(text)
  if (rows.length === 0) {
    throw new Error('CSV is empty')
  }

  const header = rows[0].map((h) => h.trim().toLowerCase())
  const frontIndex = header.indexOf('front')
  const backIndex = header.indexOf('back')
  if (frontIndex === -1 || backIndex === -1) {
    throw new Error('CSV header must include "Front" and "Back" columns')
  }

  const cards = []
  for (const row of rows.slice(1)) {
    const front = (row[frontIndex] ?? '').trim()
    const back = (row[backIndex] ?? '').trim()
    if (!front || !back) continue
    cards.push({ front, back })
  }
  return cards
}

function escapeCsvField(value) {
  if (/[",\r\n]/.test(value)) {
    return `"${value.replace(/"/g, '""')}"`
  }
  return value
}

// The reverse of parseCsv - just Front/Back, all scheduling fields are
// deliberately dropped (see docs/flashcards-spec.md).
export function serializeCsv(cards) {
  const lines = ['Front,Back']
  for (const card of cards) {
    lines.push(`${escapeCsvField(card.front)},${escapeCsvField(card.back)}`)
  }
  return lines.join('\r\n') + '\r\n'
}
