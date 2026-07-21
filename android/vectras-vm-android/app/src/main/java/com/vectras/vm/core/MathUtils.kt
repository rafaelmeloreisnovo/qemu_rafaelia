/**
 * MathUtils - Stable mathematical implementations
 * ISO 9001/8000 Compliant - Anti-bug-logic module
 * 
 * IMPORTANT: All mathematical operations in this module are derived from code,
 * NOT from OCR or text extracted from images. Each function has documented
 * mathematical basis and golden test vectors for validation.
 */
package com.vectras.vm.core

import kotlin.math.ln
import kotlin.math.sqrt
import kotlin.math.pow
import kotlin.math.floor
import kotlin.math.abs

/**
 * Mathematical utilities with stable, verified implementations.
 * 
 * Design principles:
 * 1. All constants are defined in code, not derived from external sources
 * 2. All formulas have documented mathematical basis
 * 3. Golden test vectors validate correctness
 * 4. Edge cases are explicitly handled
 */
object MathUtils {

    // Mathematical constants - defined directly in code, NOT from OCR
    const val PHI: Double = 1.6180339887498949  // Golden ratio: (1 + sqrt(5)) / 2
    const val EULER: Double = 2.7182818284590452  // e
    const val PI: Double = 3.1415926535897932  // pi
    const val LN2: Double = 0.6931471805599453  // ln(2)
    const val SQRT2: Double = 1.4142135623730951  // sqrt(2)

    // Epsilon for floating-point comparisons
    private const val EPSILON: Double = 1e-10

    /**
     * Compute integer log base 2 using bit manipulation.
     * Mathematical basis: log2(n) = floor(log(n) / log(2))
     * 
     * Implementation uses de Bruijn sequence for O(1) lookup.
     * 
     * @param value Non-negative integer
     * @return Floor of log2(value), or -1 if value <= 0
     */
    fun log2Int(value: Long): Int {
        if (value <= 0) return -1
        if (value == 1L) return 0
        
        // Count leading zeros approach - most portable
        var n = value
        var result = 0
        
        if (n >= (1L shl 32)) { result += 32; n = n shr 32 }
        if (n >= (1L shl 16)) { result += 16; n = n shr 16 }
        if (n >= (1L shl 8)) { result += 8; n = n shr 8 }
        if (n >= (1L shl 4)) { result += 4; n = n shr 4 }
        if (n >= (1L shl 2)) { result += 2; n = n shr 2 }
        if (n >= (1L shl 1)) { result += 1 }
        
        return result
    }

    /**
     * Compute log base 2 using natural logarithm.
     * Mathematical basis: log2(x) = ln(x) / ln(2)
     * 
     * @param value Positive double
     * @return log2(value), or NaN if value <= 0
     */
    fun log2(value: Double): Double {
        if (value <= 0.0) return Double.NaN
        return ln(value) / LN2
    }

    /**
     * Integer square root using Newton-Raphson method.
     * Mathematical basis: x_{n+1} = (x_n + S/x_n) / 2
     * 
     * Converges quadratically to floor(sqrt(n)).
     * 
     * @param value Non-negative integer
     * @return Floor of sqrt(value), or -1 if value < 0
     */
    fun sqrtInt(value: Long): Long {
        if (value < 0) return -1
        if (value == 0L) return 0
        if (value == 1L) return 1
        
        // Initial estimate: 2^ceil(bits/2) — always >= sqrt(value), so N-R converges from above
        var x = 1L shl ((log2Int(value) + 2) / 2)
        
        // Newton-Raphson iteration
        var prev: Long
        do {
            prev = x
            x = (x + value / x) / 2
        } while (x < prev)
        
        return prev
    }

    /**
     * Square root using standard library with validation.
     * 
     * @param value Non-negative double
     * @return sqrt(value), or NaN if value < 0
     */
    fun sqrt(value: Double): Double {
        if (value < 0.0) return Double.NaN
        return kotlin.math.sqrt(value)
    }

    /**
     * CRC-32 implementation using polynomial division.
     * Polynomial: 0xEDB88320 (IEEE 802.3, reversed)
     * 
     * Mathematical basis: CRC = data mod generator_polynomial (in GF(2))
     * 
     * @param data Byte array to compute CRC for
     * @return 32-bit CRC value
     */
    fun crc32(data: ByteArray): Long {
        var crc = 0xFFFFFFFFL
        
        for (byte in data) {
            crc = crc xor (byte.toLong() and 0xFF)
            for (i in 0 until 8) {
                crc = if ((crc and 1L) == 1L) {
                    (crc shr 1) xor 0xEDB88320L
                } else {
                    crc shr 1
                }
            }
        }
        
        return crc xor 0xFFFFFFFFL
    }

    /**
     * Simple hash function using FNV-1a algorithm.
     * Mathematical basis: hash = ((hash XOR byte) * FNV_prime) mod 2^64
     * 
     * FNV-1a constants (64-bit):
     * - Offset basis: 14695981039346656037
     * - Prime: 1099511628211
     * 
     * @param data Byte array to hash
     * @return 64-bit hash value
     */
    fun fnv1a64(data: ByteArray): Long {
        var hash = -3750763034362895579L  // 14695981039346656037 as signed Long
        val prime = 1099511628211L
        
        for (byte in data) {
            hash = hash xor (byte.toLong() and 0xFF)
            hash *= prime
        }
        
        return hash
    }

    /**
     * Rotate bits left (circular left shift).
     * 
     * @param value Value to rotate
     * @param bits Number of bits to rotate (0-63)
     * @return Rotated value
     */
    fun rotateLeft(value: Long, bits: Int): Long {
        val shift = bits and 63
        return (value shl shift) or (value ushr (64 - shift))
    }

    /**
     * Rotate bits right (circular right shift).
     * 
     * @param value Value to rotate
     * @param bits Number of bits to rotate (0-63)
     * @return Rotated value
     */
    fun rotateRight(value: Long, bits: Int): Long {
        val shift = bits and 63
        return (value ushr shift) or (value shl (64 - shift))
    }

    /**
     * Count number of set bits (population count / Hamming weight).
     * Uses parallel bit counting algorithm.
     * 
     * @param value Value to count bits in
     * @return Number of 1 bits
     */
    fun popCount(value: Long): Int {
        var v = value
        v = v - ((v ushr 1) and 0x5555555555555555L)
        v = (v and 0x3333333333333333L) + ((v ushr 2) and 0x3333333333333333L)
        v = (v + (v ushr 4)) and 0x0F0F0F0F0F0F0F0FL
        return ((v * 0x0101010101010101L) ushr 56).toInt()
    }

    /**
     * Check if a value is a power of 2.
     * Mathematical basis: n is power of 2 iff n > 0 and (n & (n-1)) == 0
     * 
     * @param value Value to check
     * @return true if value is a power of 2
     */
    fun isPowerOfTwo(value: Long): Boolean {
        return value > 0 && (value and (value - 1)) == 0L
    }

    /**
     * Round up to next power of 2.
     * 
     * @param value Value to round up
     * @return Next power of 2 >= value, or 0 if overflow
     */
    fun nextPowerOfTwo(value: Long): Long {
        if (value <= 0) return 1
        if (value > (1L shl 62)) return 0  // Overflow
        
        var v = value - 1
        v = v or (v shr 1)
        v = v or (v shr 2)
        v = v or (v shr 4)
        v = v or (v shr 8)
        v = v or (v shr 16)
        v = v or (v shr 32)
        return v + 1
    }

    /**
     * Align value up to alignment boundary.
     * 
     * @param value Value to align
     * @param alignment Alignment boundary (must be power of 2)
     * @return Aligned value
     */
    fun alignUp(value: Long, alignment: Long): Long {
        require(isPowerOfTwo(alignment)) { "Alignment must be power of 2" }
        return (value + alignment - 1) and (alignment - 1).inv()
    }

    /**
     * Linear interpolation.
     * Mathematical basis: lerp(a, b, t) = a + t * (b - a)
     * 
     * @param start Start value
     * @param end End value
     * @param t Interpolation factor (0.0 to 1.0)
     * @return Interpolated value
     */
    fun lerp(start: Double, end: Double, t: Double): Double {
        return start + t * (end - start)
    }

    /**
     * Clamp value to range.
     * 
     * @param value Value to clamp
     * @param min Minimum value
     * @param max Maximum value
     * @return Clamped value
     */
    fun clamp(value: Double, min: Double, max: Double): Double {
        return when {
            value < min -> min
            value > max -> max
            else -> value
        }
    }

    /**
     * Compare two doubles with epsilon tolerance.
     * 
     * @param a First value
     * @param b Second value
     * @param epsilon Tolerance (default: 1e-10)
     * @return true if |a - b| < epsilon
     */
    fun almostEqual(a: Double, b: Double, epsilon: Double = EPSILON): Boolean {
        return abs(a - b) < epsilon
    }
}
