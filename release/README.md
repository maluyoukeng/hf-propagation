# Release artifacts

This directory contains local build outputs. They are not source code and are
ignored by Git (`release/*.bin`).

| File | Purpose | Flash offset |
| --- | --- | --- |
| `hf-propagation-full.bin` | Bootloader + partition table + OTA data + app | `0x0` |
| `hf-propagation.bin` | Application-only image | `0x20000` |

SHA-256:

```text
hf-propagation-full.bin  a35081563760a20ed2aafa3dfeee1f59be49c180e209283ff85d3c7eadea2b4e
hf-propagation.bin       0fdb4f2badf8cee981e1841189b7217a01b6a5c9397079b8d8b2d1636fb3dec
```

For GitHub, attach the full image to a **Release** instead of uploading it as
source. If you drag the repository into GitHub's web uploader, exclude this
`release/` folder.
