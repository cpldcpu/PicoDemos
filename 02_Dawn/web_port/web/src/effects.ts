/**
 * Text and blur effects
 * Port of Amiga text rendering and blur routines
 *
 * Original text: dawn_final.s:582-647
 * Original blur: dawn_final.s:653-670
 */

import { ChunkyBuffer } from './chunkyBuffer.js';
import { Palette } from './palette.js';

/**
 * Blur transition effect (screen melting)
 * Original: dawn_final.s:653-670
 */
/**
 * Blur transition effect for scene fadeouts
 * Original: dawn_final.s:499-509 (fadeout routine)
 *
 * The fadeout routine:
 * 1. Clears frame counter (wa.count) at start
 * 2. Loops for 70 frames calling only handle_screen and blur_it
 * 3. No rendering happens during fadeout - just blurs existing content
 */
export class BlurTransition {
    private isActive = false;
    private phase = 0;
    private readonly steps = 70;  // moveq #70,d0 at line 504
    private blurTable: Uint16Array;

    constructor() {
        this.blurTable = Palette.createBlurTable();
    }

    start(): void {
        this.isActive = true;
        this.phase = 0;  // clr.l wa.count at line 500
    }

    reset(): void {
        this.isActive = false;
        this.phase = 0;
    }

    get active(): boolean {
        return this.isActive;
    }

    /**
     * Apply blur transition using proper blur_it implementation.
     * Returns true when finished (after 70 frames).
     *
     * Original: dawn_final.s:653-670 (blur_it routine)
     * - Processes pixels as 16-bit words (pixel pairs)
     * - Averages current line with line BELOW
     * - Uses blur lookup table for color remapping
     * - Clears bottom line after processing
     */
    apply(buffer: ChunkyBuffer): boolean {
        if (!this.isActive) {
            return false;
        }

        // Apply blur effect using the proper blur_it routine
        // This uses the blur lookup table just like the original
        buffer.blurVertical(this.blurTable);

        this.phase += 1;
        if (this.phase >= this.steps) {
            this.reset();
            return true;
        }

        return false;
    }
}

/**
 * Text definitions from original demo
 * Original: dawn_final.s:631-646
 */
const TEXT_DAWN = [
    0x0000, 0x0000, 0xFEFE, 0x8282, 0x82FE, // Leading padding + D
    0x7C00, 0xFCFE, 0x1212, 0x12FE, // A
    0xFC00, 0x7EFE, 0x80E0, 0xE080, // W
    0xFE7E, 0x00FE, 0xFE02, 0x0202, // N
    0xFEFC,
];

const TEXT_BY = [
    0, 0, 0x0000, 0x1038, 0x1000, 0x0000, // by
    0x00FE, 0xFE92, 0x92FE, 0x6C00,
    0x8E9E, 0x9090, 0xFE7E, 0x0000,
    0x0000, 0x1038, 0x1000, 0x0000,
];

const TEXT_AZURE = [
    0, 0, 0x007E, 0x7F09, 0x7F7E, 0x0071, // Azure
    0x7949, 0x4F47, 0x003F, 0x7F40,
    0x407F, 0x3F00, 0x7F7F, 0x097F,
    0x7600, 0x7F7F, 0x4949, 0x4100, 0, 0,
];

export type TextType = 'dawn' | 'by' | 'azure';

/**
 * Text renderer with additive blending
 */
export class TextRenderer {
    private tmScrtab: Uint16Array;
    private rnd: number = 0;
    private glyphCache: Map<TextType, Uint8Array>;

    constructor() {
        this.tmScrtab = TextRenderer.createTmScrtab();
        this.glyphCache = new Map();
    }

    /**
     * Render text with additive blending
     * Original: dawn_final.s:582-625
     */
    render(buffer: ChunkyBuffer, text: TextType): void {
        const glyph = this.getGlyphBytes(text);
        const bufferData = buffer.buffer;
        const width = buffer.width;
        const totalPixels = bufferData.length;
        const tmScrtab = this.tmScrtab;
        const tmLength = tmScrtab.length;

        this.rnd = (this.rnd + 5) & 0xFFFF;
        let tmIndex = this.rnd & 0x3;
        const yOffset = ((((this.rnd >> 2) & 0x3) + 40) * width) | 0;

        let glyphIdx = 0;
        for (let row = 0; row <= 38; row++) {
            let glyphByte = 0;
            if (glyphIdx < glyph.length) {
                glyphByte = glyph[glyphIdx];
            }
            glyphIdx += 1;

            for (let block = 0; block < 4; block++) {
                const baseIndex = tmScrtab[tmIndex];
                tmIndex += 1;
                if (tmIndex >= tmLength) {
                    tmIndex = 0;
                }

                let pointer = baseIndex + yOffset;
                let mask = glyphByte;

                for (let bit = 0; bit < 8; bit++) {
                    let cursor = pointer;
                    if ((mask & 0x1) !== 0) {
                        for (let step = 0; step < 8; step++) {
                            if (cursor >= 0 && cursor < totalPixels) {
                                const newValue = bufferData[cursor] + 8;
                                bufferData[cursor] = newValue > 50 ? 50 : newValue;
                            }
                            cursor += width;
                        }
                    } else {
                        cursor += width * 8;
                    }
                    pointer = cursor;
                    mask >>>= 1;
                }
            }
        }
    }

    private getGlyphBytes(text: TextType): Uint8Array {
        const cached = this.glyphCache.get(text);
        if (cached) {
            return cached;
        }

        const words = this.getTextWords(text);
        const bytes = TextRenderer.wordsToBytes(words);
        this.glyphCache.set(text, bytes);
        return bytes;
    }

    private getTextWords(text: TextType): number[] {
        switch (text) {
            case 'dawn':
                return TEXT_DAWN;
            case 'by':
                return TEXT_BY;
            case 'azure':
                return TEXT_AZURE;
        }

        throw new Error(`Unknown text type: ${text as string}`);
    }

    private static wordsToBytes(words: readonly number[]): Uint8Array {
        const bytes = new Uint8Array(words.length * 2);
        let idx = 0;

        for (const word of words) {
            bytes[idx++] = (word >> 8) & 0xFF;
            bytes[idx++] = word & 0xFF;
        }

        return bytes;
    }

    private static createTmScrtab(): Uint16Array {
        const table = new Uint16Array(161);

        for (let value = 0; value <= 160; value++) {
            table[value] = value;
        }

        return table;
    }
}

/**
 * Blur effect controller
 */
export class BlurEffect {
    private blurTable: Uint16Array;

    constructor() {
        this.blurTable = Palette.createBlurTable();
    }

    /**
     * Apply vertical blur to chunky buffer
     * Original: dawn_final.s:653-670
     */
    apply(buffer: ChunkyBuffer): void {
        buffer.blurVertical(this.blurTable);
    }
}
