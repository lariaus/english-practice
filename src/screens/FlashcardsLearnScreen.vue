<template>
  <main class="screen">
    <!-- inert while the Dictionary popup is open - see docs/dictionary-spec.md's
         "Global ownership" section. -->
    <div class="screen-content" :inert="isDictionaryOpen || null">
      <div class="screen-header">
        <button class="back-button" @click="handleBack">&larr; Back</button>
        <h1>Learn</h1>
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
import { getSet, markCardsLearned } from '../engine/flashcardsClient.js'
import { FlashcardsLearningEngine } from '../engine/flashcardsLearningEngine.js'
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

// The first k slots are the k cards learned so far, in the order they were
// learned, and never move again once there. The rest mirror the engine's
// live queue order (a card graded Hard/Good-without-graduating on
// [foo, bar, baz] moves to [bar, baz, foo] - only still-cycling cards
// reorder; a completed card just stays put and the current pointer advances
// past it).
const progressDots = computed(() => {
  version.value // eslint-disable-line no-unused-expressions
  if (!engine) return []
  const currentUid = engine.currentUid
  const learnedUids = engine.learnedCards.map((c) => c.uid)
  const order = [...learnedUids, ...engine.queueUids]
  return order.map((uid, i) => ({
    uid,
    grade: lastGradeByUid.get(uid) ?? null,
    isCurrent: uid === currentUid,
    isDone: i < learnedUids.length,
  }))
})

function shuffle(array) {
  const copy = [...array]
  for (let i = copy.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1))
    ;[copy[i], copy[j]] = [copy[j], copy[i]]
  }
  return copy
}

onMounted(async () => {
  try {
    const set = await getSet(props.setName)
    const newCards = set.cards.filter((c) => c.state === 'NEW')
    cardsByUid = new Map(newCards.map((c) => [c.uid, c]))
    const batch = shuffle(newCards).slice(0, BATCH_SIZE).map((c) => c.uid)
    engine = new FlashcardsLearningEngine(batch)
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

async function handleGrade(button) {
  lastGradeByUid.set(currentCard.value.uid, button)
  engine.grade(button)
  flipped.value = false
  version.value += 1

  if (engine.isDone && engine.learnedCards.length > 0) {
    try {
      await markCardsLearned(props.setName, today(), engine.learnedCards)
    } catch (e) {
      showToast(e.message, { type: 'error' })
    }
  }
}

async function handleBack() {
  if (engine && !engine.isDone && engine.learnedCards.length > 0) {
    try {
      await markCardsLearned(props.setName, today(), engine.learnedCards)
    } catch {
      // Best-effort on early exit - the learned cards from this session are
      // simply lost if this fails, same tradeoff as an unresolved lapse in
      // Review (see docs/flashcards-spec.md).
    }
  }
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
