//
// Created by christoph on 30.09.18.
//

#include <cmath>
#include <functional>

#include "ReferenceMetric.hpp"

inline double sqr(double v) {
    return v*v;
}


double mse(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    int N = expected->getW() * expected->getH() * expected->getChannels();
    double sum = 0.0;
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
    for (int i = 0; i < N; i++) {
        sum += elementGetter(i);
    }
    return sum/N;
}

double ssim(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    // Constants of the algorithm
    const double k1 = 0.01;
    const double k2 = 0.03;
    const double L = 255.0; // Max. range
    const double c1 = sqr(k1 * L);
    const double c2 = sqr(k2 * L);

    int N = expected->getW() * expected->getH() * expected->getChannels();
    uint8_t *X = expected->getPixels();
    uint8_t *Y = observed->getPixels();

    double mu_x = mean([X](int i) -> double { return X[i]; }, N);
    double mu_y = mean([Y](int i) -> double { return Y[i]; }, N);
    double rho2_x = mean([X,mu_x](int i) -> double { return sqr(X[i] - mu_x); }, N);
    double rho2_y = mean([Y,mu_y](int i) -> double { return sqr(Y[i] - mu_y); }, N);
    double rho_x = std::sqrt(rho2_x);
    double rho_y = std::sqrt(rho2_y);
    // Compute the covariance
    double rho_xy = mean([X,mu_x,Y,mu_y](int i) -> double { return (X[i] - mu_x)*(Y[i] - mu_y); }, N);

    return ((2.0 * mu_x * mu_y + c1) * (2.0 * rho_xy + c2))
           / ((mu_x*mu_x + mu_y*mu_y + c1) * (rho2_x + rho2_y + c2));
}


double psnr(const sgl::BitmapPtr &expected, const sgl::BitmapPtr &observed)
{
    int N = expected->getW() * expected->getH() * expected->getChannels();

    uint8_t max_I = 0;
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
    if (maxDifferenceRGB >= 1 && maxDifferenceAlpha >= 1) {
        for (int i = 0; i < N; i++) {
            if (i % 4 == 3) {
                //differenceMap->getPixels()[i] = static_cast<double>(differences[i])
                //                                / static_cast<double>(maxDifferenceAlpha) * 255.0;
                differenceMap->getPixels()[i] = 255;
            } else {
                differenceMap->getPixels()[i] = static_cast<double>(differences[i])
                                                / static_cast<double>(maxDifferenceRGB) * 255.0;
            }
        }
    } else {
        differenceMap->fill(sgl::Color(0, 0, 0, 0));
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