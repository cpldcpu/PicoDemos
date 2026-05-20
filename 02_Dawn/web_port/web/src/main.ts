/**
 * DAWN - Browser Port
 * Main entry point and render loop
 *
 * Original Amiga 1200 demo by AZURE/ARTWORK (Tim Böscke), 1995
 * Browser port transliteration
 */

import { ChunkyBuffer } from './chunkyBuffer.js';
import { Palette } from './palette.js';
import { DemoSequencer } from './sequencer.js';
import { AudioPlayer } from './audio.js';

class DawnDemo {
    private canvas: HTMLCanvasElement;
    private ctx: CanvasRenderingContext2D;
    private buffer: ChunkyBuffer;
    private palette: Palette;
    private sequencer: DemoSequencer;
    private audio: AudioPlayer;

    // Performance tracking
    private lastFrameTime: number = 0;
    private fps: number = 0;
    private frameCount: number = 0;
    private accumulator: number = 0;

    // Target 50Hz (PAL)
    private readonly TARGET_FPS = 50;
    private readonly FRAME_TIME = 1000 / this.TARGET_FPS;

    private running: boolean = false;
    private isPaused: boolean = false;
    private pauseButton?: HTMLButtonElement;
    private muteButton?: HTMLButtonElement;

    constructor() {
        // Get canvas
        this.canvas = document.getElementById('canvas') as HTMLCanvasElement;
        if (!this.canvas) {
            throw new Error('Canvas not found');
        }

        const ctx = this.canvas.getContext('2d', { alpha: false });
        if (!ctx) {
            throw new Error('Could not get 2D context');
        }
        this.ctx = ctx;

        // Disable image smoothing for pixel-perfect rendering
        this.ctx.imageSmoothingEnabled = false;

        // Initialize systems
        this.buffer = new ChunkyBuffer(160, 128);
        this.palette = new Palette();
        this.sequencer = new DemoSequencer(this.buffer, this.palette);
        this.audio = new AudioPlayer();

        // Setup UI
        this.setupUI();

        console.log('DAWN - Browser port initialized');
        console.log('Original by AZURE/ARTWORK (Tim Böscke), 1995');
    }

    private setupUI(): void {
        // Fullscreen button
        const fullscreenBtn = document.getElementById('fullscreen');
        if (fullscreenBtn) {
            fullscreenBtn.addEventListener('click', () => {
                if (document.fullscreenElement) {
                    document.exitFullscreen();
                } else {
                    document.documentElement.requestFullscreen();
                }
            });
        }

        // Restart button
        const restartBtn = document.getElementById('restart');
        if (restartBtn) {
            restartBtn.addEventListener('click', () => {
                this.restart();
            });
        }

        // Pause/Resume button
        const pauseBtn = document.getElementById('pause') as HTMLButtonElement | null;
        if (pauseBtn) {
            this.pauseButton = pauseBtn;
            this.pauseButton.addEventListener('click', () => this.togglePause());
            this.pauseButton.textContent = 'Pause';
        }

        // Mute button - start muted, enable on first click
        const muteBtn = document.getElementById('mute') as HTMLButtonElement | null;
        if (muteBtn) {
            this.muteButton = muteBtn;
            this.muteButton.addEventListener('click', async () => {
                // Initialize and start audio on first unmute
                if (!this.audio.isInitialized()) {
                    console.log('Initializing audio...');
                    await this.audio.init();
                    await this.audio.start();
                    console.log('Audio started');
                }
                this.toggleMute();
            });
            this.updateMuteButton();
        }
    }

    private toggleMute(): void {
        this.audio.toggleMute();
        this.updateMuteButton();
    }

    private updateMuteButton(): void {
        if (!this.muteButton) return;

        if (this.audio.getMuted()) {
            this.muteButton.textContent = '🔇';
            this.muteButton.title = 'Unmute audio';
        } else {
            this.muteButton.textContent = '🔊';
            this.muteButton.title = 'Mute audio';
        }
    }

    /**
     * Start demo
     */
    async start(): Promise<void> {
        if (this.running) return;

        console.log('Starting demo...');

        // Audio will be initialized later when user clicks mute button
        // Don't initialize here to avoid any blocking

        this.running = true;
        this.isPaused = false;
        if (this.pauseButton) {
            this.pauseButton.textContent = 'Pause';
        }
        this.lastFrameTime = performance.now();
        this.accumulator = 0;

        this.renderLoop();
    }

    /**
     * Stop demo
     */
    stop(): void {
        this.running = false;
        this.isPaused = false;
        this.audio.stop();
        if (this.pauseButton) {
            this.pauseButton.textContent = 'Pause';
        }
    }

    /**
     * Restart demo
     */
    restart(): void {
        this.stop();
        this.frameCount = 0;
        this.sequencer = new DemoSequencer(this.buffer, this.palette);
        this.start();
    }

    private togglePause(): void {
        if (!this.running) {
            return;
        }

        if (this.isPaused) {
            this.resume();
        } else {
            this.pause();
        }
    }

    private pause(): void {
        if (this.isPaused) {
            return;
        }
        this.isPaused = true;
        this.audio.stop();
        if (this.pauseButton) {
            this.pauseButton.textContent = 'Resume';
        }
    }

    private resume(): void {
        if (!this.isPaused) {
            return;
        }
        this.isPaused = false;
        this.lastFrameTime = performance.now();
        this.accumulator = 0;
        this.audio.start();
        if (this.pauseButton) {
            this.pauseButton.textContent = 'Pause';
        }
    }

    /**
     * Main render loop
     */
    private renderLoop = (): void => {
        if (!this.running) return;

        const currentTime = performance.now();
        const deltaTime = currentTime - this.lastFrameTime;
        this.lastFrameTime = currentTime;

        if (this.isPaused) {
            requestAnimationFrame(this.renderLoop);
            return;
        }

        this.accumulator += deltaTime;

        let didUpdate = false;

        while (this.accumulator >= this.FRAME_TIME) {
            this.update();
            this.accumulator -= this.FRAME_TIME;
            this.frameCount++;
            didUpdate = true;
        }

        if (didUpdate) {
            this.fps = this.TARGET_FPS;
            this.render();
            this.updateStats();
        }

        requestAnimationFrame(this.renderLoop);
    };

    /**
     * Update demo logic
     */
    private update(): void {
        // Update sequencer
        this.sequencer.update();

        // Update audio
        this.audio.update(this.frameCount);
    }

    /**
     * Render frame
     */
    private render(): void {
        // Convert chunky buffer to RGB using current palette
        this.buffer.convertToRGB(this.palette.colors);

        // Draw to canvas
        this.ctx.putImageData(this.buffer.getImageData(), 0, 0);
    }

    /**
     * Update stats display
     */
    private updateStats(): void {
        const fpsElem = document.getElementById('fps');
        const frameElem = document.getElementById('frame');
        const sceneElem = document.getElementById('scene');

        if (fpsElem) {
            fpsElem.textContent = this.fps.toFixed(1);
        }

        if (frameElem) {
            frameElem.textContent = this.sequencer.getFrameCount().toString();
        }

        if (sceneElem) {
            sceneElem.textContent = this.sequencer.getSceneName();
        }
    }
}

// Start demo when page loads
window.addEventListener('DOMContentLoaded', async () => {
    try {
        const demo = new DawnDemo();
        await demo.start();

        // Expose to window for debugging
        (window as any).demo = demo;

        console.log('Demo started! Click anywhere to enable audio.');
    } catch (error) {
        console.error('Failed to start demo:', error);
        alert('Failed to start demo. Please check console for details.');
    }
});
