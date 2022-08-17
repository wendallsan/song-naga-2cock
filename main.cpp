#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

static DaisySeed hw;
static Oscillator lfo;

static const size_t kDacBufferSize = 48;
static uint16_t DMA_BUFFER_MEM_SECTION dac_buffer[kDacBufferSize];

void handleDac(uint16_t **out, size_t size ){
    for( size_t i = 0; i < size; i++ ){
        out[0][i] = (lfo.Process() + 1.0f) * 2047.f;
    }
}

int main(void)
{
	hw.Init();

    DacHandle::Config dacConfig;
	dacConfig.target_samplerate = 48000;
    dacConfig.bitdepth = DacHandle::BitDepth::BITS_12;
    dacConfig.buff_state = DacHandle::BufferState::ENABLED;
    dacConfig.mode = DacHandle::Mode::DMA;
    dacConfig.chn = DacHandle::Channel::ONE;

    hw.dac.Init( dacConfig );
    hw.dac.Start( dac_buffer, kDacBufferSize, handleDac );

	lfo.Init(dacConfig.target_samplerate);
	lfo.SetFreq(1.0f);
	lfo.SetAmp(1.0f);

	while(1) {}
}
