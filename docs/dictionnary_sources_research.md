# Dictionary & Pronunciation Sources Research

Research spike for a possible future "click a word in the transcript → see a
definition, hear it pronounced" feature in YT Shadowing (see
`ytShadowingPlayerScreen.vue`'s transcript sidebar). Two questions: (1) how
does the Chrome extension **Language Reactor** do this, and (2) what
free/low-cost APIs exist for word definitions and pronunciation audio that a
personal, non-commercial, low-volume hobby project could realistically use.

Not a design doc or a commitment to build anything - purely a survey to
inform a decision later. All findings below were gathered by two research
agents (2026-07-18) that actually fetched live docs/pricing pages rather than
relying on background knowledge, since pricing/limits/ToS change over time.
Confidence level is noted per claim; treat anything marked "speculative" as
an informed guess, not fact.

## 1. How Language Reactor does it

**What it is**: a Chrome extension (formerly "Language Learning with
Netflix"), by developers David Wilkinson and Ognjen Apic under the name
"Dioco." Clicking a word in its subtitle sidebar pauses the video, opens a
popup with a definition/translation, and (per a "Speak word on click"
setting) plays pronunciation audio.

The research agent went further than reading reviews - it downloaded the
real, current Chrome extension package (id `hoombieeljmmljlkjmnheibnpciblicm`,
v5.1.8) directly from Google's official distribution endpoint and searched
the actual JavaScript for API calls and third-party service names. That's
the strongest evidence available short of the developers confirming it
directly, since it's the real client code, not secondhand description.

### Dictionary/definition source

- **Confirmed** (from the extension's own code): the word-click popup calls
  Language Reactor's *own* backend, `POST
  https://api.dioco.io/base_dict_getHoverDictEntriesForSubs`, sending the
  subtitle words plus source/target language codes and getting back a
  word→translation map. No call to any named third-party dictionary or
  translation API (Google Translate, DeepL, Merriam-Webster, Oxford,
  Collins, Wiktionary, Linguee, etc.) appears anywhere in the code - it's
  all proxied through their own servers.
- **Confirmed**: a second endpoint, `base_dict_llmDict`, takes a single word
  + language pair and is called with an unusually long 60-second timeout -
  consistent with a live LLM generation call server-side rather than a
  static database lookup. A third, `base_lexa_generate` (the "Lexa AI"
  deeper-explanation panel), passes a prompt template + context sentence +
  word to Dioco's servers - again LLM-shaped, but which underlying model
  (OpenAI, in-house, etc.) is **unconfirmed** - one third-party reviewer
  guessed OpenAI, but that's speculation, not sourced fact.
- **Confirmed** (developer's own forum post): the separate frequency/CEFR
  word-list feature (related but distinct from the definition popup) is
  built from "40% movie subtitles, 40% YouTube subtitles, 20% other web
  content," based on the OpenSubtitles-v2018 corpus, tokenized in-house.
- **Speculative/unconfirmed**: what Dioco's backend does *internally* to
  produce a definition (own bilingual database vs. licensed dictionary vs.
  live machine translation vs. LLM) isn't visible from a client-side
  teardown and no official source addresses it. No evidence of a
  per-language-pair difference in mechanism was found either.

### Audio/pronunciation source

- **Confirmed** (from code): pronunciation audio comes from Language
  Reactor's own CDN - `GET
  https://api-cdn-plus.dioco.io/base_items_getSentenceTTS_3` (and a sibling
  `getSavedItemTTS` endpoint), taking arbitrary text + a caching hash, used
  directly as an `<audio>` element's source. No reference to Google
  Translate TTS, Google Cloud/Azure/Amazon TTS, ElevenLabs, or Forvo appears
  in the code.
- The browser's `speechSynthesis` API is referenced only via a `getVoices()`
  call, with no `SpeechSynthesisUtterance` usage found - so the local
  OS/browser TTS voices don't appear to actually be used to speak words in
  this build, despite an unverified forum commenter's guess that it's
  "Edge/Microsoft TTS" (that claim is user speculation, not from Language
  Reactor staff, and the code contradicts it).
- **Open question**: which TTS engine powers Dioco's own backend endpoint is
  unknown - no official source names it.
- No tier-gating logic (free vs. paid using different sources) was found in
  a client-side code grep; several unofficial competitor/review sites
  describe the paid tier as adding quota/features (unlimited saved
  words/phrases, Anki export, a beefier Lexa AI), not different underlying
  data sources - but this is inferred from third-party reviews, not an
  official statement.

**Bottom line**: Language Reactor doesn't call any public dictionary/TTS API
directly from the client - everything is proxied through their own backend,
which likely does the real work (translation/LLM/TTS) server-side using
services we can't identify from outside. It's a proprietary, sizeable
backend investment, not a thin wrapper around a public API - not something
straightforwardly reproducible by calling "the same API they use."

## 2. Dictionary / definition APIs surveyed

| Source | Free tier | Signup | Caching allowed? | Language coverage | Notes |
|---|---|---|---|---|---|
| **Free Dictionary API** (dictionaryapi.dev) | Free, no documented limit | None | Not addressed either way (no formal ToS) | English deepest; ~13 other languages listed | Single-maintainer hobby service handling 10M+ req/month per its own README, soliciting donations - real risk of future instability. GPL-3.0 licensed code. Bundles phonetics + community audio URLs (from Wiktionary). |
| **Merriam-Webster Developer APIs** | 1,000 queries/day/key, max 2 reference works, non-commercial only | Required | Ambiguous - bulk/automated querying explicitly banned; per-lookup caching not addressed | Primarily English; has a dedicated Spanish-English product | Mandatory Merriam-Webster logo attribution. Includes audio. Real dictionary depth. |
| **Wordnik API** | 100 calls/hour ("Basic," free for nonprofit/research) | Required (API key) | **Explicitly disallowed on the free tier** ("we do not allow caching of any data") | English only | Aggregates 5 dictionary sources + 10M+ real example sentences + audio. Funding is thin (non-profit, donation-reliant). |
| **Datamuse API** | ~100k req/day, informally, **no key today - key required starting Jan 1 2027** | None (until 2027) | Not addressed; underlying sources (WordNet, Wiktionary, CMU dict, Google Books Ngrams) are open-licensed, favorable for reuse | English-centric | Not a real dictionary - definitions only via an opt-in `md=d` flag, and it's a thin passthrough of Wiktionary/WordNet glosses, not original content. |
| **WordsAPI** (via RapidAPI) | 2,500 req/day ("Basic") | RapidAPI account + key | **Yes, but time-limited**: "may cache... for up to 24 hours, after which cached data must be purged" | English only | Definitions from WordNet, frequency from OpenSubtitles, pronunciation/syllables from the CMU Pronouncing Dictionary. No resale; can't be used to build a competing dictionary. |
| **Oxford Dictionaries API** | 500-call sandbox trial only - **no ongoing free tier** | Required | Not applicable (paid) | ~26-35 languages | Confirmed still actively maintained (changelog entries as recent as May 2026). Paid plans start ~£50/month. The free consumer site (Lexico) shut down in 2022, but the *developer API* is unaffected and still running. |
| **Wiktionary** (live API) | No hard limit, but must be "considerate" (sequential requests, batch lookups); mandatory `User-Agent` header identifying your app | None for read access | Governed by Wiktionary's own license (see below) | All languages Wiktionary covers | `en.wiktionary.org/w/api.php`, `action=parse`/`action=query`. CC BY-SA 4.0 + GFDL - attribution and share-alike required for reuse/derivatives. |
| **Kaikki.org / wiktextract** | Free, static JSON/JSONL downloads | None | Yes - it's a downloadable dataset | English + ~21 languages w/ English glosses + ~20 native-gloss editions | Pre-parsed Wiktionary dumps (word/pos/lang/senses/translations/pronunciations already extracted). The extraction tool itself (`wiktextract`) is MIT-licensed; underlying content still follows Wiktionary's CC BY-SA terms. Great fit if you want a local, no-network dictionary. |
| **Cambridge Dictionary** | **No free API tier at all** | N/A | **Explicitly prohibited** - site terms ban scraping/storing content "on a server... or create an electronic database" | - | Official API is licensing-only (contact sales); a 30-day eval key exists only after a sales conversation. High legal risk if scraped instead. |
| **Collins Dictionary** | **5,000 calls/month, genuinely free**, self-serve signup | Required (apply via form) | Not addressed for the API; site terms ban bulk reproduction/storage of *scraped* content specifically | English + bilingual pairs | The one "big name" dictionary with a real, self-serve free API tier. Automated creation of multiple keys is banned. |
| **ConceptNet** | Free, no key; 3,600 req/hour (bursts to 120/min) | None | Governed by CC BY-SA (see below) | Multilingual | Not a dictionary - a semantic relation graph (`UsedFor`, `IsA`, `RelatedTo`, etc.). Useful for "related words," not definitions. Mandatory attribution + credit to its own upstream sources (Wiktionary, WordNet, DBPedia, etc). |
| **PanLex** | Free; soft guidance of ≤2 req/sec | Unclear from docs | CC BY-NC-SA 4.0 for the live API/DB - **non-commercial use is fine**, commercial requires written permission | "Thousands" of language varieties | Cross-lingual *translation* lookups, not definitions/glosses. |
| **WordNet** (Princeton) | Free, no rate limit (it's a local dataset, not a hosted API) | None | Trivially yes - you host it | English (extend via Open Multilingual WordNet / Global WordNet for others) | Very permissive license (free for any purpose, just keep the copyright notice, don't use Princeton's name in advertising). Best license clarity of anything surveyed. Requires building your own lookup/serving layer (e.g. via Python's NLTK). |
| **Lexicala API** | 200 calls/month, non-commercial | Required | Not included by default; must request it | 50 languages | Aggregates multiple publishers (K Dictionaries). 200/month is thin for a live click-to-define feature without caching, and caching needs a support request. Can't modify/alter displayed content beyond shortening. |
| **dbnary** | Free; public SPARQL endpoint (soft query-complexity limits, no hard quota published) + monthly dumps | None | Governed by CC BY-SA (inherited from Wiktionary) | 26-27 Wiktionary language editions | Wiktionary re-published as queryable Linked Data (RDF/OntoLex). Real definitions + translations, live queryable, but SPARQL is a steeper technical lift than a plain REST/JSON API. |
| **Urban Dictionary** (unofficial wrappers only) | Free, "no rate limits" per one wrapper | None | Unclear - built on scraping | English slang | **No official API exists.** Wrappers explicitly disclaim affiliation and acknowledge scraping UD's content - real (if low-stakes for a hobby project) legal/ToS ambiguity. Fine for slang flavor, not a primary source. |

## 3. Pronunciation / audio APIs surveyed

| Source | Type | Free? | Caching allowed? | Notes |
|---|---|---|---|---|
| **Forvo API** | Real native-speaker recordings, 6M+ clips, 430+ languages | **No free tier anymore** - cheapest is "Non-Profit" at **$2/month**, 500 req/day, non-commercial only | **Explicitly prohibited** - "It is not allowed to cache audio pronunciations," and generated links expire after 2 hours (must re-fetch live each time) | Requires a visible attribution badge/link on all non-Corporate plans. Some stale write-ups online still claim a free tier - the live pricing page confirms that's no longer true. Highest-authenticity audio of anything surveyed, but real cost + no-caching constraint for a hobby app. |
| **Google Translate TTS** (unofficial endpoint) | Synthetic | Yes, free (unofficial/reverse-engineered, no ToS/quota docs at all) | No governing terms either way | Already used elsewhere in this project (Robot Shadowing's TTS). Can break/be rate-limited/blocked without notice since it's not a sanctioned product - known, accepted tradeoff already documented in `robot-shadowing-spec.md`. |
| **Web Speech API** (`speechSynthesis`) | Synthetic, fully local (device/OS's own voices) | Yes - not a network API at all, no key/quota/signup | N/A - nothing to cache, generated on-device each time | Zero cost, zero network dependency, zero ToS. Voice availability/quality varies a lot by OS/browser - some devices only ship one robotic voice per language. Reliably fires on iOS Safari specifically inside a direct user-gesture handler - which a word click already is, so that fragility mostly doesn't bite this specific use case. Widely supported (Chrome, Edge, Firefox, Safari, desktop + mobile). |
| **Dictionary APIs with bundled audio** | Real or synthetic depending on source | Same as parent dictionary API | Same as parent dictionary API | Free Dictionary API bundles community audio URLs (sourced from Wiktionary) alongside phonetics; Merriam-Webster and WordsAPI (via CMU Pronouncing Dictionary) also include pronunciation data/audio in their responses. No separate integration needed if already using one of these for definitions. |

## 4. Verification: proof the Merriam-Webster API is real, and gives real audio

Prompted by a direct ask to actually prove this rather than take it on
priors: a follow-up research pass re-fetched Merriam-Webster's developer
docs live and dug up the exact technical details, since "there's an API for
that" is a claim worth backing with more than a name-drop.

- **The API is live today, not a dead/legacy product.** `dictionaryapi.com`
  describes itself as giving "access to a comprehensive resource of
  dictionary and thesaurus content as well as specialized medical, Spanish,
  ESL, and student-friendly vocabulary." The product page for **"Merriam-
  Webster's Collegiate® Dictionary with Audio"**
  (`dictionaryapi.com/products/api-collegiate-dictionary`) states it has
  "more than 250,000 authoritative definitions, **111,000 audio
  pronunciations**." Registration (`dictionaryapi.com/register/index`) is a
  live, working signup form - not a broken/abandoned page.
- **The audio field and playback-URL construction are fully, precisely
  documented** (`dictionaryapi.com/products/json`), not hand-waved:
  - The JSON response's `hwi.prs[].sound.audio` field holds a base filename
    (e.g. `attack001`).
  - The exact subdirectory rule, quoted directly from their docs: *"if
    `audio` begins with 'bix', the subdirectory should be 'bix'; if `audio`
    begins with 'gg', the subdirectory should be 'gg'; if `audio` begins
    with a number or punctuation, the subdirectory should be 'number';
    otherwise, the subdirectory is equal to the first letter of `audio`."*
  - Full URL template: `https://media.merriam-webster.com/audio/prons/
    [language]/[country]/[format]/[subdirectory]/[filename].[format]` - e.g.
    `.../audio/prons/en/us/mp3/p/pajama02.mp3`. Note the `en/us` segment is
    baked directly into the URL scheme - this is specifically the American
    English audio path.
  - Three supported formats: `mp3`, `wav`, `ogg`.
  - This is a deterministic, no-extra-auth-needed URL you can build entirely
    from one API response field - about as "provably real" as an API gets.
- **Real human recordings, not TTS** - Merriam-Webster's own consumer App
  Store listing states plainly: *"Audio Pronunciations: voiced by real
  English speakers, not text-to-speech robots."* That's their own marketing
  claim (for the consumer app rather than the API docs page specifically),
  but the same `media.merriam-webster.com` audio files back both the app and
  the API, so it's a reasonable read that the API serves the same
  human-recorded clips. A separate MW help page (accessed via cache after a
  direct 403) adds: *"Male and female voices recorded all the pronunciation
  words,"* with about 105,000 entries carrying audio - consistent with the
  111,000 figure above, and explicitly **not** covering thesaurus entries,
  abbreviations, or open compounds.
- **Free-tier terms, re-quoted directly from the live ToS/FAQ** (not
  reworded from memory): *"Reference queries do not exceed 1000/day/
  reference work"*; *"you do not use more than two reference works"*; *"you
  agree not to... submit any automated or recorded queries to the Service
  unless otherwise approved in writing by Merriam-Webster"*; *"All
  applications using Merriam-Webster APIs must feature the Merriam-Webster
  logo."* A live, per-click lookup from a hobby app isn't the kind of bulk
  automation this ban targets, but it's worth knowing it's there.

Net: this isn't a guess or an inference from a marketing page - it's a real,
currently-registerable API with a fully documented, reproducible audio-URL
scheme and a specific, credible claim of human (not synthetic) recordings.

## 5. Cloud TTS APIs - higher-quality alternatives to Google Translate/Web Speech

A separate research pass (prompted by "Google Translate already sounds
robotic") surveyed the major paid cloud TTS providers, since actual
*dictionary* audio (Merriam-Webster, Forvo, etc.) is real human speech but
narrower in coverage (only whole dictionary words, not arbitrary sentences),
while these are synthetic but dramatically better than Google Translate's
free endpoint or the browser's Web Speech API.

| Provider | Best free quota | Duration | Card required? | Caching explicitly OK? | Free tier allows commercial use? |
|---|---|---|---|---|---|
| **Google Cloud Text-to-Speech** | 4M chars/mo (Standard voices); 1M/mo for WaveNet/Neural2/Studio/Chirp3-HD | Perpetual ("Always Free") | Yes | Not addressed either way; nothing prohibits it | Not restricted by tier |
| **Amazon Polly** | 5M chars/mo (Standard, perpetual); 1M/mo (Neural), 500K/mo (Long-Form), 100K/mo (Generative) - the latter three only for the account's first 12 months | Standard: perpetual. Others: 12 months, then billed | Yes | **Yes, explicit**: *"You can cache and replay Amazon Polly's generated speech at no additional cost."* | Not restricted by tier |
| **Microsoft Azure AI Speech (F0)** | 500K chars/mo neural TTS | Perpetual (no expiry, unlike AWS) | Yes | Reported as generally fine, not spelled out in the primary ToS | **No** - commercial-use rights for prebuilt neural voice output are reserved to the paid tier |
| **ElevenLabs** | 10,000 credits/mo (~10 minutes of audio) | Perpetual | No | Yes, explicit (you retain rights to Output) | **No** - free tier is non-commercial only, plus attribution required if the audio is published publicly |

Quality notes: Amazon Polly's Neural/Long-Form/Generative tiers and Azure's
HD voices are both described in their own docs as approaching natural human
speech (Polly's Long-Form is pitched as sounding "like a live human being");
ElevenLabs has the strongest general reputation for realism but the
smallest free quota. Google's WaveNet/Neural2 voices sit a clear step above
its free Standard/Translate-endpoint voices in naturalness.

For this project specifically: since this is a non-commercial personal app,
the commercial-use restrictions on Azure/ElevenLabs' free tiers don't
matter. Amazon Polly stands out as the most generous *and* the most
explicit about caching being fine - a good pick if dictionary-only audio
coverage (Merriam-Webster/Forvo) proves too narrow and a synthetic
fallback for arbitrary words/sentences is wanted.

## 6. Recommendation, if this gets built

Given this is a personal, non-commercial, low-volume project, and the app
already has a working pattern for exactly this kind of problem
(`native-exp-server/` - a local companion server that does server-side
fetching to sidestep CORS/ToS issues a pure client-side app would hit):

- **Definitions**: start with **Free Dictionary API** (dictionaryapi.dev) -
  zero setup, no key, and it already bundles phonetics/audio references. Its
  single-maintainer reliability risk is the main downside; if that becomes a
  problem, **WordNet via NLTK** is a strong fallback since it's a local
  dataset with no rate limit and the most permissive license of anything
  surveyed - it would fit naturally as another NativeExpServer endpoint,
  same pattern as the existing `/subtitles` route.
- **Pronunciation audio, real (non-robotic) American English specifically**:
  **Merriam-Webster's Collegiate Dictionary with Audio** is the strongest
  fit given the explicit ask - real recorded speech, ~111,000 entries,
  generous 1,000/day free quota, deterministic no-extra-cost audio URLs (see
  §4). The friction is signup + a visible logo + no bulk automation, all
  manageable for a live per-click lookup. **Forvo** would give the most
  authentic native-speaker recordings and lets you filter to American
  speakers specifically, but now costs a minimum $2/month and explicitly
  forbids caching results.
- **If a word has no dictionary-audio entry, or arbitrary sentence-level
  audio is needed**: fall back to a synthetic voice. **Amazon Polly**
  (generous free quota, explicit caching permission) or **Google Cloud
  TTS's WaveNet/Neural2 voices** are meaningfully better-sounding than
  Google Translate's free endpoint or the browser's Web Speech API, at the
  cost of needing a cloud account + card on file. **Web Speech API** stays
  the zero-setup fallback if avoiding any signup/cost entirely matters more
  than voice quality.
- Anything CC BY-SA-licensed (Wiktionary, Kaikki, dbnary, ConceptNet) is
  free to use but needs a visible attribution line somewhere in the app if
  used directly.

No decision has been made to build this - this doc exists to make that
decision informed, whenever it comes up.
