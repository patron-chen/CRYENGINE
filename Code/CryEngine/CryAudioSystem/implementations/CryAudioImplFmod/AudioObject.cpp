// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioObject.h"
#include "AudioEvent.h"
#include <AudioLogger.h>

using namespace CryAudio::Impl::Fmod;

extern FmodSwitchToIndexMap g_switchToIndex;
extern AudioParameterToIndexMap g_parameterToIndex;

//////////////////////////////////////////////////////////////////////////
CAudioObject::CAudioObject(FmodAudioObjectId const id)
	: m_id(id)
	, m_obstruction(0.0f)
	, m_occlusion(0.0f)
{
	ZeroStruct(m_attributes);

	// Reserve enough room for events to minimize/prevent runtime allocations.
	m_audioEvents.reserve(2);
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::RemoveAudioEvent(CAudioEvent* const pAudioEvent)
{
	stl::find_and_erase(m_audioEvents, pAudioEvent);
}

//////////////////////////////////////////////////////////////////////////
bool CAudioObject::SetAudioEvent(CAudioEvent* const pAudioEvent)
{
	bool bSuccess = false;

	// Update the event with all parameter and switch values
	// that are currently set on the audio object before starting it.
	if (pAudioEvent->PrepareForOcclusion())
	{
		m_audioEvents.push_back(pAudioEvent);
		FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

		for (auto const& parameterPair : m_audioParameters)
		{
			AudioParameterToIndexMap::iterator const iter(g_parameterToIndex.find(parameterPair.first));

			if (iter != g_parameterToIndex.end())
			{
				if (pAudioEvent->GetEventPathId() == iter->first->GetEventPathId())
				{
					if (iter->second != FMOD_IMPL_INVALID_INDEX)
					{
						FMOD::Studio::ParameterInstance* pParameterInstance = nullptr;
						pAudioEvent->GetInstance()->getParameterByIndex(iter->second, &pParameterInstance);

						if (pParameterInstance != nullptr)
						{
							fmodResult = pParameterInstance->setValue(parameterPair.second);
							ASSERT_FMOD_OK;
						}
					}
					else
					{
						SetParameter(iter->first, parameterPair.second);
					}
				}
			}
			else
			{
				g_audioImplLogger.Log(eAudioLogType_Warning, "Trying to set an unknown Fmod parameter during \"AddAudioEvent\": %s", parameterPair.first->GetName().c_str());
			}
		}

		for (auto const& switchPair : m_audioSwitches)
		{
			FmodSwitchToIndexMap::iterator const iter(g_switchToIndex.find(switchPair.second));

			if (iter != g_switchToIndex.end())
			{
				if (pAudioEvent->GetEventPathId() == iter->first->eventPathId)
				{
					if (iter->second != FMOD_IMPL_INVALID_INDEX)
					{
						FMOD::Studio::ParameterInstance* pParameterInstance = nullptr;
						pAudioEvent->GetInstance()->getParameterByIndex(iter->second, &pParameterInstance);

						if (pParameterInstance != nullptr)
						{
							fmodResult = pParameterInstance->setValue(switchPair.second->value);
							ASSERT_FMOD_OK;
						}
					}
					else
					{
						SetSwitch(switchPair.second);
					}
				}
			}
			else
			{
				g_audioImplLogger.Log(eAudioLogType_Warning, "Trying to set an unknown Fmod switch during \"AddAudioEvent\": %s", switchPair.second->name.c_str());
			}
		}

		for (auto const& environmentPair : m_audioEnvironments)
		{
			pAudioEvent->TrySetEnvironment(environmentPair.first, environmentPair.second);
		}

		pAudioEvent->SetObstructionOcclusion(m_obstruction, m_occlusion);

		fmodResult = pAudioEvent->GetInstance()->start();
		ASSERT_FMOD_OK;

		bSuccess = true;
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::RemoveParameter(CAudioParameter const* const pParameter)
{
	m_audioParameters.erase(pParameter);
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::SetParameter(CAudioParameter const* const pAudioParameter, float const value)
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

	for (auto const pAudioEvent : m_audioEvents)
	{
		if (pAudioEvent->GetEventPathId() == pAudioParameter->GetEventPathId())
		{
			FMOD::Studio::ParameterInstance* pParameterInstance = nullptr;
			AudioParameterToIndexMap::iterator const iter(g_parameterToIndex.find(pAudioParameter));

			if (iter != g_parameterToIndex.end())
			{
				if (iter->second != FMOD_IMPL_INVALID_INDEX)
				{
					pAudioEvent->GetInstance()->getParameterByIndex(iter->second, &pParameterInstance);

					if (pParameterInstance != nullptr)
					{
						fmodResult = pParameterInstance->setValue(pAudioParameter->GetValueMultiplier() * value + pAudioParameter->GetValueShift());
						ASSERT_FMOD_OK;
					}
					else
					{
						g_audioImplLogger.Log(eAudioLogType_Warning, "Unknown Fmod parameter index (%d) for (%s)", iter->second, pAudioParameter->GetName().c_str());
					}
				}
				else
				{
					int parameterCount = 0;
					fmodResult = pAudioEvent->GetInstance()->getParameterCount(&parameterCount);
					ASSERT_FMOD_OK;

					for (int index = 0; index < parameterCount; ++index)
					{
						fmodResult = pAudioEvent->GetInstance()->getParameterByIndex(index, &pParameterInstance);
						ASSERT_FMOD_OK;

						FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
						fmodResult = pParameterInstance->getDescription(&parameterDescription);
						ASSERT_FMOD_OK;

						if (pAudioParameter->GetName().compareNoCase(parameterDescription.name) == 0)
						{
							iter->second = index;
							fmodResult = pParameterInstance->setValue(pAudioParameter->GetValueMultiplier() * value + pAudioParameter->GetValueShift());
							ASSERT_FMOD_OK;

							break;
						}
					}
				}
			}
			else
			{
				g_audioImplLogger.Log(eAudioLogType_Warning, "Trying to set an unknown Fmod parameter: %s", pAudioParameter->GetName().c_str());
			}
		}
	}

	AudioParameters::iterator const iter(m_audioParameters.find(pAudioParameter));

	if (iter != m_audioParameters.end())
	{
		iter->second = value;
	}
	else
	{
		m_audioParameters.emplace(pAudioParameter, value);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::RemoveSwitch(CAudioSwitchState const* const pSwitch)
{
	m_audioSwitches.erase(pSwitch->eventPathId);
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::SetSwitch(CAudioSwitchState const* const pSwitch)
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

	for (auto const pAudioEvent : m_audioEvents)
	{
		if (pAudioEvent->GetEventPathId() == pSwitch->eventPathId)
		{
			FMOD::Studio::ParameterInstance* pParameterInstance = nullptr;
			FmodSwitchToIndexMap::iterator const iter(g_switchToIndex.find(pSwitch));

			if (iter != g_switchToIndex.end())
			{
				if (iter->second != FMOD_IMPL_INVALID_INDEX)
				{
					pAudioEvent->GetInstance()->getParameterByIndex(iter->second, &pParameterInstance);

					if (pParameterInstance != nullptr)
					{
						fmodResult = pParameterInstance->setValue(pSwitch->value);
						ASSERT_FMOD_OK;
					}
				}
				else
				{
					int parameterCount = 0;
					fmodResult = pAudioEvent->GetInstance()->getParameterCount(&parameterCount);
					ASSERT_FMOD_OK;

					for (int index = 0; index < parameterCount; ++index)
					{
						fmodResult = pAudioEvent->GetInstance()->getParameterByIndex(index, &pParameterInstance);
						ASSERT_FMOD_OK;

						FMOD_STUDIO_PARAMETER_DESCRIPTION parameterDescription;
						fmodResult = pParameterInstance->getDescription(&parameterDescription);
						ASSERT_FMOD_OK;

						if (pSwitch->name.compareNoCase(parameterDescription.name) == 0)
						{
							iter->second = index;
							fmodResult = pParameterInstance->setValue(pSwitch->value);
							ASSERT_FMOD_OK;

							break;
						}
					}
				}
			}
			else
			{
				g_audioImplLogger.Log(eAudioLogType_Warning, "Trying to set an unknown Fmod switch: %s", pSwitch->name.c_str());
			}
		}
	}

	AudioSwitches::iterator const iter(m_audioSwitches.find(pSwitch->eventPathId));

	if (iter != m_audioSwitches.end())
	{
		iter->second = pSwitch;
	}
	else
	{
		m_audioSwitches.emplace(pSwitch->eventPathId, pSwitch);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::RemoveEnvironment(CAudioEnvironment const* const pEnvironment)
{
	m_audioEnvironments.erase(pEnvironment);
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::SetEnvironment(CAudioEnvironment const* const pEnvironment, float const value)
{
	bool bSet = true;
	AudioEnvironments::iterator const iter(m_audioEnvironments.find(pEnvironment));

	if (iter != m_audioEnvironments.end())
	{
		if (bSet = (fabs(iter->second - value) > 0.001f))
		{
			iter->second = value;
		}
	}
	else
	{
		m_audioEnvironments.emplace(pEnvironment, value);
	}

	if (bSet)
	{
		for (auto const pAudioEvent : m_audioEvents)
		{
			pAudioEvent->TrySetEnvironment(pEnvironment, value);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::Set3DAttributes(CryAudio::Impl::SAudioObject3DAttributes const& attributes)
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;
	FillFmodObjectPosition(attributes, m_attributes);

	for (auto const pAudioEvent : m_audioEvents)
	{
		fmodResult = pAudioEvent->GetInstance()->set3DAttributes(&m_attributes);
		ASSERT_FMOD_OK;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::StopAllEvents()
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

	for (auto const pAudioEvent : m_audioEvents)
	{
		fmodResult = pAudioEvent->GetInstance()->stop(FMOD_STUDIO_STOP_IMMEDIATE);
		ASSERT_FMOD_OK;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::StopEvent(uint32 const eventPathId)
{
	FMOD_RESULT fmodResult = FMOD_ERR_UNINITIALIZED;

	for (auto const pAudioEvent : m_audioEvents)
	{
		if (pAudioEvent->GetEventPathId() == eventPathId)
		{
			fmodResult = pAudioEvent->GetInstance()->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
			ASSERT_FMOD_OK;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::SetObstructionOcclusion(float const obstruction, float const occlusion)
{
	for (auto const pAudioEvent : m_audioEvents)
	{
		pAudioEvent->SetObstructionOcclusion(obstruction, occlusion);
	}

	m_obstruction = obstruction;
	m_occlusion = occlusion;
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::Reset()
{
	m_audioEvents.clear();
	m_audioParameters.clear();
	m_audioSwitches.clear();
	m_obstruction = 0.0f;
	m_occlusion = 0.0f;
}
