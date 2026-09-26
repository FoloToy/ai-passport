<p align="right">
  <a href="merged-image-flashing-and-stored-data.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# What Flashing a Merged Image Does to Stored Data

Collected while verifying the **Asunabi** release on hardware, where one image and
one board gave two different answers to "does this flash wipe the device?".

## The merged image is 0xFF wherever it has nothing to write

The delivery artifact is the merged full image, flashed from offset 0. In the
default layout the NVS partition sits at `0x9000` (24 KiB) and `phy_init` at
`0xF000` (4 KiB). Inside the 7,917,142-byte merged image both regions are entirely
`0xFF`, because `merge-bin` pads the gaps between the bootloader, the partition
table, the application and the data partition.

It is easy to read that padding as "this flash erases NVS". It does not.

## The padding is not a reliable erase

The same image was flashed twice, minutes apart, on the same board:

- After the first flash the application logged that it had **restored** its saved
  state — a resume point in chapter 10 — from NVS.
- After the second flash the same firmware logged that there was **no saved state**
  and used its defaults.

Nothing was corrupted, and no write failed in either case. The point is not which
of the two is "correct": it is that writing a merged image must not be described as
"wipes user data", and must not be used as a way to clear it. The repository's
flashing policy already says preservation is not guaranteed; this is what that
looks like in practice.

## What to do instead

- **To keep stored data**, do not write the merged image blindly: confirm the
  partition layout matches and write the component images at their configured
  offsets, leaving NVS alone.
- **To clear stored data**, do it explicitly and say so — the application's own
  erase/format step for its namespace, or an explicit `erase_region` of the whole
  partition. Do not rely on a full-image write as an erase.
- **When reporting a device test, state what was preserved** rather than assuming
  either answer: the boot log usually tells you, for example a line saying a saved
  state was restored or that defaults were used.
- **Verify the published artifact** rather than a local build when the point is to
  accept a release; that habit is already recorded in
  [Size Static Buffers from the Panel, and Verify the Release Artifact](release-artifact-verification.md).

## Related

- [A Packed Asset Name Is a Contract Between the Packer and the Firmware](asset-pack-name-contract.md)
- [Shutting Down On-Board Peripherals Before Deep-Sleep](deep-sleep-peripheral-power-off.md)
