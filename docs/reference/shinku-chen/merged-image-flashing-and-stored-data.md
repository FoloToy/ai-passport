<p align="right">
  <a href="merged-image-flashing-and-stored-data.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# What Flashing a Merged Image Does to Stored Data

Collected while verifying the **Asunabi** release on hardware, where one image and
one board gave two different answers to "does this flash wipe the device?" — and
the answer depends on which flashing operation is used.

## The merged image is 0xFF wherever it has nothing to write

The delivery artifact is the merged full image, flashed from offset 0. In the
default layout the NVS partition sits at `0x9000` (24 KiB) and `phy_init` at
`0xF000` (4 KiB). Inside the 7,917,142-byte merged image both regions are entirely
`0xFF`, because `merge-bin` pads the gaps between the bootloader, the partition
table, the application and the data partition.

Those `0xFF` bytes are padding, not a preservation promise. A merged image is
written at `0x0` and covers every byte of the range it contains; the writer erases
and writes each sector the file reaches, NVS and `phy_init` included. The
repository's flashing policy states the same thing: because the merged file pads
the gaps between images, flashing it can reset the NVS and PHY data regions.

## A segmented flash and a full-image write are different operations

The same image was flashed twice, minutes apart, on the same board:

- After the first flash the application logged that it had **restored** its saved
  state — a resume point in chapter 10 — from NVS.
- After the second flash the same firmware logged that there was **no saved state**
  and used its defaults.

Nothing was corrupted and no write failed in either case, but the two runs
disagree. That disagreement is the finding: a full-image write must not be
described as preserving stored data, and a successful boot after one is not
evidence that it did.

- **Segmented flashing** — `idf.py flash`, or writing selected component images at
  their configured offsets — can leave data-region sectors that are not written
  untouched, provided the partition layout matches and no written target covers
  them. This is the operation to use during normal development when existing NVS
  state should be preserved.
- **A full merged-image write** covers the whole range of the file, including NVS
  and `phy_init`, and can reset both regions. Use it for blank-device provisioning
  or an intentional complete refresh, and do not promise that NVS or any other
  stored user data survives it.

## What to do instead

- **To keep stored data**, export or save it first using a method supported by the
  application, then use segmented flashing with a compatible partition layout and
  flash targets that do not overwrite the data regions. Do not write the merged
  image blindly.
- **To clear stored data**, do it explicitly and say so — the application's own
  erase/format step for its namespace, or an explicit `erase_region` of the whole
  partition. Do not rely on a full-image write as an erase.
- **When reporting a device test, state which operation was used and what was
  preserved** rather than assuming an answer: the boot log usually tells you, for
  example a line saying a saved state was restored or that defaults were used.
- **Verify the published artifact** rather than a local build when the point is to
  accept a release; that habit is already recorded in
  [Size Static Buffers from the Panel, and Verify the Release Artifact](release-artifact-verification.md).

## Related

- [A Packed Asset Name Is a Contract Between the Packer and the Firmware](asset-pack-name-contract.md)
- [Shutting Down On-Board Peripherals Before Deep-Sleep](deep-sleep-peripheral-power-off.md)
