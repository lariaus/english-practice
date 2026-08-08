<template>
  <main class="screen">
    <!-- inert while the Dictionary popup is open - see docs/dictionary-spec.md's
         "Global ownership" section. -->
    <div class="screen-content" :inert="isDictionaryOpen || null">
      <div class="screen-header">
        <button class="back-button" @click="handleBack">&larr; Back</button>
        <h1>Review</h1>
        <span class="screen-header-spacer"></span>
      </div>

      <p v-if="error" class="error-message">{{ error }}</p>
      <p v-else-if="loading" class="subtitle">Loading…</p>
      <p v-else-if="!currentCard" class="subtitle">All done for now!</p>

      <template v-else>
        <div class="progress-dots">
          <span
            v-for="dot in progressDots"
            :key="dot.uid"
            class="progress-dot"
            :class="[dot.grade ? `dot-${dot.grade.toLowerCase()}` : '', { 'dot-current': dot.isCurrent, 'dot-done': dot.isDone }]"
          ></span>
        </div>

        <div :key="currentCard.uid" class="flashcard" :class="{ flipped }" @click="handleFlip">
          <div class="flashcard-inner">
            <div class="flashcard-face flashcard-front">
              <div class="flashcard-stacked-content">
                <div class="flashcard-stacked-top">
                  <FlashcardText :text="currentCard.front" @word-click="handleWordClick" />
                  <FaceAudioControls :audio="frontAudio" />
                </div>
                <hr class="tile-divider" />
                <div class="flashcard-stacked-bottom">
                  <p class="flashcard-placeholder">?????</p>
                </div>
              </div>
            </div>
            <div class="flashcard-face flashcard-back">
              <div class="flashcard-stacked-content">
                <div class="flashcard-stacked-top">
                  <FlashcardText :text="currentCard.front" @word-click="handleWordClick" />
                  <FaceAudioControls :audio="frontAudio" />
                </div>
                <hr class="tile-divider" />
                <div class="flashcard-stacked-bottom">
                  <FlashcardText :text="currentCard.back" @word-click="handleWordClick" />
                  <FaceAudioControls :audio="backAudio" />
                </div>
              </div>
            </div>
          </div>

          <div class="flashcard-overlay-bottom grade-buttons" @click.stop>
            <button class="grade-button grade-again" @click="handleGrade('AGAIN')">Again</button>
            <button class="grade-button grade-hard" @click="handleGrade('HARD')">Hard</button>
            <button class="grade-button grade-good" @click="handleGrade('GOOD')">Good</button>
            <button class="grade-button grade-easy" @click="handleGrade('EASY')">Easy</button>
          </div>
        </div>
      </template>
    </div>
  </main>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { getSet, getToReviewCards, sendReviewResults } from '../engine/flashcardsClient.js'
import { FlashcardsReviewEngine } from '../engine/flashcardsReviewEngine.js'
import { today } from '../engine/flashcardsToday.js'
import { BATCH_SIZE } from '../engine/flashcardsConstants.js'
import { cleanWord } from '../engine/wordTokenizer.js'
import { useFlashcardFaceAudio } from '../composables/useFlashcardFaceAudio.js'
import { showToast } from '../composables/useToast.js'
import FaceAudioControls from '../components/FaceAudioControls.vue'
import FlashcardText from '../components/FlashcardText.vue'

const props = defineProps({
  setName: { type: String, required: true },
  dictionaryOpen: { type: Boolean, default: false },
})
const emit = defineEmits(['back', 'show-word'])

const isDictionaryOpen = computed(() => props.dictionaryOpen)

const loading = ref(true)
const error = ref(null)
const flipped = ref(false)
const version = ref(0) // bumped on every engine mutation to force recomputation

let engine = null
let cardsByUid = new Map()
let lastGradeByUid = new Map() // uid -> most recent button pressed for it, for the dots' colors

const currentCard = computed(() => {
  version.value // eslint-disable-line no-unused-expressions
  const uid = engine?.currentUid
  return uid == null ? null : cardsByUid.get(uid)
})

const frontAudio = useFlashcardFaceAudio(() => currentCard.value.front)
const backAudio = useFlashcardFaceAudio(() => currentCard.value.back)

// The first k slots are the k cards completed so far, in the order they
// finished, and never move again once there. The rest mirror the engine's
// live queue order (a card graded Again/Hard-while-recovering on
// [foo, bar, baz] moves to [bar, baz, foo] - only still-cycling cards
// reorder; a completed card just stays put and the current pointer advances
// past it).
const progressDots = computed(() => {
  version.value // eslint-disable-line no-unused-expressions
  if (!engine) return []
  const currentUid = engine.currentUid
  const doneUids = engine.results.map((r) => r.card_uid)
  const order = [...doneUids, ...engine.queueUids]
  return order.map((uid, i) => ({
    uid,
    grade: lastGradeByUid.get(uid) ?? null,
    isCurrent: uid === currentUid,
    isDone: i < doneUids.length,
  }))
})

onMounted(async () => {
  try {
    const todayStr = today()
    const [set, dueUids] = await Promise.all([getSet(props.setName), getToReviewCards(props.setName, todayStr)])
    cardsByUid = new Map(set.cards.map((c) => [c.uid, c]))
    const batch = dueUids.slice(0, BATCH_SIZE)
    engine = new FlashcardsReviewEngine(batch)
  } catch (e) {
    error.value = e.message
  } finally {
    loading.value = false
  }
})

function handleFlip() {
  flipped.value = !flipped.value
}

// Each word is independently clickable and looks up itself - a card like
// "voiture rouge" / "red car" has 4 separate lookups, not one lookup for
// the whole front text (see docs/flashcards-spec.md).
function handleWordClick(token, event) {
  event.stopPropagation()
  const cleaned = cleanWord(token)
  if (!cleaned) return
  emit('show-word', cleaned)
}

async function submitResults() {
  if (!engine || engine.results.length === 0) return
  try {
    await sendReviewResults(props.setName, today(), engine.results)
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}

async function handleGrade(button) {
  lastGradeByUid.set(currentCard.value.uid, button)
  engine.grade(button)
  flipped.value = false
  version.value += 1

  if (engine.isDone) await submitResults()
}

async function handleBack() {
  // Best-effort on early exit - matching Learning's tradeoff, and the
  // spec's own accepted "a lapse can be lost" tradeoff for anything still
  // mid-retry. submitResults() reports its own failure via a toast (a
  // global singleton, unlike error.value - it stays visible even though
  // this screen navigates away immediately after).
  await submitResults()
  emit('back')
}
</script>

<style scoped>
/* The flashcard fills all remaining vertical space below the header,
   instead of the app's usual centered-column-of-content layout. An
   explicit height (not just min-height) makes the flex-grow distribution
   below reliable - a min-height-only flex parent can end up not giving its
   flex:1 child any real space to grow into in some engines. */
.screen {
  height: 100vh;
  height: 100dvh;
  justify-content: flex-start;
  overflow-y: auto;
}

.screen-content {
  flex: 1;
  min-height: 0;
}

.flashcard-face {
  padding-bottom: 4.5rem;
}

.flashcard-overlay-bottom {
  position: absolute;
  left: 1rem;
  right: 1rem;
  bottom: 0.9rem;
  z-index: 5;
  /* .grade-buttons already sets width/max-width for its normal
     below-the-card usage elsewhere - not appropriate for an absolutely
     positioned overlay confined to the card's own width. */
  width: auto;
  max-width: none;
}
</style>
