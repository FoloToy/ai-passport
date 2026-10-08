<p align="right">
  <a href="shared-block-buffer-caches.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Shared Block Buffers Need One Owner

A reader that decompresses several logical streams (a page table, a text stream,
image row blocks) usually shares one scratch buffer to survive on a part without
PSRAM. The trap is caching per stream: if the page-table lookup remembers "block 7
is in the buffer" and the text reader then decompresses block 3 into that same
buffer, the next page-table lookup is a cache hit that returns *text* bytes.

## Symptom

Everything looks fine for a single page: jump to a page, render it, correct
picture and correct text. The defect only appears when reading *continues*: from
the second page on, records decode to garbage, the text slice no longer resolves,
and a reader that treats a failed load as "no more content" silently falls back to
the title screen. Players describe it as "the chapters do not connect" or "I get
bounced back after two lines", which points at the story data rather than at the
buffer.

This is why a single-frame screenshot pipeline (`jump to page N`, then capture)
cannot find it: every capture starts from a cold or self-consistent cache state.
Continuity bugs need a test that *advances* - injected key presses in a loop, or a
host-side walk over the same page graph.

## Fix

Make the shared buffer single-owner: whenever a block is decompressed, invalidate
the *other* stream's cache entry, not just set its own.

```c
int32_t *cache = is_page ? &scn->cached_page_block : &scn->cached_text_block;
int32_t *other = is_page ? &scn->cached_text_block : &scn->cached_page_block;
if (*cache == (int32_t)index) return scn->scratch;
/* ... decompress into scn->scratch ... */
*cache = (int32_t)index;
*other = -1;   /* this decompression just destroyed the other stream's data */
```

Two adjacent rules keep the same class of bug out:

- Treat "decompression failed" and "stream ended" as different outcomes in the
  caller. A port that maps both onto "no more content" turns a buffer bug into a
  plausible-looking ending and hides it from logs.
- Log the first decoded bytes at startup. Printing the first block's byte count
  and first line turns "the reader exits immediately" into a one-line diagnosis.

## Context

Found while porting DRACU-RIOT! to the AI Passport (ESP32-C3, no PSRAM), where a
4 KB scratch buffer serves both the script page table and the text stream. The
same pattern applies to any pack format with several block streams and one
scratch buffer; see the port's application archive for the surrounding pipeline.
