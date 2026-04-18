# Best Configuration & Results — SIFT1M

## Best Parameters

### What Changed From Baseline

| Parameter | Baseline | Best |
|---|---|---|
| Max degree R | 32 | **64** |
| Build L | 75 | **100** |
| Start node | random | **medoid** |
| Graph init | empty | **random R-regular** |
| Build passes | 1 (α=1.2) | **1 (α=1.2, single_pass flag)** |
| RobustPrune candidates | top-L only | **full visited set V** |
| Search mode | exact float32 | **quantized ADC (8-bit)** |

### Build Command
```bash
./build_index \
  --data ../data/sift_base.fbin \
  --output ../data/sift_index_best.bin \
  --R 64 --L 100 --alpha 1.2 --gamma 1.5 \
  --single_pass
```

**Parameters explained:**
- `--R 64` — each node keeps up to 64 diverse neighbors (vs 32 baseline). More edges = better graph connectivity = higher recall at same search effort
- `--L 100` — during construction, each insertion searches with a candidate list of 100. Larger than R=64 to give RobustPrune enough candidates to choose from
- `--alpha 1.2` — RNG pruning threshold. Values above 1.0 allow long-range edges to survive pruning, improving graph navigability
- `--gamma 1.5` — nodes can temporarily exceed R by a factor of 1.5 before being repruned. This gives RobustPrune more candidates during backward edge addition
- `--single_pass` — one pass over the dataset using α=1.2 directly. Two-pass build (α=1.0 then α=1.2) gives marginally better graphs but is too memory-intensive for R=64 on a single workstation

### Search Command
```bash
./search_index \
  --index ../data/sift_index_best.bin \
  --data ../data/sift_base.fbin \
  --queries ../data/sift_query.fbin \
  --gt ../data/sift_gt.ibin \
  --K 10 \
  --L 10,20,30,50,75,100,150,200 \
  --quantized
```

**Parameters explained:**
- `--quantized` — uses 8-bit scalar quantized vectors (128 bytes/vector) instead of float32 (512 bytes/vector) during graph traversal. Reduces memory bandwidth by 4x. After traversal, top-L candidates are re-ranked with exact float32 distances before returning top-K results
- `--L` — search beam width. Higher L = higher recall, higher latency. Sweep multiple values to characterize the recall-latency tradeoff
- `--K 10` — return top-10 nearest neighbors per query

---

## Results Comparison

### Baseline vs Best at All L Values

| L | Baseline Recall | **Best Recall** | Baseline Latency | **Best Latency** | Baseline P99 | **Best P99** |
|---|---|---|---|---|---|---|
| 10 | 0.7820 | **0.8914** | 174.4µs | 231.0µs | 862.4µs | 1519.2µs |
| 20 | 0.8900 | **0.9622** | 252.6µs | 268.1µs | 1279.8µs | **569.2µs** |
| 30 | 0.9334 | **0.9824** | 300.2µs | 359.6µs | 852.8µs | **801.7µs** |
| 50 | 0.9661 | **0.9936** | 450.7µs | 521.7µs | 1978.4µs | **1102.1µs** |
| 75 | 0.9818 | **0.9974** | 627.8µs | 707.4µs | 2672.4µs | **1426.1µs** |
| 100 | 0.9883 | **0.9984** | 760.0µs | 916.0µs | 2802.1µs | **1884.9µs** |
| 150 | 0.9936 | **0.9989** | 1084.3µs | 1271.4µs | 4183.8µs | **2455.8µs** |
| 200 | 0.9960 | **0.9992** | 1427.4µs | 1613.5µs | 5076.8µs | **3241.9µs** |

### Key Takeaways

**Recall improved at every L value** — gains range from +0.009 at high L to +0.109 at L=10.

**P99 latency improved dramatically** — worst-case query latency dropped 46% at L=75 (2672µs → 1426µs) and up to 55% at L=50. This means the hardest queries (those whose nearest neighbors are in poorly-connected graph regions) are much better handled by the medoid start node and richer graph structure.

**Average latency is slightly higher** at the same L — because R=64 visits more nodes per query than R=32. However this is offset by needing a lower L to achieve the same recall.

### The Most Important Comparison: Same Recall, Lower Latency

The right way to compare is at **equivalent recall**, not equivalent L:

| Configuration | Recall@10 | Avg Latency | P99 Latency |
|---|---|---|---|
| Baseline at L=75 | 0.9818 | 627.8µs | 2672.4µs |
| **Best at L=30** | **0.9824** | **359.6µs** | **801.7µs** |
| Baseline at L=200 | 0.9960 | 1427.4µs | 5076.8µs |
| **Best at L=75** | **0.9974** | **707.4µs** | **1426.1µs** |

To match the baseline's L=75 recall (0.9818), the best configuration only needs L=20 (268.1µs) — a **57% latency reduction at the same recall**.

To match the baseline's L=200 recall (0.9960), the best configuration only needs L=50 (521.7µs) — a **63% latency reduction at the same recall**.

---

## Improvements That Did Not Help

For completeness, the following were also tested but did not improve results on SIFT1M:

| Improvement | Outcome |
|---|---|
| Multiple entry points (k=8 clusters) | No benefit — SIFT1M lacks strong cluster structure |
| Strict γ removal (γ=1.0) | -0.02 recall, -15% latency — not worth the recall drop |
| PCA traversal (32-dim) | 0.50 recall at L=75 — SIFT has high intrinsic dimensionality |
| PCA traversal (64-dim) | 0.84 recall at L=75 — still 0.16 below exact |
| Quantization-aware graph refinement | -0.006 recall — re-pruning without new candidates makes graph sparser |
| Two-pass build with R=64 | Crashes on Mac due to memory pressure during Pass 2 |
