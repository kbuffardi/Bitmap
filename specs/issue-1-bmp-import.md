# Implementation Plan: Common BMP Import to 24-bpp RGB Matrix

## Feature Description
Expand `Bitmap::open()` so the library can read a defined set of commonly supported Windows BMP files that can be converted into the existing `PixelMatrix` representation, where every stored pixel is 24-bpp RGB through `Pixel.red`, `Pixel.green`, and `Pixel.blue`. For this issue, "commonly supported" means CORE/INFO/V2/V3/V4/V5 headers using indexed or direct `BI_RGB`, 16/32-bpp `BI_BITFIELDS`, or `BI_RLE4`/`BI_RLE8`. The public API remains source-compatible except for a new public `bool isLossy()` function. `isLossy()` reports whether the most recent successful `open()` discarded color information or precision that cannot be represented in the library's RGB-only matrix; it never refers to spatial dimensions.

This feature makes the library useful with a much wider range of real BMP files while keeping the teaching-friendly `Pixel` and `Bitmap` API simple.

## User Story
As a C++ user of the Bitmap library
I want to open common BMP variants and receive a normal RGB pixel matrix
So that I can process images without first converting them to uncompressed 24-bpp BMP files in another tool.

## Problem Statement
The current implementation assumes a fixed 14-byte file header, a 40-byte Windows DIB header, uncompressed 24-bpp BGR pixel data, and native struct layout. It rejects every other bit depth and every compressed encoding, even when the BMP can be decoded into the existing RGB `PixelMatrix`. Issue #1 originally described multiple bit-depth support, but the desired scope is now broader: support any commonly supported BMP input that can be converted to a 24-bpp RGB matrix, and expose whether the conversion lost source image information.

## Solution Statement
Replace the single 24-bpp read path with a private BMP decoder pipeline:

1. Read little-endian BMP fields explicitly into a validated internal header model.
2. Support the common Windows DIB header families: the legacy 12-byte `BITMAPCOREHEADER` plus the 40-byte `BITMAPINFOHEADER` and its 52/56/108/124-byte extensions, always honoring `header_size` and `bmp_offset`.
3. Decode supported BMP pixel encodings into `PixelMatrix` while preserving row orientation and validating malformed/truncated inputs atomically.
4. Keep `save()` output as uncompressed 24-bpp `BI_RGB`.
5. Add `Bitmap::isLossy()` as an additive API that returns `true` after a successful `open()` only when source information was discarded during import.

Recommended support matrix:

| Encoding | Bits per pixel | Import plan | `isLossy()` |
|---|---:|---|---|
| `BI_RGB` indexed | 1, 4, 8 | Decode palette indexes through RGBQUAD color table | `false` |
| `BI_RGB` direct | 16 | Decode RGB 5-5-5 and expand to 8-bit channels | `false` because no source precision is discarded |
| `BI_RGB` direct | 24 | Decode existing BGR rows | `false` |
| `BI_RGB` direct | 32 | Decode BGRX RGB channels; ignore the undefined/reserved byte | `false`; an undeclared reserved byte is not image information |
| `BI_BITFIELDS` | 16, 32 | Decode masks, including RGB 5-6-5 and common 32-bit masks; preserve stored RGB values when dropping alpha | `true` only if at least one pixel in a declared alpha channel is non-opaque or an RGB channel has more than 8 bits of precision |
| `BI_RLE8` | 8 | Feasible; implement Microsoft RLE8 encoded, absolute, EOL, EOB, and delta modes | `false` |
| `BI_RLE4` | 4 | Feasible; implement Microsoft RLE4 encoded, absolute, EOL, EOB, and delta modes | `false` |
| `BI_JPEG` / `BI_PNG` | implied | Feasible only with a new image decoder dependency or platform API; explicitly out of scope for this issue | n/a |
| CMYK BMP variants | varies | Reject for now; RGB conversion policy is outside the current library model | n/a |
| 12-byte `BITMAPCOREHEADER` | 1, 4, 8, 24 | Decode its unsigned dimensions and RGBTRIPLE palette layout | `false` |
| Later OS/2 2.x headers/compression | varies | Reject; these are distinct, uncommon extensions rather than the legacy Windows-compatible CORE layout | n/a |

Authoritative references for implementers:

- Microsoft BITMAPINFOHEADER documents bit depth, top-down/bottom-up height, `BI_RGB`, `BI_BITFIELDS`, palette rules, and DWORD stride calculation: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader
- Microsoft Bitmap Storage documents the BMP file layout, color table, pixel-index array, and row order: https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-storage
- Microsoft Bitmap Compression documents `BI_RLE8` and `BI_RLE4` encoded/absolute modes and escape records: https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-compression
- Microsoft WMF compression enumeration documents common compression values and notes that bottom-up bitmaps can be compressed while top-down compressed bitmaps cannot: https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-wmf/4e588f70-bd92-4a6f-b77f-35d0feaf7a57

## Relevant Files
Use these files to implement the feature:

- `bitmap.h`
  - Declares the public `Pixel`, `PixelMatrix`, and `Bitmap` API.
  - Add `bool isLossy();` and a private flag such as `bool lossy;`.
  - Update comments so read support and write support are not confused.
- `bitmap.cpp`
  - Contains all current BMP parsing, saving, validation, and matrix conversion.
  - Replace native packed-struct reads with explicit little-endian parsing helpers.
  - Add private decoder helpers for palettes, bitfields, uncompressed rows, RLE rows, and lossiness tracking.
- `README.md`
  - Update the public documentation with the new import support matrix and `isLossy()` behavior.
  - Clarify that saving still writes 24-bpp uncompressed BMP files.
  - Add explicit Supported, Lossy conversion, and Unsupported/limitations tables rather than describing compatibility only in prose.
- `test_runner.sh`
  - Change from compile-only to compiling and running a dependency-free test executable.

### New Files
- `tests/bitmap_tests.cpp`
  - Add deterministic unit tests that generate tiny BMP fixtures in code and assert exact RGB matrices, `isImage()`, and `isLossy()`.
- `tests/fixtures/README.md` or inline fixture helpers
  - Optional. Prefer inline fixture builders in `tests/bitmap_tests.cpp` unless binary fixture files are truly clearer.

## Implementation Plan
### Phase 1: Foundation
Define the import contract, lossiness semantics, and test harness before touching decoder behavior. Build a small internal BMP parsing layer that reads fields safely and validates file/header invariants before allocating or exposing pixels.

### Phase 2: Core Implementation
Implement import support in vertical slices: uncompressed indexed formats, uncompressed direct-color formats, bitfields, then RLE4/RLE8. Each slice should add tests before or alongside implementation and leave existing 24-bpp behavior intact.

### Phase 3: Integration
Wire `isLossy()` through `open()`, `fromPixelMatrix()`, and failure paths; update documentation; keep `save()` as 24-bpp `BI_RGB`; and run the full validation command set.

## Step by Step Tasks
IMPORTANT: Execute every step in order, top to bottom.

### Task 1: Define API Contract and Test Harness
- Add `bool isLossy();` to `Bitmap` in `bitmap.h`.
- Add a private `bool lossy;` field initialized to `false`.
- Fix `isImage()` so an empty matrix returns `false` before accessing `pixels[0]`; the new failed-open contract must be safe to query.
- Define behavior:
  - New/default `Bitmap`: `isLossy() == false`.
  - Failed `open()`: image is empty and `isLossy() == false`.
  - Successful `open()`: `isLossy()` reflects only loss during import into `PixelMatrix`.
  - `fromPixelMatrix(...)`: `isLossy() == false` because there is no opened source image.
  - `save(...)`: does not change `isLossy()`.
- Create `tests/bitmap_tests.cpp` with small assertion helpers and BMP byte-vector fixture builders.
- Update `test_runner.sh` to compile `bitmap.cpp` plus tests and run the executable.

### Task 2: Replace Native Struct Reads with Safe Header Parsing
- Add private fixed-width little-endian readers for 16-bit and 32-bit signed/unsigned fields.
- Parse the BMP file header and DIB header without relying on compiler struct padding or host endianness.
- Validate magic bytes, DIB header size, planes, width, height, bit depth, compression, color table length, pixel offset, and file length before decoding.
- Preserve atomic failure semantics: never expose a partially decoded matrix.
- Support the known 12/40/52/56/108/124-byte DIB layouts; reject unknown layouts until their field and palette rules are defined.
- Detect V4/V5 non-default color-space, gamma, and profile metadata. Preserve numeric RGB values without applying color-profile conversion, and mark the import lossy when meaningful metadata is present but cannot be represented.

### Task 3: Implement Shared Row, Palette, and Orientation Utilities
- Use the BMP stride formula for uncompressed RGB data: row bytes rounded up to the nearest 4-byte boundary.
- Preserve bottom-up and top-down behavior for supported uncompressed/bitfield BMPs.
- Read `RGBQUAD` palettes for 1/4/8-bpp `BI_RGB` images; when `biClrUsed` is zero, use `2^bits_per_pixel`.
- Validate palette availability and palette indexes.
- Read RGBTRIPLE palettes for 12-byte `BITMAPCOREHEADER` images and RGBQUAD palettes for the Windows INFO/V4/V5 families.

### Task 4: Decode Uncompressed Indexed and Legacy CORE BMPs
- Decode 1-bpp pixels from high bit to low bit.
- Decode 4-bpp pixels from high nibble to low nibble.
- Decode 8-bpp pixels directly as palette indexes.
- Decode the 12-byte `BITMAPCOREHEADER` using unsigned dimensions, RGBTRIPLE palettes, bottom-up rows, and its standard 1/4/8/24-bpp depths.
- Ignore row padding, do not create extra pixels from partially used final bytes, and reject truncated data.
- Add tests for every supported CORE depth, odd widths, top-down and bottom-up orientation where legal, palette lookup, invalid palette references, offsets, and truncation.

### Task 5: Decode Uncompressed Direct-Color BMPs
- Decode 16-bpp `BI_RGB` as RGB 5-5-5 and scale to 0-255 deterministically.
- Keep 24-bpp BGR decoding behavior unchanged.
- Decode 32-bpp `BI_RGB` as BGRX; the high byte is reserved and does not by itself make the conversion lossy.
- Add tests for 16-bpp endpoints/midpoints, 24-bpp regressions, 32-bpp reserved-byte behavior, stride padding, and orientation.

### Task 6: Decode BI_BITFIELDS BMPs
- Support 16-bpp and 32-bpp `BI_BITFIELDS`.
- Read RGB masks from the documented mask location after the DIB header for BITMAPINFOHEADER-style files, or from V4/V5 header fields when applicable.
- Convert masked channels to 8-bit RGB based on mask width and shift.
- Recognize common RGB 5-6-5, RGB 5-5-5, XRGB8888, and ARGB8888 masks.
- Preserve stored RGB channel values when discarding alpha. Mark `lossy` true only when at least one decoded alpha value is non-opaque, or when an RGB mask carries more than 8 bits of precision and must be quantized.
- Add tests for 565, 555, XRGB, fully opaque ARGB, partially transparent ARGB, fully transparent ARGB, preserved RGB-under-alpha values, malformed masks, and unsupported mask layouts.

### Checkpoint: Common uncompressed import
- All CORE/INFO/V2/V3/V4/V5 uncompressed and bitfield fixtures decode to exact RGB values.
- Existing 24-bpp behavior remains compatible.
- Failed imports leave an empty image, and both `isImage()` and `isLossy()` are safe to call.
- This is a shippable milestone before introducing the independent RLE state machine.

### Task 7: Add Feasible Compressed BMP Import with RLE4 and RLE8
- Implement `BI_RLE8` for 8-bpp indexed BMPs using encoded mode, absolute mode, EOL, EOB, and delta escape handling.
- Implement `BI_RLE4` for 4-bpp indexed BMPs using alternating high/low nibbles in encoded and absolute modes.
- Enforce bounds: runs and deltas cannot write outside the image matrix.
- Apply palette lookup after expanding indexes, or while writing decoded pixels.
- Reject top-down RLE images because Microsoft documents compressed BMPs as bottom-up only.
- Add tests for encoded runs, absolute runs, word padding in absolute mode, EOL, EOB, deltas, malformed streams, and out-of-bounds writes.

### Checkpoint: Native BMP compression
- Both RLE variants pass complete command-mode and malformed-stream coverage.
- RLE expansion produces the same RGB matrix as an equivalent uncompressed indexed fixture and remains non-lossy.

### Task 8: Keep Embedded JPEG/PNG BMPs Explicitly Out of Scope
- Document that `BI_JPEG` and `BI_PNG` are feasible only by adding a decoder dependency or platform-specific decoder.
- Do not add a decoder dependency in this issue.
- If support is requested later, create a separate issue covering dependency selection, licensing, security updates, and codec-specific tests.
- Add tests that current `BI_JPEG` and `BI_PNG` BMPs are rejected clearly without partial image state.

### Task 9: Preserve and Tighten Save Behavior
- Keep `save()` output as 24-bpp uncompressed Windows BMP.
- Reuse checked stride/padding calculations so file size and row padding are correct for all image widths.
- Ensure saved images reopen to the same RGB matrix and `isLossy() == false` on the newly opened saved file.
- Add save/reopen tests for widths with 0, 1, 2, and 3 padding bytes.

### Task 10: Update Public Documentation
- Update `bitmap.h` comments for `open()`, `save()`, and `isLossy()`.
- Add README compatibility tables listing supported header families, depth/encoding combinations, and compression modes.
- Add a README lossiness table covering non-opaque alpha, greater-than-8-bit RGB quantization, ignored non-default color metadata, and conversions that remain lossless.
- Add a README limitations table explicitly excluding embedded JPEG/PNG, CMYK, later OS/2-specific variants, ICC/profile conversion, alpha preservation, and alternate-depth/compressed output.
- State that `save()` remains 24-bpp `BI_RGB` only.
- Include a short example showing `image.open(...)`, `image.isImage()`, and `image.isLossy()`.

### Task 11: Run Validation Commands
- Execute every command in the Validation Commands section.
- Fix any failing tests, warnings, or documentation/code mismatches before handing off.

## Testing Strategy
### Unit Tests
Use generated in-memory/minimal file fixtures so each test controls headers, palettes, rows, padding, compression bytes, and truncation cases. Tests should write temporary BMP files under a test temp directory, call the public API, and assert public behavior only.

Required coverage:

- API state: default object, failed open, successful open, `fromPixelMatrix`, `save`.
- Uncompressed indexed: 1, 4, and 8 bpp.
- Uncompressed direct: 16, 24, and 32 bpp.
- Bitfields: 16-bpp 565/555 and common 32-bpp masks.
- RLE: RLE8 and RLE4 encoded mode, absolute mode, EOL, EOB, delta, and malformed streams.
- Orientation: positive-height bottom-up and negative-height top-down where supported.
- Padding: all row padding sizes.
- Atomic failure: unsupported/malformed/truncated files leave `isImage() == false` and `toPixelMatrix().empty()`.

### Edge Cases
- Zero width, zero height, negative width, and minimum/maximum safe dimensions.
- Valid 12-byte CORE and 40/52/56/108/124-byte Windows headers; unknown header sizes; pixel offset before required metadata.
- `biClrUsed` smaller/larger than required palette indexes.
- Palette data overlapping pixel data.
- Arithmetic overflow in stride and image-size calculations.
- Truncated file in header, palette, mask, uncompressed pixel rows, and RLE streams.
- RLE runs crossing row boundaries.
- RLE delta moving outside bounds.
- Top-down compressed BMPs.
- 32-bpp `BI_RGB` data with an ignored reserved byte that is zero versus nonzero (both remain non-lossy).
- Alpha masks with all alpha values opaque versus varied values.
- Transparent pixels whose stored RGB values differ from their visible composited color, proving RGB is preserved rather than composited.
- V4/V5 default metadata versus meaningful non-default color-space/gamma/profile metadata.

## Acceptance Criteria
- `Bitmap::open()` imports common BMP files that can be represented as RGB pixels: 12-byte CORE images at 1/4/8/24 bpp, `BI_RGB` 1/4/8/16/24/32 bpp, `BI_BITFIELDS` 16/32 bpp, `BI_RLE4`, and `BI_RLE8`.
- Imported images produce the expected `PixelMatrix` dimensions, orientation, and exact RGB values.
- `Bitmap::isLossy()` is available as a public function and follows the documented state rules.
- `isLossy()` returns `false` for palette expansion, RLE expansion, 16-bpp expansion, 24-bpp import, and RGB-only bitfield import.
- `isLossy()` returns `true` when import discards color information or precision: at least one declared alpha sample is non-opaque, an RGB component wider than 8 bits is quantized, or meaningful non-default color-management metadata is not applied. Reserved/undefined bytes and an entirely opaque alpha channel do not count.
- When alpha is discarded, stored RGB component values are preserved without compositing against any background.
- Unsupported compressed variants, embedded JPEG/PNG, CMYK BMPs, malformed BMPs, and unsafe dimensions fail cleanly.
- Existing code using `open`, `save`, `isImage`, `toPixelMatrix`, and `fromPixelMatrix` remains source-compatible.
- `save()` still writes uncompressed 24-bpp BMP files and round-trips valid `PixelMatrix` values.
- README and header comments accurately document the supported compatibility matrix, lossy cases, explicit limitations, write format, and `isLossy()` behavior.

## Validation Commands
Execute every command to validate the feature works correctly with zero regressions.

```bash
./test_runner.sh
```

```bash
g++ -std=c++11 -Wall -Wextra -pedantic -c bitmap.cpp -o /tmp/bitmap.o
```

```bash
g++ -std=c++11 -Wall -Wextra -pedantic example.cpp bitmap.cpp -o /tmp/bitmap-example
```

```bash
/tmp/bitmap-example
```

```bash
git diff --check
```

## Notes
- `isLossy()` means "did the library discard color information or precision while converting the opened BMP into the RGB-only `PixelMatrix`?" It does not report whether dimensions changed or whether the source had previously undergone lossy compression. The implementation never resamples; image dimensions remain unchanged.
- A declared alpha channel is lossy only when at least one alpha sample is non-opaque. RGB values are preserved unchanged when alpha is discarded; the importer never composites against an assumed background.
- V4/V5 profile and gamma conversion is deliberately out of scope. Non-default metadata is detected, numeric RGB values are preserved, and `isLossy()` reports the discarded color interpretation.
- RLE4 and RLE8 support is feasible without new dependencies because Microsoft documents the byte stream formats. They are lossless encodings of palette indexes.
- `BI_BITFIELDS` is not compression in the usual sense; it is direct RGB data with masks. It should be treated as a common import format.
- Embedded `BI_JPEG` and `BI_PNG` BMPs are feasible but not dependency-free. Supporting them would expand project scope into general image decoding, dependency selection, licensing, and security handling.
- Include the 12-byte `BITMAPCOREHEADER` because Microsoft lists it as a basic legacy BMP header and common decoders retain it for backward compatibility. Keep later OS/2 2.x-only headers and compression modes out of scope.
