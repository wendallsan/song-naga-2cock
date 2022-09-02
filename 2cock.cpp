#include "daisy_seed.h"
#include "daisysp.h"
using namespace daisy;
using namespace daisysp;

#define PIN_ADC_MUX_IN daisy::seed::A0
#define PIN_ADC_ADSR2_DELAY_IN daisy::seed::A1
#define MUX_CHANNELS 8
#define PIN_MUX_SW_1 11
#define PIN_MUX_SW_2 12
#define PIN_MUX_SW_3 13
#define PIN_TRIGGER 14
#define SAMPLE_RATE 48000
#define MAX_DELAY static_cast<size_t>( SAMPLE_RATE * 4.0 )

DaisySeed hw;
Adsr adsr1, adsr2;
Switch triggerButton;
static DelayLine<bool, MAX_DELAY> DSY_SDRAM_BSS delayLine;

bool buttonIsPressed = false;
static const size_t kDacBufferSize = 48;
static uint16_t DMA_BUFFER_MEM_SECTION dac_buffer1[ kDacBufferSize ],
    dac_buffer2[ kDacBufferSize ];

// SET lastProcessResult TO AN INVALID VALUE SO IT REPORTS THE FIRST TIME IT CHANGES
float process1Result = 0.0, 
    lastProcess1Result = 2.0,
    process2Result = 0.0,
    lastProcess2Result = 2.0,
    adsr1AttackValue,
    adsr1DecayValue,
    adsr1SustainValue,
    adsr1ReleaseValue,
    adsr2DelayValue,
    adsr2AttackValue,
    adsr2DecayValue,
    adsr2SustainValue,
    adsr2ReleaseValue, 
    lastAdsr2DelayValue = 2.0;

enum adcChannels {
	muxSignals,
    delaySignal,
	NUM_ADC_CHANNELS
};

enum muxChannels {
    ADSR1_ATTACK,
    ADSR1_DECAY,
    ADSR1_SUSTAIN,
    ADSR1_RELEASE,
    ADSR2_ATTACK,
    ADSR2_DECAY,
    ADSR2_SUSTAIN,
    ADSR2_RELEASE
};

void handleDac( uint16_t **out, size_t size ){
    for( size_t i = 0; i < size; i++ ){
        // WRITE buttonIsPressed STATE INTO THE DELAY LINE
        delayLine.Write( buttonIsPressed );
        // CONVERT TO A 12 BIT INTEGER RANGE (0 - 4095) FOR THE DAC
        out[0][i] = adsr1.Process( buttonIsPressed ) * 4095.0;
        // HANDLE OUT 2: READ FROM THE DELAYLINE
        out[1][i] = adsr2.Process( delayLine.Read() && buttonIsPressed ) * 4095.0;
        // out[1][i] = adsr2.Process( triggerButton.Pressed() ) * 4095.0;
        // out[1][i] = out[0][i];
    }
}

void handleKnobs(){
    // HANDLE MUXED KNOBS
    adsr1AttackValue = hw.adc.GetMuxFloat( muxSignals, ADSR1_ATTACK );
    adsr1DecayValue = hw.adc.GetMuxFloat( muxSignals, ADSR1_DECAY );
    adsr1SustainValue = hw.adc.GetMuxFloat( muxSignals, ADSR1_SUSTAIN );
    adsr1ReleaseValue = hw.adc.GetMuxFloat( muxSignals, ADSR1_RELEASE );
    adsr2AttackValue = hw.adc.GetMuxFloat( muxSignals, ADSR2_ATTACK );
    adsr2DecayValue = hw.adc.GetMuxFloat( muxSignals, ADSR2_DECAY );
    adsr2SustainValue = hw.adc.GetMuxFloat( muxSignals, ADSR2_SUSTAIN );
    adsr2ReleaseValue = hw.adc.GetMuxFloat( muxSignals, ADSR2_RELEASE );

    // HANDLE THE NON-MUXED KNOBS
    adsr2DelayValue = hw.adc.GetFloat( delaySignal );

    // MAP TIME-RELATED VALUES TO 0 - 4 SECONDS
    adsr1.SetAttackTime( fmap( adsr1AttackValue, 0.0, 4.0 ) );
    adsr1.SetDecayTime( fmap( adsr1DecayValue, 0.0, 4.0 ) );
    adsr1.SetSustainLevel( adsr1SustainValue );
    adsr1.SetReleaseTime( fmap( adsr1ReleaseValue, 0.0, 4.0 ) );

    adsr2.SetAttackTime( fmap( adsr2AttackValue, 0.0, 4.0 ) );
    adsr2.SetDecayTime( fmap( adsr2DecayValue, 0.0, 4.0 ) );
    adsr2.SetSustainLevel( adsr2SustainValue );
    adsr2.SetReleaseTime( fmap( adsr2ReleaseValue, 0.0, 4.0 ) );

    if( adsr2DelayValue != lastAdsr2DelayValue ){ // HANDLE DELAY CONTROL
        lastAdsr2DelayValue = adsr2DelayValue;
        float delayTime = fmap( adsr2DelayValue, 0.0, 4.0 ) * SAMPLE_RATE;
        if( delayTime < 1.0 ) delayTime = 1.0;
        delayLine.SetDelay( delayTime );
    }
}

void initADC(){
    AdcChannelConfig adcConfig[ NUM_ADC_CHANNELS ];
    adcConfig[ muxSignals ].InitMux( 
        PIN_ADC_MUX_IN, 
        MUX_CHANNELS, 
        hw.GetPin( PIN_MUX_SW_1 ), 
        hw.GetPin( PIN_MUX_SW_2 ), 
        hw.GetPin( PIN_MUX_SW_3 )
    );
    adcConfig[ delaySignal ].InitSingle( PIN_ADC_ADSR2_DELAY_IN );
    hw.adc.Init( adcConfig, NUM_ADC_CHANNELS );
    hw.adc.Start();
}

void initDAC(){
    DacHandle::Config dacConfig;
    dacConfig.target_samplerate = SAMPLE_RATE;
    dacConfig.bitdepth = DacHandle::BitDepth::BITS_12;
    dacConfig.buff_state = DacHandle::BufferState::ENABLED;
    dacConfig.mode = DacHandle::Mode::DMA;
    dacConfig.chn = DacHandle::Channel::BOTH;
    hw.dac.Init( dacConfig );
    hw.dac.Start( dac_buffer1, dac_buffer2, kDacBufferSize, handleDac );
}

int main( void ){
    hw.Init();
    hw.StartLog();
    initADC();
    initDAC();
    triggerButton.Init( hw.GetPin( PIN_TRIGGER ), 10 );
    adsr1.Init( SAMPLE_RATE );
    adsr2.Init( SAMPLE_RATE );
    delayLine.Init();
    int i = 0;
    while( true ){
        triggerButton.Debounce();
        buttonIsPressed = triggerButton.Pressed();
        handleKnobs();
        i++;
        if( i >= 500 ){
            hw.PrintLine( FLT_FMT3, FLT_VAR3( adsr2DelayValue ) );
            i = 0;
        }
        System::Delay( 1 );
    }
}