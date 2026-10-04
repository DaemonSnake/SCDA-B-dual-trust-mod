#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace audio {

constexpr uint32_t kRate = 48000;

// the platform output pulls interleaved stereo float frames from render
void StartOutput(std::function<void(float* out, uint32_t frames)> render);

// headset audio (voice.cpp), any thread: the recording devices' names, as app::Settings::microphone keeps them,
// and 16-bit PCM streams, null when a device would not open
std::vector<std::string> Microphones();
struct VoiceStream;
VoiceStream* OpenMicrophone(const std::string& name, uint32_t rate, uint32_t channels);  // empty: the system default
VoiceStream* OpenSpeaker(uint32_t rate, uint32_t channels);
uint32_t Available(VoiceStream* s);  // recorded bytes ready
uint32_t Read(VoiceStream* s, void* out, uint32_t bytes);
uint32_t Queued(VoiceStream* s);     // bytes still to play
void Write(VoiceStream* s, const void* data, uint32_t bytes);
void Close(VoiceStream* s);

// the wideband voice link (voice_link.cpp): Opus on UDP 1011 to the peers the game sends its voice to, played in
// place of the game's own while it arrives. Started and stopped with the game's microphone, counted.
void StartVoiceLink(const std::string& microphone);
void StopVoiceLink();
void VoiceSent(uint32_t address);  // the game sent voice to this IPv4, network order
bool VoiceLinkHeard();             // the link's voice arrived lately: the game's is not played

}  // namespace audio
