#include "wiktionary_parsing_detail.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using dictionary_utils::AudioPronunciation;
using dictionary_utils::hasUsAccent;
using dictionary_utils::IpaPronunciation;
using dictionary_utils::WiktionaryWordInfo;
namespace detail = dictionary_utils::detail;

namespace {

// Real shape, captured from
// action=parse&page=gift&prop=tocdata&format=json - a single-language page.
constexpr const char* kGiftTocFixture = R"({
  "parse": {
    "title": "gift",
    "tocdata": {
      "sections": [
        {"tocLevel": 1, "line": "English", "index": "1"},
        {"tocLevel": 2, "line": "Alternative forms", "index": "2"},
        {"tocLevel": 2, "line": "Etymology", "index": "3"},
        {"tocLevel": 2, "line": "Pronunciation", "index": "4"},
        {"tocLevel": 2, "line": "Noun", "index": "5"}
      ]
    }
  }
})";

// A two-language page (e.g. an English/German homograph) - each language
// has its own Pronunciation subsection, so findPronunciationSection must
// stop at the next tocLevel==1 boundary rather than picking up the wrong
// language's.
constexpr const char* kTwoLanguageTocFixture = R"({
  "parse": {
    "tocdata": {
      "sections": [
        {"tocLevel": 1, "line": "English", "index": "1"},
        {"tocLevel": 2, "line": "Pronunciation", "index": "2"},
        {"tocLevel": 2, "line": "Noun", "index": "3"},
        {"tocLevel": 1, "line": "German", "index": "4"},
        {"tocLevel": 2, "line": "Pronunciation", "index": "5"},
        {"tocLevel": 2, "line": "Noun", "index": "6"}
      ]
    }
  }
})";

// A language section with no Pronunciation subsection at all.
constexpr const char* kNoPronunciationTocFixture = R"({
  "parse": {
    "tocdata": {
      "sections": [
        {"tocLevel": 1, "line": "English", "index": "1"},
        {"tocLevel": 2, "line": "Noun", "index": "2"}
      ]
    }
  }
})";

// Real shape (trimmed), captured from action=parse&page=interrupt&
// prop=tocdata&format=json - a word whose two senses (verb, noun) stress
// differently, so Wiktionary splits pronunciation into two numbered
// headings instead of one shared "Pronunciation".
constexpr const char* kNumberedPronunciationTocFixture = R"({
  "parse": {
    "tocdata": {
      "sections": [
        {"tocLevel": 1, "line": "English", "index": "1"},
        {"tocLevel": 2, "line": "Etymology", "index": "3"},
        {"tocLevel": 2, "line": "Pronunciation 1", "index": "4"},
        {"tocLevel": 3, "line": "Verb", "index": "5"},
        {"tocLevel": 2, "line": "Pronunciation 2", "index": "11"},
        {"tocLevel": 3, "line": "Noun", "index": "12"},
        {"tocLevel": 2, "line": "Further reading", "index": "16"}
      ]
    }
  }
})";

}  // namespace

TEST_CASE("parses tocdata sections in order", "[wiktionary_utils][toc]") {
  auto sections = detail::parseTocSections(nlohmann::json::parse(kGiftTocFixture));

  REQUIRE(sections.size() == 5);
  CHECK(sections[0].tocLevel == 1);
  CHECK(sections[0].line == "English");
  CHECK(sections[3].line == "Pronunciation");
  CHECK(sections[3].index == "4");
}

TEST_CASE("finds the Pronunciation subsection under the requested language",
          "[wiktionary_utils][toc]") {
  auto sections = detail::parseTocSections(nlohmann::json::parse(kGiftTocFixture));
  auto lookup = detail::findPronunciationSection(sections, "English");

  CHECK(lookup.languageFound);
  CHECK(lookup.pronunciationFound);
  CHECK(lookup.sectionIndex == "4");
}

TEST_CASE("does not leak a different language's Pronunciation section",
          "[wiktionary_utils][toc]") {
  auto sections = detail::parseTocSections(nlohmann::json::parse(kTwoLanguageTocFixture));

  auto englishLookup = detail::findPronunciationSection(sections, "English");
  CHECK(englishLookup.sectionIndex == "2");

  auto germanLookup = detail::findPronunciationSection(sections, "German");
  CHECK(germanLookup.sectionIndex == "5");
}

TEST_CASE("a language not present on the page is not found, not an error",
          "[wiktionary_utils][toc]") {
  auto sections = detail::parseTocSections(nlohmann::json::parse(kGiftTocFixture));
  auto lookup = detail::findPronunciationSection(sections, "German");

  CHECK_FALSE(lookup.languageFound);
  CHECK_FALSE(lookup.pronunciationFound);
}

TEST_CASE("a language section with no Pronunciation subsection is found but has none",
          "[wiktionary_utils][toc]") {
  auto sections = detail::parseTocSections(nlohmann::json::parse(kNoPronunciationTocFixture));
  auto lookup = detail::findPronunciationSection(sections, "English");

  CHECK(lookup.languageFound);
  CHECK_FALSE(lookup.pronunciationFound);
}

TEST_CASE("finds every numbered 'Pronunciation N' heading, in page order, not just a plain "
          "'Pronunciation'",
          "[wiktionary_utils][toc]") {
  // Real shape from "interrupt": regression test for a real bug - a plain
  // string-equality check against "Pronunciation" found neither numbered
  // heading at all, so the word looked like it had no Pronunciation
  // section whatsoever, even though real IPA and US audio exist under
  // "Pronunciation 1".
  auto sections = detail::parseTocSections(nlohmann::json::parse(kNumberedPronunciationTocFixture));
  auto lookup = detail::findPronunciationSection(sections, "English");

  CHECK(lookup.languageFound);
  CHECK(lookup.pronunciationFound);
  CHECK(lookup.sectionIndex == "4");  // first one - kept for convenience/back-compat
  CHECK(lookup.sectionIndices == std::vector<std::string>({"4", "11"}));
}

TEST_CASE("selectPronunciationSection picks the first candidate that has both real IPA and US "
          "audio",
          "[wiktionary_utils][toc]") {
  // Real shape from "interrupt": Pronunciation 1 (verb) has both; the
  // caller should never even need to look at Pronunciation 2 (noun, IPA
  // only, no audio at all) - modeled here as a single-candidate list, since
  // that's what the caller passes once it stops fetching early.
  detail::ParsedPronunciationWikitext pronunciation1 = detail::parsePronunciationWikitext(
      "===Pronunciation 1===\n"
      "* {{IPA|en|/ˌɪntəˈɹʌpt/}}\n"
      "* {{audio|en|en-us-interrupt.ogg|a=US}}\n");

  REQUIRE(detail::hasIpaAndUsAudio(pronunciation1));

  auto selected = detail::selectPronunciationSection({pronunciation1});
  REQUIRE(selected.ipa.size() == 1);
  CHECK(selected.ipa[0].text == "/ˌɪntəˈɹʌpt/");
  REQUIRE(selected.audio.size() == 1);
  CHECK(selected.audio[0].filename == "en-us-interrupt.ogg");
}

TEST_CASE("selectPronunciationSection falls back to the first candidate when none have both",
          "[wiktionary_utils][toc]") {
  // Real shape from "interrupt"'s Pronunciation 2 (noun): IPA only, no
  // audio at all - hasIpaAndUsAudio() is false for every candidate here.
  detail::ParsedPronunciationWikitext pronunciation1 =
      detail::parsePronunciationWikitext("===Pronunciation 1===\n* {{IPA|en|/one/}}\n");
  detail::ParsedPronunciationWikitext pronunciation2 =
      detail::parsePronunciationWikitext("===Pronunciation 2===\n* {{IPA|en|/two/}}\n");

  REQUIRE_FALSE(detail::hasIpaAndUsAudio(pronunciation1));
  REQUIRE_FALSE(detail::hasIpaAndUsAudio(pronunciation2));

  auto selected = detail::selectPronunciationSection({pronunciation1, pronunciation2});

  // First has precedence - not the second, even though neither satisfies
  // hasIpaAndUsAudio().
  REQUIRE(selected.ipa.size() == 1);
  CHECK(selected.ipa[0].text == "/one/");
}

TEST_CASE("selectPronunciationSection skips a candidate missing IPA/audio for a later one that "
          "has both",
          "[wiktionary_utils][toc]") {
  detail::ParsedPronunciationWikitext noAudio =
      detail::parsePronunciationWikitext("===Pronunciation 1===\n* {{IPA|en|/one/|a=UK}}\n");
  detail::ParsedPronunciationWikitext both = detail::parsePronunciationWikitext(
      "===Pronunciation 2===\n"
      "* {{IPA|en|/two/}}\n"
      "* {{audio|en|en-us-two.ogg|a=US}}\n");

  REQUIRE_FALSE(detail::hasIpaAndUsAudio(noAudio));
  REQUIRE(detail::hasIpaAndUsAudio(both));

  auto selected = detail::selectPronunciationSection({noAudio, both});

  REQUIRE(selected.ipa.size() == 1);
  CHECK(selected.ipa[0].text == "/two/");
}

TEST_CASE("selectPronunciationSection returns a default-constructed result for an empty "
          "candidate list",
          "[wiktionary_utils][toc]") {
  auto selected = detail::selectPronunciationSection({});
  CHECK(selected.ipa.empty());
  CHECK(selected.audio.empty());
}

TEST_CASE("parses audio and IPA templates with a single accent tag",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from section=2 of "smart".
  auto parsed = detail::parsePronunciationWikitext(R"(
===Pronunciation===
* {{IPA|en|/smɑɹt/|a=GenAm}}
* {{IPA|en|/smɑːt/|a=RP}}
* {{audio|en|en-us-smart.ogg|a=US}}
* {{rhymes|en|ɑː(ɹ)t|s=1}}
)");

  REQUIRE(parsed.ipa.size() == 2);
  CHECK(parsed.ipa[0].text == "/smɑɹt/");
  CHECK(parsed.ipa[0].accents == std::vector<std::string>{"GenAm"});
  CHECK(parsed.ipa[1].accents == std::vector<std::string>{"RP"});

  REQUIRE(parsed.audio.size() == 1);
  CHECK(parsed.audio[0].filename == "en-us-smart.ogg");
  CHECK(parsed.audio[0].accents == std::vector<std::string>{"US"});
}

TEST_CASE("parses an audio template with no accent tag at all", "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from section=4 of "gift" - the second audio
  // entry has no a= param whatsoever.
  auto parsed = detail::parsePronunciationWikitext(R"(
===Pronunciation===
* {{enPR|gĭft|a=US,UK}}, {{IPA|en|/ɡɪft/}}
* {{audio|en|en-us-gift.ogg|a=US}}
* {{audio|en|LL-Q1860 (eng)-Back ache-gift.wav}}
* {{rhymes|en|ɪft|s=1}}
)");

  REQUIRE(parsed.audio.size() == 2);
  CHECK(parsed.audio[0].filename == "en-us-gift.ogg");
  CHECK(parsed.audio[0].accents == std::vector<std::string>{"US"});  // own a= param, not inherited
  CHECK(parsed.audio[1].filename == "LL-Q1860 (eng)-Back ache-gift.wav");
  CHECK(parsed.audio[1].accents.empty());  // no enPR on this line to inherit from

  // {{enPR|gĭft|a=US,UK}} isn't {{audio}} or {{IPA}} itself, but its accent
  // carries over to the unlabeled {{IPA|en|/ɡɪft/}} right after it on the
  // same line, since that IPA template has no a= param of its own.
  REQUIRE(parsed.ipa.size() == 1);
  CHECK(parsed.ipa[0].text == "/ɡɪft/");
  CHECK(parsed.ipa[0].accents == std::vector<std::string>({"US", "UK"}));
}

TEST_CASE("parses 'until', where accent lives on enPR across multiple lines, including a "
          "nested line with its own explicit accent",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from section=3 of "until" - the regression case
  // for the enPR/IPA accent-inheritance fix. Three top-level lines each
  // pair an {{enPR|...|a=X}} with an accent-less {{IPA|...}}; a nested
  // (`**`) line in between has its own explicit "a=nonstandard" and must
  // NOT inherit UK/CA from the enPR two lines up.
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{enPR|ŭn-tĭlʹ|ən-tĭlʹ|a=UK,CA}} {{IPA|en|/ʌnˈtɪl/|/ənˈtɪl/|/ʊnˈtɪl/|}}\n"
      "* {{enPR|ŭn-tĭlʹ|ən-tĭlʹ|a=US}} {{IPA|en|/ʌnˈtɪl/|/ənˈtɪl/}}\n"
      "** {{IPA|en|/ɪnˈtɪl/|a=nonstandard}}\n"
      "* {{enPR|ŭn-tĭlʹ|a=Appalachians,also}} {{IPA|en|/ˈʌntəl/}}\n"
      "* {{audio|en|En-us-until.ogg|a=US}}\n"
      "* {{rhymes|en|ɪl|s=2}}\n");

  REQUIRE(parsed.ipa.size() == 4);
  CHECK(parsed.ipa[0].text == "/ʌnˈtɪl/");
  CHECK(parsed.ipa[0].accents == std::vector<std::string>({"UK", "CA"}));
  CHECK(parsed.ipa[1].text == "/ʌnˈtɪl/");
  CHECK(parsed.ipa[1].accents == std::vector<std::string>{"US"});
  CHECK(parsed.ipa[2].text == "/ɪnˈtɪl/");
  CHECK(parsed.ipa[2].accents == std::vector<std::string>{"nonstandard"});  // own tag, not inherited
  CHECK(parsed.ipa[3].text == "/ˈʌntəl/");
  CHECK(parsed.ipa[3].accents == std::vector<std::string>({"Appalachians", "also"}));

  REQUIRE(parsed.audio.size() == 1);
  CHECK(parsed.audio[0].accents == std::vector<std::string>{"US"});
}

TEST_CASE("parses 'already': a standalone {{a|en|GA}} block tags every nested line beneath it",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from section=2 of "already". Unlike "until"'s
  // same-line enPR inheritance, Wiktionary's other common convention is a
  // standalone {{a|en|...}} template on its own line, tagging every line
  // nested more deeply (by leading '*' count) until a line at the same or
  // shallower depth appears. Regression test for a real bug: this template
  // wasn't recognized at all, so the two GA-block IPA lines below came out
  // unlabeled ("cot-caught"/"non-cot-caught" alone, neither a recognized
  // region) instead of carrying the inherited "GA".
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{IPA|en|/ɔːlˈɹɛdi/|a=RP}}\n"
      "* {{a|en|GA}}\n"
      "** {{IPA|en|/ɑlˈɹɛdi/|a=cot-caught}}\n"
      "** {{IPA|en|/ɔlˈɹɛdi/|a=non-cot-caught}}\n"
      "* {{audio|en|en-us-already.ogg|a=US}}\n"
      "* {{rhymes|en|ɛdi|s=3}}\n");

  REQUIRE(parsed.ipa.size() == 3);
  CHECK(parsed.ipa[0].text == "/ɔːlˈɹɛdi/");
  CHECK(parsed.ipa[0].accents == std::vector<std::string>{"RP"});
  CHECK(parsed.ipa[1].text == "/ɑlˈɹɛdi/");
  CHECK(parsed.ipa[1].accents == std::vector<std::string>({"GA", "cot-caught"}));
  CHECK(hasUsAccent(parsed.ipa[1].accents));
  CHECK(parsed.ipa[2].text == "/ɔlˈɹɛdi/");
  CHECK(parsed.ipa[2].accents == std::vector<std::string>({"GA", "non-cot-caught"}));
  CHECK(hasUsAccent(parsed.ipa[2].accents));

  // The block's scope ends at the next same-depth ('*') line - the audio
  // line comes after it and must not inherit "GA" (it doesn't need to;
  // it's already explicitly tagged "US", but the block must still be
  // closed correctly for later, unrelated lines).
  REQUIRE(parsed.audio.size() == 1);
  CHECK(parsed.audio[0].accents == std::vector<std::string>{"US"});
}

TEST_CASE("parses 'activity': a {{a|en|GA|CA}} block's scope ends at the next line at the same "
          "depth, even a plain audio line",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from section=2 of "activity". Checks that a
  // block doesn't leak past its own scope: the RP IPA and UK audio lines
  // before the block, and the AU line after the block closes, must stay
  // unaffected by the GA/CA in between.
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{IPA|en|/ækˈtɪv.ɪ.ti/|a=RP}}\n"
      "* {{audio|en|LL-Q1860 (eng)-Back ache-activity.wav|a=UK|q=male voice}}\n"
      "* {{a|en|GA|CA}}\n"
      "** {{IPA|en|/ækˈtɪv.ə.ti/|[ækˈtɪv.ə.ɾi]|a=weak vowel}}\n"
      "** {{IPA|en|/ækˈtɪv.ɪ.ti/|[ækˈtɪv.ɪ.ɾi]|a=non-weak vowel}}\n"
      "* {{audio|en|en-us-activity.ogg|a=US}}\n"
      "* {{IPA|en|/ækˈtɪv.ə.ti/|[ækˈtɪv.ə.ɾi]|a=AU}}\n"
      "* {{rhymes|en|ɪvɪti|s=4}}\n");

  REQUIRE(parsed.ipa.size() == 4);
  CHECK(parsed.ipa[0].accents == std::vector<std::string>{"RP"});
  CHECK(parsed.ipa[1].accents == std::vector<std::string>({"GA", "CA", "weak vowel"}));
  CHECK(hasUsAccent(parsed.ipa[1].accents));
  CHECK(parsed.ipa[2].accents == std::vector<std::string>({"GA", "CA", "non-weak vowel"}));
  CHECK(hasUsAccent(parsed.ipa[2].accents));
  // Block closed by the depth-1 audio line before this depth-1 IPA line -
  // must not still carry GA/CA.
  CHECK(parsed.ipa[3].accents == std::vector<std::string>{"AU"});
  CHECK_FALSE(hasUsAccent(parsed.ipa[3].accents));

  REQUIRE(parsed.audio.size() == 2);
  CHECK(parsed.audio[0].accents == std::vector<std::string>{"UK"});
  CHECK(parsed.audio[1].accents == std::vector<std::string>{"US"});
}

TEST_CASE("parses 'advantage': two levels of nested {{a|en|...}} blocks both stay active for "
          "their own descendants",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext (trimmed), captured from section=2 of "advantage" - the
  // worst real-world case found: an outer {{a|en|GA|...}} block wraps two
  // inner blocks ({{a|en|non-æ-tensing}} and {{a|en|æ-tensing}}), each
  // wrapping their own pair of IPA lines. All four inner IPA lines need to
  // inherit "GA" from the outer block while the two inner blocks stay
  // scoped to only their own two lines each.
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{IPA|en|/ədˈvɑːn.tɪd͡ʒ/|a=RP}}\n"
      "* {{a|en|GA}}\n"
      "** {{a|en|non-æ-tensing}}\n"
      "*** {{IPA|en|/ədˈvæn.tɪd͡ʒ/|a=non-nt-flapping}}\n"
      "*** {{IPA|en|/ədˈvæɾ̃.ɪd͡ʒ/|a=nt-flapping}}\n"
      "** {{a|en|æ-tensing}}\n"
      "*** {{IPA|en|/ədˈveə̯n.tɪd͡ʒ/|a=non-nt-flapping}}\n"
      "*** {{IPA|en|/ədˈveə̯ɾ̃.ɪd͡ʒ/|a=nt-flapping}}\n"
      "**** {{audio|en|En-us-advantage.ogg|a=US}}\n"
      "* {{IPA|en|/əɖˈʋɑːn.ʈeːdʒ/|a=Indic}}\n");

  REQUIRE(parsed.ipa.size() == 6);
  CHECK(parsed.ipa[0].accents == std::vector<std::string>{"RP"});
  CHECK(parsed.ipa[1].accents == std::vector<std::string>({"GA", "non-æ-tensing", "non-nt-flapping"}));
  CHECK(hasUsAccent(parsed.ipa[1].accents));
  CHECK(parsed.ipa[2].accents == std::vector<std::string>({"GA", "non-æ-tensing", "nt-flapping"}));
  CHECK(hasUsAccent(parsed.ipa[2].accents));
  CHECK(parsed.ipa[3].accents == std::vector<std::string>({"GA", "æ-tensing", "non-nt-flapping"}));
  CHECK(hasUsAccent(parsed.ipa[3].accents));
  CHECK(parsed.ipa[4].accents == std::vector<std::string>({"GA", "æ-tensing", "nt-flapping"}));
  CHECK(hasUsAccent(parsed.ipa[4].accents));
  // Both the outer GA block and the inner æ-tensing block close at this
  // depth-1 line - must not still carry either.
  CHECK(parsed.ipa[5].accents == std::vector<std::string>{"Indic"});
  CHECK_FALSE(hasUsAccent(parsed.ipa[5].accents));

  REQUIRE(parsed.audio.size() == 1);
  CHECK(hasUsAccent(parsed.audio[0].accents));
}

TEST_CASE("parses 'dictator': a citation template nested inside an IPA template's own ref= "
          "parameter no longer swallows the whole line",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from section=2 of "dictator". The GA-tagged IPA
  // line embeds a citation template ({{R:en:Pyles:1972|432}}) inside its
  // own ref3= parameter. Regression test for a real bug: a non-nested-brace
  // regex could only ever match a "{{...}}" span with no further "{{"/"}}"
  // inside it, so this whole outer IPA template - including its perfectly
  // good "a=GA" - was invisible entirely, leaving only the RP line.
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{IPA|en|/dɪkˈteɪtə/|a=RP}}\n"
      "* {{IPA|en|/ˈdɪkˌteɪtəɹ/|[-ɾəɹ]|/ˌdɪkˈteɪtəɹ/|a=GA|a3=dated|ref3={{R:en:Pyles:1972|432}}}}\n"
      "* {{audio|en|En-us-dictator.ogg|a=GA}}\n"
      "* {{homophones|en|dictater}}\n"
      "* {{rhymes|en|eɪtə(ɹ)|s=3}}\n"
      "* {{hyphenation|en|dic|ta|tor}}\n");

  REQUIRE(parsed.ipa.size() == 2);
  CHECK(parsed.ipa[0].text == "/dɪkˈteɪtə/");
  CHECK(parsed.ipa[0].accents == std::vector<std::string>{"RP"});
  CHECK(parsed.ipa[1].text == "/ˈdɪkˌteɪtəɹ/");
  // The nested citation template's own content never leaks into the
  // OUTER template's name/text/accent extraction - only the mangled
  // "ref3=..." remainder (harmless, not read by anything) would show up if
  // it did, and it doesn't affect any of these checks either way.
  CHECK(hasUsAccent(parsed.ipa[1].accents));

  REQUIRE(parsed.audio.size() == 1);
  CHECK(parsed.audio[0].filename == "En-us-dictator.ogg");
  CHECK(hasUsAccent(parsed.audio[0].accents));
}

TEST_CASE("findTopLevelTemplates skips past nested templates instead of stopping at the first "
          "inner '}}'",
          "[wiktionary_utils][wikitext]") {
  // A template appearing entirely after a nested-template line must still
  // be found - proof the scanner correctly resumes after the nested one
  // closes, rather than getting confused about brace depth.
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{IPA|en|/test/|a=US|ref={{R:test|1}}}}\n"
      "* {{IPA|en|/second/|a=UK}}\n");

  REQUIRE(parsed.ipa.size() == 2);
  CHECK(parsed.ipa[0].text == "/test/");
  CHECK(hasUsAccent(parsed.ipa[0].accents));
  CHECK(parsed.ipa[1].text == "/second/");
  CHECK(parsed.ipa[1].accents == std::vector<std::string>{"UK"});
}

TEST_CASE("parses 'incredible': enPR/IPA on one line, three audio candidates each with their "
          "own accent tag",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from section=3 of "incredible". Unlike "until",
  // the enPR and IPA are on the SAME line here, and all three audio
  // templates carry their own a= param directly (no inheritance needed for
  // those) - this is the fixture behind the live investigation into why
  // "incredible" showed no US audio despite Wiktionary having one: the
  // labeling itself is fine (see checks below); the actual gap is in
  // WiktionaryAPIEntry::fetch()'s candidate loop only ever trying the
  // first hasUsAccent() match ("GA", not the later explicit "US" one) and
  // giving up for good if that one download fails - not exercised by this
  // pure parsing test, which has no network layer.
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{IPA|en|/ɪŋˈkɹɛdɪbəl/|a=UK}}\n"
      "* {{audio|en|LL-Q1860 (eng)-Vealhurl-incredible.wav|a=Southern England}}\n"
      "* {{enPR|ĭngkrĕ'dəbəl|a=US}}, {{IPA|en|/ɪŋˈkɹɛdəbəl/|[ɪ̈ŋˈkɹ̥ʷɛɾəbəɫ]|[ɪ̈ŋˈkɹ̥ʷɛɾəbɫ̩]}}\n"
      "* {{audio|en|LL-Q1860 (eng)-Naomi Persephone Amethyst (NaomiAmethyst)-incredible.wav|a=GA}}\n"
      "* {{audio|en|LL-Q1860 (eng)-Wodencafe-incredible.wav|a=US}}\n"
      "* {{rhymes|en|ɛdɪbəl|s=4}}\n");

  REQUIRE(parsed.ipa.size() == 2);
  CHECK(parsed.ipa[0].text == "/ɪŋˈkɹɛdɪbəl/");
  CHECK(parsed.ipa[0].accents == std::vector<std::string>{"UK"});
  CHECK(parsed.ipa[1].text == "/ɪŋˈkɹɛdəbəl/");  // only the first of 3 bundled transcriptions
  CHECK(parsed.ipa[1].accents == std::vector<std::string>{"US"});  // inherited from same-line enPR

  REQUIRE(parsed.audio.size() == 3);
  CHECK(parsed.audio[0].accents == std::vector<std::string>{"Southern England"});
  CHECK_FALSE(hasUsAccent(parsed.audio[0].accents));
  CHECK(parsed.audio[1].filename ==
        "LL-Q1860 (eng)-Naomi Persephone Amethyst (NaomiAmethyst)-incredible.wav");
  CHECK(parsed.audio[1].accents == std::vector<std::string>{"GA"});
  CHECK(hasUsAccent(parsed.audio[1].accents));  // the candidate actually tried today
  CHECK(parsed.audio[2].filename == "LL-Q1860 (eng)-Wodencafe-incredible.wav");
  CHECK(parsed.audio[2].accents == std::vector<std::string>{"US"});
  CHECK(hasUsAccent(parsed.audio[2].accents));  // never reached - loop breaks after audio[1]
}

TEST_CASE("parses a multi-word accent tag", "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from section=2 of "smarter".
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{audio|en|LL-Q1860 (eng)-Vealhurl-smarter.wav|a=Southern England}}");

  REQUIRE(parsed.audio.size() == 1);
  CHECK(parsed.audio[0].accents == std::vector<std::string>{"Southern England"});
}

TEST_CASE("splits a comma-separated multi-accent tag into separate entries",
          "[wiktionary_utils][wikitext]") {
  auto parsed =
      detail::parsePronunciationWikitext("* {{audio|en|word.ogg|a=US,UK}}");

  REQUIRE(parsed.audio.size() == 1);
  CHECK(parsed.audio[0].accents == std::vector<std::string>({"US", "UK"}));
}

TEST_CASE("resolves a single audio file's metadata from an imageinfo response",
          "[wiktionary_utils][imageinfo]") {
  // Real shape - note "missing" can be present even though imageinfo is
  // still populated, for files that live on Commons rather than locally.
  // Only ever queried for one title now (the primary candidate), so no
  // filename-matching is needed - just take whatever's resolved.
  auto response = nlohmann::json::parse(R"({
    "query": {
      "pages": {
        "-1": {
          "title": "File:en-us-hello.ogg",
          "missing": "",
          "imageinfo": [
            {
              "url": "https://upload.wikimedia.org/wikipedia/commons/5/52/En-us-hello.ogg",
              "mime": "application/ogg",
              "size": 9036,
              "duration": 0.4876190476190476
            }
          ]
        }
      }
    }
  })");

  auto resolved = detail::resolveAudioMetadata(response);

  REQUIRE(resolved.has_value());
  CHECK(resolved->url == "https://upload.wikimedia.org/wikipedia/commons/5/52/En-us-hello.ogg");
  CHECK(resolved->mimeType == "application/ogg");
  CHECK(resolved->sizeBytes == 9036);
  CHECK(resolved->durationSeconds > 0.48);
}

TEST_CASE("resolveAudioMetadata returns nullopt when nothing resolved, not an error",
          "[wiktionary_utils][imageinfo]") {
  auto response = nlohmann::json::parse(R"({"query": {"pages": {}}})");
  CHECK_FALSE(detail::resolveAudioMetadata(response).has_value());
}

TEST_CASE("languageSectionForCode maps 'en' to 'English'", "[wiktionary_utils][language]") {
  CHECK(detail::languageSectionForCode("en") == "English");
}

TEST_CASE("shouldDownloadAudio covers all four combinations", "[wiktionary_utils][cache]") {
  CHECK(detail::shouldDownloadAudio(false, false));  // not cached yet -> download
  CHECK(detail::shouldDownloadAudio(false, true));   // not cached, forced anyway -> download
  CHECK(detail::shouldDownloadAudio(true, true));    // cached but ignoreCache -> re-download
  CHECK_FALSE(detail::shouldDownloadAudio(true, false));  // cached, no override -> skip
}

TEST_CASE("wiktionaryWordInfoToJson/wiktionaryWordInfoFromJson round-trip losslessly",
          "[wiktionary_utils][cache]") {
  WiktionaryWordInfo info;
  info.word = "smart";
  info.hasPronunciationSection = true;

  AudioPronunciation audio;
  audio.filename = "en-us-smart.ogg";
  audio.audio = "dictionaries/wiktionaryapi/en-us-smart.ogg";
  audio.mimeType = "application/ogg";
  audio.durationSeconds = 0.5;
  audio.sizeBytes = 1234;
  audio.accents = {"US"};
  info.audio.push_back(audio);

  IpaPronunciation ipa;
  ipa.text = "/smɑɹt/";
  ipa.accents = {"GenAm"};
  info.ipa.push_back(ipa);

  auto roundTripped = detail::wiktionaryWordInfoFromJson(detail::wiktionaryWordInfoToJson(info));

  CHECK(roundTripped.word == info.word);
  CHECK(roundTripped.hasPronunciationSection == info.hasPronunciationSection);
  REQUIRE(roundTripped.audio.size() == 1);
  CHECK(roundTripped.audio[0].audio == "dictionaries/wiktionaryapi/en-us-smart.ogg");
  CHECK(roundTripped.audio[0].sizeBytes == 1234);
  REQUIRE(roundTripped.ipa.size() == 1);
  CHECK(roundTripped.ipa[0].text == "/smɑɹt/");
}

TEST_CASE("isUsAccentTag recognizes every spelling seen in practice",
          "[wiktionary_utils][us_accent]") {
  CHECK(dictionary_utils::isUsAccentTag("US"));
  CHECK(dictionary_utils::isUsAccentTag("us"));
  CHECK(dictionary_utils::isUsAccentTag("GenAm"));
  CHECK(dictionary_utils::isUsAccentTag("General American"));
  // "GA" - seen in practice on "endurance"'s {{IPA|...|a=GA}}, distinct from
  // {{audio|...|a=US}} on the very same word's Pronunciation section - two
  // independently-tagged templates using two different real abbreviations
  // for the same accent.
  CHECK(dictionary_utils::isUsAccentTag("GA"));
  CHECK(dictionary_utils::isUsAccentTag("ga"));
  CHECK_FALSE(dictionary_utils::isUsAccentTag("RP"));
  CHECK_FALSE(dictionary_utils::isUsAccentTag("Southern England"));
}

TEST_CASE("isUsAccentTag recognizes compound '<region> US' dialect labels",
          "[wiktionary_utils][us_accent]") {
  // Real tags seen on "online"'s Pronunciation section - a specific American
  // sub-dialect rather than a plain "US"/"GenAm"/"GA". Regression test for a
  // real bug: these were previously unrecognized entirely.
  CHECK(dictionary_utils::isUsAccentTag("Midland US"));
  CHECK(dictionary_utils::isUsAccentTag("Southern US"));
  CHECK(dictionary_utils::isUsAccentTag("Northern US"));
  // Case-insensitive, same as every other check here.
  CHECK(dictionary_utils::isUsAccentTag("midland us"));
  // Must match "us" as a whole word, not just a substring - "Aus" isn't US.
  CHECK_FALSE(dictionary_utils::isUsAccentTag("Aus"));
  CHECK_FALSE(dictionary_utils::isUsAccentTag("Australia"));
}

TEST_CASE("parses 'online': compound '<region> US' accent tags on plain (non-block) IPA lines "
          "are recognized as American",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext (trimmed), captured from section=2 of "online". These are
  // ordinary inline a= tags (no {{a|en|...}} block involved) - the gap was
  // purely in isUsAccentTag() not recognizing the compound region names.
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "*** {{IPA|en|/ˈɑnlaɪn/|a=<<Northern US>> or with the <<cot-caught>>}}\n"
      "*** {{IPA|en|/ˈɔnlaɪn/|a=Midland US,Southern US,non-cot-caught}}\n"
      "*** {{IPA|en|[ˈɒnlaɪn]|a=CA}}\n");

  REQUIRE(parsed.ipa.size() == 3);
  // The messy, non-comma-separated tag is a known, separate limitation -
  // not expected to be recognized as US.
  CHECK_FALSE(hasUsAccent(parsed.ipa[0].accents));
  // Comma-separated, and each individual tag is clean - both "Midland US"
  // and "Southern US" are real, individually-recognizable American labels.
  CHECK(parsed.ipa[1].accents ==
        std::vector<std::string>({"Midland US", "Southern US", "non-cot-caught"}));
  CHECK(hasUsAccent(parsed.ipa[1].accents));
  CHECK_FALSE(hasUsAccent(parsed.ipa[2].accents));  // "CA" here means Canada, not recognized as US
}

TEST_CASE("isUsAccentTag recognizes the 'North American' regional label",
          "[wiktionary_utils][us_accent]") {
  // Real tag seen on "blue"'s Pronunciation section
  // ({{IPA|en|/blu/|a=North American}}), alongside a separately-tagged
  // {{audio|en|en-us-blue.ogg|a=US}} line for the same pronunciation.
  // Regression test for a real bug: "North American" wasn't recognized as a
  // synonym for US at all.
  CHECK(dictionary_utils::isUsAccentTag("North American"));
  CHECK(dictionary_utils::isUsAccentTag("north american"));
}

TEST_CASE("parses 'police': a {{a|...}} block indented with ':' (not '*') still scopes over its "
          "nested lines",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext shape, captured from "police"'s Pronunciation section -
  // nested via "::" (MediaWiki's definition-list indent) rather than "**".
  // Regression test for a real bug: bulletDepth() only counted '*', so the
  // {{a|en|GA|CA}} block's own depth (1, from its leading "*") was already
  // >= the "::"-indented children's depth (0, since they have no leading
  // '*' at all) - the block popped immediately and never applied at all.
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{a|en|RP|AU|Scotland}}\n"
      ":: {{IPA|en|/pəˈliːs/}}\n"
      ":: {{audio|en|En-uk-police.ogg|a=UK}}\n"
      "* {{a|en|GA|CA}}\n"
      ":: {{IPA|en|/pəˈlis/}}\n"
      ":: {{audio|en|en-us-police.ogg|a=US}}\n");

  REQUIRE(parsed.ipa.size() == 2);
  CHECK_FALSE(hasUsAccent(parsed.ipa[0].accents));
  CHECK(parsed.ipa[1].text == "/pəˈlis/");
  CHECK(parsed.ipa[1].accents == std::vector<std::string>({"GA", "CA"}));
  CHECK(hasUsAccent(parsed.ipa[1].accents));

  REQUIRE(parsed.audio.size() == 2);
  CHECK(parsed.audio[1].accents == std::vector<std::string>({"GA", "CA", "US"}));
}

TEST_CASE("parses 'them': a {{a|...}} block applies to templates on its own line, not just "
          "more deeply-nested ones",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from "them"'s Pronunciation section. Regression
  // test for a real gap: inheritedAccents was computed once per line,
  // before that line's own {{a|en|stressed}} was processed, so the IPA
  // template sharing that same line never picked it up - even though a
  // more deeply-nested line right below it correctly did (that part
  // already worked, see the "already"/"advantage" tests above).
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{a|en|stressed}} {{IPA|en|/ˈðɛm/}}\n"
      "** {{audio|en|En-us-them.ogg|a=US}}\n"
      "* {{IPA|en|/ðəm/}}\n");

  REQUIRE(parsed.ipa.size() == 2);
  CHECK(parsed.ipa[0].text == "/ˈðɛm/");
  CHECK(parsed.ipa[0].accents == std::vector<std::string>{"stressed"});
  CHECK(parsed.ipa[1].accents.empty());  // no {{a|...}} active on this later line

  REQUIRE(parsed.audio.size() == 1);
  CHECK(parsed.audio[0].accents == std::vector<std::string>({"stressed", "US"}));
}

TEST_CASE("parses 'l': {{q|...}} and {{sense|...}} qualifiers distinguish the letter-name "
          "reading from the raw phoneme",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext shapes, captured from "l" ({{q|...}}) and "c"/"e"
  // ({{sense|...}}) - two different template names for the same kind of
  // register/sense qualifier, carried the same way enPR's own accent
  // already is (see the "until" test above).
  auto viaQ = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{q|name of letter}} {{IPA|en|/ɛl/}}\n"
      "* {{q|phoneme}} {{IPA|en|/l/}}\n"
      "* {{audio|en|En-us-L.ogg|a=US}}\n");

  REQUIRE(viaQ.ipa.size() == 2);
  CHECK(viaQ.ipa[0].accents == std::vector<std::string>{"name of letter"});
  CHECK(viaQ.ipa[1].accents == std::vector<std::string>{"phoneme"});

  auto viaSense = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{sense|letter name}} {{IPA|en|/siː/}}\n"
      "* {{sense|phoneme}} {{IPA|en|/k/|/s/|/tʃ/}}\n");

  REQUIRE(viaSense.ipa.size() == 2);
  CHECK(viaSense.ipa[0].accents == std::vector<std::string>{"letter name"});
  CHECK(viaSense.ipa[1].accents == std::vector<std::string>{"phoneme"});
}

TEST_CASE("parses 'blue': the 'North American' accent label is recognized as American",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from "blue"'s Pronunciation section.
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{IPA|en|/bluː/|a=RP}}\n"
      "* {{IPA|en|/blu/|a=North American}}\n"
      "** {{audio|en|en-us-blue.ogg|a=US}}\n");

  REQUIRE(parsed.ipa.size() == 2);
  CHECK_FALSE(hasUsAccent(parsed.ipa[0].accents));
  CHECK(parsed.ipa[1].text == "/blu/");
  CHECK(hasUsAccent(parsed.ipa[1].accents));
}

TEST_CASE("parses 'door' and 'lost': the 'aa=' accent parameter is recognized, not just 'a='",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from "door" and "lost"'s Pronunciation sections.
  // Both tag certain IPA lines' accent via "aa=" (appears to mean "all
  // accents", set directly on the {{IPA|...}} template) rather than the
  // "a=" parameter already handled elsewhere - regression test for a real
  // bug: "aa=" was never read at all, so these lines always came back with
  // an empty accents list regardless of what the source actually said.
  auto door = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{IPA|en|/dɔː/|aa=RP}}\n"
      "* {{IPA|en|/doɹ/|aa=GenAm,Canada}}\n"
      "** {{audio|en|en-us-door.ogg|a=US}}\n");

  REQUIRE(door.ipa.size() == 2);
  CHECK_FALSE(hasUsAccent(door.ipa[0].accents));
  CHECK(door.ipa[1].text == "/doɹ/");
  CHECK(door.ipa[1].accents == std::vector<std::string>({"GenAm", "Canada"}));
  CHECK(hasUsAccent(door.ipa[1].accents));

  auto lost = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{IPA|en|/lɔːst/|aa=UK,dated}}\n"
      "* {{IPA|en|/lɔst/|aa=US,non-cot-caught}}\n");

  REQUIRE(lost.ipa.size() == 2);
  CHECK_FALSE(hasUsAccent(lost.ipa[0].accents));
  CHECK(lost.ipa[1].text == "/lɔst/");
  CHECK(hasUsAccent(lost.ipa[1].accents));
}

TEST_CASE("parses 'technology': a named 'a=' parameter appearing before the positional IPA text "
          "no longer gets captured as the transcription itself",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from "technology"'s Pronunciation section.
  // Regression test for a real bug: MediaWiki named parameters ("a=India")
  // don't consume a positional slot, but the old code always took the raw
  // 3rd pipe-separated field as "the text" regardless of what came before
  // it - so this line's real IPA was replaced by the literal string
  // "a=India".
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{IPA|en|/tɛkˈnɒl.ə.dʒi/|a=RP}}\n"
      "* {{IPA|en|/tɛkˈnɑ.lə.dʒi/|a=GenAm}}\n"
      "* {{IPA|en|a=India|/ˈʈek(h).noˈlɔː.dʒiː/|/ʈek(h)ˈnɔː.lə.dʒiː/}}\n");

  REQUIRE(parsed.ipa.size() == 3);
  CHECK(parsed.ipa[0].text == "/tɛkˈnɒl.ə.dʒi/");
  CHECK(parsed.ipa[1].text == "/tɛkˈnɑ.lə.dʒi/");
  CHECK(hasUsAccent(parsed.ipa[1].accents));
  CHECK(parsed.ipa[2].text == "/ˈʈek(h).noˈlɔː.dʒiː/");  // not the literal "a=India"
  CHECK(parsed.ipa[2].accents == std::vector<std::string>{"India"});
}

TEST_CASE("parses an audio template whose filename is preceded by nothing named, unaffected by "
          "the positional-parameter fix",
          "[wiktionary_utils][wikitext]") {
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{audio|en|en-us-door.ogg|a=US}}\n");

  REQUIRE(parsed.audio.size() == 1);
  CHECK(parsed.audio[0].filename == "en-us-door.ogg");
  CHECK(hasUsAccent(parsed.audio[0].accents));
}

TEST_CASE("parses 'mommy': an IPA transcription embedded inside an {{audio|...}} template's own "
          "'IPA=' parameter, accented via a nested {{a|en|...}} value",
          "[wiktionary_utils][wikitext]") {
  // Real wikitext, captured from "mommy"'s Pronunciation section. The real
  // American IPA doesn't live in a separate {{IPA|...}} line at all - it's
  // a named "IPA=" parameter directly on the {{audio|...}} template, with
  // its accent given by a *nested* {{a|en|GA}} template as another
  // parameter's own value. Regression test for a real gap: this shape was
  // never recognized at all, so "mommy" always came back with no US IPA.
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{audio|en|En-us-mommy.ogg|IPA=/ˈmɑ.mi/|3={{a|en|GA}}}}\n"
      "* {{IPA|en|/ˈmɒm.i/|a=RP}}\n"
      "* {{IPA|en|a=Indic|/ˈmɔ.mi/}}\n");

  REQUIRE(parsed.audio.size() == 1);
  CHECK(parsed.audio[0].filename == "En-us-mommy.ogg");

  REQUIRE(parsed.ipa.size() == 3);
  CHECK(parsed.ipa[0].text == "/ˈmɑ.mi/");  // the embedded one, extracted first
  CHECK(hasUsAccent(parsed.ipa[0].accents));
  CHECK(parsed.ipa[1].text == "/ˈmɒm.i/");
  CHECK_FALSE(hasUsAccent(parsed.ipa[1].accents));
  // The already-fixed "a=X before positional text" bug: real text, not the
  // literal "a=Indic".
  CHECK(parsed.ipa[2].text == "/ˈmɔ.mi/");
  CHECK(parsed.ipa[2].accents == std::vector<std::string>{"Indic"});
}

TEST_CASE("an {{audio|...}} template with no 'IPA=' parameter contributes no extra IPA entry",
          "[wiktionary_utils][wikitext]") {
  auto parsed = detail::parsePronunciationWikitext(
      "===Pronunciation===\n"
      "* {{audio|en|en-us-door.ogg|a=US}}\n");

  CHECK(parsed.ipa.empty());
  REQUIRE(parsed.audio.size() == 1);
  CHECK(parsed.audio[0].filename == "en-us-door.ogg");
}

TEST_CASE("selectPrimaryUsAudio prefers a US audio candidate that shares a qualifier tag with an "
          "existing IPA line over the first US candidate found",
          "[wiktionary_utils][us_accent]") {
  // Real shape from "dog": two US-tagged audio candidates exist - a plain
  // "US" one (first in page order) and a "US, cot-caught" one - and only
  // the cot-caught-tagged IPA line has a matching qualifier. Regression
  // test for a real gap: always taking the first US-tagged candidate meant
  // the downstream qualifier-subset rule (buildPhonetics) never had a
  // matching tag to work with, even though a better candidate existed.
  IpaPronunciation rp;
  rp.text = "/dɒɡ/";
  rp.accents = {"RP"};
  IpaPronunciation nonCotCaught;
  nonCotCaught.text = "/dɔɡ/";
  nonCotCaught.accents = {"non-cot-caught"};
  IpaPronunciation cotCaught;
  cotCaught.text = "/dɑɡ/";
  cotCaught.accents = {"cot-caught"};
  std::vector<IpaPronunciation> ipa = {rp, nonCotCaught, cotCaught};

  AudioPronunciation plainUs;
  plainUs.filename = "En-us-ne-dog.ogg";
  plainUs.accents = {"US"};
  AudioPronunciation cotCaughtUs;
  cotCaughtUs.filename = "en-us-dog.ogg";
  cotCaughtUs.accents = {"US", "cot-caught"};
  std::vector<AudioPronunciation> audio = {plainUs, cotCaughtUs};

  auto* primary = detail::selectPrimaryUsAudio(audio, ipa);

  REQUIRE(primary != nullptr);
  CHECK(primary->filename == "en-us-dog.ogg");
}

TEST_CASE("selectPrimaryUsAudio falls back to the first US-tagged candidate when none share a "
          "qualifier with any IPA line",
          "[wiktionary_utils][us_accent]") {
  IpaPronunciation rp;
  rp.text = "/smɑːt/";
  rp.accents = {"RP"};

  AudioPronunciation firstUs;
  firstUs.filename = "en-us-smart-a.ogg";
  firstUs.accents = {"US"};
  AudioPronunciation secondUs;
  secondUs.filename = "en-us-smart-b.ogg";
  secondUs.accents = {"US"};
  std::vector<AudioPronunciation> audio = {firstUs, secondUs};

  auto* primary = detail::selectPrimaryUsAudio(audio, {rp});

  REQUIRE(primary != nullptr);
  CHECK(primary->filename == "en-us-smart-a.ogg");
}

TEST_CASE("selectPrimaryUsAudio returns nullptr when there's no US-tagged candidate at all",
          "[wiktionary_utils][us_accent]") {
  AudioPronunciation uk;
  uk.filename = "en-uk-smart.ogg";
  uk.accents = {"UK"};
  std::vector<AudioPronunciation> audio = {uk};

  CHECK(detail::selectPrimaryUsAudio(audio, {}) == nullptr);
}

TEST_CASE("WiktionaryWordInfo::usAudio and usIpa find the US-tagged entry among several",
          "[wiktionary_utils][us_accent]") {
  WiktionaryWordInfo info;

  AudioPronunciation ukAudio;
  ukAudio.filename = "en-uk-smart.ogg";
  ukAudio.accents = {"RP"};
  AudioPronunciation usAudio;
  usAudio.filename = "en-us-smart.ogg";
  usAudio.accents = {"US"};
  info.audio = {ukAudio, usAudio};

  IpaPronunciation ukIpa;
  ukIpa.text = "/smɑːt/";
  ukIpa.accents = {"RP"};
  IpaPronunciation usIpa;
  usIpa.text = "/smɑɹt/";
  usIpa.accents = {"GenAm"};
  info.ipa = {ukIpa, usIpa};

  REQUIRE(info.usAudio() != nullptr);
  CHECK(info.usAudio()->filename == "en-us-smart.ogg");
  REQUIRE(info.usIpa() != nullptr);
  CHECK(info.usIpa()->text == "/smɑɹt/");
}

TEST_CASE("usAudio returns nullptr when nothing is tagged US", "[wiktionary_utils][us_accent]") {
  WiktionaryWordInfo info;
  AudioPronunciation ukAudio;
  ukAudio.filename = "en-uk-word.ogg";
  ukAudio.accents = {"RP"};
  info.audio = {ukAudio};

  CHECK(info.usAudio() == nullptr);
}

TEST_CASE("isUsAudioCandidate falls back to the en-us- filename convention when the accent tag "
          "isn't recognized as American at all",
          "[wiktionary_utils][us_accent]") {
  // Same shape as "talent"/"totally" (Wiktionary's own recording named
  // "en-us-..." but tagged with a specific dialect rather than a generic US
  // accent), but using a tag isUsAccentTag() genuinely doesn't recognize -
  // not "US"/"GenAm"/"GA", not a compound "<region> US" label, and not a US
  // state name either (unlike "California", which is a real state and is
  // now recognized directly - see the state-name tests below).
  AudioPronunciation midAtlanticAudio;
  midAtlanticAudio.filename = "en-us-talent.ogg";
  midAtlanticAudio.accents = {"Mid-Atlantic"};

  CHECK_FALSE(dictionary_utils::hasUsAccent(midAtlanticAudio.accents));
  CHECK(dictionary_utils::isUsAudioCandidate(midAtlanticAudio));
}

TEST_CASE("isUsAudioCandidate's filename fallback is case-insensitive and requires the en-us- "
          "prefix specifically",
          "[wiktionary_utils][us_accent]") {
  AudioPronunciation upperCase;
  upperCase.filename = "En-US-Talent.ogg";
  upperCase.accents = {"Mid-Atlantic"};
  CHECK(dictionary_utils::isUsAudioCandidate(upperCase));

  AudioPronunciation notUsPrefixed;
  notUsPrefixed.filename = "LL-Q1860 (eng)-Some Person-talent.wav";
  notUsPrefixed.accents = {"Yorkshire"};
  CHECK_FALSE(dictionary_utils::isUsAudioCandidate(notUsPrefixed));
}

TEST_CASE("WiktionaryWordInfo::usAudio finds a California-tagged en-us- recording via the "
          "filename fallback",
          "[wiktionary_utils][us_accent]") {
  WiktionaryWordInfo info;
  AudioPronunciation ukAudio;
  ukAudio.filename = "en-uk-talent.ogg";
  ukAudio.accents = {"UK"};
  AudioPronunciation californiaAudio;
  californiaAudio.filename = "en-us-talent.ogg";
  californiaAudio.accents = {"California"};
  info.audio = {ukAudio, californiaAudio};

  REQUIRE(info.usAudio() != nullptr);
  CHECK(info.usAudio()->filename == "en-us-talent.ogg");
}

TEST_CASE("isUsAccentTag recognizes US state names, with or without a filename to fall back on",
          "[wiktionary_utils][us_accent]") {
  // Real tags seen on "vulnerability", "respect", and "similarity" - all
  // Lingua Libre recordings ("LL-Q1860 (eng)-<contributor>-<word>.wav"),
  // never Wiktionary's own "en-us-..." convention, so the state name in the
  // accent tag is the only signal available at all. Regression test for a
  // real bug: these were previously unrecognized entirely, unlike
  // "California" (already handled) - every US state should be, not just
  // that one.
  CHECK(dictionary_utils::isUsAccentTag("Colorado"));
  CHECK(dictionary_utils::isUsAccentTag("New Mexico"));
  CHECK(dictionary_utils::isUsAccentTag("Texas"));
  // Case-insensitive, same as every other check here.
  CHECK(dictionary_utils::isUsAccentTag("colorado"));
  // A compass-qualified state name - "Western Pennsylvania" specifically.
  CHECK(dictionary_utils::isUsAccentTag("Western Pennsylvania"));

  // Not a state, and not confusable with one.
  CHECK_FALSE(dictionary_utils::isUsAccentTag("Yorkshire"));
  CHECK_FALSE(dictionary_utils::isUsAccentTag("Ontario"));
}

TEST_CASE("WiktionaryWordInfo::usAudio finds a US-state-tagged Lingua Libre recording with no "
          "en-us- filename at all",
          "[wiktionary_utils][us_accent]") {
  // Real shape from "vulnerability": no en-us- filename exists anywhere on
  // the page, so this word's US audio is only findable via the state name
  // in its accent tag.
  WiktionaryWordInfo info;
  AudioPronunciation coloradoAudio;
  coloradoAudio.filename = "LL-Q1860 (eng)-Vininn126-vulnerability.wav";
  coloradoAudio.accents = {"Colorado"};
  info.audio = {coloradoAudio};

  REQUIRE(info.usAudio() != nullptr);
  CHECK(info.usAudio()->filename == "LL-Q1860 (eng)-Vininn126-vulnerability.wav");
}
