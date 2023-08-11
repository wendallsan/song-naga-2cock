#include "daisy_seed.h"
#include "daisysp.h"
using namespace daisy;
using namespace daisysp;

/*
Tests to do:

verify we can read trigger and gate inputs.

NOTES: 
    U5 -> PIN_ADC_MUX_IN_1 (CV SIGNALS)
    U7 -> PIN_ADC_MUX_IN_2 (MAIN KNOBS)
    U8 -> PIN_ADC_MUX_IN_3 (CV ADJUST KNOBS)
*/


#define PIN_ADC_MUX_IN_1 daisy::seed::A0
#define PIN_ADC_MUX_IN_2 daisy::seed::A1
#define PIN_ADC_MUX_IN_3 daisy::seed::A2
#define PIN_ADC_DELAY_IN daisy::seed::A3
#define PIN_ADC_DELAY_ADJUST_IN daisy::seed::A4
#define MUX_CHANNELS 8
#define PIN_MUX_SW_1 11
#define PIN_MUX_SW_2 12
#define PIN_MUX_SW_3 13

// TODO: CAN'T USE THIS FOR SOME REASON, REMOVE IF WE CAN'T GET IT TO WORK
#define NORMAL_WRITER_PIN daisy::seed::D1;

// TODO: THESE GATE AND TRIGGER PINS MAY BE OFF BY ONE (SHOULD START AT PIN 6 INSTEAD OF 7)
#define ADSR1_GATE_PIN 7
#define ADSR1_TRIGGER_PIN 8
#define ADSR2_GATE_PIN 9
#define ADSR2_TRIGGER_PIN 10
#define SAMPLE_RATE 48000
#define NORMAL_MATRIX_LENGTH 10
#define MAX_DELAY static_cast<size_t>( SAMPLE_RATE * 4.f )
#define NORMAL_MATRIX_INIT_STATE { false, false, false, false, false, false, false, false, false, false }
DaisySeed hw;
Adsr adsr1, adsr2;
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
static uint16_t DMA_BUFFER_MEM_SECTION dac1Buffer[ kDacBufferSize ],
    dac2Buffer[ kDacBufferSize ];
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

    adsr1AttackAdjust,
    adsr1DecayAdjust,
    adsr1SustainAdjust,
    adsr1ReleaseAdjust,
    adsr2DelayAdjust,
    adsr2AttackAdjust,
    adsr2DecayAdjust,
    adsr2SustainAdjust,
    adsr2ReleaseAdjust,

    delayCV,
    attackCV,
    decayCV,
    sustainCV,
    releaseCV,

    lastAdsr2DelayValue = 2.f,
    delayTime = 1.f;
bool adsr1GateNormalMatrix[ NORMAL_MATRIX_LENGTH ] = NORMAL_MATRIX_INIT_STATE,
    adsr1TriggerNormalMatrix[ NORMAL_MATRIX_LENGTH ] = NORMAL_MATRIX_INIT_STATE,
    adsr2GateNormalMatrix[ NORMAL_MATRIX_LENGTH ] = NORMAL_MATRIX_INIT_STATE,
    adsr2TriggerNormalMatrix[ NORMAL_MATRIX_LENGTH ] = NORMAL_MATRIX_INIT_STATE;
int matrixCounter = 0;
enum adcChannels {
	ADC_MUX_1,
    ADC_MUX_2,
    ADC_MUX_3,
    DELAY,
    DELAY_ADJUST,
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
// ADD CV ADJUST KNOBS MUX CONTROLS
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
        out[0][i] = adsr1.Process( adsr1State ) * 4095.f;
        // HANDLE OUT 2: READ FROM THE DELAY LINE
        out[1][i] = adsr2.Process( delayLine.Read() ) * 4095.f;
    }
}
void handleKnobs(){
    // HANDLE THE MUXED MAIN KNOBS
    adsr1AttackValue = hw.adc.GetMuxFloat( ADC_MUX_2, ADSR1_ATTACK );
    adsr1DecayValue = hw.adc.GetMuxFloat( ADC_MUX_2, ADSR1_DECAY );
    adsr1SustainValue = hw.adc.GetMuxFloat( ADC_MUX_2, ADSR1_SUSTAIN );
    adsr1ReleaseValue = hw.adc.GetMuxFloat( ADC_MUX_2, ADSR1_RELEASE );
    adsr2AttackValue = hw.adc.GetMuxFloat( ADC_MUX_2, ADSR2_ATTACK );
    adsr2DecayValue = hw.adc.GetMuxFloat( ADC_MUX_2, ADSR2_DECAY );
    adsr2SustainValue = hw.adc.GetMuxFloat( ADC_MUX_2, ADSR2_SUSTAIN );
    adsr2ReleaseValue = hw.adc.GetMuxFloat( ADC_MUX_2, ADSR2_RELEASE );

    // HANDLE THE MUXED CV ADJUST KNOBS
    adsr1AttackAdjust = hw.adc.GetMuxFloat( ADC_MUX_3, ADSR1_ATTACK );
    adsr1DecayAdjust = hw.adc.GetMuxFloat( ADC_MUX_3, ADSR1_DECAY );
    adsr1SustainAdjust = hw.adc.GetMuxFloat( ADC_MUX_3, ADSR1_SUSTAIN );
    adsr1ReleaseAdjust = hw.adc.GetMuxFloat( ADC_MUX_3, ADSR1_RELEASE );
    adsr2AttackAdjust = hw.adc.GetMuxFloat( ADC_MUX_3, ADSR2_ATTACK );
    adsr2DecayAdjust = hw.adc.GetMuxFloat( ADC_MUX_3, ADSR2_DECAY );
    adsr2SustainAdjust = hw.adc.GetMuxFloat( ADC_MUX_3, ADSR2_SUSTAIN );
    adsr2ReleaseAdjust = hw.adc.GetMuxFloat( ADC_MUX_3, ADSR2_RELEASE );

    // HANDLE THE NON-MUXED KNOBS
    adsr2DelayValue = hw.adc.GetFloat( DELAY );
    adsr2DelayAdjust = hw.adc.GetFloat( DELAY_ADJUST );
}
void initADC(){
    AdcChannelConfig adcConfig[ NUM_ADC_CHANNELS ];
    adcConfig[ ADC_MUX_1 ].InitMux( 
        PIN_ADC_MUX_IN_1, 
        MUX_CHANNELS, 
        hw.GetPin( PIN_MUX_SW_1 ), 
        hw.GetPin( PIN_MUX_SW_2 ), 
        hw.GetPin( PIN_MUX_SW_3 )
    );
    adcConfig[ ADC_MUX_2 ].InitMux( 
        PIN_ADC_MUX_IN_2, 
        MUX_CHANNELS, 
        hw.GetPin( PIN_MUX_SW_1 ), 
        hw.GetPin( PIN_MUX_SW_2 ), 
        hw.GetPin( PIN_MUX_SW_3 )
    );
    adcConfig[ ADC_MUX_3 ].InitMux( 
        PIN_ADC_MUX_IN_3, 
        MUX_CHANNELS, 
        hw.GetPin( PIN_MUX_SW_1 ), 
        hw.GetPin( PIN_MUX_SW_2 ), 
        hw.GetPin( PIN_MUX_SW_3 )
    );
    adcConfig[ DELAY ].InitSingle( PIN_ADC_DELAY_IN );
    adcConfig[ DELAY_ADJUST ].InitSingle( PIN_ADC_DELAY_IN );
    hw.adc.Init( adcConfig, NUM_ADC_CHANNELS );
    hw.adc.Start();
}
void initDAC(){
    DacHandle::Config cfg;
    cfg.bitdepth   = DacHandle::BitDepth::BITS_12;
    cfg.buff_state = DacHandle::BufferState::ENABLED;
    cfg.mode       = DacHandle::Mode::POLLING;
    cfg.chn        = DacHandle::Channel::BOTH;
    hw.dac.Init( cfg );
    hw.dac.Start( dac1Buffer, dac2Buffer, kDacBufferSize, handleDac );
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

    if( adsr1TriggerState ) hw.PrintLine( "ADSR 1 Trigger" );
    if( adsr2TriggerState ) hw.PrintLine( "ADSR 2 Trigger" );

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
void handleCVInputs(){
    delayCV = hw.adc.GetMuxFloat( ADC_MUX_1, DELAY_CV );
    attackCV = hw.adc.GetMuxFloat( ADC_MUX_1, ATTACK_CV );
    decayCV = hw.adc.GetMuxFloat( ADC_MUX_1, DECAY_CV );
    sustainCV = hw.adc.GetMuxFloat( ADC_MUX_1, SUSTAIN_CV );
    releaseCV = hw.adc.GetMuxFloat( ADC_MUX_1, RELEASE_CV );
}
float mapControls( float a, float b, float c ){
    return fclamp( a + ( b * c ), 0.f, 1.f );
}
void adjustEnvelopes(){
    // MAP TIME-RELATED VALUES TO 0 - 4 SECONDS
    adsr1.SetAttackTime( fmap( mapControls( adsr1AttackValue, adsr1AttackAdjust, attackCV ), 0.f, 4.f ) );
    adsr1.SetDecayTime( fmap( mapControls( adsr1DecayValue, adsr1DecayAdjust, decayCV ), 0.f, 4.f ) );
    adsr1.SetSustainLevel( mapControls( adsr1SustainValue, adsr1SustainAdjust, sustainCV ) );
    adsr1.SetReleaseTime( fmap( mapControls( adsr1ReleaseValue, adsr1ReleaseAdjust, releaseCV ), 0.f, 4.f ) );
    adsr2.SetAttackTime( fmap( mapControls( adsr2AttackValue, adsr2AttackAdjust, attackCV ), 0.f, 4.f ) );
    adsr2.SetDecayTime( fmap( mapControls( adsr2DecayValue, adsr2DecayAdjust, decayCV ), 0.f, 4.f ) );
    adsr2.SetSustainLevel( mapControls( adsr2SustainValue, adsr2SustainAdjust, sustainCV ) );
    adsr2.SetReleaseTime( fmap( mapControls( adsr2ReleaseValue, adsr2ReleaseAdjust, releaseCV ), 0.f, 4.f ) );
    delayLine.SetDelay( fclamp(
        fmap( mapControls( adsr2DelayValue, adsr2DecayAdjust, delayCV ), 0.f, 4.f ) * SAMPLE_RATE,
        1.f, 
        MAX_DELAY
    ) );
}
int main(){
    hw.Init();
    hw.StartLog();
    initADC();
    // TODO: INITDAC CAUSES SERIAL MONITOR TO NOT WORK.
    initDAC();

    initTriggersAndGates();
    // initNormalSignalTimer();
    // normalSignalWriterPin.Init( daisy::seed::D1, GPIO::Mode::OUTPUT );
    // adsr1.Init( SAMPLE_RATE );
    // adsr2.Init( SAMPLE_RATE );
    // delayLine.Init();
    int debugCounter = 0;
    bool ledHigh = false;
    while( true ){
        // handleKnobs();
        handleTriggersAndGates();
        // handleCVInputs();
        // adjustEnvelopes();
        // if( adsr1FilteredTrigger && adsr1GateState ) adsr1.Retrigger( false );
        // if( adsr2FilteredTrigger && adsr2GateState ) adsr2.Retrigger( false );
        // // SET ADSR STATE TO FALSE TO START, THEN CHECK GATE AND TRIGGER IF THEY'RE PLUGGED IN
        // adsr1State = false;
        // if( !adsr1GateIsUnplugged ) adsr1State = adsr1GateState;
        // if( !adsr1State && !adsr1TriggerIsUnplugged ) adsr1State = adsr1FilteredTrigger;
        // adsr2State = false;
        // if( !adsr2GateIsUnplugged ) adsr2State = adsr2GateState;
        // if( !adsr2State && !adsr2TriggerIsUnplugged ) adsr2State = adsr2FilteredTrigger;
        // adsr1State = adsr1GateState || adsr1TriggerState;
        // adsr2State = adsr2GateState || adsr2TriggerState;
        // if( !adsr2GateState && lastAdsr2GateState ){
        //     delayLine.Reset();
        //     delayLine.SetDelay( delayTime ); // SET THE DELAY TIME AGAIN, SINCE RESET SETS THIS
        // }
        debugCounter++;
        if( debugCounter >= 500 ){
            ledHigh = !ledHigh;
            hw.SetLed( ledHigh );
            debugCounter = 0;
            hw.PrintLine( "." );
        }
        System::Delay( 1 );
    }
}