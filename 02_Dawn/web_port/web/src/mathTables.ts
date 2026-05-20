/**
 * Mathematical lookup tables
 * Port of Amiga sine, division, and normalization tables
 *
 * Original sine generation: dawn_final.s:1413-1455
 * Original division table: dawn_final.s:1388-1404
 * Original normalization: dawn_final.s:1466-1492
 */

export class MathTables {
    // Sine/cosine table: 1024 entries with wraparound (total 8192 values)
    // Values are signed 16-bit (-32768 to +32767)
    sinTable: Int16Array;

    // Division table: 256x256 for fast fixed-point division
    // Format: (y/x)*256
    divTable: Int16Array;

    // Vector normalization table: length² -> multiplier for length 128
    normTable: Uint16Array;

    constructor() {
        this.sinTable = new Int16Array(1024 * 8);
        this.divTable = new Int16Array(256 * 256);
        this.normTable = new Uint16Array(65536);

        this.generateSinTable();
        this.generateDivTable();
        this.generateNormTable();
    }

    /**
     * Generate sine table using Taylor series
     * Original: dawn_final.s:1413-1455
     *
     * Taylor series: sin(x) ≈ x - x³/6 + x⁵/120
     * Creates 256 values per quadrant, then mirrors for other quadrants
     */
    private generateSinTable(): void {
        let phase = 0;

        // Generate first quadrant (0 to π/2)
        for (let i = 0; i < 256; i++) {
            // Extract high word of phase
            const x = (phase >> 16) & 0xFFFF;

            // Taylor series calculation
            let sinVal = x;

            // x² term
            let x2 = (x * x) >> 16;
            x2 = (x2 << 4) & 0xFFFF;

            // x³/6
            let x3 = (x2 * x) / 6144;
            sinVal -= x3;

            // x⁵/120
            let x5 = (x2 * x3) / 20480;
            sinVal += x5;

            // Scale and store
            sinVal = (sinVal >> 2) & 0xFFFF;
            sinVal = (sinVal * 63) & 0xFFFF;

            // Convert to signed 16-bit
            if (sinVal > 32767) sinVal -= 65536;

            this.sinTable[i] = sinVal;
            this.sinTable[511 - i] = sinVal; // Mirror for quadrant 2

            // Quadrants 3 and 4 (negative)
            this.sinTable[i + 512] = -sinVal;
            this.sinTable[1023 - i] = -sinVal;

            // Increment phase (823550 is π/2 in 16.16 fixed point)
            phase += 823550;
        }

        // Create wraparound copies (for easy indexing without modulo)
        // Original creates 8 copies total
        for (let copy = 1; copy < 8; copy++) {
            for (let i = 0; i < 1024; i++) {
                this.sinTable[copy * 1024 + i] = this.sinTable[i];
            }
        }
    }

    /**
     * Get sine value (input: 0-1023 for 0-2π)
     */
    sin(angle: number): number {
        return this.sinTable[angle & 0x3FF];
    }

    /**
     * Get cosine value (input: 0-1023 for 0-2π)
     */
    cos(angle: number): number {
        return this.sinTable[(angle + 256) & 0x3FF];
    }

    /**
     * Generate division table for fast fixed-point division
     * Original: dawn_final.s:1388-1404
     *
     * Creates table for (y/x)*256 for all 8-bit x,y pairs
     */
    private generateDivTable(): void {
        for (let divisor = 0; divisor < 256; divisor++) {
            if (divisor === 0) {
                // Skip division by zero
                for (let dividend = 0; dividend < 256; dividend++) {
                    this.divTable[divisor * 256 + dividend] = 0;
                }
                continue;
            }

            for (let dividend = 0; dividend < 256; dividend++) {
                // Sign-extend to 8-bit signed
                let div = dividend;
                if (div > 127) div -= 256;

                // Calculate (dividend / divisor) * 256
                const result = ((div << 8) / divisor) | 0;

                this.divTable[divisor * 256 + dividend] = result;
            }
        }
    }

    /**
     * Fast division lookup
     * Input: high byte = divisor, low byte = dividend
     */
    div(divisor: number, dividend: number): number {
        const index = ((divisor & 0xFF) << 8) | (dividend & 0xFF);
        return this.divTable[index];
    }

    /**
     * Generate vector normalization table
     * Original: dawn_final.s:1466-1492
     *
     * For each length² value, compute multiplier to achieve length 128
     * Uses integer square root algorithm
     */
    private generateNormTable(): void {
        let sqrtVal = 1;
        let oddSum = 1;
        let sumOfOdds = 0;

        this.normTable[0] = 0;
        this.normTable[1] = 0;

        for (let lengthSq = 1; lengthSq < 50000; lengthSq++) {
            const target = lengthSq << 8;

            // Integer square root using odd number sum method
            while (sumOfOdds < target) {
                sumOfOdds += oddSum;
                oddSum += 2;
                sqrtVal++;
            }

            // Calculate multiplier: (127 << 16) / (sqrtVal << 4)
            const divisor = sqrtVal << 4;
            const multiplier = divisor > 0 ? ((127 << 16) / divisor) | 0 : 0;

            if (lengthSq < 65536) {
                this.normTable[lengthSq] = Math.min(65535, Math.max(0, multiplier));
            }
        }

        // Fill remaining values with safe defaults
        for (let i = 50000; i < 65536; i++) {
            this.normTable[i] = this.normTable[49999];
        }
    }

    /**
     * Get normalization multiplier for length²
     */
    getNormMultiplier(lengthSquared: number): number {
        const index = Math.min(65535, Math.max(0, lengthSquared));
        return this.normTable[index];
    }

    /**
     * Normalize a 3D vector to length 128
     */
    normalize(x: number, y: number, z: number): { x: number; y: number; z: number } {
        const lengthSq = ((x * x + y * y + z * z) >> 4) & 0xFFFF;
        const multiplier = this.getNormMultiplier(lengthSq);

        return {
            x: ((x * multiplier) >> 8) & 0xFFFF,
            y: ((y * multiplier) >> 8) & 0xFFFF,
            z: ((z * multiplier) >> 8) & 0xFFFF,
        };
    }
}

// Global instance (initialized once)
export const mathTables = new MathTables();
