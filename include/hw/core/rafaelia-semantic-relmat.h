/*
 * RAFAELIA Semantic Relationship Matrix (rafaelia-semantic-relmat.h)
 *
 * Pre-computes pairwise semantic friction scores between Diamond Architecture
 * integration nodes at init time.
 *
 * Properties:
 *   - O(1) friction(i,j) lookup after initialization
 *   - O(N) cost to add a new node (compute vs existing nodes only; no
 *     recomputation of previously stored pairs — eliminates cycle problem)
 *   - Upper-triangular packed storage: N*(N-1)/2 cells
 *   - No heap allocation — caller owns the RafSemanticRelmat struct
 *   - Deterministic: same descriptor → same fingerprint → same friction
 *
 * Friction metric: Q16 Hamming distance between 64-bit FNV-1a phi fingerprints.
 *   0x0000 = identical semantics (zero friction)
 *   0xffff = maximally different semantics (maximum friction)
 *
 * Usage:
 *   RafSemanticRelmat rm;
 *   raf_semrel_init(&rm);
 *   raf_semrel_populate_diamond(&rm);          // pre-fills 5 Diamond nodes
 *   uint16_t f = raf_semrel_friction(&rm, 1, 3); // O(1) lookup
 *   raf_semrel_audit(&rm);                     // print full matrix to stderr
 */

#ifndef HW_RAFAELIA_SEMANTIC_RELMAT_H
#define HW_RAFAELIA_SEMANTIC_RELMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Maximum nodes supported. 16 → 120 upper-triangular cells, 240 bytes. */
#define RAF_SEMREL_MAX_NODES  16u
#define RAF_SEMREL_CELL_COUNT (RAF_SEMREL_MAX_NODES * (RAF_SEMREL_MAX_NODES - 1u) / 2u)

/*
 * A semantic node: named element with a 64-bit phi fingerprint derived from
 * its descriptor bytes. The fingerprint is computed once at add time.
 */
typedef struct {
    char     name[32];
    uint64_t phi;
} RafSemanticNode;

/*
 * The matrix.  Allocate on stack or as a static variable; pass by pointer.
 */
typedef struct {
    RafSemanticNode nodes[RAF_SEMREL_MAX_NODES];
    uint16_t        cells[RAF_SEMREL_CELL_COUNT];
    uint8_t         n;   /* number of nodes added */
} RafSemanticRelmat;

/* Pre-defined node indices for the Diamond Architecture (5 repos). */
typedef enum {
    RAF_SEMREL_QEMU     = 0,   /* qemu_rafaelia — core/hub          */
    RAF_SEMREL_USERLAND = 1,   /* UserLAnd — Android userspace       */
    RAF_SEMREL_MAGISK   = 2,   /* Magisk_Rafaelia — kernel/root      */
    RAF_SEMREL_LLAMA    = 3,   /* llamaRafaelia — AI/LLM             */
    RAF_SEMREL_PRIVATE  = 4,   /* Rafaelia_Private — extensions      */
    RAF_SEMREL_DIAMOND_COUNT = 5,
} RafSemanticNodeId;

/*
 * Zero-initialize the matrix. Must be called before any other function.
 */
void raf_semrel_init(RafSemanticRelmat *rm);

/*
 * Add one node by descriptor bytes.
 *
 * Computes the phi fingerprint from `descriptor[0..desc_len)`, then computes
 * friction scores against all nodes already in the matrix and stores them.
 * Previously stored scores are never recomputed — adding a new node is O(N).
 *
 * Returns the index of the new node, or -1 if the matrix is full.
 */
int raf_semrel_add_node(RafSemanticRelmat *rm,
                        const char        *name,
                        const uint8_t     *descriptor,
                        size_t             desc_len);

/*
 * Convenience wrapper: add a node whose descriptor is a C string.
 */
int raf_semrel_add_node_str(RafSemanticRelmat *rm,
                            const char        *name,
                            const char        *descriptor);

/*
 * Pre-populate the matrix with the 5 Diamond Architecture nodes.
 * Calls raf_semrel_add_node() for each; stores 10 friction scores total.
 * Must be called after raf_semrel_init() and before raf_semrel_friction().
 */
void raf_semrel_populate_diamond(RafSemanticRelmat *rm);

/*
 * O(1) lookup.
 *
 * Returns the friction score between nodes i and j.
 * Returns 0x0000 when i == j (a node has zero friction with itself).
 * Returns 0xffff if either index is out of range.
 */
uint16_t raf_semrel_friction(const RafSemanticRelmat *rm, uint8_t i, uint8_t j);

/*
 * Print the full N×N matrix to stderr for auditing.
 */
void raf_semrel_audit(const RafSemanticRelmat *rm);

#endif /* HW_RAFAELIA_SEMANTIC_RELMAT_H */
