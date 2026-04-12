#pragma once

#include <cstdint>

// Squared L2 (Euclidean) distance between two float vectors.
// No sqrt — monotonic, so rankings are preserved.
// Compiler auto-vectorizes this with -O3 -march=native.
float compute_l2sq(const float* a, const float* b, uint32_t dim);

// Asymmetric L2 squared: float32 query vs uint8 quantized vector.
// Dequantizes each dimension on-the-fly: reconstructed = quantized[d] * scale[d] + min[d]
// Used for fast graph traversal; final top-K are re-ranked with exact float32.
float compute_l2sq_asymmetric(const float* query, const uint8_t* quantized,
                              const float* dim_min, const float* dim_scale,
                              uint32_t dim);
