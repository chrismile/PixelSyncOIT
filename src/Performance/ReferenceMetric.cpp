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
