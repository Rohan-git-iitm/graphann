#include "distance.h"
#ifdef _MSC_VER
    #include <malloc.h>
    #define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
    #define aligned_free(ptr) _aligned_free(ptr)
#else
    #include <cstdlib>
    #define aligned_free(ptr) free(ptr)
#endif

float compute_l2sq(const float* a, const float* b, uint32_t dim) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

float compute_l2sq_asymmetric(const float* query, const uint8_t* quantized,
                              const float* dim_min, const float* dim_scale,
                              uint32_t dim) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float reconstructed = quantized[i] * dim_scale[i] + dim_min[i];
        float diff = query[i] - reconstructed;
        sum += diff * diff;
    }
    return sum;
}
