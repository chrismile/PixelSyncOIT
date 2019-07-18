//
// Created by christoph on 30.09.18.
//

#ifndef PIXELSYNCOIT_REFERENCEMETRIC_HPP
#define PIXELSYNCOIT_REFERENCEMETRIC_HPP

#include <Graphics/Texture/Bitmap.hpp>

/// Returns mean squared error (RMSE)
double mse(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed);

/// Returns root mean squared error (RMSE)
double rmse(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed);

/**
 * Returns the structural similarity index (SSIM).
 *
 * Wang, Z., Bovik, A. C., Sheikh, H. R., and Simoncelli, E. P. 2004. Image Quality Assessment:
 * From Error Visibility to Structural Similarity. Trans. Img. Proc. 13, 4 (2004), 600–612.
 */
double ssim(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed);

/**
 * Returns an structural similarity index (SSIM) difference image for the specified kernel size.
 *
 * Wang, Z., Bovik, A. C., Sheikh, H. R., and Simoncelli, E. P. 2004. Image Quality Assessment:
 * From Error Visibility to Structural Similarity. Trans. Img. Proc. 13, 4 (2004), 600–612.
 */
sgl::BitmapPtr ssimDifferenceImage(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed, int kernelSize);

/// Returns peak signal-to-noise ratio (PSNR, in dB)
double psnr(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed);

/// Computes the difference map between the two passed bitmaps. The map is normalized so that the differences
/// assume the total interval of [0,255] in the image.
sgl::BitmapPtr computeNormalizedDifferenceMap(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed);

#endif //PIXELSYNCOIT_REFERENCEMETRIC_HPP
