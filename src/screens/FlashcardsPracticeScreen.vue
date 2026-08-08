<template>
  <main class="screen">
    <!-- inert while the Dictionary popup is open - see docs/dictionary-spec.md's
         "Global ownership" section. -->
    <div class="screen-content" :inert="isDictionaryOpen || null">
      <div class="screen-header">
        <button class="back-button" @click="$emit('back')">&larr; Back</button>
        <h1>Practice</h1>
        <span class="screen-header-spacer"></span>
      </div>

      <p v-if="error" class="error-message">{{ error }}</p>
      <p v-else-if="loading" class="subtitle">Loading…</p>
      <p v-else-if="!currentCard" class="subtitle">This set has no cards yet.</p>

      <template v-else-if="editing">
        <input class="text-input" v-model="editFront" placeholder="Front" />
        <input class="text-input" v-model="editBack" placeholder="Back" />
        <div class="grade-buttons">
          <button class="primary-button" @click="handleSaveEdit">Save</button>
          <button class="toggle-button" @click="editing = false">Cancel</button>
        </div>
      </template>

      <template v-else>
        <div :key="currentCard.uid" class="flashcard" :class="{ flipped: revealed }" @click="handleFlip">
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

          <div class="flashcard-overlay-top" @click.stop>
            <button
              class="tile-button"
              :disabled="isOffline"
              :title="isOffline ? 'Requires a connection' : null"
              @click="handleStartEdit"
            >Edit</button>
            <button
              class="tile-button tile-delete"
              :disabled="isOffline"
              :title="isOffline ? 'Requires a connection' : null"
              @click="handleDeleteCurrent"
            >Delete</button>
          </div>

          <div class="flashcard-overlay-bottom grade-buttons" @click.stop>
            <button class="grade-button grade-again" @click="handleNext">Again</button>
            <button class="grade-button grade-hard" @click="handleNext">Hard</button>
            <button class="grade-button grade-good" @click="handleNext">Good</button>
            <button class="grade-button grade-easy" @click="handleNext">Easy</button>
          </div>
        </div>
      </template>
    </div>
  </main>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { deleteCardCached, editCardCached, getSetOrCached } from '../engine/flashcardsOfflineCache.js'
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
const cards = ref([])
const index = ref(0)
const revealed = ref(false)
const editing = ref(false)
const editFront = ref('')
const editBack = ref('')
const isOffline = ref(false)

const currentCard = computed(() => cards.value[index.value] ?? null)

const frontAudio = useFlashcardFaceAudio(() => currentCard.value.front)
const backAudio = useFlashcardFaceAudio(() => currentCard.value.back)

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
    const result = await getSetOrCached(props.setName)
    cards.value = shuffle(result.set.cards)
    isOffline.value = result.offline
  } catch (e) {
    error.value = e.message
    // See FlashcardsEditScreen.vue's onMounted for why this still matters
    // even with nothing cached to show.
    if (e.offline) isOffline.value = true
  } finally {
    loading.value = false
  }
})

function handleFlip() {
  revealed.value = !revealed.value
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

// AGAIN/HARD/GOOD/EASY are all inert here (TODO for future, per
// docs/flashcards-spec.md's Practice section) - visible from the start
// (not gated behind flipping), every button just moves on to the next
// random card, reshuffling once the pool is exhausted.
function handleNext() {
  revealed.value = false
  index.value += 1
  if (index.value >= cards.value.length) {
    cards.value = shuffle(cards.value)
    index.value = 0
  }
}

function handleStartEdit() {
  editFront.value = currentCard.value.front
  editBack.value = currentCard.value.back
  editing.value = true
}

async function handleSaveEdit() {
  const uid = currentCard.value.uid
  const updated = { front: editFront.value, back: editBack.value }
  try {
    await editCardCached(props.setName, uid, updated)
    Object.assign(cards.value[index.value], updated)
    editing.value = false
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
}

async function handleDeleteCurrent() {
  const uid = currentCard.value.uid
  try {
    await deleteCardCached(props.setName, uid)
    cards.value.splice(index.value, 1)
    revealed.value = false
    if (index.value >= cards.value.length) {
      index.value = 0
    }
  } catch (e) {
    showToast(e.message, { type: 'error' })
  }
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
  padding-top: 3.2rem;
  padding-bottom: 4.5rem;
}

.flashcard-overlay-top {
  position: absolute;
  top: 0.7rem;
  right: 0.7rem;
  display: flex;
  gap: 0.5rem;
  z-index: 5;
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
