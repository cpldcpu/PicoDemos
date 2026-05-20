/**
 * Chunky buffer system - Direct pixel manipulation
 * Port of Amiga chunky buffer rendering
 *
 * Original: 160x128 pixels, 6-bit palette (0-63)
 * Each pixel is a palette index, converted to RGB via lookup table
 */

export class ChunkyBuffer {
    width: number;
    height: number;
    buffer: Uint8Array;
    private wordWidth: number;
    private wordBuffer: Uint16Array;

    // For Canvas rendering
    imageData: ImageData;
    rgbBuffer: Uint8ClampedArray;

    constructor(width: number = 160, height: number = 128) {
        this.width = width;
        this.height = height;

        // Chunky buffer - palette indices (0-63)
        this.buffer = new Uint8Array(width * height);
        this.wordWidth = width >> 1;
        this.wordBuffer = new Uint16Array(this.buffer.buffer);

        // RGB output buffer for Canvas
        this.imageData = new ImageData(width, height);
        this.rgbBuffer = this.imageData.data;
    }

    /**
     * Clear buffer to specified palette index
     */
    clear(colorIndex: number = 0): void {
        const value = colorIndex & 0x3F;
        if (value === 0) {
            this.wordBuffer.fill(0);
        } else {
            this.buffer.fill(value);
        }
    }

    /**
     * Set pixel in chunky buffer
     */
    setPixel(x: number, y: number, colorIndex: number): void {
        if (x >= 0 && x < this.width && y >= 0 && y < this.height) {
            this.buffer[y * this.width + x] = colorIndex & 0x3F; // Clamp to 6-bit
        }
    }

    /**
     * Get pixel from chunky buffer
     */
    getPixel(x: number, y: number): number {
        if (x >= 0 && x < this.width && y >= 0 && y < this.height) {
            return this.buffer[y * this.width + x];
        }
        return 0;
    }

    /**
     * Convert chunky buffer to RGB using palette
     * This replaces the Amiga's chunky-to-planar conversion
     *
     * Original C2P: dawn_final.s:379-480
     * We skip this and go directly to RGB
     */
    convertToRGB(palette: Uint32Array): void {
        const len = this.buffer.length;
        let rgbIdx = 0;

        for (let i = 0; i < len; i++) {
            const paletteIndex = this.buffer[i] & 0x3F; // Ensure 6-bit
            const rgb = palette[paletteIndex] || 0;

            // Extract RGB from 32-bit color (0x00RRGGBB)
            this.rgbBuffer[rgbIdx++] = (rgb >> 16) & 0xFF; // R
            this.rgbBuffer[rgbIdx++] = (rgb >> 8) & 0xFF;  // G
            this.rgbBuffer[rgbIdx++] = rgb & 0xFF;         // B
            this.rgbBuffer[rgbIdx++] = 255;                // A
        }
    }

    /**
     * Get ImageData for Canvas rendering
     */
    getImageData(): ImageData {
        return this.imageData;
    }

    /**
     * Get raw buffer offset for scanline rendering
     */
    getOffset(x: number, y: number): number {
        return y * this.width + x;
    }

    /**
     * Vertical blur effect using paired-pixel lookup
     * Original: dawn_final.s:653-670
     */
    blurVertical(blurTable: Uint16Array): void {
        const widthWords = this.wordWidth;
        const rows = this.height - 1;
        const words = this.wordBuffer;

        for (let row = 0; row < rows; row++) {
            const topIndex = row * widthWords;
            const bottomIndex = topIndex + widthWords;
            for (let col = 0; col < widthWords; col++) {
                const sum = (words[topIndex + col] + words[bottomIndex + col]) >>> 1;
                words[topIndex + col] = blurTable[sum & 0x3FFF];
            }
        }

        const tailStart = widthWords * (this.height - 1);
        words.fill(0, tailStart);
    }
}
