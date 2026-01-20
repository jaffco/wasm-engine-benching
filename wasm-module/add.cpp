#include <cmath>

static float phase = 0.0f;

extern "C" {

float get_sample(float dummy) {
    (void)dummy; // unused
    const float freq = 220.0f;
    const float sample_rate = 44100.0f;
    const float pi = 3.1415926535f;
    const float increment = 2.0f * pi * freq / sample_rate;

    phase += increment;
    if (phase > 2.0f * pi) phase -= 2.0f * pi;

    return sinf(phase);
}

}