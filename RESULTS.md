# Search Optimization Results

This document summarizes the performance improvements achieved by implementing Proposals A, B, and D for DiskANN Vamana Search optimization.

## Implementation Summaries

- **Proposal A (Asymmetric Distance Computation via Quantization):** Compressed the dataset from float32 (~488 MB) to uint8 (~128 MB) using per-dimension min/scale scalar block quantization. Graph traversal employs an asymmetric distance function (float32 query vs. uint8 data), with exact float32 re-ranking for the final L candidates. The metadata overhead is negligible (~1KB).
- **Proposal B (Early-Abandoning SIMD Distance):** Added early-exit logic to both exact and asymmetric distance functions. The logic checks if the partial distance sum exceeds the worst candidate's distance at intervals of 32 dimensions, terminating early to save computations while preserving SIMD vectorization.
- **Proposal D (Flattening `std::set`):** Replaced the inner-loop `std::set` (Red-Black Tree) and `std::set` visited frontier map with a pre-allocated contiguous `std::vector` array maintaining sortedness by distance. Candidate insertion shifts contiguous memory via `memmove`. The frontier iterator uses a simple array index cursor (`expand_pos`), avoiding heap allocations entirely in the inner loop. 

---

## Experimental Results

The tests were run with the SIFT1M dataset (1 million vectors, 128 dimensions, Top-100 ground truth) utilizing Windows MSVC environment. 

### 1. Proposal D Impact (Data Structure Flattening)
Flattening the memory structures to rely exclusively on contiguous vectors proved immediately impactful due to removing costly heap allocations per node traversal.

_Comparing Exact Float32 (Standard) vs. Exact Float32 (Flat Vector):_

| L  | Recall@10 | Baseline Avg Latency | **Flat Vector Avg Latency** | Improvement |
|----|-----------|----------------------|-----------------------------|-------------|
| 10 | 0.7727    | 715.8 µs             | **627.6 µs**                | **~12.3%**  |
| 50 | 0.9665    | 1650.1 µs            | **1411.9 µs**               | **~14.4%**  |
| 75 | 0.9820    | 2224.1 µs            | **1934.4 µs**               | **~13.0%**  |
| 150| 0.9939    | 3895.4 µs            | **3291.5 µs**               | **~15.5%**  |
| 200| 0.9961    | 5581.9 µs            | **3928.2 µs**               | **~29.6%**  |

**Observations:** Flat vectors scale drastically better as $L$ increases. Removing object allocation overhead shifts the search inner loop’s bottleneck squarely onto memory bandwidth and distance computations.

---

### 2. Proposal A Impact (Asymmetric Quantization)
ADC was benchmarked natively on the flattened vector setup (Proposal D integrated) to evaluate bandwidth reduction. 

_Comparing Exact Float32 (Flat) vs. Quantized ADC (Flat):_

| L  | Exact Recall | ADC Recall | Exact Avg Latency | **ADC Avg Latency** | Latency Delta |
|----|--------------|------------|-------------------|---------------------|---------------|
| 10 | 0.7727       | 0.7718     | 627.6 µs          | **557.5 µs**        | -11.2% Faster |
| 30 | 0.9332       | 0.9333     | 1022.8 µs         | **986.1 µs**        | -3.6% Faster  |
| 75 | 0.9820       | 0.9817     | 1934.4 µs         | **1897.4 µs**       | -1.9% Faster  |
| 150| 0.9939       | 0.9939     | 3291.5 µs         | **3387.3 µs**       | +2.9% Slower  |
| 200| 0.9961       | 0.9961     | 3928.2 µs         | **4398.8 µs**       | +12.0% Slower |

**Observations:** 
- **Recall Matching:** Float32 re-ranking for the final $L$ candidates works exceptionally well, fully preserving recall boundaries within 0.001 limits.
- **Latency Trade-offs:** The 4x database layout compression improves search speed consistently at small $L$, avoiding memory eviction bounds. However, at large $L$, float32 re-ranking the entire candidate list begins bottlenecking the system and the ALU dequantization math adds cost that offsets the bandwidth savings. This results in slightly worse latencies around $L \approx 150+$.

---

### 3. Proposal B Impact (Early Abandonment)
_The early cancellation logic evaluates exactly identifiably to baseline behavior under normal search ranges._

**Observations:** 
In tests, Early Abandonment kept Recall computations perfectly identical, but the absolute reduction in raw operations (`dist_cmps`) did not yield massive changes. This indicates that Vamana points evaluated during beam-search traversal are largely within the radius of expected thresholds naturally. However, adding it operates at virtually zero cost (evaluating every 32 elements linearly) and offers protective upper bounds when exploring sparse search queries far from target sets.
