/**
 * Demo sequencer - Timeline and scene management
 * Port of Amiga demo sequence
 *
 * Original: dawn_final.s:159-271
 */

import { ChunkyBuffer } from './chunkyBuffer.js';
import { Palette } from './palette.js';
import { Vector3DSystem, ScreenCoord } from './vector3d.js';
import { Torus } from './torus.js';
import { EnvironmentMap, TextureMapper } from './textureMap.js';
import { VoxelLandscape } from './voxel.js';
import { TextRenderer, BlurEffect, BlurTransition, TextType } from './effects.js';

enum Scene {
    TEXT_DAWN,
    TEXT_BY,
    TEXT_AZURE,
    TORUS_1,
    TORUS_2,
    VOXEL,
    TORUS_3,
    TORUS_BLUR,
    FINALE,
}

/**
 * Main demo sequencer
 */
export class DemoSequencer {
    private buffer: ChunkyBuffer;
    private palette: Palette;
    private vector3d: Vector3DSystem;
    private torus: Torus;
    private envMap: EnvironmentMap;
    private textureMapper: TextureMapper;
    private voxel: VoxelLandscape;
    private textRenderer: TextRenderer;
    private blurEffect: BlurEffect;
    private blurTransition: BlurTransition;
    private lightAngle: number = 0;
    private lightRadius: number = 0;

    private currentScene: Scene = Scene.TEXT_DAWN;
    private frameCount: number = 0;
    private sceneStartFrame: number = 0;

    // Projected vertices
    private screenCoords: ScreenCoord[] = new Array(256);
    private uvCoords: { u: number; v: number }[] = new Array(256);

    constructor(buffer: ChunkyBuffer, palette: Palette) {
        this.buffer = buffer;
        this.palette = palette;

        this.vector3d = new Vector3DSystem();
        this.torus = new Torus();
        this.envMap = new EnvironmentMap();
        this.textureMapper = new TextureMapper(this.envMap);
        this.voxel = new VoxelLandscape();
        this.textRenderer = new TextRenderer();
        this.blurEffect = new BlurEffect();
        this.blurTransition = new BlurTransition();

        // Initialize translation and zoom
        // Original: dawn_final.s:1630 - zoom = 1000
        this.vector3d.zoom = 1000;
        this.setInitialTranslation(this.currentScene);
        this.textureMapper.setLightOffset(0, 0);
        this.updateLightRadiusForScene(this.currentScene);
        this.updateEnvironment(this.currentScene);

        // Initialize screen coords
        for (let i = 0; i < 256; i++) {
            this.screenCoords[i] = { x: 0, y: 0, z: 0 };
            this.uvCoords[i] = { u: 0, v: 0 };
        }
    }

    /**
     * Update and render current scene
     *
     * Original: dawn_final.s:159-271
     * When fadeout is active, NO rendering happens - only blur_it is applied
     * This matches the original where "bsr fadeout" enters a loop that only
     * calls handle_screen and blur_it (lines 499-509)
     */
    update(): void {
        // If fadeout is active, STOP rendering and only apply blur
        // This is critical - the original fadeout routine does NOT render
        if (this.blurTransition.active) {
            this.applyFadeIfNeeded();
            this.frameCount++;
            return;
        }

        const sceneFrame = this.frameCount - this.sceneStartFrame;
        this.updateEnvironment(this.currentScene);

        // Scene transitions (based on original timing)
        switch (this.currentScene) {
            case Scene.TEXT_DAWN:
                this.renderTextScene('dawn', sceneFrame);
                if (sceneFrame > 150) {
                    this.startFadeOut(Scene.TEXT_BY, 'colors2');
                }
                break;

            case Scene.TEXT_BY:
                this.renderTextScene('by', sceneFrame);
                if (sceneFrame > 150) {
                    this.startFadeOut(Scene.TEXT_AZURE, 'colors3');
                }
                break;

            case Scene.TEXT_AZURE:
                this.renderTextScene('azure', sceneFrame);
                if (sceneFrame > 150) {
                    this.startFadeOut(Scene.TORUS_1, 'colors1');
                }
                break;

            case Scene.TORUS_1:
                this.updateTranslation();
                this.renderTorusScene();
                if (sceneFrame > 15 * 50) {
                    this.startFadeOut(Scene.TORUS_2, 'colors1');
                }
                break;

            case Scene.TORUS_2:
                this.updateTranslation();
                this.renderTorusScene();
                if (sceneFrame > 20 * 50) {
                    this.startFadeOut(Scene.VOXEL, 'colors1');
                }
                break;

            case Scene.VOXEL:
                this.renderVoxelScene();
                if (sceneFrame > 25 * 50) {
                    this.startFadeOut(Scene.TORUS_3, 'colors5');
                }
                break;

            case Scene.TORUS_3:
                this.updateTranslation();
                this.renderTorusScene();
                if (sceneFrame > 15 * 50) {
                    this.startFadeOut(Scene.TORUS_BLUR, 'colors1');
                }
                break;

            case Scene.TORUS_BLUR:
                this.updateTranslation();
                this.renderTorusWithBlur();
                if (sceneFrame > 20 * 50) {
                    this.startFadeOut(Scene.FINALE, 'colors4');
                }
                break;

            case Scene.FINALE:
                this.renderFinale();
                // Loop back to start after a while
                if (sceneFrame > 30 * 50) {
                    this.reset();
                }
                break;
        }

        this.frameCount++;
    }

    private renderTextScene(text: TextType, sceneFrame: number): void {
        if (sceneFrame === 0) {
            this.buffer.clear();
        }

        this.textRenderer.render(this.buffer, text);
        this.blurEffect.apply(this.buffer);
    }

    private updateLightRadiusForScene(scene: Scene): void {
        this.lightRadius = scene === Scene.TORUS_2 ? 50 : 0;
    }

    /**
     * Update translation (ro_trans) - animates objects moving in
     * Original: dawn_final.s:167-191, 187-191
     *
     * The original code decrements ro_trans by 20 each frame until it reaches 800:
     *   lea    ro_trans(pc),a0
     *   cmp    #800,(a0)
     *   blt.s  .ee
     *   sub    #20,(a0)
     *
     * This creates a "zoom in" effect where objects start far away (high Z)
     * and move closer (lower Z) over time.
     */
    private updateTranslation(): void {
        if (this.vector3d.translation > 800) {
            this.vector3d.translation -= 20;
        }
    }

    private updateEnvironment(scene: Scene): void {
        const shouldUseChecker =
            scene === Scene.TORUS_3 ||
            scene === Scene.TORUS_BLUR;

        if (shouldUseChecker !== !!this.envMap.ppicSwitch) {
            this.envMap.ppicSwitch = shouldUseChecker ? 1 : 0;
            this.envMap.generate();
        }
    }

    private updateTorusFrame(sceneFrame: number): void {
        const frameCount = this.torus.getFrameCount();
        if (frameCount === 0) {
            return;
        }

        const clampIndex = (index: number) =>
            Math.max(0, Math.min(frameCount - 1, ((index % frameCount) + frameCount) % frameCount));

        let frameIndex: number;
        switch (this.currentScene) {
            case Scene.TORUS_1:
                frameIndex = clampIndex(32);
                break;
            case Scene.TORUS_2:
                frameIndex = clampIndex(33);
                break;
            case Scene.TORUS_3:
                frameIndex = clampIndex(34);
                break;
            case Scene.TORUS_BLUR:
                frameIndex = clampIndex(35);
                break;
            case Scene.FINALE: {
                const angleIndex = (sceneFrame << 2) & 0x3ff;
                const angle = (angleIndex / 1024) * Math.PI * 2;
                const normalized = (Math.sin(angle) + 1) * 0.5;
                const maxFrames = Math.min(32, frameCount);
                frameIndex = clampIndex(Math.floor(normalized * (maxFrames - 1)));
                break;
            }
            default:
                frameIndex = clampIndex(sceneFrame);
        }

        this.torus.setFrame(frameIndex);
    }

    private renderTorusScene(): void {
        this.drawTorus(false, true);
    }

    private renderTorusWithBlur(): void {
        // Don't clear - blur accumulates
        this.drawTorus(true, false);
        this.blurEffect.apply(this.buffer);
    }

    private renderVoxelScene(): void {
        this.buffer.clear();
        this.voxel.render(this.buffer, this.frameCount);
    }

    /**
     * Render finale scene: text + torus with blur
     * Original: dawn_final.s:244-267
     *
     * Assembly sequence per frame:
     *   1. text - render text additively
     *   2. rb_moveovj - render torus with REPLACE mode (blur flag = 1)
     *   3. blur_it - apply blur effect
     *
     * Key difference from TORUS_BLUR:
     *   - TORUS_BLUR uses MAX blending (blur=0)
     *   - FINALE uses REPLACE blending (blur=1)
     */
    private renderFinale(): void {
        // Don't clear - accumulate with blur

        // 1. Render text (additive)
        this.textRenderer.render(this.buffer, 'dawn');

        // 2. Render torus with REPLACE mode (blurMode=false, not MAX)
        // This is different from TORUS_BLUR which uses MAX mode
        this.drawTorus(false, false);

        // 3. Apply blur (ONCE, not twice!)
        this.blurEffect.apply(this.buffer);
    }

    private drawTorus(blurMode: boolean, clearBuffer: boolean): void {
        if (clearBuffer) {
            this.buffer.clear();
        }

        const sceneFrame = this.frameCount - this.sceneStartFrame;

        // Assembly rotation increments (lines 1078-1080):
        // ro_wi[0] (Y angle) += -50
        // ro_wi[1] (X angle) += 80
        // ro_wi[2] advances the environment-map light phase (+600)
        // These are raw angles divided by 32 before sine lookup (lsr #5)
        const stepY = -50;
        const stepX = 80;
        const stepZ = 600;
        const ANGLE_SCALE = (Math.PI * 2) / (1024 * 32);
        const LIGHT_SINE_SCALE = 32255 / 65536;

        // Angles wrap at 32768 (16-bit signed), then divided by 32 for sine table
        this.vector3d.angleY = (this.vector3d.angleY + stepY) & 0xFFFF;
        this.vector3d.angleX = (this.vector3d.angleX + stepX) & 0xFFFF;
        this.vector3d.angleZ = (this.vector3d.angleZ + stepZ) & 0xFFFF;

        if (this.lightRadius !== 0) {
            this.lightAngle = (this.lightAngle + stepZ) & 0xFFFF;
            const offset =
                Math.sin(this.lightAngle * ANGLE_SCALE) *
                this.lightRadius *
                LIGHT_SINE_SCALE;
            this.textureMapper.setLightOffset(offset, 0);
        } else {
            this.textureMapper.setLightOffset(0, 0);
        }

        this.updateTorusFrame(sceneFrame);

        // Transform torus vertices
        this.vector3d.transformPoints(this.torus.objectData.vertices, this.screenCoords);

        // Calculate UV coordinates from normals
        this.calculateUVFromNormals();

        interface VisiblePoly {
            depth: number;
            indices: number[];
        }

        const visiblePolys: VisiblePoly[] = [];

        for (const poly of this.torus.objectData.polygons) {
            const indices = poly.vertices.slice(0, poly.vertexCount);
            if (indices.length < 3) continue;

            const v0 = this.screenCoords[indices[0]];
            const v1 = this.screenCoords[indices[1]];
            const v2 = this.screenCoords[indices[2]];
            if (!v0 || !v1 || !v2) continue;

            const cross = Vector3DSystem.crossProduct2D(v0, v1, v2);
            if (cross <= 0) continue; // Backface culling

            let depthSum = 0;
            for (let i = 0; i < indices.length; i++) {
                depthSum += this.screenCoords[indices[i]].z;
            }
            visiblePolys.push({
                depth: depthSum / indices.length,
                indices,
            });
        }

        visiblePolys.sort((a, b) => b.depth - a.depth);

        this.textureMapper.setBlurMode(blurMode);
        for (const poly of visiblePolys) {
            const vertices = poly.indices.map(idx => this.screenCoords[idx]);
            const uvs = poly.indices.map(idx => this.uvCoords[idx]);
            this.textureMapper.renderPolygon(this.buffer, vertices, uvs);
        }
    }

    private calculateUVFromNormals(): void {
        // Calculate UV coordinates from vertex normals (environment mapping)
        // Original: dawn_final.s:1693-1727
        // Normals are rotated INVERSELY to keep lighting from viewer direction
        for (let i = 0; i < this.torus.objectData.normals.length; i++) {
            const normal = this.torus.objectData.normals[i];
            // Rotate the normal inversely and convert to UV coordinates
            this.uvCoords[i] = this.vector3d.rotateNormalToUV(normal);
        }
    }

    private startFadeOut(nextScene: Scene, paletteScheme: string): void {
        if (this.blurTransition.active) {
            return;
        }
        this.blurTransition.start();
        this.pendingScene = { nextScene, paletteScheme };
    }

    private pendingScene: { nextScene: Scene; paletteScheme: string } | null = null;

    private applyFadeIfNeeded(): boolean {
        if (!this.blurTransition.active) {
            return false;
        }

        const finished = this.blurTransition.apply(this.buffer);
        if (finished && this.pendingScene) {
            const { nextScene, paletteScheme } = this.pendingScene;
            this.pendingScene = null;

            this.currentScene = nextScene;
            this.updateLightRadiusForScene(nextScene);
            this.updateEnvironment(nextScene);
            if (this.lightRadius === 0) {
                this.textureMapper.setLightOffset(0, 0);
            }

            // Set initial ro_trans (translation) value for each scene
            // Original: dawn_final.s:179, 230
            this.setInitialTranslation(nextScene);

            this.sceneStartFrame = this.frameCount;
            this.palette.setScheme(paletteScheme as any);
        }

        return true;
    }

    /**
     * Set initial translation value when entering a scene
     * Original: dawn_final.s:179 (move #3000,ro_trans), line 230 (move #1000,ro_trans)
     */
    private setInitialTranslation(scene: Scene): void {
        switch (scene) {
            case Scene.TORUS_1:
                // First torus scene - starts at 3000 (implicit from initial state)
                this.vector3d.translation = 3000;
                break;
            case Scene.TORUS_2:
                // Line 179: move #3000,ro_trans
                this.vector3d.translation = 3000;
                break;
            case Scene.TORUS_3:
                // After voxel - no explicit set in original, likely inherits/stays at 800
                // But for consistency with zoom-in effect, reset to 3000
                this.vector3d.translation = 3000;
                break;
            case Scene.TORUS_BLUR:
                // Line 230: move #1000,ro_trans (shorter zoom-in)
                this.vector3d.translation = 1000;
                break;
            default:
                // Text and voxel scenes don't use translation animation
                this.vector3d.translation = 800;
                break;
        }
    }    private reset(): void {
        this.currentScene = Scene.TEXT_DAWN;
        this.sceneStartFrame = 0;
        this.frameCount = 0;
        this.palette.setScheme('colors1');
        this.vector3d.zoom = 1000;
        this.setInitialTranslation(this.currentScene);
        this.lightAngle = 0;
        this.updateLightRadiusForScene(this.currentScene);
        this.updateEnvironment(this.currentScene);
        this.textureMapper.setLightOffset(0, 0);
        this.blurTransition.reset();
        this.pendingScene = null;
    }

    getSceneName(): string {
        return Scene[this.currentScene];
    }

    getFrameCount(): number {
        return this.frameCount;
    }
}








