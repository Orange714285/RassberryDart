#include "one_euro_filter.hpp"

OneEuroFilter::OneEuroFilter(double x0,
                              double d_x0,
                              double min_cut_off,
                              double beta,
                              double d_cut_off)
    : x_pre_(x0),
      d_x_pre_(d_x0),
      min_cut_off_(min_cut_off),
      beta_(beta),
      d_cut_off_(d_cut_off) {}

void OneEuroFilter::setParameter(double x0,
                                  double d_x0,
                                  double min_cut_off,
                                  double beta,
                                  double d_cut_off)
{
    x_pre_ = x0;
    d_x_pre_ = d_x0;
    min_cut_off_ = min_cut_off;
    beta_ = beta;
    d_cut_off_ = d_cut_off;
}

double OneEuroFilter::filter(double x, double dt)
{
    // 防止 dt 为 0 或过小导致除零/爆炸
    if (dt <= 1e-6) dt = 1e-6;

    const double a_d = smoothingFactor(dt, d_cut_off_);
    const double dx = (x - x_pre_) / dt;
    const double dx_hat = exponentialSmoothing(dx, d_x_pre_, a_d);

    const double cut_off = min_cut_off_ + beta_ * std::fabs(dx_hat);
    const double alpha = smoothingFactor(dt, cut_off);
    const double x_hat = exponentialSmoothing(x, x_pre_, alpha);

    x_pre_ = x_hat;
    d_x_pre_ = dx_hat;
    return x_hat;
}

double OneEuroFilter::smoothingFactor(double t_e, double cut_off)
{
    const double r = 2.0 * M_PI * cut_off * t_e;
    return r / (r + 1.0);
}

double OneEuroFilter::exponentialSmoothing(double x, double prev_x, double alpha)
{
    return alpha * x + (1.0 - alpha) * prev_x;
}

