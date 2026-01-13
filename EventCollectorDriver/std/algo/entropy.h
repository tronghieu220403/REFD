#ifndef ENTROPY_H
#define ENTROPY_H

#ifndef FLTUSED
extern "C" int _fltused = 1;
#define FLTUSED
#endif // !FLTUSED

#include "math.h"

namespace math {

    // Compute Shannon byte entropy from frequency table
    // freq   : array[256] of byte frequencies
    // total  : total number of bytes (sum of freq)
    // return : entropy in bits [0, 8]
    double ComputeByteEntropyFromFreq(
        const long long* freq,
        long long total)
    {
        if (total == 0)
            return 0.0;

        double entropy = 0.0;
        double invTotal = 1.0 / (double)total;

        for (int i = 0; i < 256; ++i) {
            long long c = freq[i];
            if (c == 0)
                continue;

            double p = (double)c * invTotal;

            // Shannon entropy
            entropy -= p * Log2(p);
        }

        return entropy;
    }
}

#endif