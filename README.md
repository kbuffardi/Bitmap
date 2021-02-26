# C++ Bitmap header files

## Getting Started

1. Clone this repository onto your development environment
2. Copy `bitmap.h` and `bitmap.cpp` to your project directory
3. In your C++ project, include the header file with
`#include "bitmap.h"` and include bitmap in your compilation
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

Represents a bitmap where a grid of pixels (in row-major order)
describes the color of each pixel within the image. Limited to Windows BMP
formatted images with no compression and 24 bit color depth.

### Functions

#### open

`void open(std::string)`

*Opens a file as its name is provided and reads pixel-by-pixel the colors
into a matrix of RGB pixels. Any errors will cout but will result in an
empty matrix (with no rows and no columns).*

*parameter: name of the filename to be opened and read as a matrix of pixels*

#### save

`void save(std::string)`

*Saves the current image, represented by the matrix of pixels, as a
Windows BMP file with the name provided by the parameter. File extension
is not forced but should be .bmp. Any errors will cout and will NOT
attempt to save the file.*

#### isImage

`bool isImage()`

*Validates whether or not the current matrix of pixels represents a
proper image with non-zero-size rows and consistent non-zero-size
columns for each row. In addition, each pixel in the matrix is validated
to have red, green, and blue components with values between 0 and 255*

*return: boolean value of whether or not the matrix is a valid image*

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
  return 0;
}
```
