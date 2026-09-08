# TODOList

## Recorder Loop

- [ ]  [P2] Add controls about speed / repeating multiple times

## Robot Shadowing

- [ ]  [P2] Add a new section “Words” with simple sentences from the most used English words as well as my flashcards
- [ ]  [P2] Need to experiment with new TTS, Google one is good enough but not so high quality.
- [ ]  [P3] Add new themes / subjects
- [ ]  [P3] Hard section is broken, would need to do it again
- [ ]  [P3] Add another section with my own difficult sentences
- [ ]  [P3] Add another section for tongue twisters. And maybe a special mode to repeat them many times in a loop.

### YT shadowing

- [x]  Need to be able to click words, play it and show infos about word
- [x]  Use google translate for now to speak words
- [x]  Update auto shadow mode to use line by line of subtitle instead
- [x]  When shift+click on a word, directly open Cambridge dictionary window.
- [x]  Need a way to record / shadow a word from the word pop up window
- [x]  Unify the play / pause button between word pop up and youtube player. shift + play (or long press) makes it play at 0.5. No pause button for word pop (or in general small audio)
- [x]  Write a new doc about these commons features through all app: play / record / shadow buttons, clickable (and shift clickable word) in all app.
- [x]  Try a mode where I listen N+1 second of what I just recorded, with x1 or x2
- [x]  Cleanup the GUI: One line with all controls, including time.
- [x]  Need to figure out how to control with single button the record and / or shadow mode
- [x]  Have a cleaner GUI that fits both iphone and laptop, and allow to have bigger videos when possible.
- [x]  Find a way to download list of videos / subtitles (or sync the last used list) directly using some form or cloud or other external storage.
- [x]  For record button, shift + click lets you record 2 times (auto duration or not for second time ?)
- [ ]  [P1] Add options to save in KV a captured part (in capture mode).
- [ ]  [P1][BUG] Fix auto shadow mode not working on safari ? (Not sure, might actually be working)
- [ ]  [P1] Use bips between shadow (to know when to speak). Only for words ? Or for sentences too ? Also maybe too short for words (TODO: Need more time before deciding)
- [ ]  [P3] Try to figure out a real hand-free mode where we can shadow efficiently without touching anything (like robot shadowing, except it’s no robot)
- [ ]  [P3] Find a make to make YT videos downloadable (and extend to support other videos)
- [ ]  [P3] Try with Webster dictionary free API to speak words

### Dictionary

- [ ]  [P1] Fix some bugs with audio not found (eg some words have regional pronunciations).
- [ ]  [P2] Fix some bugs with IPA text not found (not sure why)
- [ ]  [P2] Find a way to pack download set of words (eg most used 20k words, and add way to transfer to iphone DB).
- [ ]  [P3] Try the webster-dictionary API instead.
- [ ]  [P3] When a word is not found, add a “did you mean …” with list of closest words (see possible implementation details in `docs/dictionary-spec.md`)

### Flashcards

- [x]  Allow building sets of flashcards
- [x]  One type of flashcards: pronunciation
- [x]  Need algorithms for paced repetition
- [x]  Saved with cloud storage
- [x]  Build an actual real set of words for intonation
- [x]  [P0] Add a beap and slightmy more time for shadowing.
- [ ]  [P0] Fix the half speed more (long press doesn't work ?)
- [ ]  [P2] Add new ways options / filter / etc to control a lot more the “Practice Mode”.
- [ ]  [P2] Have new ways to add new words to flashcards directly from YT shadowing or other apps
- [ ]  [P2] Figure out a way to automatically have good IPAs and stop writing it myself.
- [ ]  [P3] The Loop mode for practice shouldn't chose words randomly, based on the flaschards learning status.

### [WIP] Phonemes

- [ ]  [P2] Have list of phonemes with words, like American pronunciation
- [ ]  [P2] Have links / sections / text and all for each phonemes
- [ ]  [P2] Have ways to shadow / practice small words.
- [ ]  [P2] Have ways to shadow random sentences with lots of the same phoneme
- [ ]  [P2] Have ways to do minimal pairs
- [ ]  [P2] Have ways to add difficult words to flashcards

### [WIP] YT Search

- [ ]  [P1] Build a list of good US accent channels

### General Features

- [x]  [P2] Fix the issue with the html / JS etc being cached (not reloaded) between builds on iOS.
- [ ]  [P2] Fix the mic / audio reconnection issue (infos in `docs/mic-audio-issues`)

## Future Ideas

- Maybe a mode where the app asks questions in a loop (feel like back to storytelling, not HP for now)
- Need new experiments to shadow from YouTube and / or audio files (locally for now)
- Using the native server, use a tool to automatically prompt something and generate random sentences for robot shadowing, and return a JSON with all sentences (can even have a small local LLM running)
- Have some form of review of my recordings: eg transcript + analysis by an LLM, would be really powerful. Local LLM, or directly with Claude.