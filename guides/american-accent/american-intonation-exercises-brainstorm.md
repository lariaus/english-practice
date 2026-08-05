# Exercise Brainstorm — Using the App for Everything Above

Status: **brainstorm**. Ideas flowing, not curated, not sequenced into a final program. Some
entries are obviously good, some are half-baked, a few are honest guesses. Prune later.

Sources this pulls from: `american-intonation-megadoc-summary-v1.md`, `-v2.md`, `-v3.md`
(the three passes at the same 5-stage system — v1/v2 keep appendix material v3 compressed away,
so both get used here), `american-intonation-study-notes.md` (the rougher raw notes), and
`american-intonation-megadoc-exercises.md` (the existing flat drill list — most of its items
get a concrete app mapping below rather than staying generic).

The point of this doc specifically: **every exercise says which tool, which buttons/settings,
and what to listen for** — not just "practice word stress."

---

## Quick reference — which tool for what

| Tool | Best for | Can't do |
|---|---|---|
| **Recorder Loop** | Pure self-production + immediate playback. No reference audio, no prompt — just you, talking, then hearing it. Any duration 5/10/15/30/60/90s, locked for the session. | No model to compare against, no scoring, no pitch visualization. Everything here is ear-only self-judgment. |
| **Robot Shadowing** | Bulk, hands-free, *automatically randomized* repetition — hear a phrase, repeat it, hear yourself. "Repeat model" toggle gives a direct reference-vs-you A/B. Three fixed difficulty tiers (Easy/Medium/Hard). | Can't target a specific word, sound, or grammatical pattern — phrases are random from a fixed 1000-sentence list per tier, no transcript shown while it plays. |
| **YT Shadowing** | Authentic native audio. Adjustable speed (0.5x–2x), loop any segment (Capture), transcript + click-a-word, Auto Shadow walks a whole video hands-free. This is the tool that can do almost everything the study-notes doc wished for ("podcast at 50% speed," "TED talk transcript," "hum the melody of a real clip"). | Needs a decent video with (ideally) captions. Caption timestamps are per-*line*, not per-word, so capture always rounds to full lines. |
| **Dictionary popup** (`Option+Shift+D` from anywhere, or click any YT transcript word) | Instant single-word phonetics (US/UK/AU), a real pronunciation clip, and its own mini Record+Shadow pair — all in one popup, no video needed. | Sentence-level only in the sense that it's word-by-word — no phrase/sentence lookup. Coverage depends on the Free Dictionary API's entries; numbers and initialisms may come up thin (fall back to shift-click → Cambridge). |

---

## Stage 0 — Ear training

The megadoc's Stage 0 protocol is a scored, shuffled, 60-word flashcard quiz (Forvo/YouGlish/TTS
voices, check immediately, write the score down). **The app has no scoring/shuffling/logging
mechanism anywhere** — nothing here replicates that rigor exactly. These are ear-only
approximations using what already exists.

- **Guess-then-check, word by word.** Open the Dictionary popup (`Option+Shift+D`), type a word
  you're unsure of, **look away or cover the phonetics line**, say out loud which syllable you
  think is stressed, *then* reveal the phonetics and hit Play to check. Do 15-20 words from your
  own vocabulary in a sitting. No score is recorded anywhere — keep your own tally on paper if
  you want the "write it down" effect.
- **Real-speaker tune ID.** In YT Shadowing, pick a clip of someone answering interview
  questions (varied, natural intonation). Turn on CC. Before each caption line finishes playing,
  guess the tune (fall / rise / fall-rise / etc. — see Stage 4) — check by listening again with
  the line highlighted. 10-15 lines is a session.
- **Transcribe-then-reveal.** In YT Shadowing, pick a 30-60s clip, turn CC **off**, and try to
  write down what's said by ear alone. Then turn CC on and check against the real transcript —
  the *systematic* misses (not the occasional one) point at what your ear currently can't parse.
  This is the same diagnostic idea as "dictate to your phone" from Stage 2, just for listening
  instead of speaking.
- **Hum the melody, no words.** Capture a 3-8s phrase in YT Shadowing (press-hold Capture, or
  shift-click a transcript line), let it loop. On one pass just hum the pitch contour with no
  words at all, then say the actual words on the next loop. The melody-only pass is the one the
  study-notes doc flagged as "hard in practice" — looping removes the need to re-cue it manually
  each time, which is the part that made it hard before.
- **Voice-variety, cheaply.** Robot Shadowing's Voice dropdown only has two options (Google TTS
  / Samantha), which is a poor substitute for "3-4 different voices," but it's free and built
  in: run the same Database tier with each voice for a few minutes and notice whether a given
  word's stress pattern is more or less obvious depending on the voice. Not rigorous, still
  something.

---

## Stage 1 — Make weak syllables genuinely weak

The single biggest win per every version of the summary. All of these are Recorder Loop by
default — no reference audio is needed, this is entirely about producing and then judging your
own reduction.

- **Mirror + schwa check, recorded.** Pick the 5s or 10s duration. Say *about, support, problem,
  common, famous, again, banana* on repeat for the whole window. On playback, don't just listen
  — this is one of the few checks that's *visual*: redo it once in front of a mirror while
  recording and confirm your lips stayed slack on every weak syllable, no forward movement.
- **Syllabic consonants, list drill.** 10s duration, repeat: *button, bottle, sudden, written,
  little, middle, total, certain, often* — never opening your mouth after the stop consonant
  (`BUT-n` not `BUT-ən`). Playback check: count syllables you hear vs. syllables you meant.
- **Unreleased finals, list drill.** Same setup, words ending in stop consonants: *stop, want,
  work, look, that, good*. Freeze your mouth shut on the final consonant, no trailing `-ə`.
- **Build-outward drill.** 5s window (short and punchy on purpose): `TAH → TAH-grəf →
  fə-TAH-grə-fi`, `TAH → TAH-blish → əs-TAH-blish-mənt`. Pick 3-4 of your own long words and
  build them outward from the stressed beat every rep.
- **Metronome fit — DOGS EAT BONES.** No metronome built into the app; use your phone's separate
  metronome app (or just tap the table) at ~90bpm while a Recorder Loop 15s or 30s cycle records
  you fitting `DOGS EAT BONES` / `The DOGS will EAT the BONES` / `The dogs would have EATen all
  of the BONES` into the same three beats each. Playback check: does the third sentence
  noticeably outrun the other two?
- **Exaggerate-to-300%, then the parody test.** 15-30s duration. First rep: say a sentence
  swallowing weak syllables to an absurd degree (`That's the MAAAIN PROOOBlem`). Then, separately,
  do a full ridiculous parody-American-accent take on anything, and actually listen to the
  playback — per the guide, this is supposed to sound like a mild, pleasant accent rather than a
  joke. Worth doing once, deliberately, as a calibration check rather than a drill.
- **Bulk scrambled reps — the "drill then scramble" rule, basically for free.** Robot Shadowing,
  Database = Easy, Repeat = 1, Repeat model = off. Just let it run hands-free for 10-15 minutes,
  one focus only ("today: reduction"), no analysis mid-session. Because each phrase is randomly
  drawn from 1000 unrelated sentences, this *is* the interleaving the megadoc says blocked
  practice never gets you — you can't predict the next sentence, so there's nothing to
  memorize your way through.
- **Reduction under complexity load.** Same as above but Database = Hard (14-22 word sentences
  with subordinate clauses). Longer, more complex sentences force real compression — if you're
  not reducing enough, Hard-tier sentences will physically not fit in the response window Robot
  Shadowing gives you (TTS time + 1s), which is itself useful feedback: running out of time is a
  sign you gave every syllable too much weight.
- **Duration-ratio self-check, ear-only (no Praat in the app).** Recorder Loop, 10s duration, say
  *administration* five times. There's no way to measure the actual ratio in-app — this is a
  flagged gap (see "Known gaps" below) — but you can still eyeball a phone stopwatch/waveform app
  separately, or just judge by ear whether the long syllable feels dramatically longer than the
  short ones.

---

## Stage 2 — Word stress on your own vocabulary

This is knowledge, not motor skill — it moves fast and is good for morale. The Dictionary popup
is the main tool here since it's the only one that shows phonetics.

- **The cognate-trap table, worked systematically.** Pull the table from
  `american-intonation-megadoc-summary-v1.md` Part "The cognate trap" / v2's Appendix A
  (*important, development, comfortable, necessary, category, laboratory, image, message,
  village, document, argument, element, management, government, character, architect, difficult,
  excellent, evident, president, comparable, preferable, admirable, determine, examine, imagine,
  purchase, surface, service, practice…*). For each word: `Option+Shift+D` → type it → check the
  phonetics/stress → hit Play → hit **Record** then **Shadow** right there in the popup, 2-3 reps
  each, before moving to the next word. A 20-30 word sitting covers the whole table.
- **Noun/verb pairs, same workflow.** `RECord/reCORD, PRESent/preSENT, OBject/obJECT,
  CONtract/conTRACT, PROject/proJECT, INcrease/inCREASE, TRANSfer/transFER, UPdate/upDATE` — look
  each half up separately in the Dictionary popup, Play both, then say a sentence using each
  ("We need to **RECord** it" / "Please **reCORD** the call") and self-check the shift landed.
- **`-ate` noun/adjective vs. verb pairs.** `SEPərət` (a separate room) vs. `SEPəreɪt` (to
  separate) — same popup-lookup-then-produce workflow, then bank the pair into a Recorder Loop
  session saying both forms in a real sentence.
- **`-ion/-ial/-ious/-ic/-ity` family sweep.** Take 10-15 of your own frequently-used words ending
  in these suffixes (`administration, essential, politician, economic, ability, security`), run
  each through the Dictionary popup once, say each aloud from memory afterward without
  re-checking, and only then re-check — that's the "predict before you check" rule applied to
  vocabulary instead of listening.
- **Numbers and initialisms — the one place the Dictionary popup might come up short.**
  *thirteen/thirty*, *C-E-O / A-P-I / K-P-I* are the ten-minute high-payoff items from the
  summaries, but the Free Dictionary API may not have great (or any) entries for bare numbers or
  initialisms. If a lookup comes back thin, **shift-click** the word (or use the popup's Cambridge
  link) to jump straight to a second source instead of trusting an empty/weak entry.
- **Organic word-stress spot-check during ordinary viewing.** This doesn't need to be a separate
  study session — whenever you're using YT Shadowing for anything and a transcript word looks
  French-cognate-shaped or you're just not sure, click it. Same popup, same Play/Record/Shadow,
  but it's happening *inside* a video you were already watching, which is closer to how the error
  actually shows up in real use than a dedicated vocabulary drill.
- **The running error list.** Every stage in every summary says this list matters more than any
  table: keep a plain notes-app list of words you get wrong, reviewed weekly, and re-run the worst
  ones back through the Dictionary popup workflow above until a full week passes with no new
  additions from your core vocabulary (that's the actual Stage 2 gate).

---

## Stage 3 — Put the emphasis where you mean it

Mostly Recorder Loop, since this is about producing specific stress *placements* and judging
whether you actually distinguished them — no reference clip is needed, the test is internal
consistency.

- **The six-way "I didn't say he stole it."** 30s or 60s duration, one Recorder Loop cycle. Say
  all six stress placements in a row (`↘I didn't...` / `I ↘DIDn't...` / `...↘SAY he...` /
  `...↘HE stole...` / `...↘STOLE it` / `...stole ↘IT`). Playback check: for each one, can you
  restate out loud what it implied, purely from re-hearing your own recording? If two versions
  sound the same to you, that's the actual gap, not a false pass.
- **The MANagement tail drill.** 15s duration, repeat: *It's about the ↘MANagement. / I talked to
  the ↘MANager. / That's the ↘DIFficulty. / It was comPLETEly ↘UNnecessary.* Check on playback
  specifically whether everything **after** the stressed syllable stayed flat and low, or whether
  your voice crept back up toward the end (the instinctive French move).
- **Deaccenting old information — your own Q&A pairs.** Write 5 short Q&A pairs first (e.g.
  *"Where's the report?" → "I left it on your desk."*). Record **both** turns in one Recorder Loop
  cycle, question then answer. Playback check: did the answer re-stress any word that was already
  in the question? (*"I left the rePORT on your ↘DESK"* is the wrong version — "report" was
  already known.)
- **The comPUter/compuTER edge case.** 15s duration, repeat: *Did you check the com↗PUter? / Is it
  im↗PORtant? / Are you the ↗MANager?* — the skill is keeping the word's real stress early while
  still rising to the top of your pitch range on the final unstressed syllable. This is flagged in
  both v1 and v2 as one of the highest-value, least-taught drills, and it's pure self-production —
  ideal Recorder Loop material.
- **Chunk-boundary decisions before speaking, using Hard-tier sentences.** Robot Shadowing,
  Database = Hard. These sentences have subordinate clauses and connectors (*although, because,
  since*) by design. Before repeating each one back, take half a second to mentally mark where
  you'd put a chunk break (Appendix E rules: never between a preposition and its object, never
  between determiner and noun, etc.), *then* say it with that chunking applied.
- **Predicting the nucleus in real dialogue.** YT Shadowing, a clip with a genuine back-and-forth
  (interview, panel, podcast clip with two speakers). Pause right before a response plays, guess
  out loud which word will carry the main stress, then let it play and check. This is the Stage 3
  skill applied to listening rather than production, which is the harder direction and rarely
  drilled.

---

## Stage 4 — Melody

- **Six tunes on one sentence.** Recorder Loop, 30s duration. Say *"You're going to Paris"* with
  all six tunes from the table (fall / low rise / fall-rise / rise-fall / high rise / flat), and
  state out loud what each one implies immediately after saying it — that verbal gloss is what
  makes this a comprehension check and not just a production drill.
- **Fall-rise, hand-traced.** Recorder Loop, 15s or 30s. `I'm not sure that'll ↘↗WORK.` / `↘↗SOME
  of them agreed.` / `We could ↘↗TRY that.` — trace the shape with your hand while saying it, as
  the guide suggests. Deliberately overdo it for a few reps before trying a more natural-sized
  version, then compare the two on playback.
- **Wh-questions must fall — using the TTS flatness as a foil.** Robot Shadowing with Repeat model
  **on**. TTS tends to read fairly neutrally/flatly regardless of sentence type, so when a
  wh-question phrase comes up, deliberately produce a strong fall on it (rather than matching the
  TTS's flatter reading) — the Repeat-model replay right before your own playback gives you an
  immediate flat-vs-your-fall contrast to judge.
- **Long-word falling tails.** Recorder Loop, 15s, repeat: *MANagement, deVELopment, COMfortable*
  landing a sentence — keep pitch down through the entire multi-syllable tail after the fall,
  don't let it drift up on the last syllable.
- **One sentence, 5-8 emotions.** Recorder Loop, 60s or 90s duration. Pick one neutral sentence,
  run it through several emotional readings (bored, excited, annoyed, doubtful, sincere...),
  recorded back to back. This is the closest built-in analog to widening pitch range on demand
  without a script telling you which tune to use — you're choosing the shape freely each time.
- **Tune ID gate, in real speech.** YT Shadowing, CC on. Pick 10-15 utterances from a clip with
  clear pragmatic content (a hedge, a disagreement, a list, a genuine question) and name the tune
  for each *before* re-listening closely, then check. The v1/v2 Stage 4 gate is literally "4 out
  of 6 correct in a real clip" — this is that gate, run directly against authentic material
  instead of invented examples.

---

## Connected speech & reductions (weak forms, flapping, linking, contractions)

This whole category lives in the v1/v2 appendices, not in v3's compressed stage list — but it's
concrete, list-based, and maps cleanly onto Recorder Loop.

- **Weak-forms sprint.** 15-30s duration, repeat rapid-fire: *a→ə, the→thə, and→ən, of→əv, to→tə,
  for→fər, at→ət, as→əz, can→kən, could/should/would→kəd/shəd/wəd, have/has/had→əv/əz/əd,
  was/were→wəz/wər, you/your→yə/yər, them→əm*. The point isn't the isolated word, it's doing this
  fast enough that you can't fall back into the strong form.
- **Flapping minimal pairs.** *water→wader, better→bedder, get it→gedit, a lot of→əlada, not at
  all→nadədall, party→pardy*. Recorder Loop, 15s, repeat rapid-fire, then a slower check pass —
  flapping is described as *the* most identifiable American feature and it's rhythm-driven, so if
  it's not happening, the actual bug is usually insufficient reduction on the following syllable,
  not the T itself.
- **NT-reduction list.** *twenty→tweny, interview→innerview, internet→innernet, center→senner,
  winter→winner*. Same Recorder Loop setup.
- **Linking phrases.** *pick it up→pi-ki-dup, turn it off→tur-ni-doff, an apple→ə-napple, go
  on→gowon, do it→duwit*. 10-15s, repeat until the seams disappear.
- **Contractions-obligatory A/B.** Recorder Loop, one cycle: read a short script twice, once with
  full forms (*I am going, they will not, it is*) and once fully contracted. Listen back and
  notice which version sounds native and which sounds emphatic/foreign — the summaries claim this
  difference is obvious once you hear it side by side, worth verifying on yourself.
- **Multi-word reductions — after rhythm, not instead of it.** *gonna, wanna, gotta, hafta, kinda,
  lemme, dunno*. Both summaries explicitly warn against drilling these as slang in isolation
  ("someone who inserts 'gonna' into otherwise stiff speech sounds strange") — only worth a
  Recorder Loop pass once the underlying rhythm work above already feels reasonably automatic.
- **Close the loop with authentic audio.** For any item above, find one real YT Shadowing clip
  where it occurs naturally in fast conversational speech (turn on CC, read along, spot the
  flap/link/weak-form as it happens) either before or after drilling it yourself in Recorder
  Loop — connects the "rule" to a real instance of it, rather than only ever hearing your own
  practiced version.

---

## Chunking / thought-group boundaries

- **Mark your own writing, then read it.** Take an actual email, slide, or spec you wrote. Mark
  chunk boundaries by hand using Appendix E's rules (break after a long subject, around
  subordinate clauses/parentheticals, before conjunctions joining clauses; never between
  determiner+noun, preposition+object, auxiliary+verb, or inside a compound). Read it aloud in
  one Recorder Loop cycle (60-90s), respecting your own marks.
- **The ambiguity pair.** Recorder Loop, short duration: say both readings of *"Let's eat,
  Grandma"* vs. *"Let's eat Grandma"* until the boundary difference is instant and automatic, not
  something you have to think about.
- **Caption-line boundaries vs. real prosodic boundaries — with a caveat.** YT Shadowing, CC on:
  read along a transcript and predict where the speaker will actually pause/drop pitch *before*
  it happens, then check. Caveat worth remembering: auto-caption line breaks are a transcription
  artifact, not a prosody annotation — sometimes they'll roughly line up with a real thought-group
  boundary, sometimes not at all. Treat this as an approximate exercise, not ground truth.
- **Chunking decisions on Hard-tier Robot Shadowing sentences** — see Stage 3 above, same idea,
  listed there since it doubles as an emphasis-placement drill too.

---

## Overlearned chunks (the automaticity bank)

The megadoc's highest-leverage-per-minute technique: drill one short phrase 50-100 times across
several days until it's automatic and costs zero attention, then bank it and start the next.
Starter list from the guide itself: *your name and role, "nice to meet you," "can you hear me
okay?", "let me share my screen", "sorry, could you say that again?", "does that make sense?",
"I'm not sure I follow", "that's a good question", "just to be clear"*.

- **Authentic-source version, using YT Shadowing.** Find a real clip of a native speaker using
  your target phrase (a meeting-simulation video, an interview clip, a tutorial — plenty of
  "let me share my screen" and "does that make sense" out there). Capture just that phrase
  (press-hold Capture, or select the transcript text and hit "Capture selection"). Set speed to
  0.5x, chorus along with the loop 10x, then step the speed pill up (0.6 → 0.75 → 0.9 → 1.0)
  every few loops — a literal, tool-supported version of "exaggerate, then fade." Finish with a
  few Shadow reps (record + compare) once you're near 1x.
- **No good clip available? Fall back to cold production.** Not every job-specific phrase will
  have a convenient YouTube source. For those, just drill it in Recorder Loop directly (say it,
  hear it back, repeat) — less ideal than having a native reference, but still fine for banking a
  phrase you already know is correct from the Dictionary-popup / Stage-2 workflow above.
- **A real gap worth naming (not building now):** Robot Shadowing's three databases are fixed
  generic sentences — there's no way to feed it a *custom* list of your own overlearned-chunk
  candidates so they show up in the random rotation later, for spaced, unpredictable re-exposure.
  Parking this as a brainstorm item, not a request to build it.

---

## Cross-tool combo workflows

- **The three-tool ladder: diagnose → drill → verify.** (1) Notice a weak spot organically while
  using YT Shadowing for something else entirely — click the word, confirm the correct
  phonetics/stress. (2) Switch to Robot Shadowing and run a bulk scrambled session (any tier),
  one single focus stated out loud beforehand, to see whether the fix holds up outside a
  controlled drill, since the phrases are unpredictable. (3) Finish with one Recorder Loop
  monologue (60-90s), free topic, checking only that same one thing on playback — the actual
  test of whether it transferred to unscripted speech. This uses all three tools as three
  different rungs of the transfer ladder (chorus/shadow-like → scrambled drill → spontaneous).
- **Monthly dated baseline.** Recorder Loop's max duration is 90s, not the guide's literal "2
  minutes" — so do two back-to-back 90s cycles (you'll need to Stop and re-pick if you want a
  different duration for the second half, but 90s+90s covers the cold-read + job-explanation
  combo fine). Keep the recordings dated, don't listen back for three months, then compare to be
  encouraged rather than discouraged.
- **French-vs-English pitch-range check, cheaply.** Two Recorder Loop cycles back to back, same
  duration: tell the same short story once in French, once in English. No pitch measurement
  tool in-app, but an ear-only comparison already tells you directionally whether English is
  suppressing a range you actually have (a *load* problem) rather than one you lack (a
  *capacity* problem) — worth doing even without numbers.
- **One-thing-per-session, enforced literally.** Regardless of which tool: say your single focus
  out loud before pressing Start ("today, only vowel reduction"), and hold yourself to only
  checking that one thing on any playback, in any tool, for the whole session.

---

## Known app gaps worth flagging (brainstorm only, not a build request)

- No Praat-style pitch-track or duration-ratio visualization anywhere — every "measure the
  ratio" / "check your pitch span" instruction in the summaries currently has to be done by ear,
  or with a separate app entirely.
- No scored, shuffled, logged Stage-0 quiz mechanism — the Dictionary popup supplies the raw
  material (audio + phonetics) but none of the quiz mechanics (shuffle, immediate check, running
  score).
- Robot Shadowing's phrase lists are fixed and generic — no way to point it at a custom/targeted
  word or phrase list (e.g. just the cognate-trap words, or a personal overlearned-chunk bank).
- No metronome/beat click built in anywhere, despite several drills above being metronome-based.

---

## Quick picks by time budget

**5 minutes:** Dictionary-popup guess-then-check on 10-15 words (Stage 0/2) — or one Recorder
Loop 60s cycle doing the six-way "I didn't say he stole it" (Stage 3).

**15 minutes:** Recorder Loop weak-syllable block (schwa/syllabic-consonants/unreleased-finals
lists, Stage 1) *or* one YT Shadowing capture-and-chorus session on a single overlearned chunk,
stepping speed up from 0.5x to 1x.

**30 minutes:** the three-tool ladder above (diagnose in YT Shadowing → scrambled drill in Robot
Shadowing → free monologue verify in Recorder Loop), one focus only, stated out loud first.
