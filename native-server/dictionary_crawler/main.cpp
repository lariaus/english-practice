// Standalone batch-fetch utility: walks a word-list file and populates one
// or more sources' own persistent on-disk cache under @output_dir - the
// same cache dictionary_utils's HTTP route consults, so a later real lookup
// for any of these words is a pure disk read with zero API calls.
//
// Deliberately calls each specific source class's own fetch() directly
// (FreeDictionaryAPIEntry or WiktionaryAPIEntry), never the generic
// combined DictionaryEntry::fetch() - so a run only ever exercises exactly
// the API(s) it was asked to scrape.
//
// usage: dictionary-crawler <path-to-word-list.txt> [--retry-error] [--retry-missing]
//                            [--retry-only <path>]
//
// The word-list file holds everything else: comments, parameters, and the
// words themselves, one concern per line -
//   # a comment - skipped
//   @output_dir <path>          - required; resolved relative to this
//                                  file's own directory, not the cwd
//   @api <name> [<name> ...]    - optional, space-separated; defaults to
//                                  every known source if omitted
//   @interval_time <seconds>    - optional; defaults to 2
//   <word>                      - anything else, one per line
//
// Progress and results persist next to the word-list file, in
// "<name>-status.json" (e.g. "words.txt" -> "words-status.json"): a
// {word: {api: status}} map, where status is "unknown" (not attempted),
// "ok", "error" (not found or a real fetch failure - both equally worth
// retrying later), or "|"-joined qualifiers ("no-ipa"/"no-audio" - this
// source structurally never provides that data type, static, never
// retried; "missing-ipa"/"missing-audio" - this word didn't yield a
// US-recognized one this time, worth retrying). This file is what makes
// the whole run resumable: it's reconciled against the current word list
// (new words/APIs seeded as "unknown", nothing pruned) and rewritten to
// disk after every single (word, API) attempt, so the process can be
// killed and restarted at any point - expected to matter a lot, since
// crawling thousands of words for real is a multi-hour-or-day affair.
//
// A word is "complete" as soon as any one of its configured APIs reaches
// "ok" - the rest are never attempted (this run or a future one).
// --retry-error re-attempts "error" entries (not found, or a real fetch
// failure); --retry-missing re-attempts "missing-ipa"/"missing-audio"
// entries (found, but no US-recognized IPA/audio yet). Neither ever
// retries "no-ipa"/"no-audio", which can't change. Without either flag,
// only "unknown" entries are attempted, for words not yet complete.
//
// --retry-only <path> takes precedence over both flags above: given a
// plain word list (one word per line, blank lines/"#" comments skipped,
// no @-parameters) it unconditionally re-attempts every configured API for
// exactly those words - regardless of their current status, even "ok" -
// and every other word in the main list is skipped entirely. For
// surgically retrying a small, specific set of words (e.g. ones you know
// hit a transient rate-limit/network error, identified from console
// output rather than the status file, which doesn't distinguish *why* a
// word errored) without disturbing anything else. Any word listed that
// isn't in the main word list is warned about and skipped.

#include "dictionary_utils/dictionary_entry.h"
#include "dictionary_utils/free_dictionary_api_entry.h"
#include "dictionary_utils/wiktionary_api_entry.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using nlohmann::ordered_json;

constexpr const char* kUsage =
    "usage: dictionary-crawler <path-to-word-list.txt> [--retry-error] [--retry-missing]\n"
    "                          [--retry-only <path>]\n";

enum class ApiSource { FreeDictionary, WikiDictionary };

std::optional<ApiSource> parseApiName(const std::string& name) {
  if (name == "freedictionaryapi") return ApiSource::FreeDictionary;
  if (name == "wikidictionary") return ApiSource::WikiDictionary;
  return std::nullopt;
}

std::string trim(const std::string& s) {
  size_t b = s.find_first_not_of(" \t\n\r");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\n\r");
  return s.substr(b, e - b + 1);
}

std::vector<std::string> splitWhitespace(const std::string& s) {
  std::vector<std::string> parts;
  std::istringstream iss(s);
  std::string tok;
  while (iss >> tok) parts.push_back(tok);
  return parts;
}

struct WordListConfig {
  std::filesystem::path outputDir;
  std::vector<std::string> apiNames;  // validated, in @api's own order
  int intervalSeconds = 2;
  std::vector<std::string> rawWords;  // as typed, in file order, not yet normalized/deduped
};

// Throws std::runtime_error (caught by main, printed alongside kUsage) on
// any parse/validation problem - an unknown @parameter, a bad value, an
// unknown api-name, or @output_dir missing entirely.
WordListConfig parseWordListFile(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) throw std::runtime_error("could not open word list file: " + path.string());

  std::optional<std::string> outputDirRaw;
  std::optional<std::vector<std::string>> apiNamesRaw;
  std::optional<int> intervalSeconds;
  std::vector<std::string> words;

  std::string line;
  size_t lineNumber = 0;
  while (std::getline(file, line)) {
    ++lineNumber;
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') continue;

    if (trimmed[0] == '@') {
      std::vector<std::string> tokens = splitWhitespace(trimmed.substr(1));
      if (tokens.empty()) {
        throw std::runtime_error("line " + std::to_string(lineNumber) + ": empty \"@\" parameter");
      }
      const std::string& name = tokens[0];
      std::vector<std::string> values(tokens.begin() + 1, tokens.end());

      if (name == "output_dir") {
        if (values.size() != 1) {
          throw std::runtime_error("line " + std::to_string(lineNumber) +
                                    ": @output_dir expects exactly one value");
        }
        outputDirRaw = values[0];
      } else if (name == "api") {
        if (values.empty()) {
          throw std::runtime_error("line " + std::to_string(lineNumber) +
                                    ": @api expects at least one value");
        }
        apiNamesRaw = values;
      } else if (name == "interval_time") {
        if (values.size() != 1) {
          throw std::runtime_error("line " + std::to_string(lineNumber) +
                                    ": @interval_time expects exactly one value");
        }
        try {
          intervalSeconds = std::stoi(values[0]);
        } catch (const std::exception&) {
          throw std::runtime_error("line " + std::to_string(lineNumber) +
                                    ": @interval_time expects an integer number of seconds, got \"" +
                                    values[0] + "\"");
        }
      } else {
        throw std::runtime_error("line " + std::to_string(lineNumber) + ": unknown parameter \"@" +
                                  name + "\"");
      }
      continue;
    }

    words.push_back(trimmed);
  }

  if (!outputDirRaw) {
    throw std::runtime_error("missing required parameter: @output_dir");
  }

  WordListConfig config;
  config.outputDir = path.parent_path() / *outputDirRaw;

  if (apiNamesRaw) {
    for (const auto& name : *apiNamesRaw) {
      if (!parseApiName(name)) {
        throw std::runtime_error("unknown api-name \"" + name +
                                  "\" (expected \"freedictionaryapi\" or \"wikidictionary\")");
      }
    }
    config.apiNames = *apiNamesRaw;
  } else {
    config.apiNames = {"freedictionaryapi", "wikidictionary"};
  }

  config.intervalSeconds = intervalSeconds.value_or(2);
  config.rawWords = std::move(words);
  return config;
}

// Normalizes every word (DictionaryEntry::normalizeWord - the same
// trim+lowercase the real fetch path applies) and throws listing every
// colliding group if two raw lines normalize to the same word. Returns the
// normalized words, in first-seen (== file) order.
std::vector<std::string> normalizeAndValidateWords(const std::vector<std::string>& rawWords) {
  std::unordered_map<std::string, std::vector<std::string>> rawSpellingsOf;
  std::vector<std::string> order;

  for (const auto& raw : rawWords) {
    std::string normalized = dictionary_utils::DictionaryEntry::normalizeWord(raw);
    if (rawSpellingsOf.find(normalized) == rawSpellingsOf.end()) order.push_back(normalized);
    rawSpellingsOf[normalized].push_back(raw);
  }

  std::string report;
  for (const auto& normalized : order) {
    const auto& raws = rawSpellingsOf[normalized];
    if (raws.size() <= 1) continue;
    report += "  " + normalized + " (";
    for (size_t i = 0; i < raws.size(); ++i) {
      if (i) report += ", ";
      report += "\"" + raws[i] + "\"";
    }
    report += ")\n";
  }
  if (!report.empty()) {
    throw std::runtime_error("duplicate word(s) in list:\n" + report);
  }

  return order;
}

// Loads a --retry-only file: one word per line, blank lines/"#" comments
// skipped - same convention as the main word-list body, minus @-parameters
// and duplicate-collision checking (this is a small auxiliary list, not
// the canonical word list, so a duplicate here is harmless). Normalized
// the same way the main list's words are, so it matches status-file keys.
std::vector<std::string> loadRetryOnlyWords(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) throw std::runtime_error("could not open --retry-only file: " + path.string());

  std::vector<std::string> words;
  std::string line;
  while (std::getline(file, line)) {
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') continue;
    words.push_back(dictionary_utils::DictionaryEntry::normalizeWord(trimmed));
  }
  return words;
}

std::filesystem::path statusFilePath(const std::filesystem::path& wordListPath) {
  return wordListPath.parent_path() / (wordListPath.stem().string() + "-status.json");
}

// nullopt-free: an absent status file is simply an empty map to start from.
// Throws on a status file that exists but fails to parse - this may
// represent hours/days of crawl progress, so silently discarding it is not
// an option; the user fixes or deletes it manually.
ordered_json loadStatusFile(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) return ordered_json::object();

  std::ifstream file(path);
  if (!file) throw std::runtime_error("could not open status file: " + path.string());
  std::stringstream buffer;
  buffer << file.rdbuf();

  try {
    return ordered_json::parse(buffer.str());
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error("status file \"" + path.string() + "\" is corrupted (" + e.what() +
                              ") - fix or delete it manually before retrying");
  }
}

// Atomic (temp file + rename) so a process killed mid-write never leaves a
// half-written, unparseable status file behind - same pattern as
// server_data::ServerDataStore::write.
void saveStatusFile(const std::filesystem::path& path, const ordered_json& status) {
  std::filesystem::path tmpPath = path;
  tmpPath += ".tmp";

  {
    std::ofstream file(tmpPath);
    if (!file) throw std::runtime_error("could not write status file: " + tmpPath.string());
    file << status.dump(2) << "\n";
  }

  std::error_code ec;
  std::filesystem::rename(tmpPath, path, ec);
  if (ec) {
    throw std::runtime_error("could not finalize status file \"" + path.string() + "\": " + ec.message());
  }
}

// Ensures every word x configured-api combination has an entry, defaulting
// to "unknown" for anything missing (a word or api added since the last
// run). Never touches or removes an entry for a word/api no longer in the
// current config - those are simply left alone.
void reconcile(ordered_json& status, const std::vector<std::string>& words,
               const std::vector<std::string>& apiNames) {
  for (const auto& word : words) {
    if (!status.contains(word)) status[word] = ordered_json::object();
    for (const auto& api : apiNames) {
      if (!status[word].contains(api)) status[word][api] = "unknown";
    }
  }
}

// The compact tag each source class exposes via its own getTagName() -
// used for the summary's per-word status listing, where the full @api
// config names (e.g. "freedictionaryapi") would be needlessly wide.
std::string apiTagName(ApiSource source) {
  switch (source) {
    case ApiSource::FreeDictionary:
      return dictionary_utils::FreeDictionaryAPIEntry::getTagName();
    case ApiSource::WikiDictionary:
      return dictionary_utils::WiktionaryAPIEntry::getTagName();
  }
  return "";  // unreachable
}

// Whether this source ever produces real audio at all - a static fact
// about the source, not something detected per word. FreeDictionaryAPIEntry
// has no audio URLs whatsoever (see its own doc comment); WiktionaryAPIEntry
// is audio-capable, dynamically ok/missing-audio per word. Neither source
// is IPA-incapable today, so there's no equivalent "ipaCapable" flag yet -
// "no-ipa" exists in the status vocabulary for a hypothetical future source
// only.
bool isAudioCapable(ApiSource source) {
  switch (source) {
    case ApiSource::FreeDictionary:
      return false;
    case ApiSource::WikiDictionary:
      return true;
  }
  return true;
}

// Classifies one successful fetch's result into "ok" or "|"-joined
// qualifiers - never "error"/"unknown", which the caller handles around
// the fetch/exception itself. Same hasEnUsIPA()/hasEnUsAudio() bar for
// every source, per source capability above.
std::string classify(ApiSource source, const dictionary_utils::DictionaryEntry& entry) {
  std::string flags;
  if (!entry.hasEnUsIPA()) flags += "missing-ipa";

  if (!isAudioCapable(source)) {
    if (!flags.empty()) flags += "|";
    flags += "no-audio";
  } else if (!entry.hasEnUsAudio()) {
    if (!flags.empty()) flags += "|";
    flags += "missing-audio";
  }

  return flags.empty() ? "ok" : flags;
}

// "no-ipa"/"no-audio" are static facts about a source - retrying can never
// change them. "error" (not found or a real fetch failure) is gated by
// --retry-error; "missing-ipa"/"missing-audio" (found, but no
// US-recognized data yet) is gated by --retry-missing - separately, since
// they're different bets: a retried "error" might just be a transient
// network hiccup or rate limit, while a retried "missing-*" is betting a
// code fix landed since the last attempt.
bool isRetryable(const std::string& status, bool retryError, bool retryMissing) {
  if (retryError && status == "error") return true;
  if (retryMissing &&
      (status.find("missing-ipa") != std::string::npos || status.find("missing-audio") != std::string::npos)) {
    return true;
  }
  return false;
}

// The fetch itself is what populates the source's own on-disk cache (a
// side effect of fetch(), same as the real HTTP route triggers) - the
// returned DictionaryEntry here is the converted-but-unmerged contribution
// from this one source alone (via dictionary_utils::dictionaryEntryFrom),
// nullopt if not found. ignoreCache is always true here - the status file
// is the new "should I bother" mechanism (see reconcile/isRetryable
// above), so a live attempt should never be quietly satisfied by a stale
// on-disk cache entry instead.
std::optional<dictionary_utils::DictionaryEntry> fetchWord(ApiSource source, const std::string& word,
                                                             const std::filesystem::path& outputDataDir) {
  switch (source) {
    case ApiSource::FreeDictionary: {
      auto raw = dictionary_utils::FreeDictionaryAPIEntry::fetch(word, outputDataDir, "en",
                                                                  /*ignoreCache=*/true);
      if (!raw) return std::nullopt;
      return dictionary_utils::dictionaryEntryFrom(*raw);
    }
    case ApiSource::WikiDictionary: {
      auto raw = dictionary_utils::WiktionaryAPIEntry::fetch(word, outputDataDir, "en",
                                                              /*ignoreCache=*/true);
      if (!raw) return std::nullopt;
      return dictionary_utils::dictionaryEntryFrom(*raw);
    }
  }
  return std::nullopt;  // unreachable
}

bool isWordComplete(const ordered_json& status, const std::string& word,
                     const std::vector<std::string>& apiNames) {
  for (const auto& api : apiNames) {
    if (status[word][api].get<std::string>() == "ok") return true;
  }
  return false;
}

// Whether this word has at least one api that will actually be attempted
// this run - not complete, and at least one api is either "unknown" or
// (with --retry-error/--retry-missing) a retryable status. A word that's
// already exhausted every non-"no-x" avenue without those flags (e.g.
// every api landed on "missing-ipa"/"error" from a prior run) has nothing
// left to do right now, same as a fully "ok" word - it shouldn't clutter
// the progress list or counter with a run that will just print "skipping"
// for everything.
//
// `retryOnlyWords`, when set, overrides all of the above: only words in
// that set have work this run (regardless of current status, even "ok"),
// and every word not in it has none - see this file's own top-of-file
// comment on --retry-only.
bool wordHasWorkThisRun(const ordered_json& status, const std::string& word,
                         const std::vector<std::string>& apiNames, bool retryError, bool retryMissing,
                         const std::optional<std::unordered_set<std::string>>& retryOnlyWords) {
  if (retryOnlyWords) return retryOnlyWords->count(word) > 0;

  if (isWordComplete(status, word, apiNames)) return false;
  for (const auto& api : apiNames) {
    std::string current = status[word][api].get<std::string>();
    if (current == "unknown" || isRetryable(current, retryError, retryMissing)) return true;
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);

  std::optional<std::string> wordListArg;
  std::optional<std::string> retryOnlyArg;
  bool retryError = false;
  bool retryMissing = false;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--retry-error") {
      retryError = true;
    } else if (arg == "--retry-missing") {
      retryMissing = true;
    } else if (arg == "--retry-only") {
      if (i + 1 >= args.size()) {
        std::cerr << "dictionary-crawler: --retry-only requires a file path argument\n" << kUsage;
        return 1;
      }
      retryOnlyArg = args[++i];
    } else if (!wordListArg) {
      wordListArg = arg;
    } else {
      std::cerr << "dictionary-crawler: unexpected extra argument: " << arg << "\n" << kUsage;
      return 1;
    }
  }
  if (!wordListArg) {
    std::cerr << "dictionary-crawler: missing required word-list file path\n" << kUsage;
    return 1;
  }
  if (retryOnlyArg && (retryError || retryMissing)) {
    std::cerr << "dictionary-crawler: --retry-only takes precedence - ignoring --retry-error/"
                 "--retry-missing\n";
  }

  std::filesystem::path wordListPath(*wordListArg);

  WordListConfig config;
  std::vector<std::string> words;
  try {
    config = parseWordListFile(wordListPath);
    words = normalizeAndValidateWords(config.rawWords);
  } catch (const std::exception& e) {
    std::cerr << "dictionary-crawler: " << e.what() << "\n";
    return 1;
  }

  std::optional<std::unordered_set<std::string>> retryOnlyWords;
  if (retryOnlyArg) {
    std::vector<std::string> retryList;
    try {
      retryList = loadRetryOnlyWords(*retryOnlyArg);
    } catch (const std::exception& e) {
      std::cerr << "dictionary-crawler: " << e.what() << "\n";
      return 1;
    }
    std::unordered_set<std::string> wordSet(words.begin(), words.end());
    std::unordered_set<std::string> validated;
    for (const auto& w : retryList) {
      if (wordSet.count(w)) {
        validated.insert(w);
      } else {
        std::cerr << "dictionary-crawler: warning: \"" << w
                   << "\" from --retry-only is not in the word list, skipping\n";
      }
    }
    retryOnlyWords = std::move(validated);
  }

  std::vector<ApiSource> apiSources;
  for (const auto& name : config.apiNames) apiSources.push_back(*parseApiName(name));

  std::filesystem::path statusPath = statusFilePath(wordListPath);
  ordered_json status;
  try {
    status = loadStatusFile(statusPath);
    reconcile(status, words, config.apiNames);
    saveStatusFile(statusPath, status);
  } catch (const std::exception& e) {
    std::cerr << "dictionary-crawler: " << e.what() << "\n";
    return 1;
  }

  std::vector<std::string> remaining;
  for (const auto& word : words) {
    if (wordHasWorkThisRun(status, word, config.apiNames, retryError, retryMissing, retryOnlyWords)) {
      remaining.push_back(word);
    }
  }

  std::cout << "dictionary-crawler: " << words.size() << " word(s) total, " << remaining.size()
             << " still need work, via " << apiSources.size() << " api(s) into "
             << config.outputDir.string() << "\n";

  int attemptsThisRun = 0;
  int skippedThisRun = 0;
  size_t wordCounter = 0;

  for (const auto& word : words) {
    // Silent - nothing to report, either fully done or nothing left this
    // run is eligible to attempt (see wordHasWorkThisRun's own comment).
    if (!wordHasWorkThisRun(status, word, config.apiNames, retryError, retryMissing, retryOnlyWords)) {
      continue;
    }

    ++wordCounter;
    std::cout << "[" << wordCounter << "/" << remaining.size() << "] " << word << "\n";

    bool wordDone = false;
    for (size_t ai = 0; ai < apiSources.size(); ++ai) {
      ApiSource source = apiSources[ai];
      const std::string& apiName = config.apiNames[ai];
      std::string current = status[word][apiName].get<std::string>();

      bool shouldAttempt =
          !wordDone && (retryOnlyWords ? retryOnlyWords->count(word) > 0
                                        : (current == "unknown" || isRetryable(current, retryError, retryMissing)));
      if (!shouldAttempt) {
        std::cout << "  " << apiName << ": skipping (" << (wordDone ? "already ok" : current) << ")\n";
        ++skippedThisRun;
        continue;
      }

      std::cout << "  " << apiName << "... " << std::flush;
      ++attemptsThisRun;

      std::string newStatus;
      try {
        auto entryOr = fetchWord(source, word, config.outputDir);
        if (!entryOr) {
          newStatus = "error";
          std::cout << "not found\n";
        } else {
          newStatus = classify(source, *entryOr);
          std::cout << newStatus << "\n";
        }
      } catch (const std::exception& e) {
        newStatus = "error";
        std::cout << "error: " << e.what() << "\n";
      }

      status[word][apiName] = newStatus;
      try {
        saveStatusFile(statusPath, status);
      } catch (const std::exception& e) {
        std::cerr << "dictionary-crawler: " << e.what() << "\n";
        return 1;
      }

      if (newStatus == "ok") {
        wordDone = true;
      } else {
        std::this_thread::sleep_for(std::chrono::seconds(config.intervalSeconds));
      }
    }
  }

  std::map<std::string, int> histogram;  // sorted, for stable/readable output
  int completeWords = 0;
  std::vector<std::string> incompleteWords;
  for (const auto& word : words) {
    bool complete = false;
    for (const auto& api : config.apiNames) {
      std::string s = status[word][api].get<std::string>();
      ++histogram[s];
      if (s == "ok") complete = true;
    }
    if (complete) {
      ++completeWords;
    } else {
      incompleteWords.push_back(word);
    }
  }

  std::cout << "\nSummary: " << completeWords << "/" << words.size() << " word(s) complete\n";
  std::cout << "This run: " << attemptsThisRun << " attempted, " << skippedThisRun << " skipped\n";
  std::cout << "Status breakdown (" << (words.size() * config.apiNames.size()) << " word-api pair(s)):\n";
  for (const auto& [statusValue, count] : histogram) {
    std::cout << "  " << statusValue << ": " << count << "\n";
  }

  if (!incompleteWords.empty()) {
    std::cout << "\nIncomplete word(s) (" << incompleteWords.size() << "):\n";
    for (const auto& word : incompleteWords) {
      std::cout << "  " << word << ":";
      for (size_t ai = 0; ai < apiSources.size(); ++ai) {
        if (ai) std::cout << ",";
        std::cout << " " << apiTagName(apiSources[ai]) << "="
                   << status[word][config.apiNames[ai]].get<std::string>();
      }
      std::cout << "\n";
    }
  }

  return 0;
}
