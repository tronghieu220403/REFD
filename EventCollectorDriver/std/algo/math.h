#ifndef MATH_H
#define MATH_H

#ifndef FLTUSED
extern "C" int _fltused = 1;
#define FLTUSED
#endif // !FLTUSED

namespace math
{
    inline int ExtractExponent(double x)
    {
        union {
            double d;
            unsigned long long bits;
        } v;

        v.d = x;
        return int((v.bits >> 52) & 0x7FF) - 1023;
    }

    inline double NormalizeMantissaHalf(double x)
    {
        union {
            double d;
            unsigned long long bits;
        } v;

        v.d = x;

        // set exponent = 1022 -> range [0.5,1)
        v.bits = (v.bits & ((1ULL << 52) - 1)) | (1022ULL << 52);
        return v.d;
    }

    inline double LnMantissaTaylor(double m, int degree = 7)
    {
        // m ∈ [0.5, 1)
        double y = m - 1.0;   // y ∈ [-0.5, 0)
        double term = y;
        double sum = 0.0;

        for (int k = 1; k <= degree; ++k) {
            double coeff = ((k & 1) ? 1.0 : -1.0) / k;
            sum += coeff * term;
            term *= y;
        }

        return sum;
    }

    double Log2(double x)
    {
        if (x <= 0.0)
            return -9999999.0;

        int exp = ExtractExponent(x) + 1;   // +1 because mantissa ∈ [0.5,1)
        double m = NormalizeMantissaHalf(x);

        constexpr double INV_LN2 = 1.4426950408889634;

        return double(exp) + LnMantissaTaylor(m) * INV_LN2;
    }
}

#endif