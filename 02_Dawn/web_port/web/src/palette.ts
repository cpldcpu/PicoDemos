/**
 * Palette system - 64 color palette generation
 * Port of Amiga palette management and color generation
 *
 * Original: dawn_final.s:982-1069
 */

/**
 * Color scheme definitions (from original)
 * Format: [red, green, blue] multipliers (0-65535 range)
 */
export const COLOR_SCHEMES = {
    colors1: [65000, 60000, 35000], // Main palette
    colors2: [65000, 50000, 35000], // "by" text
    colors3: [45000, 50000, 35000], // "azure" text
    colors4: [60000, 60000, 60000], // Finale (grayscale)
    colors5: [60000, 50000, 10000], // Alternate (golden)
};

export class Palette {
    // 64 RGB colors (stored as 0xRRGGBB)
    colors: Uint32Array;

    // Current color scheme
    private scheme: number[];

    constructor() {
        this.colors = new Uint32Array(64);
        this.scheme = COLOR_SCHEMES.colors1;
        this.generate();
    }

    /**
     * Generate 64-color palette based on current scheme
     * Port of makedacols routine: dawn_final.s:982-1023
     *
     * Uses the original fixed-point intensity curves for each channel.
     */
    generate(): void {
        const [baseR, baseG, baseB] = this.scheme;
        const colors = this.colors;

        for (let d7 = 63; d7 >= 0; d7--) {
            const d0 = (d7 ^ 0x3F) & 0xFFFF;

            let d3 = Math.imul(d0, d0) >>> 0;
            d3 >>>= 4;
            d3 = Math.imul(d3 & 0xFFFF, baseR) >>> 0;
            let d1 = d3 >>> 0;

            d3 = Math.imul(d0, d0) >>> 0;
            d3 = Math.imul(d3, d0) >>> 0;
            d3 >>>= 2;
            d3 >>>= 8;
            d3 = Math.imul(d3 & 0xFFFF, baseG) >>> 0;
            d3 >>>= 8;
            d1 = ((d1 & 0xFFFF0000) | (d3 & 0xFFFF)) >>> 0;

            d3 = (d0 << 2) >>> 0;
            d3 = Math.imul(d3 & 0xFFFF, baseB) >>> 0;
            d3 = ((d3 << 16) | (d3 >>> 16)) >>> 0;
            d1 = ((d1 & 0xFFFFFF00) | (d3 & 0xFF)) >>> 0;

            const red = Math.min(255, (d1 >>> 16) & 0xFFFF);
            const green = Math.min(255, (d1 >>> 8) & 0xFF);
            const blue = Math.min(255, d1 & 0xFF);

            colors[d0 & 0x3F] = ((red & 0xFF) << 16) | ((green & 0xFF) << 8) | (blue & 0xFF);
        }
    }

    /**
     * Set color scheme and regenerate palette
     */
    setScheme(schemeName: keyof typeof COLOR_SCHEMES): void {
        this.scheme = COLOR_SCHEMES[schemeName];
        this.generate();
    }

    /**
     * Get RGB color from palette index
     */
    getColor(index: number): number {
        return this.colors[index & 0x3F];
    }

    /**
     * Create blur lookup table
     * Original: dawn_final.s:60-76
     *
     * Maps 256*64 values with special wrapping logic
     */
    static createBlurTable(): Uint16Array {
        const length = 256 * 64;
        const table = new Uint16Array(length);

        for (let i = 0; i < length; i++) {
            let value = ((length - 1 - i) ^ 0x3FFF) & 0x3F3F;
            value = (value - 0x0202) & 0xFFFF;

            if ((value & 0x8000) !== 0) {
                value &= 0x00FF;
            }

            if ((value & 0x0080) !== 0) {
                value &= 0xFF00;
            }

            table[i] = value & 0xFFFF;
        }

        return table;
    }
}
