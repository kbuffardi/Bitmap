#ifndef BITMAP_H
#define BITMAP_H

#include <string>
#include <vector>

// ----------------------------------------------------------------------------
/**
 * Represents a single Pixel in the image. A Pixel has red, green, and blue
 * components that are mixed to form a color. Each of these values can range
 * from 0 to 255
**/
class Pixel
{
public:
	// Stores the individual color components.
	int red, green, blue;

	// Initializes a Pixel with a default black color.
	Pixel() : red(0), green(0), blue(0) { }

	// Initializes a color Pixel with the specified RGB values.
	Pixel(int r, int g, int b) : red(r), green(g), blue(b) { }
};

// ----------------------------------------------------------------------------
//To abbreviate a pixel matrix built as a vector of vectors
typedef std::vector < std::vector <Pixel> > PixelMatrix;

// ----------------------------------------------------------------------------
/**
 * Represents a bitmap where a grid of pixels (in row-major order) describes
 * the color of each pixel. Common Windows BMP depths and encodings are read
 * into 8-bit red, green, and blue components. Images are saved as 24-bpp BMP.
**/
class Bitmap
{
  private:
    PixelMatrix pixels;
    bool lossy = false;

  public:
    /**
     * Opens a supported Windows BMP and converts it to a matrix of RGB pixels.
     * Any errors are written to cerr and result in an empty matrix.
     *
     * @param name of the filename to be opened and read as a matrix of pixels
    **/
    void open(std::string);

    /**
     * Saves the current image, represented by the matrix of pixels, as a
     * Windows BMP file with the name provided by the parameter. File extension
     * is not forced but should be .bmp. Any errors are written to cerr and do
     * not change the current matrix or its lossiness state. The output format
     * is always uncompressed 24-bpp BMP.
     *
     * @param name of the filename to be written as a bmp image
    **/
    void save(std::string);

    /**
     * Validates whether or not the current matrix of pixels represents a
     * proper image with non-zero-size rows and consistent non-zero-size
     * columns for each row. In addition, each pixel in the matrix is validated
     * to have red, green, and blue components with values between 0 and 255
     *
     * @return boolean value of whether or not the matrix is a valid image
    **/
    bool isImage();

    /**
     * Reports whether opening the current image discarded color information
     * or precision while converting it to RGB pixels. Spatial dimensions are
     * never resampled. See README.md for the complete lossiness rules.
     *
     * @return true only when the most recent successful open was lossy
    **/
    bool isLossy();

    /**
     * Provides a vector of vector of pixels representing the bitmap
     *
     * @return the bitmap image, represented by a matrix of RGB pixels
    **/
    PixelMatrix toPixelMatrix();

    /**
     * Overwrites the current bitmap with that represented by a matrix of
     * pixels. Does not validate that the new matrix of pixels is a proper
     * image.
     *
     * @param a matrix of pixels to represent a bitmap
    **/
    void fromPixelMatrix(const PixelMatrix &);

};

#endif
