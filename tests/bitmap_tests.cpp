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

void set_u32(std::vector<unsigned char> & bytes, std::size_t offset,
             unsigned long value)
{
    bytes[offset] = static_cast<unsigned char>(value & 0xff);
    bytes[offset + 1] = static_cast<unsigned char>((value >> 8) & 0xff);
    bytes[offset + 2] = static_cast<unsigned char>((value >> 16) & 0xff);
    bytes[offset + 3] = static_cast<unsigned char>((value >> 24) & 0xff);
}

std::string write_fixture(const std::vector<unsigned char> & bytes)
{
    const std::string path = "/tmp/bitmap-test-" +
        std::to_string(fixture_number++) + ".bmp";
    std::ofstream output(path.c_str(), std::ios::binary);
    if (!bytes.empty())
    {
        output.write(reinterpret_cast<const char *>(&bytes[0]), bytes.size());
    }
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

std::vector<unsigned char> make_info_fixture(
    int width, int height, unsigned int bits_per_pixel,
    unsigned long compression, const std::vector<Pixel> & palette,
    const std::vector<unsigned long> & masks,
    const std::vector<unsigned char> & pixel_bytes,
    unsigned long dib_size = 40, bool meaningful_color_metadata = false)
{
    const bool appended_masks = compression == 3 && dib_size == 40;
    const unsigned long mask_bytes = appended_masks ? 12 : 0;
    const unsigned long pixel_offset = 14 + dib_size + mask_bytes +
        static_cast<unsigned long>(palette.size() * 4);
    std::vector<unsigned char> bytes;
    bytes.push_back('B');
    bytes.push_back('M');
    append_u32(bytes, pixel_offset + pixel_bytes.size());
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, pixel_offset);
    append_u32(bytes, dib_size);
    append_u32(bytes, static_cast<unsigned long>(width));
    append_u32(bytes, static_cast<unsigned long>(height));
    append_u16(bytes, 1);
    append_u16(bytes, bits_per_pixel);
    append_u32(bytes, compression);
    append_u32(bytes, pixel_bytes.size());
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, palette.size());
    append_u32(bytes, 0);
    bytes.resize(14 + dib_size, 0);

    if (dib_size >= 52 && masks.size() >= 3)
    {
        set_u32(bytes, 14 + 40, masks[0]);
        set_u32(bytes, 14 + 44, masks[1]);
        set_u32(bytes, 14 + 48, masks[2]);
    }
    if (dib_size >= 56 && masks.size() >= 4)
    {
        set_u32(bytes, 14 + 52, masks[3]);
    }
    if (meaningful_color_metadata && dib_size >= 108)
    {
        set_u32(bytes, 14 + 56, 0x73524742UL);
        set_u32(bytes, 14 + 96, 1000);
    }
    if (appended_masks)
    {
        append_u32(bytes, masks[0]);
        append_u32(bytes, masks[1]);
        append_u32(bytes, masks[2]);
    }
    for (std::size_t i = 0; i < palette.size(); ++i)
    {
        bytes.push_back(static_cast<unsigned char>(palette[i].blue));
        bytes.push_back(static_cast<unsigned char>(palette[i].green));
        bytes.push_back(static_cast<unsigned char>(palette[i].red));
        bytes.push_back(0);
    }
    bytes.insert(bytes.end(), pixel_bytes.begin(), pixel_bytes.end());
    return bytes;
}

std::vector<unsigned char> make_core_fixture(
    unsigned int width, unsigned int height, unsigned int bits_per_pixel,
    const std::vector<Pixel> & palette,
    const std::vector<unsigned char> & pixel_bytes)
{
    const unsigned long pixel_offset = 26 +
        static_cast<unsigned long>(palette.size() * 3);
    std::vector<unsigned char> bytes;
    bytes.push_back('B');
    bytes.push_back('M');
    append_u32(bytes, pixel_offset + pixel_bytes.size());
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, pixel_offset);
    append_u32(bytes, 12);
    append_u16(bytes, width);
    append_u16(bytes, height);
    append_u16(bytes, 1);
    append_u16(bytes, bits_per_pixel);
    for (std::size_t i = 0; i < palette.size(); ++i)
    {
        bytes.push_back(static_cast<unsigned char>(palette[i].blue));
        bytes.push_back(static_cast<unsigned char>(palette[i].green));
        bytes.push_back(static_cast<unsigned char>(palette[i].red));
    }
    bytes.insert(bytes.end(), pixel_bytes.begin(), pixel_bytes.end());
    return bytes;
}

Pixel open_single_pixel(const std::vector<unsigned char> & fixture,
                        bool expected_lossy, const std::string & context)
{
    const std::string path = write_fixture(fixture);
    Bitmap bitmap;
    bitmap.open(path);
    const PixelMatrix pixels = bitmap.toPixelMatrix();
    expect(bitmap.isImage(), context + " opens");
    expect(bitmap.isLossy() == expected_lossy, context + " lossiness");
    expect(pixels.size() == 1 && pixels[0].size() == 1,
           context + " dimensions");
    std::remove(path.c_str());
    return pixels.size() == 1 && pixels[0].size() == 1
        ? pixels[0][0] : Pixel();
}

void expect_color(const Pixel & pixel, int red, int green, int blue,
                  const std::string & context)
{
    expect(pixel.red == red && pixel.green == green && pixel.blue == blue,
           context);
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

void test_indexed_info_depths()
{
    std::vector<Pixel> two_colors;
    two_colors.push_back(Pixel(0, 0, 0));
    two_colors.push_back(Pixel(12, 34, 56));
    expect_color(open_single_pixel(make_info_fixture(
        1, 1, 1, 0, two_colors, std::vector<unsigned long>(),
        std::vector<unsigned char>{0x80, 0, 0, 0}), false, "1-bpp INFO"),
        12, 34, 56, "1-bpp palette lookup");

    std::vector<Pixel> sixteen_colors(16, Pixel());
    sixteen_colors[10] = Pixel(21, 43, 65);
    expect_color(open_single_pixel(make_info_fixture(
        1, 1, 4, 0, sixteen_colors, std::vector<unsigned long>(),
        std::vector<unsigned char>{0xa0, 0, 0, 0}), false, "4-bpp INFO"),
        21, 43, 65, "4-bpp palette lookup");

    std::vector<Pixel> colors(256, Pixel());
    colors[200] = Pixel(31, 63, 95);
    expect_color(open_single_pixel(make_info_fixture(
        1, 1, 8, 0, colors, std::vector<unsigned long>(),
        std::vector<unsigned char>{200, 0, 0, 0}), false, "8-bpp INFO"),
        31, 63, 95, "8-bpp palette lookup");
}

void test_core_depths()
{
    std::vector<Pixel> palette2;
    palette2.push_back(Pixel());
    palette2.push_back(Pixel(90, 80, 70));
    expect_color(open_single_pixel(make_core_fixture(
        1, 1, 1, palette2, std::vector<unsigned char>{0x80, 0, 0, 0}),
        false, "1-bpp CORE"), 90, 80, 70, "CORE RGBTRIPLE palette");

    std::vector<Pixel> palette16(16, Pixel());
    palette16[3] = Pixel(3, 6, 9);
    expect_color(open_single_pixel(make_core_fixture(
        1, 1, 4, palette16, std::vector<unsigned char>{0x30, 0, 0, 0}),
        false, "4-bpp CORE"), 3, 6, 9, "CORE 4-bpp decode");

    std::vector<Pixel> palette256(256, Pixel());
    palette256[17] = Pixel(17, 34, 51);
    expect_color(open_single_pixel(make_core_fixture(
        1, 1, 8, palette256, std::vector<unsigned char>{17, 0, 0, 0}),
        false, "8-bpp CORE"), 17, 34, 51, "CORE 8-bpp decode");

    expect_color(open_single_pixel(make_core_fixture(
        1, 1, 24, std::vector<Pixel>(),
        std::vector<unsigned char>{7, 8, 9, 0}), false, "24-bpp CORE"),
        9, 8, 7, "CORE 24-bpp decode");
}

void test_direct_color_and_orientation()
{
    expect_color(open_single_pixel(make_info_fixture(
        1, 1, 16, 0, std::vector<Pixel>(), std::vector<unsigned long>(),
        std::vector<unsigned char>{0x00, 0x7c, 0, 0}), false,
        "16-bpp RGB555"), 255, 0, 0, "RGB555 expansion");

    expect_color(open_single_pixel(make_info_fixture(
        1, 1, 32, 0, std::vector<Pixel>(), std::vector<unsigned long>(),
        std::vector<unsigned char>{3, 2, 1, 255}), false,
        "32-bpp BGRX"), 1, 2, 3, "reserved BGRX byte is ignored");

    const std::vector<unsigned char> rows = {
        30, 20, 10, 0,
        60, 50, 40, 0
    };
    const std::string path = write_fixture(make_info_fixture(
        1, -2, 24, 0, std::vector<Pixel>(), std::vector<unsigned long>(), rows));
    Bitmap bitmap;
    bitmap.open(path);
    const PixelMatrix pixels = bitmap.toPixelMatrix();
    expect(pixels.size() == 2 && pixels[0][0].red == 10 &&
           pixels[1][0].red == 40, "negative height preserves top-down order");
    std::remove(path.c_str());
}

void test_bitfields_and_alpha_loss()
{
    const std::vector<unsigned long> rgb565 = {0xf800, 0x07e0, 0x001f};
    expect_color(open_single_pixel(make_info_fixture(
        1, 1, 16, 3, std::vector<Pixel>(), rgb565,
        std::vector<unsigned char>{0xe0, 0x07, 0, 0}), false,
        "16-bpp RGB565"), 0, 255, 0, "RGB565 expansion");

    const std::vector<unsigned long> argb = {
        0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000
    };
    expect_color(open_single_pixel(make_info_fixture(
        1, 1, 32, 3, std::vector<Pixel>(), argb,
        std::vector<unsigned char>{30, 20, 10, 255}, 56), false,
        "opaque ARGB"), 10, 20, 30, "opaque alpha is lossless");

    expect_color(open_single_pixel(make_info_fixture(
        1, 1, 32, 3, std::vector<Pixel>(), argb,
        std::vector<unsigned char>{30, 20, 10, 64}, 56), true,
        "transparent ARGB"), 10, 20, 30,
        "transparent alpha preserves stored RGB");

    const std::vector<unsigned long> rgb101010 = {
        0x3ff00000, 0x000ffc00, 0x000003ff
    };
    open_single_pixel(make_info_fixture(
        1, 1, 32, 3, std::vector<Pixel>(), rgb101010,
        std::vector<unsigned char>{0xff, 0x03, 0, 0}), true,
        "10-bit bitfields");
}

void test_color_metadata_loss()
{
    open_single_pixel(make_info_fixture(
        1, 1, 24, 0, std::vector<Pixel>(), std::vector<unsigned long>(),
        std::vector<unsigned char>{3, 2, 1, 0}, 108, false), false,
        "default V4 metadata");
    open_single_pixel(make_info_fixture(
        1, 1, 24, 0, std::vector<Pixel>(), std::vector<unsigned long>(),
        std::vector<unsigned char>{3, 2, 1, 0}, 108, true), true,
        "meaningful V4 metadata");
}

void test_rle_imports()
{
    std::vector<Pixel> palette256(256, Pixel());
    palette256[7] = Pixel(70, 71, 72);
    const std::vector<unsigned char> rle8 = {1, 7, 0, 0, 0, 1};
    expect_color(open_single_pixel(make_info_fixture(
        1, 1, 8, 1, palette256, std::vector<unsigned long>(), rle8),
        false, "RLE8"), 70, 71, 72, "RLE8 encoded run");

    std::vector<Pixel> palette16(16, Pixel());
    palette16[10] = Pixel(100, 101, 102);
    const std::vector<unsigned char> rle4 = {1, 0xa0, 0, 0, 0, 1};
    expect_color(open_single_pixel(make_info_fixture(
        1, 1, 4, 2, palette16, std::vector<unsigned long>(), rle4),
        false, "RLE4"), 100, 101, 102, "RLE4 encoded run");

    std::vector<Pixel> palette(256, Pixel());
    palette[1] = Pixel(1, 2, 3);
    palette[2] = Pixel(4, 5, 6);
    palette[3] = Pixel(7, 8, 9);
    const std::vector<unsigned char> absolute = {
        0, 3, 1, 2, 3, 0, 0, 0, 0, 1
    };
    const std::string path = write_fixture(make_info_fixture(
        3, 1, 8, 1, palette, std::vector<unsigned long>(), absolute));
    Bitmap bitmap;
    bitmap.open(path);
    const PixelMatrix pixels = bitmap.toPixelMatrix();
    expect(bitmap.isImage() && pixels[0].size() == 3,
           "RLE8 absolute run opens");
    if (bitmap.isImage() && pixels[0].size() == 3)
    {
        expect(pixels[0][0].red == 1 && pixels[0][1].red == 4 &&
               pixels[0][2].red == 7, "RLE8 absolute indexes decode");
    }
    std::remove(path.c_str());
}

void test_rle_commands_and_orientation()
{
    std::vector<Pixel> palette(256, Pixel());
    palette[1] = Pixel(10, 0, 0);
    palette[2] = Pixel(20, 0, 0);
    const std::vector<unsigned char> commands = {
        0, 2, 1, 0,
        1, 2,
        0, 0,
        3, 1,
        0, 1
    };
    std::string path = write_fixture(make_info_fixture(
        3, 2, 8, 1, palette, std::vector<unsigned long>(), commands));
    Bitmap bitmap;
    bitmap.open(path);
    PixelMatrix pixels = bitmap.toPixelMatrix();
    expect(bitmap.isImage() && pixels.size() == 2 && pixels[0].size() == 3,
           "RLE8 delta and EOL commands open");
    if (bitmap.isImage() && pixels.size() == 2 && pixels[0].size() == 3)
    {
        expect(pixels[0][0].red == 10 && pixels[0][1].red == 10 &&
               pixels[0][2].red == 10, "RLE storage rows are bottom-up");
        expect(pixels[1][0].red == 0 && pixels[1][1].red == 20 &&
               pixels[1][2].red == 0, "RLE delta leaves palette index zero");
    }
    std::remove(path.c_str());

    std::vector<Pixel> palette16(16, Pixel());
    palette16[1] = Pixel(1, 0, 0);
    palette16[2] = Pixel(2, 0, 0);
    palette16[3] = Pixel(3, 0, 0);
    const std::vector<unsigned char> rle4_absolute = {
        0, 3, 0x12, 0x30, 0, 1
    };
    path = write_fixture(make_info_fixture(
        3, 1, 4, 2, palette16, std::vector<unsigned long>(), rle4_absolute));
    bitmap.open(path);
    pixels = bitmap.toPixelMatrix();
    expect(bitmap.isImage() && pixels[0][0].red == 1 &&
           pixels[0][1].red == 2 && pixels[0][2].red == 3,
           "RLE4 absolute nibbles decode in high-low order");
    std::remove(path.c_str());
}

void test_save_round_trip_padding()
{
    for (int width = 1; width <= 4; ++width)
    {
        PixelMatrix source(2, std::vector<Pixel>(width));
        for (int row = 0; row < 2; ++row)
        {
            for (int column = 0; column < width; ++column)
            {
                source[row][column] = Pixel(10 + row, 20 + column,
                                            30 + row + column);
            }
        }
        Bitmap saved;
        saved.fromPixelMatrix(source);
        const std::string path = "/tmp/bitmap-save-" +
            std::to_string(width) + ".bmp";
        saved.save(path);

        Bitmap reopened;
        reopened.open(path);
        const PixelMatrix result = reopened.toPixelMatrix();
        expect(reopened.isImage() && !reopened.isLossy(),
               "saved 24-bpp image reopens losslessly");
        bool matches = result.size() == source.size();
        for (std::size_t row = 0; matches && row < source.size(); ++row)
        {
            matches = result[row].size() == source[row].size();
            for (std::size_t column = 0;
                 matches && column < source[row].size(); ++column)
            {
                matches = result[row][column].red == source[row][column].red &&
                    result[row][column].green == source[row][column].green &&
                    result[row][column].blue == source[row][column].blue;
            }
        }
        expect(matches, "save/reopen preserves every padding width");
        std::remove(path.c_str());
    }
}

void test_malformed_inputs_fail_atomically()
{
    std::vector<unsigned char> truncated = make_24_bpp_fixture();
    truncated.pop_back();
    truncated.pop_back();
    Bitmap bitmap;
    std::string path = write_fixture(truncated);
    bitmap.open(path);
    expect(!bitmap.isImage() && !bitmap.isLossy(),
           "truncated rows fail atomically");
    std::remove(path.c_str());

    std::vector<Pixel> palette(256, Pixel());
    const std::vector<unsigned char> crossing_run = {2, 1, 0, 1};
    path = write_fixture(make_info_fixture(
        1, 1, 8, 1, palette, std::vector<unsigned long>(), crossing_run));
    bitmap.open(path);
    expect(!bitmap.isImage(), "RLE runs cannot cross row bounds");
    std::remove(path.c_str());

    std::vector<unsigned char> bad_offset = make_24_bpp_fixture();
    set_u32(bad_offset, 10, 20);
    path = write_fixture(bad_offset);
    bitmap.open(path);
    expect(!bitmap.isImage(), "pixel data cannot overlap its header");
    std::remove(path.c_str());

    std::vector<unsigned char> unknown_header = make_24_bpp_fixture();
    set_u32(unknown_header, 14, 41);
    path = write_fixture(unknown_header);
    bitmap.open(path);
    expect(!bitmap.isImage(), "unknown DIB headers are rejected");
    std::remove(path.c_str());

    const std::vector<unsigned long> overlapping_masks = {
        0x00ff, 0x00f0, 0xff00
    };
    path = write_fixture(make_info_fixture(
        1, 1, 16, 3, std::vector<Pixel>(), overlapping_masks,
        std::vector<unsigned char>{0, 0, 0, 0}));
    bitmap.open(path);
    expect(!bitmap.isImage(), "overlapping bitfield masks are rejected");
    std::remove(path.c_str());

    std::vector<Pixel> rle_palette(256, Pixel());
    const std::vector<unsigned char> top_down_rle = {1, 0, 0, 1};
    path = write_fixture(make_info_fixture(
        1, -1, 8, 1, rle_palette, std::vector<unsigned long>(), top_down_rle));
    bitmap.open(path);
    expect(!bitmap.isImage(), "top-down RLE is rejected");
    std::remove(path.c_str());
}
}

int main()
{
    test_public_state_contract();
    test_existing_24_bpp_behavior();
    test_indexed_info_depths();
    test_core_depths();
    test_direct_color_and_orientation();
    test_bitfields_and_alpha_loss();
    test_color_metadata_loss();
    test_rle_imports();
    test_rle_commands_and_orientation();
    test_save_round_trip_padding();
    test_malformed_inputs_fail_atomically();

    if (failures != 0)
    {
        std::cerr << failures << " test assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All bitmap tests passed\n";
    return EXIT_SUCCESS;
}
