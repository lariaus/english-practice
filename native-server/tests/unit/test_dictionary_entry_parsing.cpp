#include "dictionary_entry_detail.h"

#include <catch2/catch_test_macros.hpp>

using dictionary_utils::AudioPronunciation;
using dictionary_utils::DictionaryEntry;
using dictionary_utils::Entry;
using dictionary_utils::IpaPronunciation;
using dictionary_utils::PhoneticEntry;
using dictionary_utils::Sense;
using dictionary_utils::WiktionaryWordInfo;
using dictionary_utils::WordInfo;
namespace detail = dictionary_utils::detail;

TEST_CASE("normalizeAccentLabel recognizes every US spelling and prioritizes it",
          "[dictionary_entry][accent]") {
  CHECK(detail::normalizeAccentLabel({"US"}) == "US");
  CHECK(detail::normalizeAccentLabel({"GenAm"}) == "US");
  CHECK(detail::normalizeAccentLabel({"General American"}) == "US");
  CHECK(detail::normalizeAccentLabel({"UK", "US"}) == "US");  // US wins even when mixed
}

TEST_CASE("normalizeAccentLabel recognizes UK and AU spellings", "[dictionary_entry][accent]") {
  CHECK(detail::normalizeAccentLabel({"UK"}) == "UK");
  CHECK(detail::normalizeAccentLabel({"RP"}) == "UK");
  CHECK(detail::normalizeAccentLabel({"Received Pronunciation"}) == "UK");
  CHECK(detail::normalizeAccentLabel({"AU"}) == "AU");
  CHECK(detail::normalizeAccentLabel({"Australia"}) == "AU");
}

TEST_CASE("normalizeAccentLabel returns empty for anything not confidently classified",
          "[dictionary_entry][accent]") {
  CHECK(detail::normalizeAccentLabel({"Southern England"}).empty());
  CHECK(detail::normalizeAccentLabel({"Scotland"}).empty());
  CHECK(detail::normalizeAccentLabel({}).empty());
}

TEST_CASE("buildMeanings maps entries/senses into the JS-shaped meanings list, as a superset",
          "[dictionary_entry][meanings]") {
  Entry entry;
  entry.partOfSpeech = "adjective";
  Sense sense;
  sense.definition = "having good sense";
  sense.examples = {"a smart choice", "she is smart"};
  sense.synonyms = {"clever", "intelligent"};
  sense.antonyms = {"dumb"};
  entry.senses.push_back(sense);

  auto meanings = detail::buildMeanings({entry});

  REQUIRE(meanings.size() == 1);
  CHECK(meanings[0].partOfSpeech == "adjective");
  REQUIRE(meanings[0].definitions.size() == 1);
  const auto& def = meanings[0].definitions[0];
  CHECK(def.definition == "having good sense");
  CHECK(def.examples == std::vector<std::string>({"a smart choice", "she is smart"}));
  CHECK(def.synonyms == std::vector<std::string>({"clever", "intelligent"}));
  CHECK(def.antonyms == std::vector<std::string>{"dumb"});  // superset - not in today's JS shape
}

TEST_CASE("buildPhonetics attaches the cached audio to the matching US IPA row",
          "[dictionary_entry][phonetics]") {
  IpaPronunciation us;
  us.text = "/smɑɹt/";
  us.accents = {"GenAm"};
  IpaPronunciation uk;
  uk.text = "/smɑːt/";
  uk.accents = {"RP"};

  auto rows = detail::buildPhonetics({us, uk}, "dictionaries/wiktionaryapi/en-us-smart.ogg", {});

  REQUIRE(rows.size() == 2);
  // US-labeled row sorted first.
  CHECK(rows[0].label == "US");
  CHECK(rows[0].text == "/smɑɹt/");
  CHECK(rows[0].audio == "dictionaries/wiktionaryapi/en-us-smart.ogg");
  CHECK(rows[1].label == "UK");
  CHECK(rows[1].audio.empty());  // non-US rows never get an audio path
}

TEST_CASE("buildPhonetics recognizes 'GA' as US, merging with cached audio into one row",
          "[dictionary_entry][phonetics]") {
  // Real shape, captured from "endurance"'s Pronunciation section: the IPA
  // line tags US as "GA" while the audio line separately tags it "US" -
  // two different real abbreviations for the same accent on the same word.
  // Regression test for a real bug: "GA" wasn't recognized, so this used to
  // produce two rows (a blank-text audio-only "US" row, plus an unlabeled
  // text-only row) instead of merging into one row with both.
  IpaPronunciation ga;
  ga.text = "/ɪnˈdʊɹəns/";
  ga.accents = {"GA"};
  IpaPronunciation rp;
  rp.text = "/ɪnˈdjʊəɹəns/";
  rp.accents = {"RP"};

  auto rows = detail::buildPhonetics({ga, rp}, "dictionaries/wiktionaryapi/en-us-endurance.ogg", {});

  REQUIRE(rows.size() == 2);
  CHECK(rows[0].label == "US");
  CHECK(rows[0].text == "/ɪnˈdʊɹəns/");
  CHECK(rows[0].audio == "dictionaries/wiktionaryapi/en-us-endurance.ogg");
  CHECK(rows[1].label == "UK");
}

TEST_CASE("buildPhonetics appends an audio-only row when no IPA entry matches the cached accent",
          "[dictionary_entry][phonetics]") {
  IpaPronunciation uk;
  uk.text = "/smɑːt/";
  uk.accents = {"RP"};

  auto rows = detail::buildPhonetics({uk}, "dictionaries/wiktionaryapi/en-us-smart.ogg", {});

  // Raw source order: the IPA row first, then the appended audio-only row -
  // no US-first sorting happens here anymore (moved to dictionaryClient.js).
  REQUIRE(rows.size() == 2);
  CHECK(rows[0].label == "UK");
  CHECK(rows[1].label == "US");
  CHECK(rows[1].text.empty());
  CHECK(rows[1].audio == "dictionaries/wiktionaryapi/en-us-smart.ogg");
}

TEST_CASE("buildPhonetics merges a lone untagged IPA row with the cached audio instead of "
          "leaving two disconnected rows",
          "[dictionary_entry][phonetics]") {
  // Real shape from "hierarchy": one IPA line with no accent tag at all,
  // plus a separately-tagged {{audio|en|en-us-hierarchy.ogg|a=US}} line.
  // Wiktionary's own convention is that an untagged line applies regardless
  // of dialect - previously this produced two disconnected rows (one with
  // the real text and no audio, one audio-only with no text) instead of one
  // row with both.
  IpaPronunciation untagged;
  untagged.text = "/ˈhaɪ.ə.ɹɑː(ɹ).ki/";

  auto rows = detail::buildPhonetics({untagged}, "dictionaries/wiktionaryapi/en-us-hierarchy.ogg", {});

  REQUIRE(rows.size() == 1);
  CHECK(rows[0].label == "US");
  CHECK(rows[0].text == "/ˈhaɪ.ə.ɹɑː(ɹ).ki/");
  CHECK(rows[0].audio == "dictionaries/wiktionaryapi/en-us-hierarchy.ogg");
}

TEST_CASE("buildPhonetics does not guess which untagged row to merge when there's more than one",
          "[dictionary_entry][phonetics]") {
  IpaPronunciation untagged1;
  untagged1.text = "/ˈpi.pəl/";
  IpaPronunciation untagged2;
  untagged2.text = "[ˈpi.pɯ̽ɫ]";

  auto rows = detail::buildPhonetics({untagged1, untagged2}, "dictionaries/wiktionaryapi/en-us-people.ogg", {});

  // Falls back to the old behavior: both untagged rows stay as-is, and a
  // separate audio-only row is appended - no basis for picking one over
  // the other.
  REQUIRE(rows.size() == 3);
  CHECK(rows[0].label.empty());
  CHECK(rows[0].audio.empty());
  CHECK(rows[1].label.empty());
  CHECK(rows[1].audio.empty());
  CHECK(rows[2].label == "US");
  CHECK(rows[2].text.empty());
  CHECK(rows[2].audio == "dictionaries/wiktionaryapi/en-us-people.ogg");
}

TEST_CASE("buildPhonetics only merges a row with genuinely no accent tag, not one tagged with "
          "something non-regional",
          "[dictionary_entry][phonetics]") {
  // Real shape from "especially": the first IPA line has no accent tag at
  // all; the second is explicitly tagged "nonstandard, proscribed" - a real
  // tag, just not one normalizeAccentLabel() maps to a region. Both
  // normalize to an empty label, but only the genuinely-untagged first line
  // should be treated as the merge candidate.
  IpaPronunciation untagged;
  untagged.text = "/ɪˈspɛʃ.(ə.)li/";
  IpaPronunciation nonstandard;
  nonstandard.text = "/ɪkˈspɛʃ.(ə.)li/";
  nonstandard.accents = {"nonstandard", "proscribed"};

  auto rows =
      detail::buildPhonetics({untagged, nonstandard}, "dictionaries/wiktionaryapi/en-us-especially.ogg", {});

  REQUIRE(rows.size() == 2);
  CHECK(rows[0].label == "US");
  CHECK(rows[0].text == "/ɪˈspɛʃ.(ə.)li/");
  CHECK(rows[0].audio == "dictionaries/wiktionaryapi/en-us-especially.ogg");
  CHECK(rows[1].label.empty());
  CHECK(rows[1].text == "/ɪkˈspɛʃ.(ə.)li/");
  CHECK(rows[1].audio.empty());
}

TEST_CASE("buildPhonetics recognizes an IPA row tagged only with a qualifier shared by the "
          "cached US audio, even with no country tag on the IPA side",
          "[dictionary_entry][phonetics]") {
  // Real shape from "at": the IPA line carries only a register qualifier
  // ("strong form"), no country - but the cached audio is tagged
  // ["strong form", "US"], the same qualifier plus a country. Regression
  // test for a real gap: these used to stay two disconnected rows (an
  // unlabeled text-only row, plus a separate audio-only "US" row) because
  // neither side's tags matched a recognized region on their own.
  IpaPronunciation strong;
  strong.text = "/æt/";
  strong.accents = {"strong form"};
  IpaPronunciation weak;
  weak.text = "/ət/";
  weak.accents = {"weak form"};

  auto rows = detail::buildPhonetics({strong, weak}, "dictionaries/wiktionaryapi/en-us-at.ogg",
                                      {"strong form", "US"});

  REQUIRE(rows.size() == 2);
  CHECK(rows[0].label == "US");
  CHECK(rows[0].text == "/æt/");
  CHECK(rows[0].audio == "dictionaries/wiktionaryapi/en-us-at.ogg");
  // "weak form" is not a subset of ["strong form", "US"] - stays unlabeled.
  CHECK(rows[1].label.empty());
  CHECK(rows[1].audio.empty());
}

TEST_CASE("buildPhonetics does not treat an untagged IPA row as a qualifier-subset match",
          "[dictionary_entry][phonetics]") {
  // An empty accents list is trivially "a subset" of anything - guarded
  // against explicitly, since an untagged row is the OTHER merge rule's
  // job (buildPhonetics's own untagged-merge heuristic), not this one's.
  IpaPronunciation untagged;
  untagged.text = "/wɜːd/";

  auto rows =
      detail::buildPhonetics({untagged}, "dictionaries/wiktionaryapi/en-us-word.ogg", {"strong form", "US"});

  // Falls through to the untagged-merge rule instead (exactly one untagged
  // row + a cached US audio) - same end result, but via that rule, not the
  // qualifier-subset one.
  REQUIRE(rows.size() == 1);
  CHECK(rows[0].label == "US");
}

TEST_CASE("buildPhonetics does not promote a qualifier tag not present on the cached audio",
          "[dictionary_entry][phonetics]") {
  IpaPronunciation regional;
  regional.text = "/faɪl/";
  regional.accents = {"cot-caught"};

  // Cached audio carries no qualifier at all, just a plain country tag -
  // nothing to cross-reference against.
  auto rows = detail::buildPhonetics({regional}, "dictionaries/wiktionaryapi/en-us-file.ogg", {"US"});

  REQUIRE(rows.size() == 2);  // no IPA row promoted - audio-only row appended instead
  CHECK(rows[0].label.empty());
  CHECK(rows[1].label == "US");
  CHECK(rows[1].text.empty());
}

TEST_CASE("buildPhonetics recognizes the letter-name qualifier over the raw phoneme when the "
          "cached audio has no qualifier to cross-reference",
          "[dictionary_entry][phonetics]") {
  // Real shape from "l": the cached audio is just a plain "US" (no shared
  // qualifier for the H-style subset rule to match against), so this is a
  // narrower, dedicated fallback - a real audio recording of a single
  // letter says its NAME out loud ("ell"), not its bare phoneme ("l").
  IpaPronunciation letterName;
  letterName.text = "/ɛl/";
  letterName.accents = {"name of letter"};
  IpaPronunciation phoneme;
  phoneme.text = "/l/";
  phoneme.accents = {"phoneme"};

  auto rows = detail::buildPhonetics({letterName, phoneme}, "dictionaries/wiktionaryapi/en-us-l.ogg", {"US"});

  REQUIRE(rows.size() == 2);
  CHECK(rows[0].label == "US");
  CHECK(rows[0].text == "/ɛl/");
  CHECK(rows[0].audio == "dictionaries/wiktionaryapi/en-us-l.ogg");
  CHECK(rows[1].label.empty());  // "phoneme" alone doesn't qualify
  CHECK(rows[1].audio.empty());
}

TEST_CASE("buildPhonetics reserves the cached audio for a lone untagged row even when a "
          "differently-tagged row would otherwise normalize to US first",
          "[dictionary_entry][phonetics]") {
  // Real shape from "afraid": the untagged row is the one the US audio is
  // actually nested under in the source, but a *different* row explicitly
  // tagged "Southern US" would - without this reservation - normalize to
  // "US" via isUsAccentTag's own compound-region recognition and grab the
  // audio for itself first, leaving the real match with neither. Regression
  // test for exactly that: the untagged row must win the cached audio
  // regardless of which one is processed "first" by any other rule.
  IpaPronunciation general;
  general.text = "/əˈfɹeɪd/";
  IpaPronunciation southern;
  southern.text = "/əˈfɹɛd/";
  southern.accents = {"Southern US"};

  auto rows = detail::buildPhonetics({general, southern}, "dictionaries/wiktionaryapi/en-us-afraid.ogg", {});

  REQUIRE(rows.size() == 2);
  CHECK(rows[0].label == "US");
  CHECK(rows[0].text == "/əˈfɹeɪd/");
  CHECK(rows[0].audio == "dictionaries/wiktionaryapi/en-us-afraid.ogg");
  // Still correctly recognized as a US variant (isUsAccentTag matches
  // "Southern US") - it just doesn't ALSO get the same audio clip, since
  // that's already spoken for by the row it actually belongs to.
  CHECK(rows[1].label == "US");
  CHECK(rows[1].audio.empty());
}

TEST_CASE("buildPhonetics returns no rows and no audio-only row when nothing was cached",
          "[dictionary_entry][phonetics]") {
  IpaPronunciation uk;
  uk.text = "/smɑːt/";
  uk.accents = {"RP"};

  auto rows = detail::buildPhonetics({uk}, "", {});

  REQUIRE(rows.size() == 1);
  CHECK(rows[0].label == "UK");
}

TEST_CASE("buildPhonetics returns every row unfiltered, in source order",
          "[dictionary_entry][phonetics]") {
  // Real shape - "people" lists several near-identical unlabeled
  // transcriptions alongside its US row in Wiktionary's own source order.
  // Deduping/capping/sorting these for display is dictionaryClient.js's
  // job now, not this function's - it just reflects the source faithfully.
  IpaPronunciation unlabeled1;
  unlabeled1.text = "/ˈpi.pəl/";
  IpaPronunciation us;
  us.text = "/ˈpi.pəl/";
  us.accents = {"GA"};
  IpaPronunciation unlabeled2;
  unlabeled2.text = "[ˈpi.pɯ̽ɫ]";
  IpaPronunciation unlabeled3;
  unlabeled3.text = "/ˈpipɨl/";

  auto rows = detail::buildPhonetics({unlabeled1, us, unlabeled2, unlabeled3}, "", {});

  REQUIRE(rows.size() == 4);
  CHECK(rows[0].text == "/ˈpi.pəl/");
  CHECK(rows[0].label.empty());
  CHECK(rows[1].label == "US");
  CHECK(rows[2].text == "[ˈpi.pɯ̽ɫ]");
  CHECK(rows[3].text == "/ˈpipɨl/");
}

TEST_CASE("buildPhonetics does not dedup exact (text, label) duplicates",
          "[dictionary_entry][phonetics]") {
  // Deduping is dictionaryClient.js's job now, not this function's.
  IpaPronunciation a;
  a.text = "/wɜːd/";
  a.accents = {"RP"};
  IpaPronunciation b;
  b.text = "/wɜːd/";
  b.accents = {"UK"};  // normalizes to the same label as RP

  auto rows = detail::buildPhonetics({a, b}, "", {});
  CHECK(rows.size() == 2);
}

// Mirrors exactly the sequence DictionaryEntry::fetch() itself runs in
// full (non-fast) mode: convert each source independently, then merge with
// Wiktionary listed first - every field (word/phonetics/meanings/sourceUrl/
// license) uses the same generic first-non-empty-wins rule, no per-field
// override - see fetch()'s own comments for why Wiktionary goes first.
namespace {
DictionaryEntry assembleForTest(const WordInfo& freeDict, const WiktionaryWordInfo& wikt,
                                 const std::string& originalWord) {
  DictionaryEntry wiktEntry = dictionaryEntryFrom(wikt);
  DictionaryEntry freeDictEntry = dictionaryEntryFrom(freeDict);
  return detail::mergeDictionaryEntries({wiktEntry, freeDictEntry}, originalWord);
}
}  // namespace

TEST_CASE("dictionaryEntryFrom(WordInfo) + dictionaryEntryFrom(WiktionaryWordInfo) + "
          "mergeDictionaryEntries combine both sources when both succeed",
          "[dictionary_entry][assemble]") {
  WordInfo freeDict;
  freeDict.word = "smart";
  freeDict.source.url = "https://en.wiktionary.org/wiki/smart";
  freeDict.source.license.name = "CC BY-SA 4.0";
  Entry entry;
  entry.partOfSpeech = "adjective";
  freeDict.entries.push_back(entry);

  WiktionaryWordInfo wikt;
  wikt.word = "smart";
  IpaPronunciation ipa;
  ipa.text = "/smɑɹt/";
  ipa.accents = {"GenAm"};
  wikt.ipa.push_back(ipa);
  AudioPronunciation audio;
  audio.accents = {"US"};
  audio.audio = "dictionaries/wiktionaryapi/en-us-smart.ogg";
  wikt.audio.push_back(audio);

  auto result = assembleForTest(freeDict, wikt, "smart");

  CHECK(result.word == "smart");
  CHECK(result.sourceUrl == "https://en.wiktionary.org/wiki/smart");
  CHECK(result.license == "CC BY-SA 4.0");
  REQUIRE(result.meanings.size() == 1);
  REQUIRE(result.phonetics.size() == 1);
  CHECK(result.phonetics[0].audio == "dictionaries/wiktionaryapi/en-us-smart.ogg");
  REQUIRE(result.usPhonetics() != nullptr);
  CHECK(result.usPhonetics()->text == "/smɑɹt/");
}

TEST_CASE("word and phonetics both prefer Wiktionary's data over FreeDictionaryAPIEntry's when "
          "both are non-empty, since Wiktionary is listed first",
          "[dictionary_entry][assemble]") {
  WordInfo freeDict;
  freeDict.word = "Smart";  // deliberately different capitalization from wikt.word
  Entry entry;
  entry.partOfSpeech = "adjective";
  dictionary_utils::Pronunciation pron;
  pron.text = "/smɑːt/";  // deliberately different text from wikt's IPA below
  pron.tags = {"Received Pronunciation"};
  entry.pronunciations.push_back(pron);
  freeDict.entries.push_back(entry);

  WiktionaryWordInfo wikt;
  wikt.word = "smart";
  IpaPronunciation ipa;
  ipa.text = "/smɑɹt/";
  ipa.accents = {"GenAm"};
  wikt.ipa.push_back(ipa);

  auto result = assembleForTest(freeDict, wikt, "smart");

  CHECK(result.word == "smart");
  REQUIRE(result.phonetics.size() == 1);
  CHECK(result.phonetics[0].text == "/smɑɹt/");
}

// The following assemble-level tests exercise the same audio/IPA richness
// priority as the direct mergeDictionaryEntries tests further down, but
// through the full realistic pipeline (raw WordInfo/WiktionaryWordInfo ->
// dictionaryEntryFrom -> mergeDictionaryEntries) - confirming the priority
// actually holds once real per-source conversion (buildPhonetics's own
// audio/IPA-row-matching logic) is in the loop too, not just against
// hand-built PhoneticEntry rows.

TEST_CASE("assembling: Wiktionary's audio+IPA phonetics beats FreeDictionaryAPI's IPA-only, "
          "meanings still come from FreeDictionaryAPI regardless",
          "[dictionary_entry][assemble][phonetics-richness]") {
  WordInfo freeDict;
  freeDict.word = "smart";
  Entry entry;
  entry.partOfSpeech = "adjective";
  Sense sense;
  sense.definition = "having good sense";
  entry.senses.push_back(sense);
  dictionary_utils::Pronunciation pron;
  pron.text = "/smɑːt/";  // FreeDictionaryAPI's own weaker, IPA-only pronunciation
  pron.tags = {"Received Pronunciation"};
  entry.pronunciations.push_back(pron);
  freeDict.entries.push_back(entry);

  WiktionaryWordInfo wikt;
  wikt.word = "smart";
  IpaPronunciation ipa;
  ipa.text = "/smɑɹt/";
  ipa.accents = {"GenAm"};
  wikt.ipa.push_back(ipa);
  AudioPronunciation audio;
  audio.accents = {"US"};
  audio.audio = "dictionaries/wiktionaryapi/en-us-smart.ogg";
  wikt.audio.push_back(audio);

  auto result = assembleForTest(freeDict, wikt, "smart");

  REQUIRE(result.meanings.size() == 1);
  CHECK(result.meanings[0].definitions[0].definition == "having good sense");
  REQUIRE(result.phonetics.size() == 1);
  CHECK(result.phonetics[0].text == "/smɑɹt/");
  CHECK(result.phonetics[0].audio == "dictionaries/wiktionaryapi/en-us-smart.ogg");
}

TEST_CASE("assembling: Wiktionary's audio-only phonetics beats FreeDictionaryAPI's real IPA text, "
          "per the audio > IPA priority",
          "[dictionary_entry][assemble][phonetics-richness]") {
  WordInfo freeDict;
  freeDict.word = "example";
  Entry entry;
  entry.partOfSpeech = "noun";
  Sense sense;
  sense.definition = "an instance serving to illustrate";
  entry.senses.push_back(sense);
  dictionary_utils::Pronunciation pron;
  pron.text = "/ɪɡˈzæmpəl/";  // real IPA text, no audio - FreeDictionaryAPI never has any
  entry.pronunciations.push_back(pron);
  freeDict.entries.push_back(entry);

  // Wiktionary found real US audio but no IPA line matched it at all - the
  // real, documented "audio-only row" scenario (see buildPhonetics's own
  // fallback), simulated here by leaving wikt.ipa entirely empty.
  WiktionaryWordInfo wikt;
  wikt.word = "example";
  AudioPronunciation audio;
  audio.accents = {"US"};
  audio.audio = "dictionaries/wiktionaryapi/en-us-example.ogg";
  wikt.audio.push_back(audio);

  auto result = assembleForTest(freeDict, wikt, "example");

  REQUIRE(result.meanings.size() == 1);  // meanings still merge independently
  REQUIRE(result.phonetics.size() == 1);
  CHECK(result.phonetics[0].audio == "dictionaries/wiktionaryapi/en-us-example.ogg");
  CHECK(result.phonetics[0].text.empty());
}

TEST_CASE("assembling: an IPA-only Wiktionary entry is used as-is when it's the richest (only) "
          "phonetics available anywhere",
          "[dictionary_entry][assemble][phonetics-richness]") {
  WordInfo freeDict;
  freeDict.word = "ephemeral";
  Entry entry;
  entry.partOfSpeech = "adjective";
  Sense sense;
  sense.definition = "lasting for a very short time";
  entry.senses.push_back(sense);
  freeDict.entries.push_back(entry);  // no pronunciations at all from this source

  WiktionaryWordInfo wikt;
  wikt.word = "ephemeral";
  IpaPronunciation ipa;
  ipa.text = "/ɪˈfɛmərəl/";
  ipa.accents = {"GenAm"};
  wikt.ipa.push_back(ipa);  // found a Pronunciation section, but no US audio on the page at all

  auto result = assembleForTest(freeDict, wikt, "ephemeral");

  REQUIRE(result.meanings.size() == 1);
  REQUIRE(result.phonetics.size() == 1);
  CHECK(result.phonetics[0].text == "/ɪˈfɛmərəl/");
  CHECK(result.phonetics[0].audio.empty());
}

TEST_CASE("assembling: falls back to FreeDictionaryAPI's IPA-only phonetics when Wiktionary "
          "genuinely has none",
          "[dictionary_entry][assemble][phonetics-richness]") {
  WordInfo freeDict;
  freeDict.word = "gadget";
  Entry entry;
  entry.partOfSpeech = "noun";
  dictionary_utils::Pronunciation pron;
  pron.text = "/ˈɡædʒɪt/";
  entry.pronunciations.push_back(pron);
  freeDict.entries.push_back(entry);

  // Wiktionary's own call succeeded (found the language section) but this
  // word genuinely has no Pronunciation section at all - a normal, common
  // outcome, not an error.
  WiktionaryWordInfo wikt;
  wikt.word = "gadget";

  auto result = assembleForTest(freeDict, wikt, "gadget");

  REQUIRE(result.phonetics.size() == 1);
  CHECK(result.phonetics[0].text == "/ˈɡædʒɪt/");
}

TEST_CASE("assembling: Wiktionary-only data (FreeDictionaryAPI genuinely found nothing) still "
          "produces real phonetics, with no meanings/sourceUrl/license",
          "[dictionary_entry][assemble][phonetics-richness]") {
  WiktionaryWordInfo wikt;
  wikt.word = "smart";
  IpaPronunciation ipa;
  ipa.text = "/smɑɹt/";
  ipa.accents = {"GenAm"};
  wikt.ipa.push_back(ipa);
  AudioPronunciation audio;
  audio.accents = {"US"};
  audio.audio = "dictionaries/wiktionaryapi/en-us-smart.ogg";
  wikt.audio.push_back(audio);

  auto result = assembleForTest(WordInfo(), wikt, "smart");

  REQUIRE(result.phonetics.size() == 1);
  CHECK(result.phonetics[0].text == "/smɑɹt/");
  CHECK(result.phonetics[0].audio == "dictionaries/wiktionaryapi/en-us-smart.ogg");
  CHECK(result.meanings.empty());
  CHECK(result.sourceUrl.empty());
  CHECK(result.license.empty());
}

TEST_CASE("dictionaryEntryFrom(WiktionaryWordInfo) contributes nothing but doesn't fail when "
          "Wiktionary genuinely found nothing",
          "[dictionary_entry][assemble]") {
  WordInfo freeDict;
  freeDict.word = "smart";
  Entry entry;
  entry.partOfSpeech = "adjective";
  freeDict.entries.push_back(entry);

  // Wiktionary's own call succeeded but found nothing (found=false, no
  // ipa/audio) - the real shape this reaches in full mode today, since a
  // genuine Wiktionary failure already throws before assembly happens.
  WiktionaryWordInfo wikt;

  auto result = assembleForTest(freeDict, wikt, "smart");

  CHECK(result.word == "smart");
  CHECK(result.meanings.size() == 1);
  CHECK(result.phonetics.empty());
  CHECK(result.usPhonetics() == nullptr);
}

TEST_CASE("falls back to the original queried word if neither source echoes one",
          "[dictionary_entry][assemble]") {
  auto result = assembleForTest(WordInfo(), WiktionaryWordInfo(), "mystery-word");
  CHECK(result.word == "mystery-word");
}

TEST_CASE("mergeDictionaryEntries takes the first non-empty value per field and never lets a "
          "later entry overwrite it",
          "[dictionary_entry][merge]") {
  DictionaryEntry first;
  first.word = "first-word";
  first.sourceUrl = "https://first.example";

  DictionaryEntry second;
  second.word = "second-word";       // first already has a word - must not overwrite
  second.license = "second-license";  // first has nothing here - should win

  auto merged = detail::mergeDictionaryEntries({first, second}, "");

  CHECK(merged.word == "first-word");
  CHECK(merged.sourceUrl == "https://first.example");
  CHECK(merged.license == "second-license");
}

TEST_CASE("mergeDictionaryEntries returns an all-empty entry for an empty list",
          "[dictionary_entry][merge]") {
  auto merged = detail::mergeDictionaryEntries({}, "");
  CHECK(merged.word.empty());
  CHECK(merged.phonetics.empty());
  CHECK(merged.meanings.empty());
}

namespace {
PhoneticEntry ipaOnlyRow(std::string text) {
  PhoneticEntry row;
  row.text = std::move(text);
  return row;
}

PhoneticEntry audioOnlyRow(std::string audio) {
  PhoneticEntry row;
  row.audio = std::move(audio);
  return row;
}
}  // namespace

TEST_CASE("mergeDictionaryEntries prefers an entry with audio+IPA over an earlier "
          "IPA-only entry, even though it's listed first",
          "[dictionary_entry][merge][phonetics-richness]") {
  DictionaryEntry ipaOnly;
  ipaOnly.phonetics = {ipaOnlyRow("/ipa-only/")};

  DictionaryEntry both;
  both.phonetics = {ipaOnlyRow("/rich/"), audioOnlyRow("path/rich.ogg")};

  auto merged = detail::mergeDictionaryEntries({ipaOnly, both}, "");
  REQUIRE(merged.phonetics.size() == 2);
  CHECK(merged.phonetics[0].text == "/rich/");
  CHECK(merged.phonetics[1].audio == "path/rich.ogg");
}

TEST_CASE("mergeDictionaryEntries prefers an audio-only entry over an earlier "
          "IPA-only entry, even though it's listed first",
          "[dictionary_entry][merge][phonetics-richness]") {
  DictionaryEntry ipaOnly;
  ipaOnly.phonetics = {ipaOnlyRow("/ipa-only/")};

  DictionaryEntry audioOnly;
  audioOnly.phonetics = {audioOnlyRow("path/audio-only.ogg")};

  auto merged = detail::mergeDictionaryEntries({ipaOnly, audioOnly}, "");
  REQUIRE(merged.phonetics.size() == 1);
  CHECK(merged.phonetics[0].audio == "path/audio-only.ogg");
  CHECK(merged.phonetics[0].text.empty());
}

TEST_CASE("mergeDictionaryEntries prefers an audio+IPA entry over an earlier "
          "audio-only entry, even though it's listed first",
          "[dictionary_entry][merge][phonetics-richness]") {
  DictionaryEntry audioOnly;
  audioOnly.phonetics = {audioOnlyRow("path/audio-only.ogg")};

  DictionaryEntry both;
  both.phonetics = {ipaOnlyRow("/rich/"), audioOnlyRow("path/rich.ogg")};

  auto merged = detail::mergeDictionaryEntries({audioOnly, both}, "");
  REQUIRE(merged.phonetics.size() == 2);
  CHECK(merged.phonetics[0].text == "/rich/");
  CHECK(merged.phonetics[1].audio == "path/rich.ogg");
}

TEST_CASE("mergeDictionaryEntries keeps the first entry on an equal-richness tie",
          "[dictionary_entry][merge][phonetics-richness]") {
  DictionaryEntry first;
  first.phonetics = {ipaOnlyRow("/first/")};

  DictionaryEntry second;
  second.phonetics = {ipaOnlyRow("/second/")};

  auto merged = detail::mergeDictionaryEntries({first, second}, "");
  REQUIRE(merged.phonetics.size() == 1);
  CHECK(merged.phonetics[0].text == "/first/");
}

TEST_CASE("entryCacheRelativePath lives under dictionaries/entries/ and never collides with an "
          "audio filename",
          "[dictionary_entry][cache]") {
  CHECK(detail::entryCacheRelativePath("en", "smart") == "dictionaries/entries/en-smart.json");
  // A word containing a path separator must not escape the intended
  // filename - percent-encoded, not passed through raw.
  CHECK(detail::entryCacheRelativePath("en", "a/b") == "dictionaries/entries/en-a%2Fb.json");
}

TEST_CASE("dictionaryEntryToJson/dictionaryEntryFromJson round-trip losslessly",
          "[dictionary_entry][cache]") {
  dictionary_utils::DictionaryEntry entry;
  entry.word = "smart";
  entry.sourceUrl = "https://en.wiktionary.org/wiki/smart";
  entry.license = "CC BY-SA 4.0";

  dictionary_utils::PhoneticEntry phonetic;
  phonetic.text = "/smɑɹt/";
  phonetic.label = "US";
  phonetic.audio = "dictionaries/wiktionaryapi/en-us-smart.ogg";
  entry.phonetics.push_back(phonetic);

  dictionary_utils::MeaningEntry meaning;
  meaning.partOfSpeech = "adjective";
  dictionary_utils::DefinitionEntry def;
  def.definition = "having good sense";
  def.examples = {"a smart choice"};
  def.synonyms = {"clever"};
  def.antonyms = {"dumb"};
  meaning.definitions.push_back(def);
  entry.meanings.push_back(meaning);

  auto roundTripped = detail::dictionaryEntryFromJson(detail::dictionaryEntryToJson(entry));

  CHECK(roundTripped.word == entry.word);
  CHECK(roundTripped.sourceUrl == entry.sourceUrl);
  CHECK(roundTripped.license == entry.license);
  REQUIRE(roundTripped.phonetics.size() == 1);
  CHECK(roundTripped.phonetics[0].audio == phonetic.audio);
  REQUIRE(roundTripped.meanings.size() == 1);
  REQUIRE(roundTripped.meanings[0].definitions.size() == 1);
  CHECK(roundTripped.meanings[0].definitions[0].antonyms == std::vector<std::string>{"dumb"});
}

TEST_CASE("dictionaryEntryFrom(WordInfo) builds phonetics from its own pronunciation data, "
          "with no audio - the same conversion fastFetch uses",
          "[dictionary_entry][fast_fetch]") {
  WordInfo freeDict;
  freeDict.word = "smart";
  Entry entry;
  entry.partOfSpeech = "adjective";
  dictionary_utils::Pronunciation us;
  us.text = "/smɑɹt/";
  us.tags = {"General American"};
  dictionary_utils::Pronunciation uk;
  uk.text = "/smɑːt/";
  uk.tags = {"Received Pronunciation"};
  entry.pronunciations = {us, uk};
  freeDict.entries.push_back(entry);

  auto result = dictionaryEntryFrom(freeDict);

  CHECK(result.word == "smart");
  REQUIRE(result.phonetics.size() == 2);
  CHECK(result.phonetics[0].label == "US");
  CHECK(result.phonetics[0].text == "/smɑɹt/");
  CHECK(result.phonetics[0].audio.empty());  // fastFetch never has real audio
  CHECK(result.phonetics[1].label == "UK");
}
