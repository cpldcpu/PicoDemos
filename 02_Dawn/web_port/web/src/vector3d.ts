/**
 * 3D vector rotation and projection
 * Port of Amiga 3D transformation routines
 *
 * Original rotation: dawn_final.s:1632-1728
 * Original backface culling: dawn_final.s:1768-1822
 */

export interface Vector3D {
    x: number;
    y: number;
    z: number;
}

export interface Vector2D {
    x: number;
    y: number;
}

export interface ScreenCoord {
    x: number;  // Screen X (0-159)
    y: number;  // Screen Y (0-127)
    z: number;  // Depth for sorting
}

/**
 * 3D rotation and projection system
 */
export class Vector3DSystem {
    // Rotation angles (0-65535 range, divided by 32 for sine table)
    // Assembly stores 16-bit angles and uses "lsr #5" to get 0-1023 range
    angleX: number = 0;
    angleY: number = 0;
    angleZ: number = 0;

    // Translation distance (Z offset)
    // Assembly: RO_trans = 3000; we keep the same base distance.
    translation: number = 3000;

    // Perspective factor
    // Assembly: zoom = 1000 (constant at line 1630)
    // Must match original to maintain correct object scaling
    zoom: number = 1000;

    // Assembly uses lsr #5 to convert 16-bit angle to 0-1023 range
    private static readonly ANGLE_SCALE = (Math.PI * 2) / (1024 * 32);

    /**
     * Rotate and project a single point using floating point math.
     * Matches the 68k order: first rotate around Y, then around X.
     */
    rotateAndProject(point: Vector3D): ScreenCoord {
        const ay = this.angleY * Vector3DSystem.ANGLE_SCALE;
        const ax = this.angleX * Vector3DSystem.ANGLE_SCALE;

        const sinY = Math.sin(-ay);
        const cosY = Math.cos(ay);
        const sinX = Math.sin(-ax);
        const cosX = Math.cos(ax);

        // Rotate around Y (uses original X and Z)
        const x1 = point.x * cosY - point.z * sinY;
        const z1 = point.z * cosY + point.x * sinY;
        const y0 = point.y;

        // Rotate around X
        const y1 = y0 * cosX - z1 * sinX;
        const z2 = z1 * cosX + y0 * sinX;
        const x2 = x1;

        // Perspective projection
        // Assembly formula (dawn_final.s:1676-1686):
        //   screenX = (x >> 8) / (z + translation + zoom) + 80
        //   which is: (x / 256) / denominator + center
        //
        // TypeScript vertices are scaled by POSITION_SCALE = 1/8 (torus.ts:12)
        // The assembly shift right by 8 is a fixed-point conversion.
        // After rotation, assembly has 32-bit values that need >>8 before division.
        //
        // To match assembly projection with our floating point and 1/8 vertex scale:
        // We need to compensate for the vertex scale and match the division pattern.
        // The working formula uses: x * zoom / (zoom + z + translation)
        // With zoom=1000 (original value), this closely approximates the assembly behavior.
        const depth = z2 + this.translation;
        const denom = this.zoom + depth;
        const scale = this.zoom / (denom !== 0 ? denom : 1);

        const screenX = Math.round(x2 * scale + 80);
        const screenY = Math.round(y1 * scale + 64);

        return { x: screenX, y: screenY, z: depth };
    }

    /**
     * Rotate and project array of points
     */
    transformPoints(points: Vector3D[], output: ScreenCoord[]): void {
        for (let i = 0; i < points.length; i++) {
            output[i] = this.rotateAndProject(points[i]);
        }
    }

    /**
     * Rotate a normal vector (for environment mapping)
     * Original: dawn_final.s:1693-1727
     *
     * Rotates normals in INVERSE direction to keep lighting from viewer direction
     * as object rotates.
     */
    rotateNormalToUV(normal: Vector3D): { u: number; v: number } {
        const ax = this.angleX * Vector3DSystem.ANGLE_SCALE;
        const ay = this.angleY * Vector3DSystem.ANGLE_SCALE;

        // The matrix below already applies the inverse rotation, so we keep the original angles.
        const sinX = Math.sin(-ax);
        const cosX = Math.cos(ax);
        const sinY = Math.sin(-ay);
        const cosY = Math.cos(ay);

        // Apply inverse rotations
        let nx = normal.x;
        let ny = normal.y;
        let nz = normal.z;

        // Inverse Y rotation
        const nx1 = nx * cosY - nz * sinY;
        const nz1 = nx * sinY + nz * cosY;
        const ny1 = ny;

        // Inverse X rotation with Z doubling
        const nz1_doubled = nz1 * 1;
        const ny2 = (ny1 * cosX - nz1_doubled * sinX) / 1.0;
        const nx2 = nx1 / 1.0;

        // Add 64 offset (line 1721-1725) while preserving fractional precision
        // Fractional bits mimic the 68k fixed-point format.

        const u = Math.round(nx2 * 64 + 64) & 0xFF;
        const v = Math.round(ny2 * 64 + 64) & 0xFF;
        // const u = Math.round(nx2 ) & 0xFF;
        // const v = Math.round(ny2 ) & 0xFF;

        return { u, v };
    }

    /**
     * Calculate cross product for backface culling
     * Original: dawn_final.s:1782-1800
     *
     * Returns: positive if visible, negative/zero if backfacing
     */
    static crossProduct2D(
        p1: ScreenCoord,
        p2: ScreenCoord,
        p3: ScreenCoord
    ): number {
        // Create two edge vectors
        const v1x = p1.x - p2.x;
        const v1y = p1.y - p2.y;
        const v2x = p3.x - p2.x;
        const v2y = p3.y - p2.y;

        // Cross product (Z component)
        // v1x * v2y - v1y * v2x
        return v2x * v1y - v1x * v2y;
    }

    /**
     * Calculate 3D cross product for normal vectors
     * Original: dawn_final.s:819-846
     */
    static crossProduct3D(
        v1: Vector3D,
        v2: Vector3D
    ): Vector3D {
        // v1 x v2 = (v1y*v2z - v1z*v2y, v1z*v2x - v1x*v2z, v1x*v2y - v1y*v2x)
        const nx = ((v1.y * v2.z) - (v1.z * v2.y)) >> 8;
        const ny = ((v1.z * v2.x) - (v1.x * v2.z)) >> 8;
        const nz = ((v1.x * v2.y) - (v1.y * v2.x)) >> 8;

        return { x: nx, y: ny, z: nz };
    }
}
