/**
 * MathUtilsTest - Unit tests for mathematical utilities
 * ISO 9001/8000 Compliant - Anti-bug-logic validation
 * 
 * Tests validate implementations against golden test vectors.
 * All expected values are computed from documented formulas,
 * NOT derived from OCR or external image sources.
 */
package com.vectras.vm.core

import com.google.common.truth.Truth.assertThat
import com.google.common.truth.Truth.assertWithMessage
import org.junit.Test
import kotlin.math.abs

class MathUtilsTest {

    // ========================================================================
    // Log2 Integer Tests
    // ========================================================================

    @Test
    fun `log2Int returns correct values for powers of 2`() {
        // Powers of 2 should return exact exponent
        assertThat(MathUtils.log2Int(1L)).isEqualTo(0)
        assertThat(MathUtils.log2Int(2L)).isEqualTo(1)
        assertThat(MathUtils.log2Int(4L)).isEqualTo(2)
        assertThat(MathUtils.log2Int(8L)).isEqualTo(3)
        assertThat(MathUtils.log2Int(16L)).isEqualTo(4)
        assertThat(MathUtils.log2Int(32L)).isEqualTo(5)
        assertThat(MathUtils.log2Int(64L)).isEqualTo(6)
        assertThat(MathUtils.log2Int(128L)).isEqualTo(7)
        assertThat(MathUtils.log2Int(256L)).isEqualTo(8)
        assertThat(MathUtils.log2Int(512L)).isEqualTo(9)
        assertThat(MathUtils.log2Int(1024L)).isEqualTo(10)
    }

    @Test
    fun `log2Int returns correct values for large powers of 2`() {
        assertThat(MathUtils.log2Int(1L shl 20)).isEqualTo(20)
        assertThat(MathUtils.log2Int(1L shl 30)).isEqualTo(30)
        assertThat(MathUtils.log2Int(1L shl 40)).isEqualTo(40)
        assertThat(MathUtils.log2Int(1L shl 50)).isEqualTo(50)
        assertThat(MathUtils.log2Int(1L shl 62)).isEqualTo(62)
    }

    @Test
    fun `log2Int returns floor for non-powers of 2`() {
        assertThat(MathUtils.log2Int(3L)).isEqualTo(1)  // floor(1.58) = 1
        assertThat(MathUtils.log2Int(5L)).isEqualTo(2)  // floor(2.32) = 2
        assertThat(MathUtils.log2Int(7L)).isEqualTo(2)  // floor(2.81) = 2
        assertThat(MathUtils.log2Int(15L)).isEqualTo(3) // floor(3.91) = 3
        assertThat(MathUtils.log2Int(1000L)).isEqualTo(9) // floor(9.97) = 9
        assertThat(MathUtils.log2Int(1023L)).isEqualTo(9)
        assertThat(MathUtils.log2Int(1025L)).isEqualTo(10)
    }

    @Test
    fun `log2Int returns -1 for invalid inputs`() {
        assertThat(MathUtils.log2Int(0L)).isEqualTo(-1)
        assertThat(MathUtils.log2Int(-1L)).isEqualTo(-1)
        assertThat(MathUtils.log2Int(-100L)).isEqualTo(-1)
        assertThat(MathUtils.log2Int(Long.MIN_VALUE)).isEqualTo(-1)
    }

    @Test
    fun `log2Int validates against all golden vectors`() {
        for (vector in GoldenTestVectors.LOG2_INT_VECTORS) {
            val result = MathUtils.log2Int(vector.input)
            assertWithMessage("${vector.description}: log2Int(${vector.input})")
                .that(result)
                .isEqualTo(vector.expected)
        }
    }

    // ========================================================================
    // Log2 Double Tests
    // ========================================================================

    @Test
    fun `log2 returns correct values for powers of 2`() {
        assertThat(MathUtils.log2(1.0)).isWithin(1e-10).of(0.0)
        assertThat(MathUtils.log2(2.0)).isWithin(1e-10).of(1.0)
        assertThat(MathUtils.log2(4.0)).isWithin(1e-10).of(2.0)
        assertThat(MathUtils.log2(8.0)).isWithin(1e-10).of(3.0)
        assertThat(MathUtils.log2(16.0)).isWithin(1e-10).of(4.0)
        assertThat(MathUtils.log2(1024.0)).isWithin(1e-10).of(10.0)
    }

    @Test
    fun `log2 returns NaN for invalid inputs`() {
        assertThat(MathUtils.log2(0.0)).isNaN()
        assertThat(MathUtils.log2(-1.0)).isNaN()
        assertThat(MathUtils.log2(-100.0)).isNaN()
    }

    // ========================================================================
    // Integer Square Root Tests
    // ========================================================================

    @Test
    fun `sqrtInt returns exact values for perfect squares`() {
        assertThat(MathUtils.sqrtInt(0L)).isEqualTo(0L)
        assertThat(MathUtils.sqrtInt(1L)).isEqualTo(1L)
        assertThat(MathUtils.sqrtInt(4L)).isEqualTo(2L)
        assertThat(MathUtils.sqrtInt(9L)).isEqualTo(3L)
        assertThat(MathUtils.sqrtInt(16L)).isEqualTo(4L)
        assertThat(MathUtils.sqrtInt(25L)).isEqualTo(5L)
        assertThat(MathUtils.sqrtInt(36L)).isEqualTo(6L)
        assertThat(MathUtils.sqrtInt(49L)).isEqualTo(7L)
        assertThat(MathUtils.sqrtInt(64L)).isEqualTo(8L)
        assertThat(MathUtils.sqrtInt(81L)).isEqualTo(9L)
        assertThat(MathUtils.sqrtInt(100L)).isEqualTo(10L)
        assertThat(MathUtils.sqrtInt(10000L)).isEqualTo(100L)
        assertThat(MathUtils.sqrtInt(1000000L)).isEqualTo(1000L)
    }

    @Test
    fun `sqrtInt returns floor for non-perfect squares`() {
        assertThat(MathUtils.sqrtInt(2L)).isEqualTo(1L)
        assertThat(MathUtils.sqrtInt(3L)).isEqualTo(1L)
        assertThat(MathUtils.sqrtInt(5L)).isEqualTo(2L)
        assertThat(MathUtils.sqrtInt(8L)).isEqualTo(2L)
        assertThat(MathUtils.sqrtInt(10L)).isEqualTo(3L)
        assertThat(MathUtils.sqrtInt(99L)).isEqualTo(9L)
        assertThat(MathUtils.sqrtInt(101L)).isEqualTo(10L)
    }

    @Test
    fun `sqrtInt handles large values`() {
        assertThat(MathUtils.sqrtInt(1L shl 20)).isEqualTo(1024L)
        assertThat(MathUtils.sqrtInt(1L shl 30)).isEqualTo(32768L)
        assertThat(MathUtils.sqrtInt(1L shl 40)).isEqualTo(1048576L)
    }

    @Test
    fun `sqrtInt returns -1 for negative input`() {
        assertThat(MathUtils.sqrtInt(-1L)).isEqualTo(-1L)
        assertThat(MathUtils.sqrtInt(-100L)).isEqualTo(-1L)
    }

    @Test
    fun `sqrtInt validates against all golden vectors`() {
        for (vector in GoldenTestVectors.SQRT_INT_VECTORS) {
            val result = MathUtils.sqrtInt(vector.input)
            assertWithMessage("${vector.description}: sqrtInt(${vector.input})")
                .that(result)
                .isEqualTo(vector.expected)
        }
    }

    // ========================================================================
    // CRC-32 Tests
    // ========================================================================

    @Test
    fun `crc32 returns 0 for empty input`() {
        assertThat(MathUtils.crc32(byteArrayOf())).isEqualTo(0x00000000L)
    }

    @Test
    fun `crc32 returns standard check value for 123456789`() {
        // This is the IEEE 802.3 standard check value
        val input = "123456789".toByteArray(Charsets.US_ASCII)
        assertThat(MathUtils.crc32(input)).isEqualTo(0xCBF43926L)
    }

    @Test
    fun `crc32 validates against all golden vectors`() {
        for (vector in GoldenTestVectors.CRC32_VECTORS) {
            val result = MathUtils.crc32(vector.input)
            assertWithMessage("${vector.description}")
                .that(result)
                .isEqualTo(vector.expected)
        }
    }

    // ========================================================================
    // Bit Rotation Tests
    // ========================================================================

    @Test
    fun `rotateLeft rotates bits correctly`() {
        assertThat(MathUtils.rotateLeft(0x0000000000000001L, 1))
            .isEqualTo(0x0000000000000002L)
        assertThat(MathUtils.rotateLeft(Long.MIN_VALUE, 1))
            .isEqualTo(0x0000000000000001L)
    }

    @Test
    fun `rotateRight rotates bits correctly`() {
        assertThat(MathUtils.rotateRight(0x0000000000000001L, 1))
            .isEqualTo(Long.MIN_VALUE)
        assertThat(MathUtils.rotateRight(Long.MIN_VALUE, 1))
            .isEqualTo(0x4000000000000000L)
    }

    @Test
    fun `rotate by 0 returns original value`() {
        val value = 0x0123456789ABCDEFL
        assertThat(MathUtils.rotateLeft(value, 0)).isEqualTo(value)
        assertThat(MathUtils.rotateRight(value, 0)).isEqualTo(value)
    }

    @Test
    fun `rotate by 64 returns original value`() {
        val value = 0x0123456789ABCDEFL
        assertThat(MathUtils.rotateLeft(value, 64)).isEqualTo(value)
        assertThat(MathUtils.rotateRight(value, 64)).isEqualTo(value)
    }

    @Test
    fun `rotate validates against all golden vectors`() {
        for (vector in GoldenTestVectors.ROTATE_VECTORS) {
            val leftResult = MathUtils.rotateLeft(vector.input, vector.bits)
            val rightResult = MathUtils.rotateRight(vector.input, vector.bits)

            assertWithMessage("${vector.description}: rotateLeft")
                .that(leftResult)
                .isEqualTo(vector.expectedLeft)
            assertWithMessage("${vector.description}: rotateRight")
                .that(rightResult)
                .isEqualTo(vector.expectedRight)
        }
    }

    // ========================================================================
    // Population Count Tests
    // ========================================================================

    @Test
    fun `popCount returns correct bit counts`() {
        assertThat(MathUtils.popCount(0L)).isEqualTo(0)
        assertThat(MathUtils.popCount(1L)).isEqualTo(1)
        assertThat(MathUtils.popCount(2L)).isEqualTo(1)
        assertThat(MathUtils.popCount(3L)).isEqualTo(2)
        assertThat(MathUtils.popCount(7L)).isEqualTo(3)
        assertThat(MathUtils.popCount(15L)).isEqualTo(4)
        assertThat(MathUtils.popCount(0xFFL)).isEqualTo(8)
        assertThat(MathUtils.popCount(-1L)).isEqualTo(64)
    }

    @Test
    fun `popCount validates against all golden vectors`() {
        for (vector in GoldenTestVectors.POPCOUNT_VECTORS) {
            val result = MathUtils.popCount(vector.input)
            assertWithMessage("${vector.description}")
                .that(result)
                .isEqualTo(vector.expected)
        }
    }

    // ========================================================================
    // Power of 2 Tests
    // ========================================================================

    @Test
    fun `isPowerOfTwo correctly identifies powers of 2`() {
        assertThat(MathUtils.isPowerOfTwo(1L)).isTrue()
        assertThat(MathUtils.isPowerOfTwo(2L)).isTrue()
        assertThat(MathUtils.isPowerOfTwo(4L)).isTrue()
        assertThat(MathUtils.isPowerOfTwo(8L)).isTrue()
        assertThat(MathUtils.isPowerOfTwo(1024L)).isTrue()
        assertThat(MathUtils.isPowerOfTwo(1L shl 30)).isTrue()
        assertThat(MathUtils.isPowerOfTwo(1L shl 62)).isTrue()
    }

    @Test
    fun `isPowerOfTwo correctly rejects non-powers of 2`() {
        assertThat(MathUtils.isPowerOfTwo(0L)).isFalse()
        assertThat(MathUtils.isPowerOfTwo(3L)).isFalse()
        assertThat(MathUtils.isPowerOfTwo(5L)).isFalse()
        assertThat(MathUtils.isPowerOfTwo(6L)).isFalse()
        assertThat(MathUtils.isPowerOfTwo(7L)).isFalse()
        assertThat(MathUtils.isPowerOfTwo(-1L)).isFalse()
    }

    @Test
    fun `isPowerOfTwo validates against all golden vectors`() {
        for (vector in GoldenTestVectors.POWER_OF_TWO_VECTORS) {
            val result = MathUtils.isPowerOfTwo(vector.input)
            assertWithMessage("${vector.description}")
                .that(result)
                .isEqualTo(vector.expected)
        }
    }

    // ========================================================================
    // Next Power of 2 Tests
    // ========================================================================

    @Test
    fun `nextPowerOfTwo rounds up correctly`() {
        assertThat(MathUtils.nextPowerOfTwo(0L)).isEqualTo(1L)
        assertThat(MathUtils.nextPowerOfTwo(1L)).isEqualTo(1L)
        assertThat(MathUtils.nextPowerOfTwo(2L)).isEqualTo(2L)
        assertThat(MathUtils.nextPowerOfTwo(3L)).isEqualTo(4L)
        assertThat(MathUtils.nextPowerOfTwo(4L)).isEqualTo(4L)
        assertThat(MathUtils.nextPowerOfTwo(5L)).isEqualTo(8L)
        assertThat(MathUtils.nextPowerOfTwo(1000L)).isEqualTo(1024L)
    }

    // ========================================================================
    // Alignment Tests
    // ========================================================================

    @Test
    fun `alignUp aligns to boundary correctly`() {
        assertThat(MathUtils.alignUp(0L, 4L)).isEqualTo(0L)
        assertThat(MathUtils.alignUp(1L, 4L)).isEqualTo(4L)
        assertThat(MathUtils.alignUp(4L, 4L)).isEqualTo(4L)
        assertThat(MathUtils.alignUp(5L, 4L)).isEqualTo(8L)
        assertThat(MathUtils.alignUp(100L, 16L)).isEqualTo(112L)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `alignUp throws for non-power-of-2 alignment`() {
        MathUtils.alignUp(10L, 3L)
    }

    // ========================================================================
    // Lerp and Clamp Tests
    // ========================================================================

    @Test
    fun `lerp interpolates correctly`() {
        assertThat(MathUtils.lerp(0.0, 10.0, 0.0)).isWithin(1e-10).of(0.0)
        assertThat(MathUtils.lerp(0.0, 10.0, 0.5)).isWithin(1e-10).of(5.0)
        assertThat(MathUtils.lerp(0.0, 10.0, 1.0)).isWithin(1e-10).of(10.0)
        assertThat(MathUtils.lerp(-10.0, 10.0, 0.5)).isWithin(1e-10).of(0.0)
    }

    @Test
    fun `clamp clamps values correctly`() {
        assertThat(MathUtils.clamp(5.0, 0.0, 10.0)).isWithin(1e-10).of(5.0)
        assertThat(MathUtils.clamp(-5.0, 0.0, 10.0)).isWithin(1e-10).of(0.0)
        assertThat(MathUtils.clamp(15.0, 0.0, 10.0)).isWithin(1e-10).of(10.0)
    }

    // ========================================================================
    // Mathematical Constants Tests
    // ========================================================================

    @Test
    fun `constants match expected values`() {
        // Verify constants match documented values with high precision
        assertThat(MathUtils.PHI).isWithin(1e-14).of(1.6180339887498949)
        assertThat(MathUtils.EULER).isWithin(1e-14).of(2.7182818284590452)
        assertThat(MathUtils.PI).isWithin(1e-14).of(3.1415926535897932)
        assertThat(MathUtils.LN2).isWithin(1e-14).of(0.6931471805599453)
        assertThat(MathUtils.SQRT2).isWithin(1e-14).of(1.4142135623730951)
    }

    @Test
    fun `constants validate against all golden vectors`() {
        for (vector in GoldenTestVectors.CONSTANT_VECTORS) {
            val actualValue = when (vector.name) {
                "PHI (Golden Ratio)" -> MathUtils.PHI
                "EULER (e)" -> MathUtils.EULER
                "PI" -> MathUtils.PI
                "LN2 (ln(2))" -> MathUtils.LN2
                "SQRT2 (sqrt(2))" -> MathUtils.SQRT2
                else -> throw IllegalArgumentException("Unknown constant: ${vector.name}")
            }

            assertWithMessage("${vector.name}: ${vector.formula}")
                .that(abs(actualValue - vector.expectedValue))
                .isLessThan(vector.tolerance)
        }
    }

    @Test
    fun `PHI satisfies golden ratio property`() {
        // Golden ratio property: PHI = 1 + 1/PHI
        val computed = 1.0 + 1.0 / MathUtils.PHI
        assertThat(MathUtils.PHI).isWithin(1e-14).of(computed)
    }

    @Test
    fun `PHI squared equals PHI plus 1`() {
        // PHI^2 = PHI + 1
        val phiSquared = MathUtils.PHI * MathUtils.PHI
        val phiPlusOne = MathUtils.PHI + 1.0
        assertThat(phiSquared).isWithin(1e-14).of(phiPlusOne)
    }

    // ========================================================================
    // Almost Equal Tests
    // ========================================================================

    @Test
    fun `almostEqual compares with epsilon correctly`() {
        assertThat(MathUtils.almostEqual(1.0, 1.0)).isTrue()
        assertThat(MathUtils.almostEqual(1.0, 1.0 + 1e-11)).isTrue()
        assertThat(MathUtils.almostEqual(1.0, 1.0 + 1e-9)).isFalse()
        assertThat(MathUtils.almostEqual(1.0, 1.1, 0.2)).isTrue()
    }

    // ========================================================================
    // FNV-1a Hash Tests
    // ========================================================================

    @Test
    fun `fnv1a64 produces consistent hashes`() {
        val data = "test".toByteArray()
        val hash1 = MathUtils.fnv1a64(data)
        val hash2 = MathUtils.fnv1a64(data)
        assertThat(hash1).isEqualTo(hash2)
    }

    @Test
    fun `fnv1a64 produces different hashes for different inputs`() {
        val hash1 = MathUtils.fnv1a64("hello".toByteArray())
        val hash2 = MathUtils.fnv1a64("world".toByteArray())
        assertThat(hash1).isNotEqualTo(hash2)
    }
}
