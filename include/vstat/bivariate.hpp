// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright 2020-2021 Heal Research

#ifndef VSTAT_CORRELATION_HPP
#define VSTAT_CORRELATION_HPP

#include "combine.hpp"

namespace VSTAT_NAMESPACE {

template <typename T>
struct bivariate_accumulator {
    bivariate_accumulator(T x, T y, T w) // NOLINT
        : sum_w{w}
        , sum_x{x}
        , sum_y{y}
        , sum_xx{0}
        , sum_yy{0}
        , sum_xy{0}
    {
    }

    bivariate_accumulator(T x, T y)
        : bivariate_accumulator(x, y, T { 1.0 })
    {
    }

    static auto load_state(T sx, T sy, T sw, T sxx, T syy, T sxy) -> bivariate_accumulator<T> // NOLINT
    {
        bivariate_accumulator<T> acc(T { 0 }, T { 0 }, T { 0 });
        acc.sum_w = sw;
        acc.sum_x = sx;
        acc.sum_y = sy;
        acc.sum_xx = sxx;
        acc.sum_yy = syy;
        acc.sum_xy = sxy;
        return acc;
    }

    inline void operator()(T x, T y)
    {
        T dx = x * sum_w - sum_x;
        T dy = y * sum_w - sum_y;

        auto sum_w_old = sum_w;
        sum_w += 1;

        T f = 1. / (sum_w * sum_w_old);
        sum_xx += f * dx * dx;
        sum_yy += f * dy * dy;
        sum_xy += f * dx * dy;

        sum_x += x;
        sum_y += y;
    }

    inline void operator()(T x, T y, T w) // NOLINT
    {
        T dx = x * sum_w - sum_x;
        T dy = y * sum_w - sum_y;

        sum_x += x * w;
        sum_y += y * w;
        auto sum_w_old = sum_w;
        sum_w += w;

        T f = w / (sum_w * sum_w_old);
        sum_xx += f * dx * dx;
        sum_yy += f * dy * dy;
        sum_xy += f * dx * dy;
    }

    template <typename U>
    requires eve::simd_value<T> && eve::simd_compatible_ptr<U, T>
    inline void operator()(U const* x, U const* y)
    {
        (*this)(T{x}, T{y});
    }

    template <typename U>
    requires eve::simd_value<T> && eve::simd_compatible_ptr<U, T>
    inline void operator()(U const* x, U const* y, U const* w)
    {
        (*this)(T{x}, T{y}, T{w});
    }

    // performs a reduction on the vector types and returns the sums and the squared residuals sums
    auto stats() -> std::tuple<double, double, double, double, double, double>
    {
        if constexpr (std::is_floating_point_v<T>) {
            return { sum_w, sum_x, sum_y, sum_xx, sum_yy, sum_xy };
        } else {
            auto [sxx, syy, sxy] = combine(sum_w, sum_x, sum_y, sum_xx, sum_yy, sum_xy);
            return { eve::reduce(sum_w), eve::reduce(sum_x), eve::reduce(sum_y), sxx, syy, sxy };
        }
    }

    // sum of weights
    T sum_w;
    // means
    T sum_x;
    T sum_y;
    // squared residuals
    T sum_xx;
    T sum_yy;
    T sum_xy;
};

struct bivariate_statistics {
    double count;
    double sum_x;
    double sum_y;
    double ssr_x;
    double ssr_y;
    double sum_xy;
    double mean_x;
    double mean_y;
    double variance_x;
    double variance_y;
    double sample_variance_x;
    double sample_variance_y;
    double correlation;
    double covariance;
    double sample_covariance;

    template <typename T>
    explicit bivariate_statistics(T accumulator)
    {
        auto [sw, sx, sy, sxx, syy, sxy] = accumulator.stats();
        count = sw;
        sum_x = sx;
        sum_y = sy;
        ssr_x = sxx;
        ssr_y = syy;
        sum_xy = sxy;
        mean_x = sx / sw;
        mean_y = sy / sw;
        variance_x = sxx / sw;
        variance_y = syy / sw;
        sample_variance_x = sxx / (sw - 1);
        sample_variance_y = syy / (sw - 1);

        if (!(sxx > 0 && syy > 0)) {
            correlation = sxx == syy ? 1 : 0;
        } else {
            correlation = sxy / std::sqrt(sxx * syy);
        }

        covariance = sxy / sw;
        sample_covariance = sxy / (sw - 1);
    }
};

inline auto operator<<(std::ostream& os, bivariate_statistics const& stats) -> std::ostream&
{
    os << "count:              \t" << stats.count
       << "\nsum_x:            \t" << stats.sum_x
       << "\nssr_x:            \t" << stats.ssr_x
       << "\nmean_x:           \t" << stats.mean_x
       << "\nvariance_x:       \t" << stats.variance_x
       << "\nsample variance_x:\t" << stats.sample_variance_x
       << "\nsum_y:            \t" << stats.sum_y
       << "\nssr_y:            \t" << stats.ssr_y
       << "\nmean_y:           \t" << stats.mean_y
       << "\nvariance_y:       \t" << stats.variance_y
       << "\nsample variance_y:\t" << stats.sample_variance_y
       << "\ncorrelation:      \t" << stats.correlation
       << "\ncovariance:       \t" << stats.covariance
       << "\nsample covariance:\t" << stats.sample_covariance
       << "\n";
    return os;
}

} // namespace VSTAT_NAMESPACE

#endif
