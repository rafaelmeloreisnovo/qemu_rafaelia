/**
 * GoldenTestVectors - Fixed input/output test data for mathematical validation
 * ISO 9001/8000 Compliant - Anti-bug-logic module
 * 
 * These vectors are MANUALLY COMPUTED using reference implementations and
 * documented mathematical formulas. They are NOT derived from OCR or any
 * external image-based source.
 * 
 * Each test vector includes:
 * - Input values
 * - Expected output
 * - Mathematical basis/formula reference
 */
package com.vectras.vm.core

/**
 * Golden test vectors for validating mathematical implementations.
 * All values are computed from documented formulas.
 */
object GoldenTestVectors {

    /**
     * Log2 integer test vectors.
     * Formula: floor(log_2(n)) = position of highest set bit
     */
    data class Log2IntVector(
        val input: Long,
        val expected: Int,
        val description: String
    )

    val LOG2_INT_VECTORS = listOf(
        Log2IntVector(1L, 0, "log2(1) = 0"),
        Log2IntVector(2L, 1, "log2(2) = 1"),
        Log2IntVector(4L, 2, "log2(4) = 2"),
        Log2IntVector(8L, 3, "log2(8) = 3"),
        Log2IntVector(16L, 4, "log2(16) = 4"),
        Log2IntVector(32L, 5, "log2(32) = 5"),
        Log2IntVector(64L, 6, "log2(64) = 6"),
        Log2IntVector(128L, 7, "log2(128) = 7"),
        Log2IntVector(256L, 8, "log2(256) = 8"),
        Log2IntVector(512L, 9, "log2(512) = 9"),
        Log2IntVector(1024L, 10, "log2(1024) = 10"),
        Log2IntVector(2048L, 11, "log2(2048) = 11"),
        Log2IntVector(4096L, 12, "log2(4096) = 12"),
        Log2IntVector(1L shl 20, 20, "log2(2^20) = 20"),
        Log2IntVector(1L shl 30, 30, "log2(2^30) = 30"),
        Log2IntVector(1L shl 40, 40, "log2(2^40) = 40"),
        Log2IntVector(1L shl 62, 62, "log2(2^62) = 62"),
        // Non-power-of-2 values (floor)
        Log2IntVector(3L, 1, "floor(log2(3)) = 1"),
        Log2IntVector(5L, 2, "floor(log2(5)) = 2"),
        Log2IntVector(7L, 2, "floor(log2(7)) = 2"),
        Log2IntVector(15L, 3, "floor(log2(15)) = 3"),
        Log2IntVector(1000L, 9, "floor(log2(1000)) = 9"),
        Log2IntVector(1023L, 9, "floor(log2(1023)) = 9"),
        Log2IntVector(1025L, 10, "floor(log2(1025)) = 10"),
        // Edge cases
        Log2IntVector(0L, -1, "log2(0) is undefined, return -1"),
        Log2IntVector(-1L, -1, "log2(-1) is undefined, return -1"),
        Log2IntVector(Long.MAX_VALUE, 62, "log2(MAX_VALUE) = 62")
    )

    /**
     * Integer square root test vectors.
     * Formula: floor(sqrt(n))
     */
    data class SqrtIntVector(
        val input: Long,
        val expected: Long,
        val description: String
    )

    val SQRT_INT_VECTORS = listOf(
        SqrtIntVector(0L, 0L, "sqrt(0) = 0"),
        SqrtIntVector(1L, 1L, "sqrt(1) = 1"),
        SqrtIntVector(4L, 2L, "sqrt(4) = 2"),
        SqrtIntVector(9L, 3L, "sqrt(9) = 3"),
        SqrtIntVector(16L, 4L, "sqrt(16) = 4"),
        SqrtIntVector(25L, 5L, "sqrt(25) = 5"),
        SqrtIntVector(36L, 6L, "sqrt(36) = 6"),
        SqrtIntVector(49L, 7L, "sqrt(49) = 7"),
        SqrtIntVector(64L, 8L, "sqrt(64) = 8"),
        SqrtIntVector(81L, 9L, "sqrt(81) = 9"),
        SqrtIntVector(100L, 10L, "sqrt(100) = 10"),
        SqrtIntVector(10000L, 100L, "sqrt(10000) = 100"),
        SqrtIntVector(1000000L, 1000L, "sqrt(1000000) = 1000"),
        // Non-perfect squares (floor)
        SqrtIntVector(2L, 1L, "floor(sqrt(2)) = 1"),
        SqrtIntVector(3L, 1L, "floor(sqrt(3)) = 1"),
        SqrtIntVector(5L, 2L, "floor(sqrt(5)) = 2"),
        SqrtIntVector(8L, 2L, "floor(sqrt(8)) = 2"),
        SqrtIntVector(10L, 3L, "floor(sqrt(10)) = 3"),
        SqrtIntVector(99L, 9L, "floor(sqrt(99)) = 9"),
        SqrtIntVector(101L, 10L, "floor(sqrt(101)) = 10"),
        // Large values
        SqrtIntVector(1L shl 20, 1024L, "sqrt(2^20) = 1024"),
        SqrtIntVector(1L shl 30, 32768L, "sqrt(2^30) = 32768"),
        SqrtIntVector(1L shl 40, 1048576L, "sqrt(2^40) = 1048576"),
        // Edge case
        SqrtIntVector(-1L, -1L, "sqrt(-1) is undefined, return -1")
    )

    /**
     * CRC-32 test vectors (IEEE 802.3 polynomial).
     * Computed using reference implementation.
     */
    data class CRC32Vector(
        val input: ByteArray,
        val inputHex: String,
        val expected: Long,
        val description: String
    ) {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is CRC32Vector) return false
            return input.contentEquals(other.input) && expected == other.expected
        }
        override fun hashCode(): Int {
            return 31 * input.contentHashCode() + expected.hashCode()
        }
    }

    val CRC32_VECTORS = listOf(
        // Standard test vectors from IEEE 802.3
        CRC32Vector(
            byteArrayOf(),
            "",
            0x00000000L,
            "CRC32 of empty string"
        ),
        CRC32Vector(
            "123456789".toByteArray(Charsets.US_ASCII),
            "313233343536373839",
            0xCBF43926L,
            "CRC32 of '123456789' - standard check value"
        ),
        CRC32Vector(
            "a".toByteArray(Charsets.US_ASCII),
            "61",
            0xE8B7BE43L,
            "CRC32 of 'a'"
        ),
        CRC32Vector(
            "abc".toByteArray(Charsets.US_ASCII),
            "616263",
            0x352441C2L,
            "CRC32 of 'abc'"
        ),
        CRC32Vector(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ".toByteArray(Charsets.US_ASCII),
            "4142434445464748494A4B4C4D4E4F505152535455565758595A",
            0xABF77822L,
            "CRC32 of uppercase alphabet"
        ),
        CRC32Vector(
            byteArrayOf(0x00),
            "00",
            0xD202EF8DL,
            "CRC32 of single null byte"
        ),
        CRC32Vector(
            byteArrayOf(0xFF.toByte()),
            "FF",
            0xFF000000L,
            "CRC32 of single 0xFF byte"
        )
    )

    /**
     * Bit rotation test vectors.
     * Formula: rotateLeft(x, n) = (x << n) | (x >>> (64-n))
     */
    data class RotateVector(
        val input: Long,
        val bits: Int,
        val expectedLeft: Long,
        val expectedRight: Long,
        val description: String
    )

    val ROTATE_VECTORS = listOf(
        RotateVector(
            0x0000000000000001L, 1,
            0x0000000000000002L, Long.MIN_VALUE,
            "Rotate 1 by 1 bit"
        ),
        RotateVector(
            Long.MIN_VALUE, 1,
            0x0000000000000001L, 0x4000000000000000L,
            "Rotate MSB by 1 bit"
        ),
        RotateVector(
            0xABCDEF0123456789UL.toLong(), 4,
            0xBCDEF0123456789AUL.toLong(), 0x9ABCDEF012345678UL.toLong(),
            "Rotate by 4 bits"
        ),
        RotateVector(
            0xABCDEF0123456789UL.toLong(), 8,
            0xCDEF0123456789ABUL.toLong(), 0x89ABCDEF01234567UL.toLong(),
            "Rotate by 8 bits"
        ),
        RotateVector(
            0xABCDEF0123456789UL.toLong(), 32,
            0x23456789ABCDEF01L, 0x23456789ABCDEF01L,
            "Rotate by 32 bits (half)"
        ),
        RotateVector(
            0x0123456789ABCDEFL, 0,
            0x0123456789ABCDEFL, 0x0123456789ABCDEFL,
            "Rotate by 0 bits (no change)"
        ),
        RotateVector(
            0x0123456789ABCDEFL, 64,
            0x0123456789ABCDEFL, 0x0123456789ABCDEFL,
            "Rotate by 64 bits (full rotation)"
        )
    )

    /**
     * Population count (bit count) test vectors.
     * Formula: count of set bits in binary representation
     */
    data class PopCountVector(
        val input: Long,
        val expected: Int,
        val description: String
    )

    val POPCOUNT_VECTORS = listOf(
        PopCountVector(0L, 0, "popcount(0) = 0"),
        PopCountVector(1L, 1, "popcount(1) = 1"),
        PopCountVector(2L, 1, "popcount(2) = 1"),
        PopCountVector(3L, 2, "popcount(3) = 2"),
        PopCountVector(7L, 3, "popcount(7) = 3"),
        PopCountVector(15L, 4, "popcount(15) = 4"),
        PopCountVector(0xFFL, 8, "popcount(0xFF) = 8"),
        PopCountVector(0xFFFFL, 16, "popcount(0xFFFF) = 16"),
        PopCountVector(0xFFFFFFFFL, 32, "popcount(0xFFFFFFFF) = 32"),
        PopCountVector(-1L, 64, "popcount(-1) = 64 (all bits set)"),
        PopCountVector(0x5555555555555555L, 32, "popcount(alternating bits) = 32"),
        PopCountVector(0xAAAAAAAAAAAAAAAAUL.toLong(), 32, "popcount(alternating bits) = 32"),
        PopCountVector(0x0123456789ABCDEFL, 32, "popcount(0x0123456789ABCDEF) = 32")
    )

    /**
     * Power of 2 test vectors.
     * Formula: n is power of 2 iff n > 0 and (n & (n-1)) == 0
     */
    data class PowerOfTwoVector(
        val input: Long,
        val expected: Boolean,
        val description: String
    )

    val POWER_OF_TWO_VECTORS = listOf(
        PowerOfTwoVector(0L, false, "0 is not a power of 2"),
        PowerOfTwoVector(1L, true, "1 = 2^0 is a power of 2"),
        PowerOfTwoVector(2L, true, "2 = 2^1 is a power of 2"),
        PowerOfTwoVector(3L, false, "3 is not a power of 2"),
        PowerOfTwoVector(4L, true, "4 = 2^2 is a power of 2"),
        PowerOfTwoVector(5L, false, "5 is not a power of 2"),
        PowerOfTwoVector(6L, false, "6 is not a power of 2"),
        PowerOfTwoVector(7L, false, "7 is not a power of 2"),
        PowerOfTwoVector(8L, true, "8 = 2^3 is a power of 2"),
        PowerOfTwoVector(16L, true, "16 = 2^4 is a power of 2"),
        PowerOfTwoVector(1024L, true, "1024 = 2^10 is a power of 2"),
        PowerOfTwoVector(1L shl 30, true, "2^30 is a power of 2"),
        PowerOfTwoVector(1L shl 62, true, "2^62 is a power of 2"),
        PowerOfTwoVector(-1L, false, "-1 is not a power of 2"),
        PowerOfTwoVector(Long.MIN_VALUE, false, "MIN_VALUE is not a power of 2")
    )

    /**
     * Mathematical constant validation vectors.
     * Values computed to 15 significant digits using reference implementations.
     */
    data class ConstantVector(
        val name: String,
        val expectedValue: Double,
        val tolerance: Double,
        val formula: String
    )

    val CONSTANT_VECTORS = listOf(
        ConstantVector(
            "PHI (Golden Ratio)",
            1.6180339887498949,
            1e-14,
            "(1 + sqrt(5)) / 2"
        ),
        ConstantVector(
            "EULER (e)",
            2.7182818284590452,
            1e-14,
            "lim(n->inf) (1 + 1/n)^n"
        ),
        ConstantVector(
            "PI",
            3.1415926535897932,
            1e-14,
            "Ratio of circumference to diameter"
        ),
        ConstantVector(
            "LN2 (ln(2))",
            0.6931471805599453,
            1e-14,
            "Natural logarithm of 2"
        ),
        ConstantVector(
            "SQRT2 (sqrt(2))",
            1.4142135623730951,
            1e-14,
            "Square root of 2"
        )
    )
}
