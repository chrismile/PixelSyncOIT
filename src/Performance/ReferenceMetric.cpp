//
// Created by christoph on 30.09.18.
//

#include <cmath>
#include <functional>
#include <iostream>

#include "TransferFunctionWindow.hpp"
#include "ReferenceMetric.hpp"

inline double sqr(double v) {
    return v*v;
}


double mse(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    int N = expected->getW() * expected->getH() * expected->getChannels();
    double sum = 0.0;
    #pragma omp parallel for reduction(+: sum)
    for (int i = 0; i < N; i++) {
        double diff = expected->getPixels()[i] - observed->getPixels()[i];
        sum += diff * diff;
    }
    return sum / N;
}


double rmse(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    return std::sqrt(mse(expected, observed));
}


double mean(std::function<double(int)> elementGetter, int N)
{
    double sum = 0.0;
    #pragma omp parallel for reduction(+: sum)
    for (int i = 0; i < N; i++) {
        sum += elementGetter(i);
    }
    return sum/N;
}

double ssim(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    int inputW = expected->getW();
    int inputH = expected->getH();

    // Compute the luminance values.
    double *expectedLuminance = new double[inputW*inputH];
    double *observedLuminance = new double[inputW*inputH];
    #pragma omp parallel
    for (int y = 0; y < inputH; y++) {
        for (int x = 0; x < inputW; x++) {
            glm::vec3 sRGBColorExpected = expected->getPixelColor(x, y).getFloatColorRGB();
            glm::vec3 linearRGBColorExpected = TransferFunctionWindow::sRGBToLinearRGB(sRGBColorExpected);
            glm::vec3 sRGBColorObserved = observed->getPixelColor(x, y).getFloatColorRGB();
            glm::vec3 linearRGBColorObserved = TransferFunctionWindow::sRGBToLinearRGB(sRGBColorObserved);
            expectedLuminance[x + y*inputW] =
                    255.0 * (0.2126*linearRGBColorExpected.r
                    + 0.7152*linearRGBColorExpected.g
                    + 0.0722*linearRGBColorExpected.b);
            observedLuminance[x + y*inputW] =
                    255.0 * (0.2126*linearRGBColorObserved.r
                    + 0.7152*linearRGBColorObserved.g
                    + 0.0722*linearRGBColorObserved.b);
        }
    }

    // Constants of the algorithm
    const double k1 = 0.01;
    const double k2 = 0.03;
    const double L = 255.0; // Max. range
    const double c1 = sqr(k1 * L);
    const double c2 = sqr(k2 * L);

    int N = inputW * inputH;
    double *X = expectedLuminance;
    double *Y = observedLuminance;

    double mu_x = mean([X](int i) -> double { return X[i]; }, N);
    double mu_y = mean([Y](int i) -> double { return Y[i]; }, N);
    double rho2_x = mean([X,mu_x](int i) -> double { return sqr(X[i] - mu_x); }, N);
    double rho2_y = mean([Y,mu_y](int i) -> double { return sqr(Y[i] - mu_y); }, N);
    double rho_x = std::sqrt(rho2_x);
    double rho_y = std::sqrt(rho2_y);
    // Compute the covariance
    double rho_xy = mean([X,mu_x,Y,mu_y](int i) -> double { return (X[i] - mu_x)*(Y[i] - mu_y); }, N);

    delete[] expectedLuminance;
    delete[] observedLuminance;

    return ((2.0 * mu_x * mu_y + c1) * (2.0 * rho_xy + c2))
           / ((mu_x*mu_x + mu_y*mu_y + c1) * (rho2_x + rho2_y + c2));
}

sgl::BitmapPtr ssimDifferenceImage(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed, int kernelSize)
{
    assert(expected->getW() % kernelSize == 0 && expected->getH() % kernelSize == 0);
    int inputW = expected->getW();
    int inputH = expected->getH();
    int diffImgW = inputW / kernelSize;
    int diffImgH = inputH / kernelSize;
    int N = kernelSize * kernelSize;

    double *expectedLuminance = new double[inputW*inputH];
    double *observedLuminance = new double[inputW*inputH];
    #pragma omp parallel
    for (int y = 0; y < inputH; y++) {
        for (int x = 0; x < inputW; x++) {
            glm::vec3 sRGBColorExpected = expected->getPixelColor(x, y).getFloatColorRGB();
            glm::vec3 linearRGBColorExpected = TransferFunctionWindow::sRGBToLinearRGB(sRGBColorExpected);
            glm::vec3 sRGBColorObserved = observed->getPixelColor(x, y).getFloatColorRGB();
            glm::vec3 linearRGBColorObserved = TransferFunctionWindow::sRGBToLinearRGB(sRGBColorObserved);
            expectedLuminance[x + y*inputW] =
                    255.0 * (0.2126*linearRGBColorExpected.r
                    + 0.7152*linearRGBColorExpected.g
                    + 0.0722*linearRGBColorExpected.b);
            observedLuminance[x + y*inputW] =
                    255.0 * (0.2126*linearRGBColorObserved.r
                    + 0.7152*linearRGBColorObserved.g
                    + 0.0722*linearRGBColorObserved.b);
        }
    }

    double *ssimValues = new double[diffImgW * diffImgH];

    #pragma omp parallel
    for (int y = 0; y < diffImgH; y++) {
        for (int x = 0; x < diffImgW; x++) {
            // Constants of the algorithm
            const double k1 = 0.01;
            const double k2 = 0.03;
            const double L = 255.0; // Max. range
            const double c1 = sqr(k1 * L);
            const double c2 = sqr(k2 * L);

            double *X = expectedLuminance;
            double *Y = observedLuminance;
            int xc = x * kernelSize;
            int yc = y * kernelSize;

            double mu_x = mean([&](int i) -> double {
                int xi = xc + i % kernelSize;
                int yi = yc + i / kernelSize;
                return X[yi*inputW + xi];
            }, N);
            double mu_y = mean([&](int i) -> double {
                int xi = xc + i % kernelSize;
                int yi = yc + i / kernelSize;
                return Y[yi*inputW + xi];
            }, N);
            double rho2_x = mean([&](int i) -> double {
                int xi = xc + i % kernelSize;
                int yi = yc + i / kernelSize;
                return sqr(X[yi*inputW + xi] - mu_x);
            }, N);
            double rho2_y = mean([&](int i) -> double {
                int xi = xc + i % kernelSize;
                int yi = yc + i / kernelSize;
                return sqr(Y[yi*inputW + xi] - mu_y);
            }, N);

            double rho_x = std::sqrt(rho2_x);
            double rho_y = std::sqrt(rho2_y);
            // Compute the covariance
            double rho_xy = mean([&](int i) -> double {
                int xi = xc + i % kernelSize;
                int yi = yc + i / kernelSize;
                return (X[yi*inputW + xi] - mu_x)*(Y[yi*inputW + xi] - mu_y);
            }, N);

            ssimValues[y*diffImgW + x] = ((2.0 * mu_x * mu_y + c1) * (2.0 * rho_xy + c2))
                   / ((mu_x*mu_x + mu_y*mu_y + c1) * (rho2_x + rho2_y + c2));

        }
    }

    // Compute minimum and maximum of the SSIM values generated.
    double minSSIMValue = 1.0, maxSSIMValue = -1.0;
    #pragma omp parallel for reduction(min: minSSIMValue) reduction(max: maxSSIMValue)
    for (int y = 0; y < diffImgH; y++) {
        for (int x = 0; x < diffImgW; x++) {
            double ssimValue = ssimValues[x + y*diffImgW];
            minSSIMValue = std::min(minSSIMValue, ssimValue);
            maxSSIMValue = std::max(maxSSIMValue, ssimValue);
        }
    }


    // Normalization step.
    sgl::BitmapPtr differenceMap(new sgl::Bitmap);
    differenceMap->allocate(diffImgW, diffImgH, 32);
    #pragma omp parallel
    for (int y = 0; y < diffImgH; y++) {
        for (int x = 0; x < diffImgW; x++) {
            double ssimValue = ssimValues[x + y*diffImgW];

            double normalizedGrayscaleValue = 0.0;
            if (maxSSIMValue - minSSIMValue > 0.000001) {
                normalizedGrayscaleValue = 1.0 - (ssimValue - minSSIMValue) / (maxSSIMValue - minSSIMValue);
            }

            sgl::colorFromFloat(normalizedGrayscaleValue, normalizedGrayscaleValue, normalizedGrayscaleValue, 1.0f);
            differenceMap->setPixelColor(x, y, sgl::colorFromFloat(
                    normalizedGrayscaleValue, normalizedGrayscaleValue, normalizedGrayscaleValue, 1.0f));
        }
    }

    delete[] ssimValues;
    delete[] expectedLuminance;
    delete[] observedLuminance;

    return differenceMap;
}


double psnr(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    int N = expected->getW() * expected->getH() * expected->getChannels();

    uint8_t max_I = 0;
    #pragma omp parallel for reduction(max: max_I)
    for (int i = 0; i < N; i++) {
        max_I = std::max(max_I, expected->getPixels()[i]);
    }
    return 10 * std::log10(max_I * max_I / mse(expected, observed));
}


sgl::BitmapPtr computeNormalizedDifferenceMapRGBDiff(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    int numChannels = expected->getChannels();
    int imageSize = expected->getW() * expected->getH();
    int N = numChannels * imageSize;
    sgl::BitmapPtr differenceMap(new sgl::Bitmap);
    differenceMap->allocate(expected->getW(), expected->getH(), 32);

    // Compute maximum difference for all pixel color channels
    int *differences = new int[N];
    int maxDifferenceRGB = 0;
    int maxDifferenceAlpha = 0;
    #pragma omp parallel for reduction(max: maxDifferenceRGB) reduction(max: maxDifferenceAlpha)
    for (int i = 0; i < N; i++) {
        differences[i] = std::abs(
                static_cast<int>(expected->getPixels()[i])
                - static_cast<int>(observed->getPixels()[i]));
        if (i % 4 == 3) {
            maxDifferenceAlpha = std::max(maxDifferenceAlpha, differences[i]);
        } else {
            maxDifferenceRGB = std::max(maxDifferenceRGB, differences[i]);
        }
    }

    // Normalize the difference map
    if (maxDifferenceRGB >= 1 /*&& maxDifferenceAlpha >= 1*/) {
        #pragma omp parallel for
        for (int i = 0; i < N; i++) {
            if (i % 4 == 3) {
                //differenceMap->getPixels()[i] = static_cast<double>(differences[i])
                //                                / static_cast<double>(maxDifferenceAlpha) * 255.0;
                differenceMap->getPixels()[i] = 255;
            } else {
                //differenceMap->getPixels()[i] = static_cast<double>(differences[i])
                //                                / static_cast<double>(maxDifferenceRGB) * 255.0;
                differenceMap->getPixels()[i] = static_cast<double>(differences[i]);
            }
        }
    } else {
        #pragma omp parallel for
        for (int i = 0; i < N; i++) {
            if (i % 4 == 3) {
                differenceMap->getPixels()[i] = 255;
            } else {
                differenceMap->getPixels()[i] = 0;
            }
        }
    }
    delete[] differences;

    return differenceMap;
}

sgl::BitmapPtr computeNormalizedDifferenceMapWhiteNorm(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    int numChannels = expected->getChannels();
    int imageSize = expected->getW() * expected->getH();
    int N = numChannels * imageSize;
    sgl::BitmapPtr differenceMap(new sgl::Bitmap);
    differenceMap->allocate(expected->getW(), expected->getH(), 32);

    // Compute maximum difference for all pixel color channels
    float *l2normDifferences = new float[imageSize];
    float maxNormValue = 0.0f;
    for (int i = 0; i < imageSize; i++) {
        l2normDifferences[i] = 0.0f;
        for (int j = 0; j < numChannels; j++) {
            l2normDifferences[i] += std::abs(
                    static_cast<int>(expected->getPixels()[i*numChannels+j])
                    - static_cast<int>(observed->getPixels()[i*numChannels+j]));
        }
        l2normDifferences[i] = sqrt(l2normDifferences[i]);
        maxNormValue = std::max(maxNormValue, l2normDifferences[i]);
    }

    // Normalize the difference map
    if (maxNormValue >= 1) {
        for (int i = 0; i < imageSize; i++) {
            int pixelValue = 255.0f - l2normDifferences[i] / maxNormValue * 255.0f;
            differenceMap->getPixels()[i*4+0] = pixelValue;
            differenceMap->getPixels()[i*4+1] = pixelValue;
            differenceMap->getPixels()[i*4+2] = pixelValue;
            differenceMap->getPixels()[i*4+3] = 255;
        }
    } else {
        differenceMap->fill(sgl::Color(255, 255, 255, 255));
    }
    delete[] l2normDifferences;

    return differenceMap;
}

sgl::BitmapPtr computeNormalizedDifferenceMapNormBlack(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    int numChannels = expected->getChannels();
    int imageSize = expected->getW() * expected->getH();
    int N = numChannels * imageSize;
    sgl::BitmapPtr differenceMap(new sgl::Bitmap);
    differenceMap->allocate(expected->getW(), expected->getH(), 32);

    // Compute maximum difference for all pixel color channels
    float *l2normDifferences = new float[imageSize];
    float maxNormValue = 0.0f;
    for (int i = 0; i < imageSize; i++) {
        l2normDifferences[i] = 0.0f;
        for (int j = 0; j < numChannels; j++) {
            l2normDifferences[i] += std::abs(
                    static_cast<int>(expected->getPixels()[i*numChannels+j])
                    - static_cast<int>(observed->getPixels()[i*numChannels+j]));
        }
        l2normDifferences[i] = sqrt(l2normDifferences[i]);
        maxNormValue = std::max(maxNormValue, l2normDifferences[i]);
    }

    // Normalize the difference map
    if (maxNormValue >= 1) {
        for (int i = 0; i < imageSize; i++) {
            int pixelValue = l2normDifferences[i] / maxNormValue * 255.0f;
            differenceMap->getPixels()[i*4+0] = pixelValue;
            differenceMap->getPixels()[i*4+1] = pixelValue;
            differenceMap->getPixels()[i*4+2] = pixelValue;
            differenceMap->getPixels()[i*4+3] = 255;
        }
    } else {
        differenceMap->fill(sgl::Color(0, 0, 0, 255));
    }
    delete[] l2normDifferences;

    return differenceMap;
}

sgl::BitmapPtr computeNormalizedDifferenceMap(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    return computeNormalizedDifferenceMapRGBDiff(expected, observed);
}
