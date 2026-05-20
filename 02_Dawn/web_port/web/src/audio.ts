/**
 * Amiga Paula audio sample playback
 * Port of original 8-byte waveform sample
 *
 * Original sample: dawn_final.s:1137
 * Original playback: dawn_final.s:1092-1106
 */

/**
 * Amiga Paula-style audio player using Web Audio API
 * Uses the actual 8-byte sample from the original demo
 */
export class AudioPlayer {
    private audioContext: AudioContext | null = null;
    private sourceNodes: AudioBufferSourceNode[] = [];
    private gainNodes: GainNode[] = [];
    private isPlaying: boolean = false;
    private isMuted: boolean = true;  // Start muted by default
    private masterGain: GainNode | null = null;

    // Original 8-byte waveform from assembly (line 1137)
    private readonly SAMPLE_DATA = [0x00, 0x30, 0x59, 0x75, 0x7F, 0x75, 0x59, 0x30];

    /**
     * Initialize Web Audio context (requires user interaction)
     */
    async init(): Promise<void> {
        if (this.audioContext) {
            // Resume if suspended (browser autoplay policy)
            if (this.audioContext.state === 'suspended') {
                await this.audioContext.resume();
            }
            return;
        }

        try {
            this.audioContext = new AudioContext();

            // Resume immediately after creation if suspended
            if (this.audioContext.state === 'suspended') {
                await this.audioContext.resume();
            }

            console.log('AudioContext initialized, state:', this.audioContext.state);
        } catch (e) {
            console.error('Web Audio API not supported', e);
        }
    }

    /**
     * Create AudioBuffer from 8-byte sample data
     * Uses sample duplication to upsample (no interpolation, just like Amiga)
     */
    private createSampleBuffer(period: number): AudioBuffer {
        if (!this.audioContext) {
            throw new Error('AudioContext not initialized');
        }

        // Convert 8-bit unsigned sample data to float (-1.0 to 1.0)
        const sampleFloat = this.SAMPLE_DATA.map(byte => (byte - 0x80) / 128);

        // Calculate playback rate from period
        // On Amiga Paula: The period determines the DMA rate (how often each sample byte is fetched)
        // With an 8-byte sample, the waveform frequency is:
        // frequency = PAL_CLOCK / (period * sample_length)
        const palClock = 3579545;
        const sampleLength = this.SAMPLE_DATA.length; // 8 bytes
        const frequency = palClock / (period * sampleLength);

        // Sample rate of our buffer
        const sampleRate = this.audioContext.sampleRate;

        // Number of audio samples needed to play one cycle of the waveform
        const samplesPerCycle = sampleRate / frequency;

        // Create buffer for one cycle (loop seamlessly)
        const buffer = this.audioContext.createBuffer(1, Math.ceil(samplesPerCycle), sampleRate);
        const channelData = buffer.getChannelData(0);

        // Fill buffer by duplicating samples (no interpolation, true Amiga style)
        for (let i = 0; i < channelData.length; i++) {
            // Which of the 8 original samples should we use?
            const sourceIndex = Math.floor((i / samplesPerCycle) * sampleLength) % sampleLength;
            channelData[i] = sampleFloat[sourceIndex];
        }

        return buffer;
    }

    /**
     * Start Paula-style sample playback
     * Original: dawn_final.s:1092-1106
     *
     * Plays the 8-byte waveform on 4 channels with slightly different periods
     * Creates a chorus effect from detuning
     */
    async start(): Promise<void> {
        if (!this.audioContext) {
            console.warn('AudioContext not initialized');
            return;
        }

        if (this.isPlaying) return;

        // Ensure context is running
        if (this.audioContext.state === 'suspended') {
            await this.audioContext.resume();
        }

        if (this.audioContext.state !== 'running') {
            console.warn('AudioContext not running, state:', this.audioContext.state);
            return;
        }

        this.isPlaying = true;
        console.log('Starting Paula-style sample playback...');

        // Create master gain node for muting
        this.masterGain = this.audioContext.createGain();
        this.masterGain.gain.value = this.isMuted ? 0 : 1;
        this.masterGain.connect(this.audioContext.destination);

        // Original assembly parameters
        const basePeriod = 9724; // Line 1095: move #9724,d2
        const periodIncrement = 16; // Line 1094: moveq #16,d4
        const volume = 34 / 64; // Line 1093: moveq #34,d1 (Amiga volume 0-64)

        // Create 4 Paula channels
        for (let i = 0; i < 4; i++) {
            const period = basePeriod + (i * periodIncrement);

            // Create buffer for this channel's period
            const buffer = this.createSampleBuffer(period);

            // Create buffer source node
            const source = this.audioContext.createBufferSource();
            source.buffer = buffer;
            source.loop = true; // Loop the sample continuously

            // Create gain node for this channel
            const gain = this.audioContext.createGain();
            // Volume divided by 4 to prevent clipping with 4 channels
            gain.gain.value = volume / 4;

            // Connect: source -> gain -> master
            source.connect(gain);
            gain.connect(this.masterGain);

            // Start playback
            source.start();

            this.sourceNodes.push(source);
            this.gainNodes.push(gain);
        }

        console.log('4 Paula channels started with periods:',
            [9724, 9740, 9756, 9772]);
    }

    /**
     * Stop audio playback
     */
    stop(): void {
        if (!this.isPlaying) return;

        this.sourceNodes.forEach(source => source.stop());
        this.sourceNodes = [];
        this.gainNodes = [];
        this.masterGain = null;
        this.isPlaying = false;
    }

    /**
     * Mute audio
     */
    mute(): void {
        this.isMuted = true;
        if (this.masterGain) {
            this.masterGain.gain.value = 0;
        }
    }

    /**
     * Unmute audio
     */
    unmute(): void {
        this.isMuted = false;
        if (this.masterGain) {
            this.masterGain.gain.value = 1;
        }
    }

    /**
     * Toggle mute state
     */
    toggleMute(): void {
        if (this.isMuted) {
            this.unmute();
        } else {
            this.mute();
        }
    }

    /**
     * Get mute state
     */
    getMuted(): boolean {
        return this.isMuted;
    }

    /**
     * Check if audio has been initialized
     */
    isInitialized(): boolean {
        return this.audioContext !== null;
    }

    /**
     * Update audio parameters (called per frame)
     * The original doesn't modulate volume - it stays constant at 34/64
     */
    update(_frameCount: number): void {
        // No per-frame updates needed - original uses constant volume
        // The audio just loops continuously
    }
}
