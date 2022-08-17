#include "daisy_seed.h"
#include "daisysp.h"
using namespace daisy;
using namespace daisysp;
DaisySeed hw;
Adsr adsr1;
Switch triggerButton1;
bool buttonIsPressed = false;
uint16_t DMA_BUFFER_MEM_SECTION dac_buffer[48];
// SET lastProcessResult TO AN INVALID VALUE SO IT REPORTS THE FIRST TIME IT CHANGES
float processResult = 0.0, lastProcessResult = 2.0;
void handleDac(	uint16_t **out, size_t size ){
    for( size_t i = 0; i < size; i++ ){
        // convert to 12-bit integer (0-4095)
        processResult = adsr1.Process( triggerButton1.Pressed() );
        uint16_t value = processResult * 4095.0;
        out[0][i] = value;
    }
}
int main( void ){
    hw.Configure();
    hw.Init();
    hw.StartLog( true );
    hw.PrintLine( "logger started." );
    DacHandle::Config dacConfig;
    dacConfig.bitdepth = DacHandle::BitDepth::BITS_12;
    dacConfig.buff_state = DacHandle::BufferState::ENABLED;
    dacConfig.mode = DacHandle::Mode::DMA;
    dacConfig.chn = DacHandle::Channel::ONE;
    DacHandle::Result dacInitResult = hw.dac.Init( dacConfig );
    DacHandle::Result dacStartResult = hw.dac.Start( dac_buffer, 48, handleDac );
    hw.Print( "dac init result: " );
    hw.PrintLine( dacInitResult == DacHandle::Result::OK? "ok" : "err" );
    hw.Print( "dac start result: " );
    hw.PrintLine( dacStartResult == DacHandle::Result::OK? "ok" : "err" );
    // PIN 15 (AKA D15) IS ACTUALLY PIN 22 ON THE DAISY SEED, RIGHT?
	triggerButton1.Init( hw.GetPin( 15 ), 100 ); 
    // adsr1.Init( dacConfig.target_samplerate );
    adsr1.Init( 48000.0 );
    // STATICALLY SET ADSR VALUES FOR NOW
    adsr1.SetAttackTime( 0.5 );
    adsr1.SetDecayTime( 1.0 );
    adsr1.SetSustainLevel( 0.5 );
    adsr1.SetReleaseTime( 1.0 );
    while( true ){        
        triggerButton1.Debounce();
        if( triggerButton1.Pressed() ){
            if( buttonIsPressed != true ){
                buttonIsPressed = true;
                hw.PrintLine( "pressed!" );
            }
        } else buttonIsPressed = false;
        if( processResult != lastProcessResult ){
            lastProcessResult = processResult;
            hw.PrintLine( "process: " FLT_FMT3, FLT_VAR3( processResult ) );
        }
        System::Delay( 1 );
    }
}