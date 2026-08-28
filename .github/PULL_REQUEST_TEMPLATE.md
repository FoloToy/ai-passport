<p align="right">
  <a href="PULL_REQUEST_TEMPLATE.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

## Summary

<!-- What problem does this PR solve, and what user-visible behavior changes? -->

## Scope and compatibility

- Affected board/revision:
- Affected subsystem:
- Wiring or pin-map impact: None
- Flash, partition, or persistent-data impact: None
- Backward-compatibility impact: None

## Verification

| Check | Result | Evidence or notes |
| --- | --- | --- |
| Build | PASS / FAIL / NOT RUN | |
| Host tests | PASS / FAIL / NOT RUN | |
| Device tests | PASS / FAIL / NOT RUN | |
| Unverified | — | List every remaining board, instrument, or user check |

Commands run:

```text
./tools/validate.sh
```

## Device evidence

<!-- For firmware changes, name each hardware-guide acceptance row and attach its observation, measurement, log excerpt, or display photo. For documentation-only changes, write "Device tests: NOT RUN — no firmware change". Never include credentials or device QR secrets. -->

## Checklist

- [ ] I reviewed the complete diff and excluded unrelated/generated files.
- [ ] I ran `./tools/validate.sh`, or every `NOT RUN` field above names the unavailable environment or device.
- [ ] I separated build, host-test, and device-test results.
- [ ] I updated authoritative documentation for changed hardware facts or durable behavior.
- [ ] I did not edit `docs/CHANGELOG.md` unless this PR is a release-maintainer update for released baseline behavior.
- [ ] I removed credentials, private device links, personal data, and unsanitized logs.
