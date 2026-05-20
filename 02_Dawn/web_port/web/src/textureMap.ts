/**
 * Environment mapping and texture-mapped polygon renderer
 * Port of Amiga texture mapper with perspective correction
 *
 * Original environment map: dawn_final.s:917-959
 * Original texture mapper: dawn_final.s:1163-1351
 * Original edge setup: dawn_final.s:1186-1262
 */

import { ChunkyBuffer } from './chunkyBuffer.js';
import { ScreenCoord } from './vector3d.js';

/**
 * Environment map generator
 */
export class EnvironmentMap {
    map: Uint8Array;
    width: number = 256;
    height: number = 256;
    ppicSwitch: number = 0; // 0 = sphere, 1 = checkered

    constructor() {
        this.map = new Uint8Array(256 * 256);
        this.generate();
    }

    /**
     * Generate environment map
     * Original: dawn_final.s:917-959
     *
     * Creates either a spherical gradient or checkered pattern
     */
    generate(): void {
        const center = 128;

        for (let y = 0; y < 256; y++) {
            for (let x = 0; x < 256; x++) {
                const dx = x - center;
                const dy = y - center;

                // Calculate distance²
                let distSq = dx * dx + dy * dy;
                distSq = distSq >> 4;

                let colorVal: number;

                if (distSq > 0xFC) {
                    colorVal = 0xFE;
                } else {
                    colorVal = distSq;
                    // colorVal = (~distSq) & 0xFF;
                }

                colorVal = ~colorVal;


                if (this.ppicSwitch === 0) {
                    // Spherical gradient
                    colorVal = colorVal >> 2;
                } else {
                    // Checkered pattern
                    colorVal = colorVal & 0xFF;
                    colorVal = ((colorVal * 14500) >> 16) & 0xFF;

                    // Checkerboard test
                    const checker = x ^ y;
                    if ((checker & 0x20) === 0) {
                        colorVal = Math.min(63, colorVal + 7);
                    }
                }

                this.map[y * 256 + x] = colorVal & 0x3F;
            }
        }
    }

    sample(u: number, v: number, uOffset = 0, vOffset = 0): number {
        const x = wrapByte(Math.floor(u + uOffset + 64));
        const y = wrapByte(Math.floor(v + vOffset + 64));
        return this.map[y * 256 + x];
    }
}

function wrapByte(value: number): number {
    value = value % 256;
    if (value < 0) value += 256;
    return value;
}

function shortestDelta(a: number, b: number): number {
    const diff = wrapByte(b - a);
    return diff >= 128 ? diff - 256 : diff;
}

export class TextureMapper {
    private envMap: EnvironmentMap;
    private blurMode: boolean = true;
    private baseU: number = 0;
    private baseV: number = 0;
    private spanLeftX: Float32Array;
    private spanRightX: Float32Array;
    private spanLeftU: Float32Array;
    private spanRightU: Float32Array;
    private spanLeftV: Float32Array;
    private spanRightV: Float32Array;
    private readonly screenHeight: number;

    constructor(envMap: EnvironmentMap) {
        this.envMap = envMap;
        this.screenHeight = 128;
        this.spanLeftX = new Float32Array(this.screenHeight);
        this.spanRightX = new Float32Array(this.screenHeight);
        this.spanLeftU = new Float32Array(this.screenHeight);
        this.spanRightU = new Float32Array(this.screenHeight);
        this.spanLeftV = new Float32Array(this.screenHeight);
        this.spanRightV = new Float32Array(this.screenHeight);
    }

    setLightOffset(uOffset: number, vOffset: number): void {
        this.baseU = uOffset;
        this.baseV = vOffset;
    }

    /**
     * Render texture-mapped polygon
     * Original: dawn_final.s:1163-1351
     */
    renderPolygon(
        buffer: ChunkyBuffer,
        vertices: ScreenCoord[],
        uvCoords: { u: number; v: number }[]
    ): void {
        if (vertices.length < 3) return;

        const height = buffer.height;
        const width = buffer.width;

        let minY = height - 1;
        let maxY = 0;

        for (let i = 0; i < vertices.length; i++) {
            const y = vertices[i].y;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }

        if (maxY < 0 || minY >= height) return;

        const minYInt = Math.max(0, Math.floor(minY));
        const maxYInt = Math.min(height - 1, Math.ceil(maxY));

        for (let y = minYInt; y <= maxYInt; y++) {
            this.spanLeftX[y] = Infinity;
            this.spanRightX[y] = -Infinity;
        }

        const count = vertices.length;
        for (let i = 0; i < count; i++) {
            const next = (i + 1) % count;
            let x0 = vertices[i].x;
            let y0 = vertices[i].y;
            let u0 = wrapByte(uvCoords[i].u);
            let v0 = wrapByte(uvCoords[i].v);

            let x1 = vertices[next].x;
            let y1 = vertices[next].y;
            let u1 = wrapByte(uvCoords[next].u);
            let v1 = wrapByte(uvCoords[next].v);

            if (y0 === y1) continue;

            if (y0 > y1) {
                [x0, x1] = [x1, x0];
                [y0, y1] = [y1, y0];
                [u0, u1] = [u1, u0];
                [v0, v1] = [v1, v0];
            }

            const dy = y1 - y0;
            const dx = (x1 - x0) / dy;
            const du = shortestDelta(u0, u1) / dy;
            const dv = shortestDelta(v0, v1) / dy;

            let yStart = Math.max(minYInt, Math.ceil(y0));
            let yEnd = Math.min(maxYInt, Math.ceil(y1) - 1);

            if (yStart > yEnd) continue;

            let x = x0 + dx * (yStart - y0);
            let u = wrapByte(u0 + du * (yStart - y0));
            let v = wrapByte(v0 + dv * (yStart - y0));

            for (let y = yStart; y <= yEnd; y++) {
                if (x < this.spanLeftX[y]) {
                    this.spanLeftX[y] = x;
                    this.spanLeftU[y] = wrapByte(u);
                    this.spanLeftV[y] = wrapByte(v);
                }
                if (x > this.spanRightX[y]) {
                    this.spanRightX[y] = x;
                    this.spanRightU[y] = wrapByte(u);
                    this.spanRightV[y] = wrapByte(v);
                }
                x += dx;
                u = wrapByte(u + du);
                v = wrapByte(v + dv);
            }
        }

        const bufferData = buffer.buffer;

        for (let y = minYInt; y <= maxYInt; y++) {
            let leftX = this.spanLeftX[y];
            let rightX = this.spanRightX[y];

            if (leftX === Infinity || rightX === -Infinity || rightX <= leftX) {
                continue;
            }

            let startX = Math.max(0, Math.floor(leftX));
            let endX = Math.min(width - 1, Math.ceil(rightX));

            if (endX <= startX) continue;

            const spanLength = rightX - leftX;
            const uStart = wrapByte(this.spanLeftU[y]);
            const vStart = wrapByte(this.spanLeftV[y]);
            const uEnd = wrapByte(this.spanRightU[y]);
            const vEnd = wrapByte(this.spanRightV[y]);

            const uStep = spanLength !== 0 ? shortestDelta(uStart, uEnd) / spanLength : 0;
            const vStep = spanLength !== 0 ? shortestDelta(vStart, vEnd) / spanLength : 0;

            let u = wrapByte(uStart + uStep * (startX - leftX));
            let v = wrapByte(vStart + vStep * (startX - leftX));

            let offset = y * width + startX;

            for (let x = startX; x <= endX; x++, offset++) {
                const texColor = this.envMap.sample(u, v, this.baseU, this.baseV);

                if (this.blurMode) {
                    if (texColor > bufferData[offset]) {
                        bufferData[offset] = texColor;
                    }
                } else {
                    bufferData[offset] = texColor;
                }

                u = wrapByte(u + uStep);
                v = wrapByte(v + vStep);
            }
        }
    }

    setBlurMode(enabled: boolean): void {
        this.blurMode = enabled;
    }
}
