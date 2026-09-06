#include "vesper.h"
/* 64 bars, 128 BPM. The music and direction share these exact boundaries. */
Score score_at(uint32_t sample) {
    Score s;
    s.beat=(float)sample/BEAT_SAMPLES;
    s.bar=(int)(sample/(BEAT_SAMPLES*4u));
    float f=s.beat-(int)s.beat;
    s.pulse=1.f/(1.f+14.f*f);
    s.bar_phase=s.beat*.25f-s.bar;
    s.chapter=s.bar<8?0:s.bar<20?1:s.bar<32?2:s.bar<40?3:s.bar<56?4:5;
    return s;
}
