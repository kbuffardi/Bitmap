#include "../bitmap.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;
int fixture_number = 0;

void expect(bool condition, const std::string & message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void append_u16(std::vector<unsigned char> & bytes, unsigned int value)
{
    bytes.push_back(static_cast<unsigned char>(value & 0xff));
    bytes.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
}

void append_u32(std::vector<unsigned char> & bytes, unsigned long value)
{
    bytes.push_back(static_cast<unsigned char>(value & 0xff));
    bytes.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    bytes.push_back(static_cast<unsigned char>((value >> 16) & 0xff));
    bytes.push_back(static_cast<unsigned char>((value >> 24) & 0xff));
}

std::string write_fixture(const std::vector<unsigned char> & bytes)
{
    const std::string path = "/tmp/bitmap-test-" +
        std::to_string(fixture_number++) + ".bmp";
    std::ofstream output(path.c_str(), std::ios::binary);
    output.write(reinterpret_cast<const char *>(&bytes[0]), bytes.size());
    return path;
}

std::vector<unsigned char> make_24_bpp_fixture()
{
    std::vector<unsigned char> bytes;
    bytes.push_back('B');
    bytes.push_back('M');
    append_u32(bytes, 62);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, 54);
    append_u32(bytes, 40);
    append_u32(bytes, 2);
    append_u32(bytes, 1);
    append_u16(bytes, 1);
    append_u16(bytes, 24);
    append_u32(bytes, 0);
    append_u32(bytes, 8);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    bytes.push_back(30);
    bytes.push_back(20);
    bytes.push_back(10);
    bytes.push_back(60);
    bytes.push_back(50);
    bytes.push_back(40);
    bytes.push_back(0);
    bytes.push_back(0);
    return bytes;
}

void test_public_state_contract()
{
    Bitmap bitmap;
    expect(!bitmap.isImage(), "a default bitmap is not an image");
    expect(!bitmap.isLossy(), "a default bitmap is not lossy");

    bitmap.open("/tmp/bitmap-test-does-not-exist.bmp");
    expect(!bitmap.isImage(), "a failed open leaves an empty image");
    expect(!bitmap.isLossy(), "a failed open resets lossiness");

    PixelMatrix pixels(1, std::vector<Pixel>(1, Pixel(1, 2, 3)));
    bitmap.fromPixelMatrix(pixels);
    expect(bitmap.isImage(), "a valid pixel matrix is an image");
    expect(!bitmap.isLossy(), "a caller-provided matrix has no lossy source");
}

void test_existing_24_bpp_behavior()
{
    const std::string path = write_fixture(make_24_bpp_fixture());
    Bitmap bitmap;
    bitmap.open(path);
    const PixelMatrix pixels = bitmap.toPixelMatrix();

    expect(bitmap.isImage(), "24-bpp BI_RGB opens successfully");
    expect(!bitmap.isLossy(), "24-bpp BI_RGB is lossless");
    expect(pixels.size() == 1 && pixels[0].size() == 2,
           "24-bpp dimensions are preserved");
    if (pixels.size() == 1 && pixels[0].size() == 2)
    {
        expect(pixels[0][0].red == 10 && pixels[0][0].green == 20 &&
               pixels[0][0].blue == 30, "first BGR pixel becomes RGB");
        expect(pixels[0][1].red == 40 && pixels[0][1].green == 50 &&
               pixels[0][1].blue == 60, "second BGR pixel becomes RGB");
    }
    std::remove(path.c_str());
}
}

int main()
{
    test_public_state_contract();
    test_existing_24_bpp_behavior();

    if (failures != 0)
    {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All bitmap tests passed\n";
    return EXIT_SUCCESS;
}
