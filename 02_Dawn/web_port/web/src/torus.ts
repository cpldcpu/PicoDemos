import { Vector3D } from './vector3d.js';

const MORPH_COUNT = 40;
const CIRC_SEG = 32;
const ROUN_SEG = 8;
const TOTAL_VERTICES = CIRC_SEG * ROUN_SEG;
const POINT_STRIDE = 4;
const SIN_TABLE = buildSinTable();
const SIN_MASK = SIN_TABLE.length - 1;
const OUTER_STEP = (1024 / ROUN_SEG) | 0;
const INNER_STEP = (1024 / CIRC_SEG) | 0;
const POSITION_SCALE = 1 / 8;
const NORMAL_SCALE = 1 / 128;

export interface TorusVariation {
    innerStep: number;
    amplitude: number;
    phase: number;
}

export interface MorphGenerationOptions {
    baseInnerStep?: number;
    basePhase?: number;
    startAmplitude?: number;
    maxAmplitude?: number;
    amplitudeIncrement?: number;
    variations?: TorusVariation[];
}

export interface TorusOptions {
    morph?: MorphGenerationOptions;
}

export interface Polygon {
    vertexCount: number;
    vertices: number[];
}

export interface ObjectData {
    vertices: Vector3D[];
    normals: Vector3D[];
    polygons: Polygon[];
    faceNormals: Vector3D[];
}

export class Torus {
    objectData: ObjectData;
    private frames: Vector3D[][];
    private normalFrames: Vector3D[][];
    private frameCount: number;

    constructor(options?: TorusOptions) {
        const polygons = buildPolygons();
        const morphData = generateMorphFrames(polygons, options?.morph);

        this.frames = morphData.frames;
        this.normalFrames = morphData.normals;
        this.frameCount = this.frames.length;

        this.objectData = {
            vertices: new Array<Vector3D>(TOTAL_VERTICES),
            normals: new Array<Vector3D>(TOTAL_VERTICES),
            polygons,
            faceNormals: [],
        };

        this.setFrame(0);
    }

    setFrame(index: number): void {
        if (this.frameCount === 0) {
            return;
        }
        const wrapped = ((index % this.frameCount) + this.frameCount) % this.frameCount;
        const verts = this.frames[wrapped];
        const norms = this.normalFrames[wrapped];

        for (let i = 0; i < TOTAL_VERTICES; i++) {
            this.objectData.vertices[i] = { ...verts[i] };
            this.objectData.normals[i] = { ...norms[i] };
        }
    }

    getFrameCount(): number {
        return this.frameCount;
    }
}

function buildPolygons(): Polygon[] {
    const polygons: Polygon[] = [];
    for (let r = 0; r < ROUN_SEG; r++) {
        const baseIdx = r * CIRC_SEG;
        const nextRoundIdx = ((r + 1) % ROUN_SEG) * CIRC_SEG;

        for (let c = 0; c < CIRC_SEG; c++) {
            const nextCirc = (c + 1) % CIRC_SEG;
            const p1 = baseIdx + c;
            const p2 = baseIdx + nextCirc;
            const p3 = nextRoundIdx + nextCirc;
            const p4 = nextRoundIdx + c;
            // Assembly stores vertices in order: p1, p4, p3, p2 (not p1, p2, p3, p4)
            polygons.push({ vertexCount: 4, vertices: [p1, p4, p3, p2] });
        }
    }
    return polygons;
}

const DEFAULT_VARIATIONS: TorusVariation[] = [
    { innerStep: 0, amplitude: 0, phase: 0 },
    { innerStep: 128, amplitude: 30000, phase: 100 },
    { innerStep: 256 + 64, amplitude: 40000, phase: 0 },
    { innerStep: 256, amplitude: 20000, phase: 100 },
];

function generateMorphFrames(
    polygons: Polygon[],
    config?: MorphGenerationOptions
): { frames: Vector3D[][]; normals: Vector3D[][] } {
    const {
        baseInnerStep = 192,
        basePhase = 100,
        startAmplitude = 0,
        maxAmplitude = 48000,
        amplitudeIncrement = 1500,
        variations = DEFAULT_VARIATIONS,
    } = config ?? {};

    const objDots = new Float32Array(MORPH_COUNT * TOTAL_VERTICES * POINT_STRIDE);
    const objNormals = new Int16Array(objDots.length);
    const frameOffsets = new Int32Array(MORPH_COUNT);
    const frames: Vector3D[][] = [];
    const normals: Vector3D[][] = [];

    let frameIndex = 0;

    const emitFrame = (spec0: number, spec1: number, spec2: number) => {
        if (frameIndex >= MORPH_COUNT) {
            return;
        }

        const base = frameIndex * TOTAL_VERTICES * POINT_STRIDE;
        frameOffsets[frameIndex] = base;
        let pointer = base;
        let d5 = 0;
        const innerStep = (spec0 >>> 1) & SIN_MASK;
        const phaseOffset = (spec2 >>> 1) & SIN_MASK;
        const cosBase = (256 + phaseOffset) & SIN_MASK;

        for (let outer = ROUN_SEG - 1; outer >= 0; outer--) {
            const idx = d5 & 0xffff;
            const sinOuter = SIN_TABLE[idx & SIN_MASK];
            const cosOuter = SIN_TABLE[(cosBase + idx) & SIN_MASK];
            d5 = (d5 + OUTER_STEP) & 0xffff;

            let d0 = muls16(sinOuter, 4 * 384);
            let d1 = muls16(cosOuter, 2 * 384);  // cosOuter uses spec2 offset!
            d0 = swap32(d0);
            d0 = (d0 + 1500) | 0;
            // Combine d0 (lower word) with d1 (upper word) to create a4
            // This mimics 68k: move.w d0,d1 (preserving d1's upper 16 bits)
            // a4 lower word = major radius, a4 upper word = scaled cosine for Z
            const a4 = ((d1 & 0xffff0000) | (d0 & 0xffff)) | 0;

            let a3 = 0;
            let d2 = (d5 >>> 4) & 0xffff;

            for (let inner = CIRC_SEG - 1; inner >= 0; inner--) {
                let d3 = a4;
                const sinSample = SIN_TABLE[a3 & SIN_MASK];
                a3 = (a3 + innerStep) & SIN_MASK;

                let d0inner = toUint16(sinSample + 32768);
                let prod = mulu16(d0inner, spec1);
                prod = swap32(prod | 0);
                prod = (prod & 0xffff0000) | ((~prod) & 0xffff);
                prod = (prod & 0xffff0000) | (((prod & 0xffff) >>> 1) & 0xffff);

                const d4 = prod;

                let d0temp = muls16(d3, d4);
                d0temp = (d0temp << 1) | 0;
                d0temp = swap32(d0temp);

                d3 = swap32(d3);
                let d1temp = muls16(d4, d3);
                d1temp = (d1temp << 1) | 0;
                d1temp = swap32(d1temp);

                const sinEdge = SIN_TABLE[d2 & SIN_MASK];
                const cosEdge = SIN_TABLE[(256 + (d2 & 0xffff)) & SIN_MASK];
                d2 = (d2 + INNER_STEP) & 0xffff;

                const dx = muls16(d0temp, sinEdge);
                const dy = muls16(d0temp, cosEdge);

                const xWord = toInt16(dx >> 16);
                const yWord = toInt16(dy >> 16);
                const zWord = toInt16(d1temp & 0xffff);

                objDots[pointer] = xWord;
                objDots[pointer + 1] = yWord;
                objDots[pointer + 2] = zWord;
                objDots[pointer + 3] = 0;
                pointer += POINT_STRIDE;
            }
        }

        computeNormalsForFrame(frameIndex, objDots, objNormals, frameOffsets, polygons);
        frameIndex += 1;
    };

    let spec0 = baseInnerStep;
    let spec1 = startAmplitude;
    const spec2 = basePhase;
    while (frameIndex < MORPH_COUNT && spec1 < maxAmplitude) {
        emitFrame(spec0, spec1, spec2);
        spec1 += amplitudeIncrement;
    }

    for (const variation of variations) {
        emitFrame(variation.innerStep, variation.amplitude, variation.phase);
        if (frameIndex >= MORPH_COUNT) {
            break;
        }
    }

    const availableFrames = frameIndex;
    for (let f = 0; f < availableFrames; f++) {
        const base = frameOffsets[f];
        const frameVerts: Vector3D[] = new Array(TOTAL_VERTICES);
        const frameNormals: Vector3D[] = new Array(TOTAL_VERTICES);
        for (let i = 0; i < TOTAL_VERTICES; i++) {
            const offset = base + i * POINT_STRIDE;
            frameVerts[i] = {
                x: objDots[offset] * POSITION_SCALE,
                y: objDots[offset + 1] * POSITION_SCALE,
                z: objDots[offset + 2] * POSITION_SCALE,
            };
            frameNormals[i] = {
                x: objNormals[offset] * NORMAL_SCALE,
                y: objNormals[offset + 1] * NORMAL_SCALE,
                z: objNormals[offset + 2] * NORMAL_SCALE,
            };
        }
        frames.push(frameVerts);
        normals.push(frameNormals);
    }

    return { frames, normals };
}

function computeNormalsForFrame(frameIndex: number, objDots: Float32Array, objNormals: Int16Array, frameOffsets: Int32Array, polygons: Polygon[]): void {
    const base = frameOffsets[frameIndex];
    const accumX = new Float32Array(TOTAL_VERTICES);
    const accumY = new Float32Array(TOTAL_VERTICES);
    const accumZ = new Float32Array(TOTAL_VERTICES);

    for (const poly of polygons) {
        const verts = poly.vertices;
        // Assembly loads vertices in order: p1 (verts[0]), p4 (verts[1]), p3 (verts[2])
        // and computes: (p3 - p4) × (p1 - p4)
        const vP1 = base + verts[0] * POINT_STRIDE;
        const vP4 = base + verts[1] * POINT_STRIDE;
        const vP3 = base + verts[2] * POINT_STRIDE;
        const x1 = objDots[vP1];
        const y1 = objDots[vP1 + 1];
        const z1 = objDots[vP1 + 2];
        const x4 = objDots[vP4];
        const y4 = objDots[vP4 + 1];
        const z4 = objDots[vP4 + 2];
        const x3 = objDots[vP3];
        const y3 = objDots[vP3 + 1];
        const z3 = objDots[vP3 + 2];

        // v1 = p3 - p4, v2 = p1 - p4
        const v1x = x3 - x4;
        const v1y = y3 - y4;
        const v1z = z3 - z4;
        const v2x = x1 - x4;
        const v2y = y1 - y4;
        const v2z = z1 - z4;

        // Cross product: v1 × v2
        const nx = v1y * v2z - v1z * v2y;
        const ny = v1z * v2x - v1x * v2z;
        const nz = v1x * v2y - v1y * v2x;

        for (const idx of verts) {
            accumX[idx] += nx;
            accumY[idx] += ny;
            accumZ[idx] += nz;
        }
    }

    for (let i = 0; i < TOTAL_VERTICES; i++) {
        const nx = accumX[i];
        const ny = accumY[i];
        const nz = accumZ[i];
        const length = Math.hypot(nx, ny, nz) || 1;
        const scale = 128 / length;
        const offset = base + i * POINT_STRIDE;
        objNormals[offset] = clamp16(Math.round(nx * scale));
        objNormals[offset + 1] = clamp16(Math.round(ny * scale));
        objNormals[offset + 2] = clamp16(Math.round(nz * scale));
        objNormals[offset + 3] = 0;
    }
}

function buildSinTable(): Int16Array {
    const table = new Int16Array(1024);
    for (let i = 0; i < 1024; i++) {
        const angle = (i / 1024) * Math.PI * 2;
        table[i] = Math.round(Math.sin(angle) * 32255) | 0;
    }
    return table;
}

function toUint16(value: number): number {
    return value & 0xffff;
}

function swap32(value: number): number {
    return ((value & 0xffff) << 16) | ((value >>> 16) & 0xffff);
}

function muls16(a: number, b: number): number {
    return (toInt16(a) * toInt16(b)) | 0;
}

function mulu16(a: number, b: number): number {
    return (toUint16(a) * toUint16(b)) >>> 0;
}

function toInt16(value: number): number {
    const int = value & 0xffff;
    return int & 0x8000 ? int - 0x10000 : int;
}

function clamp16(value: number): number {
    if (value > 0x7fff) return 0x7fff;
    if (value < -0x8000) return -0x8000;
    return value | 0;
}






