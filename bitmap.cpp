#include "bitmap.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

namespace
{
const int MIN_RGB = 0;
const int MAX_RGB = 255;
const std::uint32_t BI_RGB = 0;
const std::uint32_t BI_RLE8 = 1;
const std::uint32_t BI_RLE4 = 2;
const std::uint32_t BI_BITFIELDS = 3;
const std::uint32_t LCS_SRGB = 0x73524742;
const std::uint64_t MAX_INPUT_BYTES = 512ULL * 1024 * 1024;
const std::uint64_t MAX_DECODED_PIXELS = 100000000;

struct Header
{
    std::uint32_t dib_size;
    std::int32_t width;
    std::int32_t signed_height;
    std::uint32_t height;
    std::uint16_t bits_per_pixel;
    std::uint32_t compression;
    std::uint32_t image_size;
    std::uint32_t colors_used;
    std::uint32_t pixel_offset;
    std::uint32_t red_mask;
    std::uint32_t green_mask;
    std::uint32_t blue_mask;
    std::uint32_t alpha_mask;
    std::uint32_t profile_offset;
    std::uint32_t profile_size;
    std::size_t palette_offset;
    bool core;
    bool top_down;
    bool color_metadata_lost;

    Header()
        : dib_size(0), width(0), signed_height(0), height(0),
          bits_per_pixel(0), compression(BI_RGB), image_size(0),
          colors_used(0), pixel_offset(0), red_mask(0), green_mask(0),
          blue_mask(0), alpha_mask(0), profile_offset(0), profile_size(0),
          palette_offset(0), core(false), top_down(false),
          color_metadata_lost(false)
    {
    }
};

struct MaskInfo
{
    std::uint32_t mask;
    unsigned int shift;
    unsigned int bits;
    std::uint64_t maximum;
};

bool range_fits(std::size_t offset, std::size_t length, std::size_t size)
{
    return offset <= size && length <= size - offset;
}

bool read_u16(const std::vector<unsigned char> & bytes, std::size_t offset,
              std::uint16_t & value)
{
    if (!range_fits(offset, 2, bytes.size()))
    {
        return false;
    }
    value = static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(bytes[offset + 1] << 8);
    return true;
}

bool read_u32(const std::vector<unsigned char> & bytes, std::size_t offset,
              std::uint32_t & value)
{
    if (!range_fits(offset, 4, bytes.size()))
    {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    return true;
}

bool read_s32(const std::vector<unsigned char> & bytes, std::size_t offset,
              std::int32_t & value)
{
    std::uint32_t unsigned_value = 0;
    if (!read_u32(bytes, offset, unsigned_value))
    {
        return false;
    }
    value = static_cast<std::int32_t>(unsigned_value);
    return true;
}

bool load_file(const std::string & filename, std::vector<unsigned char> & bytes)
{
    std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
    if (!file)
    {
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    if (length < 0 || static_cast<std::uint64_t>(length) > MAX_INPUT_BYTES ||
        static_cast<std::uint64_t>(length) >
            std::numeric_limits<std::size_t>::max())
    {
        return false;
    }
    file.seekg(0, std::ios::beg);
    try
    {
        bytes.resize(static_cast<std::size_t>(length));
    }
    catch (const std::bad_alloc &)
    {
        return false;
    }
    if (!bytes.empty())
    {
        file.read(reinterpret_cast<char *>(&bytes[0]), bytes.size());
        return static_cast<std::size_t>(file.gcount()) == bytes.size();
    }
    return true;
}

bool is_known_dib_size(std::uint32_t size)
{
    return size == 12 || size == 40 || size == 52 || size == 56 ||
        size == 108 || size == 124;
}

bool valid_encoding(const Header & header)
{
    if (header.core)
    {
        return header.compression == BI_RGB &&
            (header.bits_per_pixel == 1 || header.bits_per_pixel == 4 ||
             header.bits_per_pixel == 8 || header.bits_per_pixel == 24);
    }
    if (header.compression == BI_RGB)
    {
        return header.bits_per_pixel == 1 || header.bits_per_pixel == 4 ||
            header.bits_per_pixel == 8 || header.bits_per_pixel == 16 ||
            header.bits_per_pixel == 24 || header.bits_per_pixel == 32;
    }
    if (header.compression == BI_RLE8)
    {
        return header.bits_per_pixel == 8 && !header.top_down;
    }
    if (header.compression == BI_RLE4)
    {
        return header.bits_per_pixel == 4 && !header.top_down;
    }
    return header.compression == BI_BITFIELDS &&
        (header.bits_per_pixel == 16 || header.bits_per_pixel == 32);
}

bool has_nonzero_bytes(const std::vector<unsigned char> & bytes,
                       std::size_t offset, std::size_t length)
{
    if (!range_fits(offset, length, bytes.size()))
    {
        return false;
    }
    for (std::size_t index = offset; index < offset + length; ++index)
    {
        if (bytes[index] != 0)
        {
            return true;
        }
    }
    return false;
}

bool parse_header(const std::vector<unsigned char> & bytes, Header & header)
{
    if (!range_fits(0, 18, bytes.size()) || bytes[0] != 'B' || bytes[1] != 'M' ||
        !read_u32(bytes, 10, header.pixel_offset) ||
        !read_u32(bytes, 14, header.dib_size) ||
        !is_known_dib_size(header.dib_size))
    {
        return false;
    }

    std::uint16_t planes = 0;
    if (header.dib_size == 12)
    {
        std::uint16_t width = 0;
        std::uint16_t height = 0;
        if (!range_fits(14, 12, bytes.size()) ||
            !read_u16(bytes, 18, width) || !read_u16(bytes, 20, height) ||
            !read_u16(bytes, 22, planes) ||
            !read_u16(bytes, 24, header.bits_per_pixel) ||
            width == 0 || height == 0)
        {
            return false;
        }
        header.core = true;
        header.width = width;
        header.signed_height = height;
        header.height = height;
        header.palette_offset = 26;
    }
    else
    {
        if (!range_fits(14, header.dib_size, bytes.size()) ||
            !read_s32(bytes, 18, header.width) ||
            !read_s32(bytes, 22, header.signed_height) ||
            !read_u16(bytes, 26, planes) ||
            !read_u16(bytes, 28, header.bits_per_pixel) ||
            !read_u32(bytes, 30, header.compression) ||
            !read_u32(bytes, 34, header.image_size) ||
            !read_u32(bytes, 46, header.colors_used) ||
            header.width <= 0 || header.signed_height == 0 ||
            header.signed_height == std::numeric_limits<std::int32_t>::min())
        {
            return false;
        }
        header.top_down = header.signed_height < 0;
        header.height = static_cast<std::uint32_t>(header.top_down
            ? -header.signed_height : header.signed_height);
        header.palette_offset = 14 + header.dib_size;

        if (header.compression == BI_BITFIELDS)
        {
            if (header.dib_size == 40)
            {
                if (!read_u32(bytes, header.palette_offset, header.red_mask) ||
                    !read_u32(bytes, header.palette_offset + 4, header.green_mask) ||
                    !read_u32(bytes, header.palette_offset + 8, header.blue_mask))
                {
                    return false;
                }
                header.palette_offset += 12;
            }
            else if (!read_u32(bytes, 54, header.red_mask) ||
                     !read_u32(bytes, 58, header.green_mask) ||
                     !read_u32(bytes, 62, header.blue_mask))
            {
                return false;
            }
            if (header.dib_size >= 56 && !read_u32(bytes, 66, header.alpha_mask))
            {
                return false;
            }
        }

        if (header.dib_size >= 108)
        {
            std::uint32_t color_space = 0;
            if (!read_u32(bytes, 70, color_space))
            {
                return false;
            }
            const bool calibrated_values = has_nonzero_bytes(bytes, 74, 48);
            header.color_metadata_lost = calibrated_values ||
                (color_space != 0 && color_space != LCS_SRGB);
            if (header.dib_size == 124)
            {
                std::uint32_t profile_data = 0;
                if (!read_u32(bytes, 126, profile_data) ||
                    !read_u32(bytes, 130, header.profile_size))
                {
                    return false;
                }
                const std::uint64_t profile_offset = 14ULL + profile_data;
                if (header.profile_size != 0 &&
                    (profile_offset < header.pixel_offset ||
                     profile_offset > bytes.size() ||
                     header.profile_size > bytes.size() - profile_offset))
                {
                    return false;
                }
                header.profile_offset = static_cast<std::uint32_t>(profile_offset);
                header.color_metadata_lost = header.color_metadata_lost ||
                    header.profile_size != 0;
            }
        }
    }

    const std::uint64_t pixel_count =
        static_cast<std::uint64_t>(header.width) * header.height;
    return planes == 1 && pixel_count <= MAX_DECODED_PIXELS &&
        valid_encoding(header) && header.palette_offset <= header.pixel_offset &&
        header.pixel_offset <= bytes.size();
}

bool read_palette(const std::vector<unsigned char> & bytes,
                  const Header & header, std::vector<Pixel> & palette)
{
    if (header.bits_per_pixel > 8)
    {
        return true;
    }
    const std::uint32_t maximum = 1U << header.bits_per_pixel;
    const std::uint32_t count = header.core || header.colors_used == 0
        ? maximum : header.colors_used;
    if (count == 0 || count > maximum)
    {
        return false;
    }
    const std::size_t entry_size = header.core ? 3 : 4;
    const std::uint64_t palette_bytes =
        static_cast<std::uint64_t>(count) * entry_size;
    if (palette_bytes > header.pixel_offset - header.palette_offset ||
        !range_fits(header.palette_offset, static_cast<std::size_t>(palette_bytes),
                    bytes.size()))
    {
        return false;
    }
    palette.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const std::size_t offset = header.palette_offset + index * entry_size;
        palette.push_back(Pixel(bytes[offset + 2], bytes[offset + 1],
                                bytes[offset]));
    }
    return true;
}

bool checked_stride(const Header & header, std::size_t & stride)
{
    const std::uint64_t row_bits =
        static_cast<std::uint64_t>(header.width) * header.bits_per_pixel;
    const std::uint64_t row_bytes = ((row_bits + 31) / 32) * 4;
    if (row_bytes > std::numeric_limits<std::size_t>::max())
    {
        return false;
    }
    stride = static_cast<std::size_t>(row_bytes);
    return true;
}

bool describe_mask(std::uint32_t mask, unsigned int stored_bits,
                   MaskInfo & info)
{
    if (mask == 0 || (stored_bits < 32 && (mask >> stored_bits) != 0))
    {
        return false;
    }
    unsigned int shift = 0;
    while (((mask >> shift) & 1U) == 0U)
    {
        ++shift;
    }
    unsigned int bits = 0;
    std::uint32_t shifted = mask >> shift;
    while ((shifted & 1U) != 0U)
    {
        ++bits;
        shifted >>= 1;
    }
    if (shifted != 0 || bits == 0)
    {
        return false;
    }
    info.mask = mask;
    info.shift = shift;
    info.bits = bits;
    info.maximum = bits == 32 ? 0xffffffffULL : ((1ULL << bits) - 1);
    return true;
}

int decode_component(std::uint32_t value, const MaskInfo & info)
{
    const std::uint64_t component = (value & info.mask) >> info.shift;
    return static_cast<int>((component * 255 + info.maximum / 2) /
                            info.maximum);
}

bool decode_uncompressed(const std::vector<unsigned char> & bytes,
                         const Header & header,
                         const std::vector<Pixel> & palette,
                         PixelMatrix & pixels, bool & lossy)
{
    std::size_t stride = 0;
    if (!checked_stride(header, stride))
    {
        return false;
    }
    const std::uint64_t data_size =
        static_cast<std::uint64_t>(stride) * header.height;
    if (data_size > bytes.size() - header.pixel_offset)
    {
        return false;
    }
    if (header.profile_size != 0 && header.profile_offset <
            header.pixel_offset + data_size)
    {
        return false;
    }

    MaskInfo red = {0, 0, 0, 0};
    MaskInfo green = {0, 0, 0, 0};
    MaskInfo blue = {0, 0, 0, 0};
    MaskInfo alpha = {0, 0, 0, 0};
    const bool bitfields = header.compression == BI_BITFIELDS;
    const bool has_alpha = bitfields && header.alpha_mask != 0;
    if (bitfields)
    {
        if ((header.red_mask & header.green_mask) != 0 ||
            (header.red_mask & header.blue_mask) != 0 ||
            (header.green_mask & header.blue_mask) != 0 ||
            !describe_mask(header.red_mask, header.bits_per_pixel, red) ||
            !describe_mask(header.green_mask, header.bits_per_pixel, green) ||
            !describe_mask(header.blue_mask, header.bits_per_pixel, blue) ||
            (has_alpha && ((header.alpha_mask & (header.red_mask |
                header.green_mask | header.blue_mask)) != 0 ||
                !describe_mask(header.alpha_mask, header.bits_per_pixel, alpha))))
        {
            return false;
        }
        lossy = lossy || red.bits > 8 || green.bits > 8 || blue.bits > 8;
    }

    pixels.assign(header.height,
        std::vector<Pixel>(static_cast<std::size_t>(header.width)));
    for (std::uint32_t stored_row = 0; stored_row < header.height; ++stored_row)
    {
        const std::size_t row_offset = header.pixel_offset +
            static_cast<std::size_t>(stored_row) * stride;
        const std::uint32_t target_row = header.top_down
            ? stored_row : header.height - 1 - stored_row;
        for (std::int32_t column = 0; column < header.width; ++column)
        {
            Pixel pixel;
            if (header.bits_per_pixel == 1 || header.bits_per_pixel == 4 ||
                header.bits_per_pixel == 8)
            {
                std::uint32_t palette_index = 0;
                if (header.bits_per_pixel == 1)
                {
                    palette_index = (bytes[row_offset + column / 8] >>
                        (7 - (column % 8))) & 1U;
                }
                else if (header.bits_per_pixel == 4)
                {
                    const unsigned char packed = bytes[row_offset + column / 2];
                    palette_index = column % 2 == 0 ? packed >> 4 : packed & 0xf;
                }
                else
                {
                    palette_index = bytes[row_offset + column];
                }
                if (palette_index >= palette.size())
                {
                    return false;
                }
                pixel = palette[palette_index];
            }
            else if (header.bits_per_pixel == 16)
            {
                const std::size_t offset = row_offset + column * 2;
                const std::uint32_t value = bytes[offset] |
                    (static_cast<std::uint32_t>(bytes[offset + 1]) << 8);
                if (bitfields)
                {
                    pixel = Pixel(decode_component(value, red),
                        decode_component(value, green),
                        decode_component(value, blue));
                }
                else
                {
                    pixel = Pixel(static_cast<int>((((value >> 10) & 0x1f) * 255 + 15) / 31),
                        static_cast<int>((((value >> 5) & 0x1f) * 255 + 15) / 31),
                        static_cast<int>(((value & 0x1f) * 255 + 15) / 31));
                }
            }
            else if (header.bits_per_pixel == 24)
            {
                const std::size_t offset = row_offset + column * 3;
                pixel = Pixel(bytes[offset + 2], bytes[offset + 1], bytes[offset]);
            }
            else
            {
                const std::size_t offset = row_offset + column * 4;
                const std::uint32_t value = bytes[offset] |
                    (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                    (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                    (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
                if (bitfields)
                {
                    pixel = Pixel(decode_component(value, red),
                        decode_component(value, green),
                        decode_component(value, blue));
                    if (has_alpha && ((value & alpha.mask) >> alpha.shift) !=
                            alpha.maximum)
                    {
                        lossy = true;
                    }
                }
                else
                {
                    pixel = Pixel(bytes[offset + 2], bytes[offset + 1],
                                  bytes[offset]);
                }
            }
            pixels[target_row][column] = pixel;
        }
    }
    return true;
}

bool decode_rle(const std::vector<unsigned char> & bytes,
                const Header & header, const std::vector<Pixel> & palette,
                PixelMatrix & pixels)
{
    std::size_t end = bytes.size();
    if (header.profile_size != 0)
    {
        end = header.profile_offset;
    }
    if (header.image_size != 0)
    {
        if (header.image_size > bytes.size() - header.pixel_offset)
        {
            return false;
        }
        const std::size_t image_end =
            static_cast<std::size_t>(header.pixel_offset) + header.image_size;
        if (image_end > end)
        {
            return false;
        }
        end = image_end;
    }
    pixels.assign(header.height,
        std::vector<Pixel>(header.width, palette[0]));
    std::size_t position = header.pixel_offset;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    bool complete = false;

    while (!complete && range_fits(position, 2, end))
    {
        const unsigned int count = bytes[position++];
        const unsigned int value = bytes[position++];
        if (count != 0)
        {
            if (y >= header.height || count > static_cast<std::uint32_t>(header.width) - x)
            {
                return false;
            }
            for (unsigned int index = 0; index < count; ++index)
            {
                const unsigned int palette_index = header.compression == BI_RLE8
                    ? value : (index % 2 == 0 ? value >> 4 : value & 0xf);
                if (palette_index >= palette.size())
                {
                    return false;
                }
                pixels[header.height - 1 - y][x++] = palette[palette_index];
            }
        }
        else if (value == 0)
        {
            x = 0;
            ++y;
            if (y > header.height)
            {
                return false;
            }
        }
        else if (value == 1)
        {
            complete = true;
        }
        else if (value == 2)
        {
            if (!range_fits(position, 2, end))
            {
                return false;
            }
            const std::uint32_t dx = bytes[position++];
            const std::uint32_t dy = bytes[position++];
            if (y >= header.height || dx > static_cast<std::uint32_t>(header.width) - x ||
                dy >= header.height - y)
            {
                return false;
            }
            x += dx;
            y += dy;
        }
        else
        {
            const unsigned int literal_count = value;
            const unsigned int data_bytes = header.compression == BI_RLE8
                ? literal_count : (literal_count + 1) / 2;
            const unsigned int padded_bytes = data_bytes + (data_bytes % 2);
            if (y >= header.height ||
                literal_count > static_cast<std::uint32_t>(header.width) - x ||
                !range_fits(position, padded_bytes, end))
            {
                return false;
            }
            for (unsigned int index = 0; index < literal_count; ++index)
            {
                const unsigned int palette_index = header.compression == BI_RLE8
                    ? bytes[position + index]
                    : (index % 2 == 0 ? bytes[position + index / 2] >> 4
                                      : bytes[position + index / 2] & 0xf);
                if (palette_index >= palette.size())
                {
                    return false;
                }
                pixels[header.height - 1 - y][x++] = palette[palette_index];
            }
            position += padded_bytes;
        }
    }
    if (!complete)
    {
        return false;
    }

    return true;
}

bool decode_bitmap(const std::vector<unsigned char> & bytes,
                   PixelMatrix & pixels, bool & lossy)
{
    try
    {
        Header header;
        std::vector<Pixel> palette;
        if (!parse_header(bytes, header) ||
            !read_palette(bytes, header, palette))
        {
            return false;
        }
        lossy = header.color_metadata_lost;
        if (header.compression == BI_RLE4 || header.compression == BI_RLE8)
        {
            return decode_rle(bytes, header, palette, pixels);
        }
        return decode_uncompressed(bytes, header, palette, pixels, lossy);
    }
    catch (const std::bad_alloc &)
    {
        pixels.clear();
        lossy = false;
        return false;
    }
}

void write_u16(std::ostream & output, std::uint16_t value)
{
    output.put(static_cast<char>(value & 0xff));
    output.put(static_cast<char>((value >> 8) & 0xff));
}

void write_u32(std::ostream & output, std::uint32_t value)
{
    output.put(static_cast<char>(value & 0xff));
    output.put(static_cast<char>((value >> 8) & 0xff));
    output.put(static_cast<char>((value >> 16) & 0xff));
    output.put(static_cast<char>((value >> 24) & 0xff));
}
}

void Bitmap::open(std::string filename)
{
    pixels.clear();
    lossy = false;

    std::vector<unsigned char> bytes;
    PixelMatrix decoded_pixels;
    bool decoded_lossy = false;
    if (!load_file(filename, bytes))
    {
        std::cerr << filename << " could not be opened.\n";
    }
    else if (!decode_bitmap(bytes, decoded_pixels, decoded_lossy))
    {
        std::cerr << filename << " is not a supported, valid BMP file.\n";
    }
    else
    {
        pixels.swap(decoded_pixels);
        lossy = decoded_lossy;
    }
}

void Bitmap::save(std::string filename)
{
    if (!isImage())
    {
        std::cerr << "Bitmap cannot be saved. It is not a valid image.\n";
        return;
    }

    const std::uint64_t width = pixels[0].size();
    const std::uint64_t height = pixels.size();
    if (width > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) ||
        height > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
    {
        std::cerr << "Bitmap cannot be saved because it is too large.\n";
        return;
    }
    const std::uint64_t stride = ((width * 24 + 31) / 32) * 4;
    const std::uint64_t image_size = stride * height;
    const std::uint64_t file_size = 54 + image_size;
    if (image_size > std::numeric_limits<std::uint32_t>::max() ||
        file_size > std::numeric_limits<std::uint32_t>::max())
    {
        std::cerr << "Bitmap cannot be saved because it is too large.\n";
        return;
    }

    std::ofstream file(filename.c_str(), std::ios::out | std::ios::binary);
    if (!file)
    {
        std::cerr << filename << " could not be opened for editing.\n";
        return;
    }

    file.put('B');
    file.put('M');
    write_u32(file, static_cast<std::uint32_t>(file_size));
    write_u16(file, 0);
    write_u16(file, 0);
    write_u32(file, 54);
    write_u32(file, 40);
    write_u32(file, static_cast<std::uint32_t>(width));
    write_u32(file, static_cast<std::uint32_t>(height));
    write_u16(file, 1);
    write_u16(file, 24);
    write_u32(file, BI_RGB);
    write_u32(file, static_cast<std::uint32_t>(image_size));
    write_u32(file, 2835);
    write_u32(file, 2835);
    write_u32(file, 0);
    write_u32(file, 0);

    const std::size_t padding = static_cast<std::size_t>(stride - width * 3);
    for (std::size_t stored_row = 0; stored_row < height; ++stored_row)
    {
        const std::vector<Pixel> & row = pixels[height - 1 - stored_row];
        for (std::size_t column = 0; column < row.size(); ++column)
        {
            file.put(static_cast<char>(row[column].blue));
            file.put(static_cast<char>(row[column].green));
            file.put(static_cast<char>(row[column].red));
        }
        for (std::size_t index = 0; index < padding; ++index)
        {
            file.put(0);
        }
    }
    if (!file)
    {
        std::cerr << filename << " could not be written completely.\n";
    }
}

bool Bitmap::isImage()
{
    if (pixels.empty() || pixels[0].empty())
    {
        return false;
    }
    const std::size_t width = pixels[0].size();
    for (std::size_t row = 0; row < pixels.size(); ++row)
    {
        if (pixels[row].size() != width)
        {
            return false;
        }
        for (std::size_t column = 0; column < width; ++column)
        {
            const Pixel & current = pixels[row][column];
            if (current.red > MAX_RGB || current.red < MIN_RGB ||
                current.green > MAX_RGB || current.green < MIN_RGB ||
                current.blue > MAX_RGB || current.blue < MIN_RGB)
            {
                return false;
            }
        }
    }
    return true;
}

bool Bitmap::isLossy()
{
    return lossy;
}

PixelMatrix Bitmap::toPixelMatrix()
{
    return isImage() ? pixels : PixelMatrix();
}

void Bitmap::fromPixelMatrix(const PixelMatrix & values)
{
    pixels = values;
    lossy = false;
}
