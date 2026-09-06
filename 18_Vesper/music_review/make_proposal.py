"""Build an isolated musical proposal from the archived original synth.

Nothing writes to vesper/, media/, the desktop player, or the firmware.
The generated C file can later be reviewed as a diff before integration.
"""
from pathlib import Path

HERE=Path(__file__).resolve().parent
source=(HERE/'synth_original.c').read_text()
source=source.replace('static int32_t bell_decay;', 'static int32_t bell_decay,bell_attack;')
source=source.replace('position=0;rng=', 'bell_attack=0;\n    position=0;rng=')
begin=source.index('static void sequence(')
end=source.index('static int16_t limit(',begin)
sequence='''/* Proposal 01: "Canticle". Explicit D-natural-minor note choices.
 * Four related phrases, each answering the same opening motif.
 * MIDI 0 denotes a rest, not a voice kill: the previous note decays naturally.
 * Harmony and all scene/drum boundaries remain at the original sample times.
 */
static const uint8_t melody[4][4][8]={
    { /* D minor: call, answer, variation, handoff to B-flat. */
        {74, 0,77,76,74, 0,69, 0},
        {72, 0,69, 0,74, 0, 0, 0},
        {74, 0,77,76,74,72,69, 0},
        {69, 0,72,69,67, 0,65, 0}
    },
    { /* B-flat major: D and F are held in common with the pad. */
        {74, 0,77,74,70, 0,65, 0},
        {72,74,70, 0,65, 0, 0, 0},
        {74, 0,77,74,72, 0,70, 0},
        {70, 0,69,67,65, 0,67, 0}
    },
    { /* F major. E appears as a short passing tone, resolving down to C. */
        {72, 0,77,76,72, 0,69, 0},
        {67,69,72, 0,69, 0, 0, 0},
        {72, 0,74,72,69, 0,65, 0},
        {69, 0,67,65,67, 0,64, 0}
    },
    { /* C major: the final E resolves down to D at the loop boundary. */
        {76, 0,74,72,67, 0,64, 0},
        {67, 0,72, 0,76, 0, 0, 0},
        {74, 0,72,69,67, 0,64, 0},
        {67, 0,69, 0,72, 0,76, 0}
    }
};
static const uint8_t pad_voicing[4][4]={
    {50,57,62,65}, /* D3 A3 D4 F4 */
    {53,58,62,65}, /* F3 Bb3 D4 F4: two common upper voices */
    {53,57,60,65}, /* F3 A3 C4 F4 */
    {48,55,60,64}  /* C3 G3 C4 E4 */
};
static void sequence(unsigned step){
    unsigned bar=step/16,s=step%16,chord=(bar/4)%4;
    int root=scale_note(bar),drums=bar>=8&&bar<56&&!(bar>=32&&bar<36);
    if(s==0){
        for(int i=0;i<4;i++){
            int n=pad_voicing[chord][i];
            inc[i*2]=notes[n];inc[i*2+1]=notes[n]+notes[n]/650;
        }
    }
    if(drums&&(s%4==0||(bar%4==3&&s==14))){kick_env=30000;kick_phase=0;}
    if(drums&&(s==4||s==12||(bar%8==7&&(s==14||s==15))))snare_env=15000;
    if(drums&&(s%2==0||bar>=40)){hat_env=s%4==2?8500:3500;}
    if(bar>=4&&bar<60&&(s%2==0)){
        static const int bass_pattern[8]={0,0,12,0,0,7,0,12};
        inc[8]=notes[root-12+bass_pattern[s/2]];bass_env=s%4==2?14000:9000;
    }
    if((bar>=2&&bar<32&&(s%2==0))||(bar>=36&&bar<60&&(s%2==0))||(bar>=32&&bar<36&&s%4==0)){
        int note=melody[chord][bar%4][s/2];
        if(note){
            inc[9]=notes[note];inc[10]=notes[note]*2;
            bell_env=bar<8?6000:bar>=40?8500:7200;
            phase[9]=phase[10]=0;bell_attack=0;
            bell_decay=bar>=32&&bar<36?32762:32758;
        }
    }
}
'''
source=source[:begin]+sequence+source[end:]
source=source.replace('fm*16000','fm*8000')
source=source.replace('bell=(bell*bell_env)>>15;',
    'bell=(bell*bell_env)>>15;\n        if(bell_attack<32768)bell_attack+=512;\n        bell=(bell*bell_attack)>>15;')
source=source.replace('8192-7500','8192-2813')
source=source.replace('send+dr*3/5','send+dr*2/5').replace('send+dl*3/5','send+dl*2/5')
(HERE/'synth_proposal.c').write_text(source)
print(HERE/'synth_proposal.c')
