#pragma once

#include <immintrin.h>
#include <cstdint>
#include <array>

namespace hft {

#if defined(__AVX2__)

inline __m256i mm256_load_epi32(const int32_t* ptr) {
    return _mm256_load_si256(reinterpret_cast<const __m256i*>(ptr));
}

inline __m256i mm256_store_epi32(int32_t* ptr, __m256i val) {
    _mm256_store_si256(reinterpret_cast<__m256i*>(ptr), val);
    return val;
}

inline __m256i mm256_add_epi32(__m256i a, __m256i b) {
    return _mm256_add_epi32(a, b);
}

inline __m256i mm256_sub_epi32(__m256i a, __m256i b) {
    return _mm256_sub_epi32(a, b);
}

inline __m256i mm256_cmpgt_epi32(__m256i a, __m256i b) {
    return _mm256_cmpgt_epi32(a, b);
}

inline __m256i mm256_cmplt_epi32(__m256i a, __m256i b) {
    return _mm256_cmpgt_epi32(b, a);
}

inline __m256i mm256_max_epi32(__m256i a, __m256i b) {
    return _mm256_max_epi32(a, b);
}

inline __m256i mm256_min_epi32(__m256i a, __m256i b) {
    return _mm256_min_epi32(a, b);
}

struct SimdBatch {
    static constexpr size_t SIZE = 8;

    std::array<int32_t, SIZE> prices;
    std::array<int32_t, SIZE> quantities;
    std::array<int32_t, SIZE> results;

    void load_prices(const int32_t* src) {
        for (size_t i = 0; i < SIZE; i++) {
            prices[i] = src[i];
        }
    }

    void load_quantities(const int32_t* src) {
        for (size_t i = 0; i < SIZE; i++) {
            quantities[i] = src[i];
        }
    }

    void store_results(int32_t* dst) {
        for (size_t i = 0; i < SIZE; i++) {
            dst[i] = results[i];
        }
    }

    void compute_value() {
        for (size_t i = 0; i < SIZE; i++) {
            results[i] = prices[i] * quantities[i];
        }
    }

    void compare_gt(int32_t threshold) {
        for (size_t i = 0; i < SIZE; i++) {
            results[i] = prices[i] > threshold ? 1 : 0;
        }
    }
};

class SimdProcessor {
public:
    static bool has_avx2() {
        return __AVX2__;
    }

    static void batch_validate_orders(const int32_t* prices, const int32_t* quantities,
                                       const int32_t* max_prices, const int32_t* max_quantities,
                                       bool* results, size_t count) {
        size_t i = 0;
        for (; i + 8 <= count; i += 8) {
            __m256i p = mm256_load_epi32(prices + i);
            __m256i q = mm256_load_epi32(quantities + i);
            __m256i mp = mm256_load_epi32(max_prices + i);
            __m256i mq = mm256_load_epi32(max_quantities + i);

            __m256i p_check = mm256_cmpgt_epi32(p, _mm256_setzero_si256());
            __m256i q_check = mm256_cmpgt_epi32(q, _mm256_setzero_si256());
            __m256i price_ok = mm256_cmplt_epi32(p, mp);
            __m256i qty_ok = mm256_cmplt_epi32(q, mq);

            __m256i combined = _mm256_and_si256(p_check, q_check);
            combined = _mm256_and_si256(combined, price_ok);
            combined = _mm256_and_si256(combined, qty_ok);

            int mask = _mm256_movemask_epi8(combined);
            results[i] = (mask & 0xFF) == 0xFF;
        }

        for (; i < count; i++) {
            results[i] = prices[i] > 0 && quantities[i] > 0 &&
                        prices[i] < max_prices[i] && quantities[i] < max_quantities[i];
        }
    }

    static void batch_compute_value(const int32_t* prices, const int32_t* quantities,
                                    int64_t* values, size_t count) {
        size_t i = 0;
        for (; i + 8 <= count; i += 8) {
            __m256i p = mm256_load_epi32(prices + i);
            __m256i q = mm256_load_epi32(quantities + i);
            __m256i v = _mm256_mul_epu32(p, q);

            alignas(32) int64_t tmp[4];
            _mm256_store_si256(reinterpret_cast<__m256i*>(tmp), v);
            for (int j = 0; j < 4; j++) {
                values[i + j * 2] = tmp[j];
            }
        }

        for (; i < count; i++) {
            values[i] = static_cast<int64_t>(prices[i]) * quantities[i];
        }
    }

    static void batch_sum(const int32_t* values, int64_t* result, size_t count) {
        *result = 0;
        size_t i = 0;
        __m256i sum = _mm256_setzero_si256();

        for (; i + 8 <= count; i += 8) {
            __m256i v = mm256_load_epi32(values + i);
            sum = mm256_add_epi32(sum, v);
        }

        alignas(32) int32_t tmp[8];
        mm256_store_epi32(tmp, sum);
        for (int j = 0; j < 8; j++) {
            *result += tmp[j];
        }

        for (; i < count; i++) {
            *result += values[i];
        }
    }

    static void batch_find_min_max(const int32_t* values, int32_t* min_result,
                                   int32_t* max_result, size_t count) {
        if (count == 0) return;

        __m256i min_v = mm256_load_epi32(values);
        __m256i max_v = min_v;

        size_t i = 8;
        for (; i + 8 <= count; i += 8) {
            __m256i v = mm256_load_epi32(values + i);
            min_v = mm256_min_epi32(min_v, v);
            max_v = mm256_max_epi32(max_v, v);
        }

        alignas(32) int32_t tmp[8];
        mm256_store_epi32(tmp, min_v);
        *min_result = tmp[0];
        for (int j = 1; j < 8; j++) {
            if (tmp[j] < *min_result) *min_result = tmp[j];
        }

        mm256_store_epi32(tmp, max_v);
        *max_result = tmp[0];
        for (int j = 1; j < 8; j++) {
            if (tmp[j] > *max_result) *max_result = tmp[j];
        }

        for (; i < count; i++) {
            if (values[i] < *min_result) *min_result = values[i];
            if (values[i] > *max_result) *max_result = values[i];
        }
    }
};

#elif defined(__SSE4_1__)

struct SimdBatch {
    static constexpr size_t SIZE = 4;

    std::array<int32_t, SIZE> prices;
    std::array<int32_t, SIZE> quantities;
    std::array<int32_t, SIZE> results;

    void load_prices(const int32_t* src) {
        for (size_t i = 0; i < SIZE; i++) {
            prices[i] = src[i];
        }
    }

    void load_quantities(const int32_t* src) {
        for (size_t i = 0; i < SIZE; i++) {
            quantities[i] = src[i];
        }
    }

    void store_results(int32_t* dst) {
        for (size_t i = 0; i < SIZE; i++) {
            dst[i] = results[i];
        }
    }

    void compute_value() {
        for (size_t i = 0; i < SIZE; i++) {
            results[i] = prices[i] * quantities[i];
        }
    }

    void compare_gt(int32_t threshold) {
        for (size_t i = 0; i < SIZE; i++) {
            results[i] = prices[i] > threshold ? 1 : 0;
        }
    }
};

class SimdProcessor {
public:
    static bool has_avx2() { return false; }

    static void batch_validate_orders(const int32_t* prices, const int32_t* quantities,
                                       const int32_t* max_prices, const int32_t* max_quantities,
                                       bool* results, size_t count) {
        for (size_t i = 0; i < count; i++) {
            results[i] = prices[i] > 0 && quantities[i] > 0 &&
                        prices[i] < max_prices[i] && quantities[i] < max_quantities[i];
        }
    }

    static void batch_compute_value(const int32_t* prices, const int32_t* quantities,
                                    int64_t* values, size_t count) {
        for (size_t i = 0; i < count; i++) {
            values[i] = static_cast<int64_t>(prices[i]) * quantities[i];
        }
    }

    static void batch_sum(const int32_t* values, int64_t* result, size_t count) {
        *result = 0;
        for (size_t i = 0; i < count; i++) {
            *result += values[i];
        }
    }

    static void batch_find_min_max(const int32_t* values, int32_t* min_result,
                                   int32_t* max_result, size_t count) {
        if (count == 0) return;
        *min_result = values[0];
        *max_result = values[0];
        for (size_t i = 1; i < count; i++) {
            if (values[i] < *min_result) *min_result = values[i];
            if (values[i] > *max_result) *max_result = values[i];
        }
    }
};

#else

struct SimdBatch {
    static constexpr size_t SIZE = 4;

    std::array<int32_t, SIZE> prices;
    std::array<int32_t, SIZE> quantities;
    std::array<int32_t, SIZE> results;

    void load_prices(const int32_t* src) {
        for (size_t i = 0; i < SIZE; i++) {
            prices[i] = src[i];
        }
    }

    void load_quantities(const int32_t* src) {
        for (size_t i = 0; i < SIZE; i++) {
            quantities[i] = src[i];
        }
    }

    void store_results(int32_t* dst) {
        for (size_t i = 0; i < SIZE; i++) {
            dst[i] = results[i];
        }
    }

    void compute_value() {
        for (size_t i = 0; i < SIZE; i++) {
            results[i] = prices[i] * quantities[i];
        }
    }

    void compare_gt(int32_t threshold) {
        for (size_t i = 0; i < SIZE; i++) {
            results[i] = prices[i] > threshold ? 1 : 0;
        }
    }
};

class SimdProcessor {
public:
    static bool has_avx2() { return false; }

    static void batch_validate_orders(const int32_t* prices, const int32_t* quantities,
                                       const int32_t* max_prices, const int32_t* max_quantities,
                                       bool* results, size_t count) {
        for (size_t i = 0; i < count; i++) {
            results[i] = prices[i] > 0 && quantities[i] > 0 &&
                        prices[i] < max_prices[i] && quantities[i] < max_quantities[i];
        }
    }

    static void batch_compute_value(const int32_t* prices, const int32_t* quantities,
                                    int64_t* values, size_t count) {
        for (size_t i = 0; i < count; i++) {
            values[i] = static_cast<int64_t>(prices[i]) * quantities[i];
        }
    }

    static void batch_sum(const int32_t* values, int64_t* result, size_t count) {
        *result = 0;
        for (size_t i = 0; i < count; i++) {
            *result += values[i];
        }
    }

    static void batch_find_min_max(const int32_t* values, int32_t* min_result,
                                   int32_t* max_result, size_t count) {
        if (count == 0) return;
        *min_result = values[0];
        *max_result = values[0];
        for (size_t i = 1; i < count; i++) {
            if (values[i] < *min_result) *min_result = values[i];
            if (values[i] > *max_result) *max_result = values[i];
        }
    }
};

#endif

}
