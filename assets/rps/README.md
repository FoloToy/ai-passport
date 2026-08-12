# Rock / paper / scissors art

`source/` keeps the two user-supplied visual references and the generated
v2 red rock, blue scissors, and gold paper trio. `firmware/` contains 240x320 previews. The raw
RGB565 little-endian frames embedded in the application live in
`main/assets/`.

Regenerate the firmware frames with:

```bash
python3 tools/gen_rps_assets.py
```

All three generated prompts use the same visual system: one centered oversized
hand, chunky arcade pixel art, thick black outline, and a dark digital radial
burst. Rock is red, scissors is blue, and paper is gold; none contains text or
logos.
