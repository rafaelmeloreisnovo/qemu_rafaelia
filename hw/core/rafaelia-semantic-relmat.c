/*
 * RAFAELIA Semantic Relationship Matrix — implementation
 *
 * Eliminates recalculation cycles in the Diamond Architecture integration hub:
 * when a new semantic concept (node) is added, only N new friction scores are
 * computed against existing nodes — already stored scores are never touched.
 *
 * Phi fingerprint: FNV-1a 64-bit hash of descriptor bytes.
 * Friction metric: Q16 Hamming distance between 64-bit fingerprints.
 *   friction_q16 = round(hamming_bits * 0xffff / 64)
 *
 * Storage: upper-triangular packed array.
 *   For pair (i,j) with i < j and MAX = RAF_SEMREL_MAX_NODES:
 *   cell_index(i,j) = i*(2*MAX - 1 - i)/2 + (j - i - 1)
 */

#include "hw/core/rafaelia-semantic-relmat.h"

#include <string.h>
#include <stdio.h>

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/* FNV-1a 64-bit: deterministic, freestanding, no state. */
static uint64_t phi_fingerprint(const uint8_t *data, size_t len)
{
    uint64_t h = 14695981039346656037ULL;
    size_t k;
    for (k = 0; k < len; k++) {
        h ^= (uint64_t)data[k];
        h *= 1099511628211ULL;
    }
    return h;
}

/* Bit population count for 64-bit value (Hamming weight). */
static uint32_t popcount64(uint64_t v)
{
#if defined(__GNUC__) || defined(__clang__)
    return (uint32_t)__builtin_popcountll((unsigned long long)v);
#else
    /* Parallel bit-count fallback (no branch, no divide). */
    v = v - ((v >> 1) & UINT64_C(0x5555555555555555));
    v = (v & UINT64_C(0x3333333333333333))
      + ((v >> 2) & UINT64_C(0x3333333333333333));
    v = (v + (v >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
    return (uint32_t)((v * UINT64_C(0x0101010101010101)) >> 56);
#endif
}

/*
 * Upper-triangular cell index for pair (a, b).
 * Swaps a and b if a > b so the formula always has i < j.
 *
 * Formula: i*(2*MAX - 1 - i)/2 + (j - i - 1)
 *   where MAX = RAF_SEMREL_MAX_NODES
 */
static uint32_t relmat_idx(uint8_t a, uint8_t b)
{
    uint32_t i = (a < b) ? (uint32_t)a : (uint32_t)b;
    uint32_t j = (a < b) ? (uint32_t)b : (uint32_t)a;
    /* i < j guaranteed; both < RAF_SEMREL_MAX_NODES */
    return i * (2u * RAF_SEMREL_MAX_NODES - 1u - i) / 2u + (j - i - 1u);
}

/* Convert Hamming distance in [0,64] to Q16 friction in [0,0xffff]. */
static uint16_t hamming_to_q16(uint32_t hd)
{
    /* Round: (hd * 0xffff + 32) / 64 */
    uint32_t q = (hd * 0xffffu + 32u) / 64u;
    return (uint16_t)(q > 0xffffu ? 0xffffu : q);
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void raf_semrel_init(RafSemanticRelmat *rm)
{
    memset(rm, 0, sizeof(*rm));
}

int raf_semrel_add_node(RafSemanticRelmat *rm,
                        const char        *name,
                        const uint8_t     *descriptor,
                        size_t             desc_len)
{
    uint8_t  idx;
    uint64_t phi;
    uint8_t  e;

    if (rm->n >= RAF_SEMREL_MAX_NODES) {
        return -1;
    }

    idx = rm->n;
    phi = phi_fingerprint(descriptor, desc_len);

    /* Store node. */
    strncpy(rm->nodes[idx].name, name, sizeof(rm->nodes[idx].name) - 1u);
    rm->nodes[idx].name[sizeof(rm->nodes[idx].name) - 1u] = '\0';
    rm->nodes[idx].phi = phi;

    /*
     * Compute friction vs all existing nodes (indices 0..idx-1).
     * Each pair is stored exactly once — existing pairs are never recomputed.
     * This is the O(N) per addition that avoids recalculation cycles.
     */
    for (e = 0; e < idx; e++) {
        uint32_t hd      = popcount64(rm->nodes[e].phi ^ phi);
        uint32_t cell    = relmat_idx(e, idx);
        rm->cells[cell]  = hamming_to_q16(hd);
    }

    rm->n = idx + 1u;
    return (int)idx;
}

int raf_semrel_add_node_str(RafSemanticRelmat *rm,
                            const char        *name,
                            const char        *descriptor)
{
    return raf_semrel_add_node(rm, name,
                               (const uint8_t *)descriptor,
                               strlen(descriptor));
}

/*
 * Pre-defined Diamond Architecture node descriptors.
 * Each descriptor encodes the semantic role of the integration layer;
 * changing a descriptor changes its fingerprint and all its friction scores.
 */
void raf_semrel_populate_diamond(RafSemanticRelmat *rm)
{
    raf_semrel_add_node_str(rm,
        "qemu_rafaelia",
        "qemu_rafaelia:core:hub:emulation:orchestration:ethical:routing");

    raf_semrel_add_node_str(rm,
        "userland",
        "userland:android:userspace:linux:container:filesystem:process");

    raf_semrel_add_node_str(rm,
        "magisk",
        "magisk:root:kernel:privileged:syscall:hook:module:patch");

    raf_semrel_add_node_str(rm,
        "llama",
        "llama:ai:llm:inference:cognition:nlp:learning:embedding");

    raf_semrel_add_node_str(rm,
        "private",
        "private:extension:proprietary:optimization:exclusive:cipher");
}

uint16_t raf_semrel_friction(const RafSemanticRelmat *rm, uint8_t i, uint8_t j)
{
    if (i >= rm->n || j >= rm->n) {
        return 0xffffu;
    }
    if (i == j) {
        return 0x0000u;
    }
    return rm->cells[relmat_idx(i, j)];
}

void raf_semrel_audit(const RafSemanticRelmat *rm)
{
    uint8_t i, j;

    fprintf(stderr,
            "raf_semrel_audit: %u nodes, %u pairs\n",
            (unsigned)rm->n,
            (unsigned)(rm->n * (rm->n - 1u) / 2u));

    /* Header row */
    fprintf(stderr, "%-20s", "");
    for (j = 0; j < rm->n; j++) {
        fprintf(stderr, " %-8s", rm->nodes[j].name);
    }
    fprintf(stderr, "\n");

    /* Matrix rows (full NxN, symmetric display) */
    for (i = 0; i < rm->n; i++) {
        fprintf(stderr, "%-20s", rm->nodes[i].name);
        for (j = 0; j < rm->n; j++) {
            if (i == j) {
                fprintf(stderr, " %-8s", "----");
            } else {
                fprintf(stderr, " 0x%04x  ", raf_semrel_friction(rm, i, j));
            }
        }
        fprintf(stderr, "  phi=0x%016llx\n",
                (unsigned long long)rm->nodes[i].phi);
    }
}
