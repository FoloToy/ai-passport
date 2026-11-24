<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Pocket Physics Quiz

A pocket physics drill for the AI Passport: ten random questions per round, three
keys, an instant right-or-wrong check, and a one-line explanation after every
answer.

## Publish information

- **Title**: Pocket Physics Quiz
- **Description**: Turn AI Passport into a pocket physics quiz. Each round draws
  10 random questions from a bank of 60: pick an answer with Up/Down and press OK
  to check it. Every question gives instant right-or-wrong feedback plus a
  one-line explanation, and the round ends with your score and accuracy. Topics
  cover mechanics, electricity, optics, heat and sound - a quick brain warm-up
  between classes or on the commute.
- **Community project**: `community-f72107a3` (project `2642`, revision `6635`).
- **Cover image**: `cover.jpg`, JPEG, 1680 × 2240 (3:4). Recorded as publish
  metadata only; the image itself is not stored in this archive.

## What it does

- **Boots straight into the quiz**: no launcher; the home screen shows the round
  size (10 questions) and the live bank size (60 questions).
- **Random rounds**: ten distinct questions are drawn from the 60-question bank
  per round, reshuffled every round.
- **Instant checking**: after submitting, the chosen option is graded at once;
  a wrong answer reveals the correct one.
- **One-line explanations**: every question carries a short explanation, so a
  wrong answer teaches something instead of only costing a point.
- **Round summary**: score out of ten, accuracy percentage, and a short comment.
- **Battery readout**: the home screen shows the battery percentage reported by
  the on-board fuel gauge.
- **Offline**: the whole bank and the CJK fonts are compiled into flash; no
  network setup is needed.

## Interaction

- **Home — OK**: start a round.
- **Question — UP / DOWN**: move the highlight across the four options.
- **Question — OK**: submit the highlighted option and switch to the answer view.
- **Answer view — OK**: next question, or the round summary after the tenth.
- **Summary — OK**: play another round.
- **Long-press OK (any screen except home)**: return to the home screen.

## Verification

- **Build**: PASS — merged image 850,912 bytes written from `0x0`; firmware
  layout verification and archive verification passed. The official
  `tools/validate.sh --firmware` gate cannot run under Git Bash on the build
  machine (ESP-IDF rejects the MSYS environment), so the equivalent build,
  `merge-bin`, `verify_firmware.py` and `archive_firmware.py` steps were run
  individually and all passed.
- **Host tests**: PASS — `tests/test_quiz_model.c` checks bank completeness, that
  each option letter appears as the correct answer, draw uniqueness and
  determinism, selection wrap-around, scoring, round completion, and accuracy
  rounding.
- **Device tests**: PASS — flashed to an ESP32-C3 AI Passport (8 MB flash,
  240 × 320 panel); the startup log shows display, LVGL, three-key ADC, I²C codec
  and fuel gauge, and battery initialization with no panic or reboot loop. The
  screen, button flow and battery readout were confirmed on hardware for the
  previous 40-question build.
- **Unverified**: audio (the app is silent), Bluetooth, Wi-Fi, deep sleep and
  long-run stability; the 60-question build was confirmed to boot cleanly, but a
  full UI walkthrough was not re-run on device.

## Source

No public source address was provided at publish time. The implementation lives
on the local branch `feature/physics-quiz` of the AI Passport checkout
(`main/app_physics.c`, `main/quiz_model.c`, `main/quiz_bank.c`, plus the generated
CJK bitmap-font subset under `assets/fonts/`).

## Notes

- The UI is Simplified Chinese; the 16 px and 22 px bitmap fonts are subsets
  generated from Noto Sans SC for exactly the strings the app renders.
- The bank holds 60 single-choice questions covering mechanics, electricity,
  optics, heat and sound; `QUIZ_BANK_MAX` leaves room for 96.
- One mechanics question was rewritten after review so that the direction of
  motion is explicit and the 14 N resultant is unambiguous.
