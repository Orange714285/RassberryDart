#pragma once
#include <cmath>

class OneEuroFilter {
public:
    OneEuroFilter(double x0,
                  double d_x0,
                  double min_cut_off,
                  double beta,
                  double d_cut_off);

    OneEuroFilter() = default;
    ~OneEuroFilter() = default;

    void setParameter(double x0,
                      double d_x0,
                      double min_cut_off,
                      double beta,
                      double d_cut_off);

    // dt: seconds (建议传秒)
    double filter(double x, double dt);

private:
    static double smoothingFactor(double t_e, double cut_off);
    static double exponentialSmoothing(double x, double prev_x, double alpha);

    double x_pre_ = 0.0;
    double d_x_pre_ = 0.0;
    double min_cut_off_ = 0.5;
    double beta_ = 0.0;
    double d_cut_off_ = 1.0;
};

 

