/**
 * Voxel landscape renderer
 * Port of Amiga voxel raycasting effect
 *
 * Original: dawn_final.s:517-575
 * Original data generation: dawn_final.s:1494-1545
 */

import { ChunkyBuffer } from './chunkyBuffer.js';
import { mathTables } from './mathTables.js';

/**
 * Voxel landscape with heightmap and colormap
 */
export class VoxelLandscape {
    heightMap: Uint8Array;
    colorMap: Uint8Array;
    shadeTable: Uint8Array;

    width: number = 256;
    height: number = 256;

    // Camera position
    yPos: number = 0;

    constructor() {
        this.heightMap = new Uint8Array(65536);
        this.colorMap = new Uint8Array(65536);
        this.shadeTable = new Uint8Array(65536);

        this.generate();
    }

    /**
     * Generate voxel landscape data
     * Original: dawn_final.s:1494-1545
     *
     * Creates heightmap and colormap using sine waves
     */
    generate(): void {
        const sin1024 = mathTables.sinTable;

        // Assembly loops count DOWN: d7 from 255→0, d6 from 255→0
        // Index = (d7 << 8) | d6
        for (let d7 = 255; d7 >= 0; d7--) {
            for (let d6 = 255; d6 >= 0; d6--) {
                const index = (d7 << 8) | d6;
                const y = d7;  // Y coordinate
                const x = d6;  // X coordinate

                // Calculate height using two overlapping sine waves
                // First wave: centered at (255-31, 255-31)
                let dx1 = x - (255 - 31);
                let dy1 = y - (255 - 31);
                let distSq1 = dx1 * dx1 + dy1 * dy1;
                distSq1 = distSq1 >> 4;

                const sinIdx1 = distSq1 & 0x3FF;
                let height1 = sin1024[sinIdx1];
                height1 = height1 >> 5;

                // Second wave: centered at (31, 31)
                let dx2 = x - 31;
                let dy2 = y - 31;
                let distSq2 = dx2 * dx2 + dy2 * dy2;
                distSq2 = distSq2 >> 5;

                const sinIdx2 = distSq2 & 0x3FF;
                let height2 = sin1024[sinIdx2];
                height2 = height2 >> 5;

                // Combine heights
                let combinedHeight = height1 + height2;  // Save original (a5 in assembly)
                let finalHeight = (combinedHeight >> 7) + 64;

                // Clamp heightmap value
                if (finalHeight < 0) finalHeight = 0;
                if (finalHeight > 255) finalHeight = 255;

                this.heightMap[index] = finalHeight;

                // Calculate color using ORIGINAL combined height (not processed)
                // Assembly lines 1536-1540: uses a5 (original), not processed height
                const slope = combinedHeight - 31;
                let color = (slope >> 6) + 32;
                color ^= 0x3F; // Invert

                this.colorMap[index] = color & 0x3F;
            }
        }

        // Generate shade table (for lighting based on height)
        // Original: dawn_final.s:1547-1563
        // d7 counts from 64 down to 0, d6 counts from 255 down to 0
        let tableIdx = 0;
        for (let d7 = 64; d7 >= 0; d7--) {
            for (let d6 = 255; d6 >= 0; d6--) {
                const d1 = (d6 & 0x7F) >> 3;
                let d0 = d7 - d1;
                if (d0 < 0) d0 = 0;

                this.shadeTable[tableIdx++] = d0 & 0x3F;
            }
        }
    }

    /**
     * Render voxel landscape using column raycasting
     * Original: dawn_final.s:517-575
     */
    render(buffer: ChunkyBuffer, frameCount: number): void {
        // Animate Y position (line 530)
        this.yPos = (frameCount & 0xFF);

        const xStart = (128 - 80) * 256;  // Line 538
        const yAdd = -80;                  // Line 537

        let xPos = xStart;

        // For each column (160 columns)
        for (let col = 0; col < 160; col++) {
            let yPointer = this.yPos;      // d2 = ypo
            let xPointer = xPos;            // d0 = xpo

            // Current screen Y position (starts at 64, line 545)
            let d4 = 64;

            // Distance counter (counts down from 99, line 546)
            let d6 = 99;

            // Screen row pointer (a3) starts at row 127 for each column (line 544)
            let screenRow = 127;

            // Raycast loop (.ilop, lines 547-566)
            while (d6 >= 0) {
                // Line 548-549: Combine xPointer (upper byte) with yPointer (lower byte)
                const lookupIdx = ((xPointer >> 8) << 8) | (yPointer & 0xFF);

                // Line 552: Read height from heightmap (byte)
                const d5 = this.heightMap[lookupIdx & 0xFFFF];

                // Line 553: Read color from colormap as WORD (2 bytes)
                // move (a2,d3.w),d7 reads 16-bit word
                const colorIdx = lookupIdx & 0xFFFF;
                const colorByte1 = this.colorMap[colorIdx];
                const colorByte2 = this.colorMap[(colorIdx + 1) & 0xFFFF];
                let d7 = (colorByte1 << 8) | colorByte2;

                // Line 554: Replace lower byte with distance counter
                // move.b d6,d7
                d7 = (d7 & 0xFF00) | (d6 & 0xFF);

                // Line 555: Apply shade table
                const shadedColor = this.shadeTable[d7 & 0xFFFF];

                // Drawing loop (.ee, lines 556-563)
                // Draw upward while d4 < d5 (height not yet reached)
                // Screen row pointer (a3) continues from previous iteration
                while (true) {
                    // Line 557-559: if (d4 == d5 || d4 > d5) break
                    if (d4 === d5 || d4 > d5) {
                        break;
                    }

                    // Line 560: Draw pixel at current screen row (a3)
                    if (screenRow >= 0 && screenRow < 128) {
                        buffer.setPixel(col, screenRow, shadedColor & 0x3F);
                    }

                    // Line 561: Move up one scanline (a3 -= 160)
                    screenRow--;

                    // Line 562: Increment height counter
                    d4++;
                }

                // Line 565: Decrement d4 after drawing
                d4--;

                // Line 550-551: Step through heightmap
                xPointer += yAdd;
                yPointer++;

                // Line 566: Decrement distance counter
                d6--;
            }

            // Line 569: Move to next column
            xPos += 256;
        }
    }
}
