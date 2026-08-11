#include "bitmangle/BitMangleEngine.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
using bitmangle::BitMangleEngine; using bitmangle::BitMangleParameters;
namespace {
std::vector<float> render(BitMangleParameters p){ BitMangleEngine e; e.prepare(48000); e.setParameters(p); e.reset(); std::vector<float> out; for(int i=0;i<4096;++i){ float x=-.9f+1.8f*float(i)/4095.f; out.push_back(e.processSample(x,x).left);} return out; }
int distinct(const std::vector<float>& v){ std::vector<int> q; for(float x:v) q.push_back(int(std::round(x*1000))); std::sort(q.begin(),q.end()); q.erase(std::unique(q.begin(),q.end()),q.end()); return int(q.size()); }
float alternatingRms(BitMangleParameters p){ BitMangleEngine e; e.prepare(48000); e.setParameters(p); e.reset(); float energy=0; int count=0; for(int i=0;i<4096;++i){ const auto x=(i&1)?0.8f:-0.8f; const auto y=e.processSample(x,x).left; if(i>=256){ energy+=y*y; ++count; }} return std::sqrt(energy/static_cast<float>(count)); }
void testSilence(){ BitMangleEngine e; e.prepare(48000); BitMangleParameters p; p.dither=0; e.setParameters(p); for(int i=0;i<4096;++i){ auto f=e.processSample(0,0); assert(std::fabs(f.left)<=1e-7f); assert(std::fabs(f.right)<=1e-7f);} }
void testBits(){ BitMangleParameters hi; hi.bits=16; hi.dither=0; hi.mix=1; BitMangleParameters lo=hi; lo.bits=3; assert(distinct(render(lo)) < distinct(render(hi)) / 5); }
void testReduce(){ BitMangleParameters a; a.reduce=0; a.dither=0; BitMangleParameters b=a; b.reduce=1; assert(distinct(render(b)) < distinct(render(a))); }
void testAntiAlias(){ BitMangleParameters bypass; bypass.bits=16; bypass.reduce=0; bypass.dither=0; bypass.motion=0; bypass.antiAlias=0; BitMangleParameters filtered=bypass; filtered.antiAlias=1; assert(alternatingRms(filtered) < alternatingRms(bypass) * .8f); }
void testMix(){ BitMangleParameters p; p.bits=2; p.mix=0; auto v=render(p); assert(distinct(v) > 100); }
void testDeterministic(){ BitMangleParameters p; p.bits=5; p.dither=.7f; auto a=render(p); auto b=render(p); for(size_t i=0;i<a.size();++i) assert(std::fabs(a[i]-b[i])<=1e-6f); }
void testFinite(){ BitMangleEngine e; BitMangleParameters p; p.bits=1000; p.reduce=1000; p.curve=1000; p.dither=1000; p.antiAlias=1000; p.motion=1000; p.mix=1000; e.prepare(0); e.setParameters(p); for(int i=0;i<4096;++i){ auto f=e.processSample(1000,-1000); assert(std::isfinite(f.left)); assert(f.left>=-.9801f&&f.left<=.9801f); } }
}
int main(){ testSilence(); testBits(); testReduce(); testAntiAlias(); testMix(); testDeterministic(); testFinite(); std::cout<<"BitMangleEngineTests passed\n"; }
