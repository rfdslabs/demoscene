// Pure-native audio: CoreAudio default output unit pulling interleaved stereo
// floats from the procedural Synth. The render callback also publishes the
// sample position that drives the whole demo's master clock.
#import <AudioToolbox/AudioToolbox.h>
#include "platform.h"
#include "synth.h"

namespace {
  AudioUnit    g_unit = nullptr;
  ti::Synth*   g_synth = nullptr;

  OSStatus render_cb(void* /*ref*/, AudioUnitRenderActionFlags* /*flags*/,
                     const AudioTimeStamp* /*ts*/, UInt32 /*bus*/,
                     UInt32 frames, AudioBufferList* io) {
    float* out = (float*)io->mBuffers[0].mData;   // interleaved L,R,L,R...
    if (g_synth) {
      g_synth->render(out, (int)frames);
      plat::g_audio_samples = g_synth->sample_pos;
    } else {
      for (UInt32 i = 0; i < frames * 2; ++i) out[i] = 0.0f;
    }
    return noErr;
  }
}

namespace plat {

  void audio_start(ti::Synth* synth) {
    g_synth = synth;
    g_audio_sr = synth ? synth->sr : 44100.0;

    AudioComponentDescription desc = {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) return;
    if (AudioComponentInstanceNew(comp, &g_unit) != noErr) return;

    AudioStreamBasicDescription fmt = {};
    fmt.mSampleRate = g_audio_sr;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked; // interleaved
    fmt.mChannelsPerFrame = 2;
    fmt.mBitsPerChannel = 32;
    fmt.mFramesPerPacket = 1;
    fmt.mBytesPerFrame = sizeof(float) * 2;
    fmt.mBytesPerPacket = fmt.mBytesPerFrame;

    AudioUnitSetProperty(g_unit, kAudioUnitProperty_StreamFormat,
                         kAudioUnitScope_Input, 0, &fmt, sizeof(fmt));

    AURenderCallbackStruct cb = {};
    cb.inputProc = render_cb;
    AudioUnitSetProperty(g_unit, kAudioUnitProperty_SetRenderCallback,
                         kAudioUnitScope_Input, 0, &cb, sizeof(cb));

    AudioUnitInitialize(g_unit);
    AudioOutputUnitStart(g_unit);
  }
}
