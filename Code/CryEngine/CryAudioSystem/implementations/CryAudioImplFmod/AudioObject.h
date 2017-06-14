// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLEntities.h"
#include <SharedAudioData.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{

using FmodAudioObjectId = uint32;

class CAudioObject final : public IAudioObject
{
public:

	explicit CAudioObject(FmodAudioObjectId const _id);
	virtual ~CAudioObject() override = default;

	CAudioObject(CAudioObject const&) = delete;
	CAudioObject(CAudioObject&&) = delete;
	CAudioObject&             operator=(CAudioObject const&) = delete;
	CAudioObject&             operator=(CAudioObject&&) = delete;

	FmodAudioObjectId         GetId() const { return m_id; }
	void                      RemoveAudioEvent(CAudioEvent* const pAudioEvent);
	bool                      SetAudioEvent(CAudioEvent* const pAudioEvent);
	void                      RemoveParameter(CAudioParameter const* const pParameter);
	void                      SetParameter(CAudioParameter const* const pAudioParameter, float const value);
	void                      RemoveSwitch(CAudioSwitchState const* const pSwitch);
	void                      SetSwitch(CAudioSwitchState const* const pSwitch);
	void                      RemoveEnvironment(CAudioEnvironment const* const pEnvironment);
	void                      SetEnvironment(CAudioEnvironment const* const pEnvironment, float const value);
	void                      Set3DAttributes(CryAudio::Impl::SAudioObject3DAttributes const& attributes);
	void                      StopAllEvents();
	void                      StopEvent(uint32 const eventPathId);
	void                      SetObstructionOcclusion(float const obstruction, float const occlusion);
	FMOD_3D_ATTRIBUTES const& Get3DAttributes() const { return m_attributes; }
	void                      Reset();

private:

	FmodAudioObjectId const m_id;
	FMOD_3D_ATTRIBUTES      m_attributes;
	float                   m_obstruction;
	float                   m_occlusion;

	AudioEvents             m_audioEvents;
	typedef std::map<CAudioParameter const* const, float>    AudioParameters;
	AudioParameters         m_audioParameters;
	typedef std::map<uint32 const, CAudioSwitchState const*> AudioSwitches;
	AudioSwitches           m_audioSwitches;
	typedef std::map<CAudioEnvironment const* const, float, std::less<CAudioEnvironment const* const>,
	                 STLSoundAllocator<std::pair<CAudioEnvironment const* const, float>>> AudioEnvironments;
	AudioEnvironments m_audioEnvironments;
};
}
}
}
