// Resumable Learn/Review session state, backed by StorageMap - a sibling
// to flashcardsOfflineCache.js (same 'flashcards' map, a different key
// namespace), but a different purpose: this isn't a read-only content
// mirror, it's the actual in-progress state of a live session (queue order,
// per-card streak/lapsed, completed-so-far list, grade history), saved the
// moment a session starts and re-saved after every grade so an app
// restart/force-quit mid-session loses nothing. See docs/flashcards-spec.md's
// "Resuming Learn/Review" section.
//
// Deliberately local-only, like the offline cache - a session started on
// one device isn't resumable from another.
//
// StorageMap's get/set/delete already catch and log their own errors (see
// storageMap.js) and never throw, so nothing here needs its own try/catch.
import { StorageMap } from './storageMap.js'

const map = StorageMap.get('flashcards')
const learnKey = (name) => `learn-progress:${name}`
const reviewKey = (name) => `review-progress:${name}`

// state: {queue: [{uid, streak}], learned: [{uid, grade}], lastGradeByUid}
// lastGradeByUid is an array of [uid, grade] pairs, not a plain object -
// uid is a number (see cloudflare-worker/src/flashcardsRoutes.js's
// next_uid), and a plain object's keys are always coerced to strings
// through JSON, which would silently break every later `.get(numericUid)`
// lookup once reconstructed into a Map.
export async function saveLearnProgress(setName, state) {
  await map.set(learnKey(setName), state)
}

// -> the saved state, or null if there's no session in progress for this set.
export async function loadLearnProgress(setName) {
  return await map.get(learnKey(setName))
}

export async function clearLearnProgress(setName) {
  await map.delete(learnKey(setName))
}

// state: {queue: [{uid, lapsed}], results: [{card_uid, grade, lapsed}], lastGradeByUid}
export async function saveReviewProgress(setName, state) {
  await map.set(reviewKey(setName), state)
}

export async function loadReviewProgress(setName) {
  return await map.get(reviewKey(setName))
}

export async function clearReviewProgress(setName) {
  await map.delete(reviewKey(setName))
}

// Any action that changes a set's cards (add/edit/delete/import/reset)
// invalidates both resumable sessions unconditionally, rather than trying
// to patch around exactly what changed - a stale queue referencing an
// edited/removed card would otherwise silently show wrong content or a
// dead uid.
export async function clearAllProgressForSet(setName) {
  await Promise.all([clearLearnProgress(setName), clearReviewProgress(setName)])
}
