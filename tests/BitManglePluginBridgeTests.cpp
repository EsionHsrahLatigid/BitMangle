#include "BitManglePlugin.h"
#include <yup_audio_processors/yup_audio_processors.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
namespace { constexpr int numChannels=2; constexpr int blockSamples=2048;
class PluginHarness { public: PluginHarness(): audio(numChannels,blockSamples), context{audio,midi,automation,nullptr,{},{}} { plugin.prepareToPlay(yup::AudioSpec(48000.0f,blockSamples,numChannels)); }
float processSilence() { audio.clear(); plugin.processBlock(context); return peak(); }
float processConstant(float value) { for(int c=0;c<audio.getNumChannels();++c) for(int s=0;s<audio.getNumSamples();++s) audio.getWritePointer(c)[s]=value; plugin.processBlock(context); return peak(); }
float peak() const { float r=0; for(int c=0;c<audio.getNumChannels();++c) { const auto* x=audio.getReadPointer(c); for(int s=0;s<audio.getNumSamples();++s) r=std::max(r,std::fabs(x[s])); } return r; }
bitmangle::plugin::BitManglePlugin plugin; private: yup::AudioBuffer<float> audio; yup::MidiBuffer midi; yup::ParameterChangeBuffer automation; yup::AudioProcessContext<float> context; };
void testHostedSilencePreserved() { PluginHarness h; for(int i=0;i<4;++i) assert(h.processSilence()<=1e-7f); }
void testHostedDoesNotAcceptMidi() { PluginHarness h; assert(!h.plugin.acceptsMidi()); assert(!h.plugin.producesMidi()); }
void testSevenParameters() { PluginHarness h; assert(h.plugin.getParameters().size()==7); }
void testParameterResponse() { PluginHarness a; auto ap=a.plugin.getParameters(); ap[0]->setValue(16.0f); ap[1]->setValue(0.0f); float soft=a.processConstant(.8f); PluginHarness b; auto bp=b.plugin.getParameters(); bp[0]->setValue(3.0f); bp[1]->setValue(1.0f); float hard=b.processConstant(.8f); assert(std::fabs(hard-soft)>.01f); assert(hard<=1.001f); }
void testStateRoundTrip() { PluginHarness source; auto sp=source.plugin.getParameters(); source.plugin.setCurrentPreset(2); sp[0]->setValue(7.0f); sp[1]->setValue(.7f); sp[2]->setValue(.4f); yup::MemoryBlock data; assert(source.plugin.saveStateIntoMemory(data).wasOk()); PluginHarness target; assert(target.plugin.loadStateFromMemory(data).wasOk()); const auto tp=target.plugin.getParameters(); assert(target.plugin.getCurrentPreset()==2); assert(std::fabs(tp[0]->getValue()-7.0f)<=1e-6f); assert(std::fabs(tp[1]->getValue()-.7f)<=1e-6f); assert(std::fabs(tp[2]->getValue()-.4f)<=1e-6f); }
}
int main(){ testHostedSilencePreserved(); testHostedDoesNotAcceptMidi(); testSevenParameters(); testParameterResponse(); testStateRoundTrip(); std::cout<<"BitManglePluginBridgeTests passed\n"; }
