# C++ Bitmap header files

This repo is compatible with the [cpp-container](https://github.com/ChicoState/cpp-container) Docker container and requires C++11 or newer.

## Getting Started

1. Clone this repository onto your development environment
2. Copy `bitmap.h` and `bitmap.cpp` to your project directory
3. In your C++ project, include the header file with
`#include "bitmap.h"` and include `bitmap.cpp` in your compilation
4. Declare your variables of type *Bitmap* or *Pixel*.

See the guides for the Bitmap and Pixel data types below.

## Pixel

Represents a single Pixel in the image. A Pixel has red, green, and blue
components that are mixed to form a color. Each of these values can range
from 0 to 255.

By default, a pixel is black with 0 red, 0 green, and 0 blue. There is an
overloaded constructor that takes three integer arguments for the red, green,
and blue components.

Each component is mutable using the member variables `red` `green` and `blue`.
They should have values between 0 and 255 but this class does *not* validate
the component values as long as they are assigned integers.

### Example of use

```
Pixel purpleDot;

purpleDot.red = 255;
purpleDot.green = 0;
purpleDot.blue = 255;
```

## Bitmap

Represents a bitmap where a grid of pixels in row-major order describes the
color of each pixel. Supported BMP inputs are normalized to 8-bit red, green,
and blue components. Saving always produces an uncompressed 24-bpp BMP.

### Input compatibility

| DIB header | Encoding | Supported depths |
|---|---|---|
| 12-byte `BITMAPCOREHEADER` | Uncompressed indexed/direct RGB | 1, 4, 8, 24 bpp |
| 40-byte `BITMAPINFOHEADER` and 52/56/108/124-byte extensions | `BI_RGB` | 1, 4, 8, 16, 24, 32 bpp |
| 40/52/56/108/124-byte headers | `BI_BITFIELDS` | 16, 32 bpp |
| 40/52/56/108/124-byte headers | `BI_RLE4` | 4 bpp |
| 40/52/56/108/124-byte headers | `BI_RLE8` | 8 bpp |

The decoder supports bottom-up images and top-down uncompressed/bitfield
images. Microsoft defines RLE-compressed BMPs as bottom-up, so top-down RLE is
rejected. Palettes, bitfields, scanline padding, and row orientation are
converted internally; the returned matrix is always top-to-bottom RGB.

### Lossiness rules

`isLossy()` reports whether the most recent successful `open()` discarded
color information or precision while creating the RGB-only matrix. It does
not refer to image width, height, or pixel density; dimensions are not
resampled.

| Import behavior | `isLossy()` |
|---|---:|
| Palette expansion, RLE expansion, row reordering, or padding removal | `false` |
| Expanding 5-bit or 6-bit RGB components to 8-bit components | `false` |
| Ignoring the undefined byte in 32-bpp `BI_RGB` BGRX data | `false` |
| Discarding a declared alpha channel whose pixels are all opaque | `false` |
| Discarding a declared alpha channel with any non-opaque pixel | `true` |
| Reducing an RGB bitfield component wider than 8 bits | `true` |
| Preserving numeric RGB values without applying meaningful V4/V5 color-profile or gamma metadata | `true` |

When alpha is discarded, the stored red, green, and blue values are preserved
unchanged. The library does not composite pixels against an assumed
background.

For a new `Bitmap`, after a failed `open()`, or after `fromPixelMatrix()`,
`isLossy()` returns `false`. Calling `save()` does not alter the value.

### Limitations

- Embedded `BI_JPEG` and `BI_PNG` payloads are not decoded.
- CMYK BMP encodings and later OS/2 2.x-specific headers/compression are not supported.
- Unknown DIB header sizes are rejected rather than guessed.
- ICC/profile and gamma transforms are not applied; numeric RGB values are retained and the import is marked lossy when meaningful metadata is present.
- Alpha is not retained in `PixelMatrix` or saved output.
- Input files larger than 512 MiB are rejected before allocation.
- Inputs whose decoded dimensions exceed 100 million pixels are rejected as a memory-safety limit.
- `save()` writes only bottom-up, uncompressed, 24-bpp `BI_RGB` files.

The decoder behavior follows Microsoft's documentation for
[bitmap headers](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-header-types),
[bitmap storage](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-storage),
[`BITMAPINFOHEADER`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader),
and [RLE compression](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-compression).

### Functions

#### open

`void open(std::string)`

*Opens a supported BMP and converts it to a matrix of RGB pixels. Any error is
written to `std::cerr` and leaves an empty matrix.*

*parameter: name of the filename to be opened and read as a matrix of pixels*

#### save

`void save(std::string)`

*Saves the current matrix as an uncompressed 24-bpp Windows BMP. The file
extension is not forced but should be `.bmp`. Errors are written to
`std::cerr`.*

#### isImage

`bool isImage()`

*Validates whether or not the current matrix of pixels represents a
proper image with non-zero-size rows and consistent non-zero-size
columns for each row. In addition, each pixel in the matrix is validated
to have red, green, and blue components with values between 0 and 255*

*return: boolean value of whether or not the matrix is a valid image*

#### isLossy

`bool isLossy()`

*Returns whether the most recent successful `open()` discarded color
information or precision while converting the input to RGB pixels. See the
lossiness table above for exact behavior.*

#### toPixelMatrix

`std::vector <std::vector <Pixel> > toPixelMatrix()`

*Provides a vector of vector of pixels representing the bitmap*

*return: the bitmap image, represented by a matrix of RGB pixels*

#### fromPixelMatrix

`void fromPixelMatrix(const std::vector <std::vector <Pixel> > &)`

*Overwrites the current bitmap with that represented by a matrix of
pixels. Does not validate that the new matrix of pixels is a proper
image.*

*parameter: a matrix of pixels to represent a bitmap*


### Example of use

```
#include <vector>
#include "bitmap.h"

using namespace std;

int main()
{
  Bitmap image;
  vector <vector <Pixel> > bmp;
  Pixel rgb;

  //read a file example.bmp and convert it to a pixel matrix
  image.open("example.bmp");

  //verify that the file opened was a valid image
  bool validBmp = image.isImage();
  bool lostColorInformation = image.isLossy();

  if( validBmp == true )
  {
    bmp = image.toPixelMatrix();


    //take all the redness out of the top-left pixel
    rgb = bmp[0][0];
    rgb.red = 0;

    //put changed image back into matrix, update the bitmap and save it
    bmp[0][0] = rgb;
    image.fromPixelMatrix(bmp);
    image.save("example.bmp");
  }
  (void)lostColorInformation;
  return 0;
}
```
