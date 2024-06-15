#pragma once
#include <EtCore/Util/Singleton.h>

#ifdef ET_PLATFORM_WIN
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#endif

namespace et {
namespace fw {


class AudioManager : public core::Singleton<AudioManager>
{
public:
	void Initialize();

	bool TestALError(std::string error);

	void SetDistanceModel(ALenum model);

	void MakeContextCurrent();

private:
	void ListAudioDevices(const ALCchar *devices);

	ALCdevice* m_Device;
	ALCcontext *m_Context;

private:
	friend class core::Singleton<AudioManager>;
	AudioManager() {}
	virtual ~AudioManager();
};


} // namespace fw
} // namespace et
