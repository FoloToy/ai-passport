<p align="right">
  <a href="visual-novel-story-graph-verification.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Verifying a Ported Visual Novel's Story Graph

Captured after the **Saya no Uta reader** release. The technique is general: any
chapter-and-choice visual novel port benefits from proving its graph before shipping.

> **Verification status.** Run against this release's committed pack: 44 of 44 chapters
> and 473 of 473 scenes are reachable from the first scene, and all three endings are
> reached by the three choice combinations. The same walk is part of the host test suite
> that runs in the validation gate.

## Model the story the way the player's advance function sees it

A graph you can analyse comes straight out of the pack:

- a **chapter** record: first scene, scene count, `next` chapter;
- a **scene** record: background, choice count, first dialogue, dialogue count, and for a
  choice scene the two option names and their target chapters;
- a **dialogue** record: text range, speaker, sprite, and the flags on the *last*
  dialogue of a scene (jump within the chapter, or an ending). Only the last dialogue
  ends a scene, so anything mid-scene is prose even if it carries flags.

Transitions, in the order the runtime applies them: a choice scene waits for a pick and
then loads the target chapter; otherwise the last dialogue's jump/ending flag decides;
otherwise the next scene; otherwise the `next` chapter; otherwise a terminal ending.

## Prove reachability and endings with a host test

Walk that graph from the first scene with a visited set and assert:

- every chapter and every scene is reachable — an unreachable chapter is either a
  packing bug or content the player can never see;
- each ending is reachable, by enumerating the choice combinations (this data has two
  choice points and therefore three routes to three endings);
- the walk terminates: advancing past an ending keeps returning the ending instead of
  reading out of bounds.

Doing this in a host test against the real pack needs no device and no flashing, so it
runs in the same gate as the compiler checks and catches a broken chapter table, a
choice target pointing at the wrong chapter, or a scene that no route reaches.

## Verify the interaction rules that sit on top of the graph

Graph reachability says nothing about the reading controls, so assert the behaviours that
depend on the same data:

- a choice scene stops the typewriter's fast-forward: the auto-advance loop must not be
  allowed to run while the player is standing on a choice;
- "skip chapter" stops at a choice the chapter has not reached yet instead of picking an
  option, and refuses when the current scene is already a choice or the chapter has no
  successor;
- after a content variant is added, re-run the same walk per variant: comparing
  per-chapter fingerprints (dialogue text, speaker, background image content, sprite,
  flags) plus the background-image set is what proves that a variant only changes the
  chapters it is supposed to change. In this release that check showed the patched
  variant differs in exactly 7 chapters and 22 images and nothing else.

## Numbers from this port

| Measurement | Value |
| --- | --- |
| Chapters / scenes | 44 / 473 |
| Dialogue lines | 3,828 |
| Choice points | 2 (chapters 10 and 20) |
| Endings | 3 (`End`, `BadEnd`, `MadEnd`) |
| Reachability | 44/44 chapters, 473/473 scenes |
