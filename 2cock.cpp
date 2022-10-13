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
#define ADSR1_GATE_PIN 7
#define ADSR1_TRIGGER_PIN 8
#define ADSR2_GATE_PIN 9
#define ADSR2_TRIGGER_PIN 10
#define SAMPLE_RATE 48000
#define NORMAL_MATRIX_LENGTH 10
#define MAX_DELAY static_cast<size_t>( SAMPLE_RATE * 4.0 )
#define NORMAL_MATRIX_INIT_STATE { false, false, false, false, false, false, false, false, false, false }
DaisySeed hw;
Adsr adsr1, adsr2;
// FOR NOW WE'LL USE THE SWITCH CLASS TO MANAGE OUR GATE AND TRIGGER INPUTS
Switch adsr1Gate, adsr1Trigger, adsr2Gate, adsr2Trigger;
GPIO normalSignalWriterPin;
static DelayLine<bool, MAX_DELAY> DSY_SDRAM_BSS delayLine;
bool adsr1GateState = false,
    adsr1TriggerState = false,
    adsr2GateState = false,
    adsr2TriggerState = false,    
    lastAdsr1GateState = false,
    lastAdsr1TriggerState = false,
    lastAdsr2GateState = false,
    lastAdsr2TriggerState = false,
    adsr1FilteredTrigger = false,
    adsr2FilteredTrigger = false,
    adsr1State = false,
    adsr2State = false,
    adsr1GateIsUnplugged = true,
    adsr1TriggerIsUnplugged = true,
    adsr2GateIsUnplugged = true,
    adsr2TriggerIsUnplugged = true,
    lastNormalSignal = false;
static const size_t kDacBufferSize = 48;
static uint16_t DMA_BUFFER_MEM_SECTION dac_buffer1[ kDacBufferSize ],
    dac_buffer2[ kDacBufferSize ];
// SET lastProcessResult TO AN INVALID VALUE SO IT REPORTS THE FIRST TIME IT CHANGES
float adsr1AttackValue,
    adsr1DecayValue,
    adsr1SustainValue,
    adsr1ReleaseValue,
    adsr2DelayValue,
    adsr2AttackValue,
    adsr2DecayValue,
    adsr2SustainValue,
    adsr2ReleaseValue, 
    lastAdsr2DelayValue = 2.0,
    delayTime = 1.0;
bool adsr1GateNormalMatrix[ NORMAL_MATRIX_LENGTH ] = NORMAL_MATRIX_INIT_STATE,
    adsr1TriggerNormalMatrix[ NORMAL_MATRIX_LENGTH ] = NORMAL_MATRIX_INIT_STATE,
    adsr2GateNormalMatrix[ NORMAL_MATRIX_LENGTH ] = NORMAL_MATRIX_INIT_STATE,
    adsr2TriggerNormalMatrix[ NORMAL_MATRIX_LENGTH ] = NORMAL_MATRIX_INIT_STATE;
int matrixCounter = 0;
enum adcChannels {
	muxSignals,
    delaySignal,
	NUM_ADC_CHANNELS
};
enum mainControlsMuxChannels {
    ADSR1_ATTACK,
    ADSR1_DECAY,
    ADSR1_SUSTAIN,
    ADSR1_RELEASE,
    ADSR2_ATTACK,
    ADSR2_DECAY,
    ADSR2_SUSTAIN,
    ADSR2_RELEASE
};
// TODO: ADD CV INPUTS MUX
enum cvMuxChannels {
    DELAY_CV,
    ATTACK_CV,
    DECAY_CV,
    SUSTAIN_CV,
    RELEASE_CV
};
// TODO: ADD CV ADJUST KNOBS MUX CONTROLS
enum cvAdjustMuxChannels {
    ADSR1_ATTACK_ADJUST,
    ADSR1_DECAY_ADJUST,
    ADSR1_SUSTAIN_ADJUST,
    ADSR1_RELEASE_ADJUST,
    ADSR2_ATTACK_ADJUST,
    ADSR2_DECAY_ADJUST,
    ADSR2_SUSTAIN_ADJUST,
    ADSR2_RELEASE_ADJUST
};
void normalSignalTimerCallback( void* data ){
    lastNormalSignal = !lastNormalSignal; // FLIPFLOP THE NORMAL SIGNAL
    normalSignalWriterPin.Write( lastNormalSignal ); // OUTPUT THE NORMAL SIGNAL ON THE PIN
    // UPDATE EACH INPUT'S NORMAL MATRIX
    adsr1GateNormalMatrix[ matrixCounter ] = adsr1GateState == lastNormalSignal;
    adsr1TriggerNormalMatrix[ matrixCounter ] = adsr1TriggerState == lastNormalSignal;
    adsr2GateNormalMatrix[ matrixCounter ] = adsr2GateState == lastNormalSignal;
    adsr2TriggerNormalMatrix[ matrixCounter ] = adsr2TriggerState == lastNormalSignal;
    // DECIDE WHETHER EACH INPUT IS PLUGGED IN OR NOT BASED ON ITS NORMAL MATRIX
    // ASSUMPTION: SOCKET IS NOT PLUGGED IN IF 8 OUT OF 10 ITEMS IN THE MATRIX ARE TRUE
    int matches = 0;
    for( int i = 0; i < NORMAL_MATRIX_LENGTH; i++ ) if( adsr1GateNormalMatrix[ i ] ) matches++;
    adsr1GateIsUnplugged = matches < 8;
    matches = 0;
    for( int i = 0; i < NORMAL_MATRIX_LENGTH; i++ ) if( adsr1TriggerNormalMatrix[ i ] ) matches++;
    adsr1TriggerIsUnplugged = matches < 8;
    matches = 0;
    for( int i = 0; i < NORMAL_MATRIX_LENGTH; i++ ) if( adsr2GateNormalMatrix[ i ] ) matches++;
    adsr2GateIsUnplugged = matches < 8;
    matches = 0;
    for( int i = 0; i < NORMAL_MATRIX_LENGTH; i++ ) if( adsr2TriggerNormalMatrix[ i ] ) matches++;
    adsr2TriggerIsUnplugged = matches < 8;
    matrixCounter++; // ITERATE THE MATRIX COUNTER
    if( matrixCounter >= NORMAL_MATRIX_LENGTH ) matrixCounter = 0;
}
void initNormalSignalTimer(){
    TimerHandle timer;
    TimerHandle::Config timerConfig;
    timerConfig.periph = TimerHandle::Config::Peripheral::TIM_5;
    timerConfig.enable_irq = true;
    auto targetFrequency = 15; // RUN AT 30x PER SECOND -- THIS FREQUENCY VALUES GETS DOUBLED, SO WE USE 15
    auto baseFrequency = System::GetPClk2Freq();
    timerConfig.period = baseFrequency / targetFrequency;
    timer.Init( timerConfig );
    timer.SetCallback( normalSignalTimerCallback );
    timer.Start();
}
void handleDac( uint16_t **out, size_t size ){
    for( size_t i = 0; i < size; i++ ){
        delayLine.Write( adsr2State ); // WRITE adsr2State INTO THE DELAY LINE
        // CONVERT TO A 12 BIT INTEGER RANGE (0 - 4095) FOR THE DAC
        out[0][i] = adsr1.Process( adsr1State ) * 4095.0;
        // HANDLE OUT 2: READ FROM THE DELAY LINE
        out[1][i] = adsr2.Process( delayLine.Read() ) * 4095.0;
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
    // HANDLE THE NON-MUXED KNOB
    adsr2DelayValue = hw.adc.GetFloat( delaySignal );
    // TODO: HANDLE THE NON-MUXED DELAY ADJUST KNOB
    // MAP TIME-RELATED VALUES TO 0 - 4 SECONDS
    adsr1.SetAttackTime( fmap( adsr1AttackValue, 0.0, 4.0 ) );
    adsr1.SetDecayTime( fmap( adsr1DecayValue, 0.0, 4.0 ) );
    adsr1.SetSustainLevel( adsr1SustainValue );
    adsr1.SetReleaseTime( fmap( adsr1ReleaseValue, 0.0, 4.0 ) );
    adsr2.SetAttackTime( fmap( adsr2AttackValue, 0.0, 4.0 ) );
    adsr2.SetDecayTime( fmap( adsr2DecayValue, 0.0, 4.0 ) );
    adsr2.SetSustainLevel( adsr2SustainValue );
    adsr2.SetReleaseTime( fmap( adsr2ReleaseValue, 0.0, 4.0 ) );
    delayLine.SetDelay( fclamp(
        fmap( adsr2DelayValue, 0.0, 4.0 ) * SAMPLE_RATE,
        1.0, 
        MAX_DELAY
    ) );
}
void initADC(){
    AdcChannelConfig adcConfig[ NUM_ADC_CHANNELS ];
    adcConfig[ muxSignals ].InitMux( 
        PIN_ADC_MUX_IN, 
        MUX_CHANNELS, 
        hw.GetPin( PIN_MUX_CHANNEL_A ), 
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
void initTriggersAndGates(){
    // THESE UPDATE EVERY 10 MS (THE LAST ARG IN THESE INIT CALLS)
    adsr1Gate.Init( hw.GetPin( ADSR1_GATE_PIN ), 10 );
    adsr1Trigger.Init( hw.GetPin( ADSR1_TRIGGER_PIN ), 10 );
    adsr2Gate.Init( hw.GetPin( ADSR2_GATE_PIN ), 10 );
    adsr2Trigger.Init( hw.GetPin( ADSR2_TRIGGER_PIN ), 10 );
}
void handleTriggersAndGates(){
    adsr1Gate.Debounce();
    adsr1Trigger.Debounce();
    adsr2Gate.Debounce();
    adsr2Trigger.Debounce();
    adsr1GateState = adsr1Gate.Pressed();
    adsr1TriggerState = adsr1Trigger.Pressed();
    adsr2GateState = adsr2Gate.Pressed();
    adsr2TriggerState = adsr2Trigger.Pressed();
    // SET TRIGGERS AND STATES TO FALSE TO START
    adsr1FilteredTrigger = false;
    adsr2FilteredTrigger = false;
    // IF SOCKETS ARE PLUGGED IN AND TRIGGER IS HIGH AND LAST TRIGGER IS LOW, SET TRIGGERS
    if( !adsr1TriggerIsUnplugged && !adsr1GateIsUnplugged ) 
        adsr1FilteredTrigger = adsr1TriggerState && !lastAdsr1TriggerState;
    if( !adsr2TriggerIsUnplugged && !adsr2GateIsUnplugged )
        adsr2FilteredTrigger = adsr2TriggerState && !lastAdsr2TriggerState;
    // SET THE 'LAST ...' SETTINGS FOR NEXT TIME
    lastAdsr1GateState = adsr1GateState;
    lastAdsr1TriggerState = adsr1TriggerState;
    lastAdsr2GateState = adsr2GateState;
    lastAdsr2TriggerState = adsr2TriggerState;
}
int main(){
    hw.Init();
    initADC();
    initDAC();
    initTriggersAndGates();
    initNormalSignalTimer();
    normalSignalWriterPin.Init( daisy::seed::D1, GPIO::Mode::OUTPUT );
    adsr1.Init( SAMPLE_RATE );
    adsr2.Init( SAMPLE_RATE );
    delayLine.Init();
    while( true ){
        handleKnobs();
        handleTriggersAndGates();
        if( adsr1FilteredTrigger && adsr1GateState ) adsr1.Retrigger( false );
        if( adsr2FilteredTrigger && adsr2GateState ) adsr2.Retrigger( false );
        // SET ADSR STATE TO FALSE TO START, THEN CHECK GATE AND TRIGGER IF THEY'RE PLUGGED IN
        adsr1State = false;
        if( !adsr1GateIsUnplugged ) adsr1State = adsr1GateState;
        if( !adsr1State && !adsr1TriggerIsUnplugged ) adsr1State = adsr1FilteredTrigger;
        adsr2State = false;
        if( !adsr2GateIsUnplugged ) adsr2State = adsr2GateState;
        if( !adsr2State && !adsr2TriggerIsUnplugged ) adsr2State = adsr2FilteredTrigger;
        adsr1State = adsr1GateState || adsr1TriggerState;
        adsr2State = adsr2GateState || adsr2TriggerState;
        if( !adsr2GateState && lastAdsr2GateState ){
            delayLine.Reset();
            delayLine.SetDelay( delayTime ); // SET THE DELAY TIME AGAIN, SINCE RESET SETS THIS
        }
        System::Delay( 1 );
    }
}