#include <cassert>
#include <iostream>

#include "../bitmap.h"

int main()
{
  Pixel purple(128, 0, 255);
  assert(purple.red == 128);
  assert(purple.green == 0);
  assert(purple.blue == 255);

  Bitmap image;
  assert(!image.isImage());

  PixelMatrix pixels(2, std::vector<Pixel>(3, Pixel(1, 2, 3)));
  image.fromPixelMatrix(pixels);
  assert(image.isImage());
  assert(image.toPixelMatrix().size() == 2);
  assert(image.toPixelMatrix()[0].size() == 3);

  pixels[1].push_back(Pixel());
  image.fromPixelMatrix(pixels);
  assert(!image.isImage());

  pixels.resize(1);
  pixels[0][0] = Pixel(256, 0, 0);
  image.fromPixelMatrix(pixels);
  assert(!image.isImage());

  std::cout << "Bitmap tests passed\n";
  return 0;
}
