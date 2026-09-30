#include <iostream>

#include "../bitmap.h"

int main()
{
  PixelMatrix pixels(1, std::vector<Pixel>(1, Pixel(0, 128, 255)));
  Bitmap image;
  image.fromPixelMatrix(pixels);

  std::cout << "Created a " << image.toPixelMatrix().size()
            << "x" << image.toPixelMatrix()[0].size() << " bitmap.\n";
  return image.isImage() ? 0 : 1;
}
