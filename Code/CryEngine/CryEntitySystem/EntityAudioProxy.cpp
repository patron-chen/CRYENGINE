// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EntityAudioProxy.h"
#include <CryAudio/IAudioSystem.h>
#include <CryAnimation/ICryAnimation.h>
#include "Entity.h"

CRYREGISTER_CLASS(CEntityComponentAudio);

CEntityComponentAudio::TAudioProxyPair CEntityComponentAudio::s_nullAudioProxyPair(INVALID_AUDIO_PROXY_ID, static_cast<IAudioProxy*>(nullptr));
CAudioObjectTransformation CEntityComponentAudio::s_audioListenerLastTransformation;

//////////////////////////////////////////////////////////////////////////
CEntityComponentAudio::CEntityComponentAudio()
	: m_audioProxyIDCounter(INVALID_AUDIO_PROXY_ID)
	, m_audioEnvironmentId(INVALID_AUDIO_ENVIRONMENT_ID)
	, m_flags(eEAPF_CAN_MOVE_WITH_ENTITY)
	, m_fadeDistance(0.0f)
	, m_environmentFadeDistance(0.0f)
{
}

//////////////////////////////////////////////////////////////////////////
CEntityComponentAudio::~CEntityComponentAudio()
{
	std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SReleaseAudioProxy());
	m_mapAuxAudioProxies.clear();
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::Initialize()
{
	assert(m_mapAuxAudioProxies.empty());

	if ((m_pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) > 0)
	{
		m_flags &= ~eEAPF_CAN_MOVE_WITH_ENTITY;
	}

	// Creating the default AudioProxy.
	CreateAuxAudioProxy();
	SetObstructionCalcType(eAudioOcclusionType_Ignore);
	OnMove();
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::OnMove()
{
	CRY_ASSERT_MESSAGE(!(((m_flags & eEAPF_CAN_MOVE_WITH_ENTITY) > 0) && ((m_pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) > 0)), "An CEntityAudioProxy cannot have both flags (eEAPF_CAN_MOVE_WITH_ENTITY & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) set simultaneously!");

	Matrix34 const& tm = m_pEntity->GetWorldTM();
	CRY_ASSERT_MESSAGE(tm.IsValid(), "Invalid Matrix34 during CEntityAudioProxy::OnMove");

	if ((m_flags & eEAPF_CAN_MOVE_WITH_ENTITY) > 0)
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SRepositionAudioProxy(tm));
	}
	else if ((m_pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) > 0)
	{
		Matrix34 transformation = tm;
		transformation += CVar::audioListenerOffset;

		if (!s_audioListenerLastTransformation.IsEquivalent(transformation, 0.01f))
		{
			s_audioListenerLastTransformation = transformation;

			SAudioRequest request;
			request.flags = eAudioRequestFlags_PriorityNormal;
			request.pOwner = this;

			SAudioListenerRequestData<eAudioListenerRequestType_SetTransformation> requestData(s_audioListenerLastTransformation);

			request.pData = &requestData;

			gEnv->pAudioSystem->PushRequest(request);

			// As this is an audio listener add its entity to the AreaManager for raising audio relevant events.
			gEnv->pEntitySystem->GetAreaManager()->MarkEntityForUpdate(m_pEntity->GetId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::OnListenerMoveInside(Vec3 const& listenerPos)
{
	m_pEntity->SetPos(listenerPos);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::OnListenerExclusiveMoveInside(IEntity const* const __restrict pEntity, IEntity const* const __restrict pAreaHigh, IEntity const* const __restrict pAreaLow, float const fade)
{
	IEntityAreaComponent const* const __restrict pAreaProxyLow = static_cast<IEntityAreaComponent const* const __restrict>(pAreaLow->GetProxy(ENTITY_PROXY_AREA));
	IEntityAreaComponent* const __restrict pAreaProxyHigh = static_cast<IEntityAreaComponent* const __restrict>(pAreaHigh->GetProxy(ENTITY_PROXY_AREA));

	if (pAreaProxyLow != nullptr && pAreaProxyHigh != nullptr)
	{
		Vec3 OnHighHull3d(ZERO);
		Vec3 const oPos(pEntity->GetWorldPos());
		EntityId const entityId = pEntity->GetId();
		bool const bInsideLow = pAreaProxyLow->CalcPointWithin(entityId, oPos);

		if (bInsideLow)
		{
			pAreaProxyHigh->ClosestPointOnHullDistSq(entityId, oPos, OnHighHull3d);
			m_pEntity->SetPos(OnHighHull3d);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::OnListenerEnter(IEntity const* const pEntity)
{
	m_pEntity->SetPos(pEntity->GetWorldPos());
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::OnListenerMoveNear(Vec3 const& closestPointToArea)
{
	m_pEntity->SetPos(closestPointToArea);
}


uint64 CEntityComponentAudio::GetEventMask() const
{
	return 
		BIT64(ENTITY_EVENT_DONE) |
		BIT64(ENTITY_EVENT_XFORM) |
		BIT64(ENTITY_EVENT_ENTERAREA) |
		BIT64(ENTITY_EVENT_MOVENEARAREA) |
		BIT64(ENTITY_EVENT_ENTERNEARAREA) |
		BIT64(ENTITY_EVENT_MOVEINSIDEAREA) |
		BIT64(ENTITY_EVENT_ANIM_EVENT);
}


//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::ProcessEvent(SEntityEvent& event)
{
	if (m_pEntity != nullptr)
	{
		switch (event.event)
		{
		case ENTITY_EVENT_XFORM:
			{
				int const flags = (int)event.nParam[0];

				if ((flags & (ENTITY_XFORM_POS | ENTITY_XFORM_ROT)) > 0)
				{
					OnMove();
				}

				break;
			}
		case ENTITY_EVENT_ENTERAREA:
			{
				if ((m_pEntity->GetFlags() & ENTITY_FLAG_VOLUME_SOUND) > 0)
				{
					EntityId const entityId = static_cast<EntityId>(event.nParam[0]); // Entering entity!
					IEntity* const pIEntity = gEnv->pEntitySystem->GetEntity(entityId);

					if ((pIEntity != nullptr) && (pIEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) > 0)
					{
						OnListenerEnter(pIEntity);
					}
				}

				break;
			}
		case ENTITY_EVENT_MOVENEARAREA:
		case ENTITY_EVENT_ENTERNEARAREA:
			{
				if ((m_pEntity->GetFlags() & ENTITY_FLAG_VOLUME_SOUND) > 0)
				{
					EntityId const entityId = static_cast<EntityId>(event.nParam[0]); // Near entering/moving entity!
					IEntity* const pIEntity = gEnv->pEntitySystem->GetEntity(entityId);

					if (pIEntity != nullptr)
					{
						if ((pIEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) > 0)
						{
							OnListenerMoveNear(event.vec);
						}
					}
				}

				break;
			}
		case ENTITY_EVENT_MOVEINSIDEAREA:
			{
				if ((m_pEntity->GetFlags() & ENTITY_FLAG_VOLUME_SOUND) > 0)
				{
					EntityId const entityId = static_cast<EntityId>(event.nParam[0]); // Inside moving entity!
					IEntity* const __restrict pIEntity = gEnv->pEntitySystem->GetEntity(entityId);

					if (pIEntity != nullptr)
					{
						EntityId const area1Id = static_cast<EntityId>(event.nParam[2]); // AreaEntityID (low)
						EntityId const area2Id = static_cast<EntityId>(event.nParam[3]); // AreaEntityID (high)

						IEntity* const __restrict pArea1 = gEnv->pEntitySystem->GetEntity(area1Id);
						IEntity* const __restrict pArea2 = gEnv->pEntitySystem->GetEntity(area2Id);

						if (pArea1 != nullptr)
						{
							if (pArea2 != nullptr)
							{
								if ((pIEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) > 0)
								{
									OnListenerExclusiveMoveInside(pIEntity, pArea2, pArea1, event.fParam[0]);
								}
							}
							else
							{
								if ((pIEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) > 0)
								{
									OnListenerMoveInside(event.vec);
								}
							}
						}
					}
				}

				break;
			}
		case ENTITY_EVENT_ANIM_EVENT:
			{
				REINST("reintroduce anim event voice playing in EntityAudioProxy");
				/*if (!IsSoundAnimEventsHandledExternally())
				   {
				   const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
				   ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
				   const char* eventName = (pAnimEvent ? pAnimEvent->m_EventName : 0);
				   if (eventName && stricmp(eventName, "sound") == 0)
				   {
				    Vec3 offset(ZERO);
				    if (pAnimEvent->m_BonePathName && pAnimEvent->m_BonePathName[0])
				    {
				      if (pCharacter)
				      {
				        IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
				        int id = rIDefaultSkeleton.GetJointIDByName(pAnimEvent->m_BonePathName);
				        if (id >= 0)
				        {
				          ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
				          QuatT boneQuat(pSkeletonPose->GetAbsJointByID(id));
				          offset = boneQuat.t;
				        }
				      }
				    }

				    int flags = FLAG_SOUND_DEFAULT_3D;
				    if (strchr(pAnimEvent->m_CustomParameter, ':') == nullptr)
				      flags |= FLAG_SOUND_VOICE;
				    PlaySound(pAnimEvent->m_CustomParameter, offset, FORWARD_DIRECTION, flags, 0, eSoundSemantic_Animation, 0, 0);
				   }
				   }*/

				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::GameSerialize(TSerialize ser)
{
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentAudio::PlayFile(SAudioPlayFileInfo const& playbackInfo, AudioProxyId const audioProxyId /* = DEFAULT_AUDIO_PROXY_ID */, SAudioCallBackInfo const& callBackInfo /* = SAudioCallBackInfo::GetEmptyObject() */)
{
	if (m_pEntity != nullptr)
	{
		if (audioProxyId != INVALID_AUDIO_PROXY_ID)
		{
			TAudioProxyPair const& audioProxyPair = GetAuxAudioProxyPair(audioProxyId);

			if (audioProxyPair.first != INVALID_AUDIO_PROXY_ID)
			{
				(SPlayFile(playbackInfo, callBackInfo))(audioProxyPair);
				return true;
			}
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
			else
			{
				gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Could not find AuxAudioProxy with id '%u' on entity '%s' to PlayFile '%s'", audioProxyId, m_pEntity->GetEntityTextDescription().c_str(), playbackInfo.szFile);
			}
#endif  // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
		}
		else
		{
			std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SPlayFile(playbackInfo, callBackInfo));
			return !m_mapAuxAudioProxies.empty();
		}
	}
	else
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Trying to play an audio file on an EntityAudioProxy without a valid entity!");
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::StopFile(
  char const* const _szFile,
  AudioProxyId const _audioProxyId /*= DEFAULT_AUDIO_PROXY_ID*/)
{
	if (m_pEntity != nullptr)
	{
		if (_audioProxyId != INVALID_AUDIO_PROXY_ID)
		{
			TAudioProxyPair const& audioProxyPair = GetAuxAudioProxyPair(_audioProxyId);

			if (audioProxyPair.first != INVALID_AUDIO_PROXY_ID)
			{
				(SStopFile(_szFile))(audioProxyPair);
			}
		}
		else
		{
			std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SStopFile(_szFile));
		}
	}
	else
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Trying to stop an audio file on an EntityAudioProxy without a valid entity!");
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentAudio::ExecuteTrigger(
  AudioControlId const audioTriggerId,
  AudioProxyId const audioProxyId /*= DEFAULT_AUDIO_PROXY_ID*/,
  SAudioCallBackInfo const& callBackInfo /*= SAudioCallBackInfo::GetEmptyObject()*/)
{
	if (m_pEntity != nullptr)
	{
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
		if (m_pEntity->GetWorldTM().GetTranslation() == Vec3Constants<float>::fVec3_Zero)
		{
			gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Trying to execute an audio trigger at (0,0,0) position in the entity %s. Entity may not be initialized correctly!", m_pEntity->GetEntityTextDescription().c_str());
		}
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE

		if ((m_pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_DISABLED) == 0)
		{
			if (audioProxyId != INVALID_AUDIO_PROXY_ID)
			{
				TAudioProxyPair const& audioProxyPair = GetAuxAudioProxyPair(audioProxyId);

				if (audioProxyPair.first != INVALID_AUDIO_PROXY_ID)
				{
					(SRepositionAudioProxy(m_pEntity->GetWorldTM()))(audioProxyPair);
					audioProxyPair.second.pIAudioProxy->ExecuteTrigger(audioTriggerId, callBackInfo);
					return true;
				}
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
				else
				{
					gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Could not find AuxAudioProxy with id '%u' on entity '%s' to ExecuteTrigger '%u'", audioProxyId, m_pEntity->GetEntityTextDescription().c_str(), audioTriggerId);
				}
#endif  // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
			}
			else
			{
				for (TAuxAudioProxies::iterator it = m_mapAuxAudioProxies.begin(); it != m_mapAuxAudioProxies.end(); ++it)
				{
					(SRepositionAudioProxy(m_pEntity->GetWorldTM()))(*it);
					it->second.pIAudioProxy->ExecuteTrigger(audioTriggerId, callBackInfo);
				}
				return !m_mapAuxAudioProxies.empty();
			}
		}
	}
	else
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Trying to execute an audio trigger on an EntityAudioProxy without a valid entity!");
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::StopTrigger(AudioControlId const audioTriggerId, AudioProxyId const audioProxyId /*= DEFAULT_AUDIO_PROXY_ID*/)
{
	if (audioProxyId != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& audioProxyPair = GetAuxAudioProxyPair(audioProxyId);

		if (audioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			(SStopTrigger(audioTriggerId))(audioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SStopTrigger(audioTriggerId));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::SetSwitchState(AudioControlId const audioSwitchId, AudioSwitchStateId const audioStateId, AudioProxyId const audioProxyId /*= DEFAULT_AUDIO_PROXY_ID*/)
{
	if (audioProxyId != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& audioProxyPair = GetAuxAudioProxyPair(audioProxyId);

		if (audioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			(SSetSwitchState(audioSwitchId, audioStateId))(audioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetSwitchState(audioSwitchId, audioStateId));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::SetRtpcValue(AudioControlId const audioRtpcId, float const value, AudioProxyId const audioProxyId /*= DEFAULT_AUDIO_PROXY_ID*/)
{
	if (audioProxyId != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& audioProxyPair = GetAuxAudioProxyPair(audioProxyId);

		if (audioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			(SSetRtpcValue(audioRtpcId, value))(audioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetRtpcValue(audioRtpcId, value));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::SetObstructionCalcType(EAudioOcclusionType const occlusionType, AudioProxyId const audioProxyId /*= DEFAULT_AUDIO_PROXY_ID*/)
{
	if (audioProxyId != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& audioProxyPair = GetAuxAudioProxyPair(audioProxyId);

		if (audioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			(SSetOcclusionType(occlusionType))(audioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetOcclusionType(occlusionType));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::SetEnvironmentAmount(AudioEnvironmentId const audioEnvironmentId, float const amount, AudioProxyId const audioProxyId /*= DEFAULT_AUDIO_PROXY_ID*/)
{
	if (audioProxyId != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& audioProxyPair = GetAuxAudioProxyPair(audioProxyId);

		if (audioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			SSetEnvironmentAmount(audioEnvironmentId, amount)(audioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetEnvironmentAmount(audioEnvironmentId, amount));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::SetCurrentEnvironments(AudioProxyId const audioProxyId /*= DEFAULT_AUDIO_PROXY_ID*/)
{
	if (audioProxyId != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair const& audioProxyPair = GetAuxAudioProxyPair(audioProxyId);

		if (audioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			SSetCurrentEnvironments(m_pEntity->GetId())(audioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetCurrentEnvironments(m_pEntity->GetId()));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::AuxAudioProxiesMoveWithEntity(bool const bCanMoveWithEntity)
{
	if (bCanMoveWithEntity)
	{
		m_flags |= eEAPF_CAN_MOVE_WITH_ENTITY;
	}
	else
	{
		m_flags &= ~eEAPF_CAN_MOVE_WITH_ENTITY;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::AddAsListenerToAuxAudioProxy(AudioProxyId const audioProxyId, void (* func)(SAudioRequestInfo const* const), EAudioRequestType requestType /*= eAudioRequestType_AudioAllRequests*/, AudioEnumFlagsType specificRequestMask /*= ALL_AUDIO_REQUEST_SPECIFIC_TYPE_FLAGS*/)
{
	TAuxAudioProxies::const_iterator const iter(m_mapAuxAudioProxies.find(audioProxyId));

	if (iter != m_mapAuxAudioProxies.end())
	{
		gEnv->pAudioSystem->AddRequestListener(func, iter->second.pIAudioProxy, requestType, specificRequestMask);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::RemoveAsListenerFromAuxAudioProxy(AudioProxyId const audioProxyId, void (* func)(SAudioRequestInfo const* const))
{
	TAuxAudioProxies::const_iterator const iter(m_mapAuxAudioProxies.find(audioProxyId));

	if (iter != m_mapAuxAudioProxies.end())
	{
		gEnv->pAudioSystem->RemoveRequestListener(func, iter->second.pIAudioProxy);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::SetAuxAudioProxyOffset(Matrix34 const& offset, AudioProxyId const audioProxyId /*= DEFAULT_AUDIO_PROXY_ID*/)
{
	if (audioProxyId != INVALID_AUDIO_PROXY_ID)
	{
		TAudioProxyPair& audioProxyPair = GetAuxAudioProxyPair(audioProxyId);

		if (audioProxyPair.first != INVALID_AUDIO_PROXY_ID)
		{
			SSetAuxAudioProxyOffset(offset, m_pEntity->GetWorldTM())(audioProxyPair);
		}
	}
	else
	{
		std::for_each(m_mapAuxAudioProxies.begin(), m_mapAuxAudioProxies.end(), SSetAuxAudioProxyOffset(offset, m_pEntity->GetWorldTM()));
	}
}

//////////////////////////////////////////////////////////////////////////
Matrix34 const& CEntityComponentAudio::GetAuxAudioProxyOffset(AudioProxyId const audioProxyId /*= DEFAULT_AUDIO_PROXY_ID*/)
{
	TAuxAudioProxies::const_iterator const iter(m_mapAuxAudioProxies.find(audioProxyId));

	if (iter != m_mapAuxAudioProxies.end())
	{
		return iter->second.offset;
	}

	static const Matrix34 identityMatrix(IDENTITY);
	return identityMatrix;
}

//////////////////////////////////////////////////////////////////////////
float CEntityComponentAudio::GetGreatestFadeDistance() const
{
	return std::max<float>(m_fadeDistance, m_environmentFadeDistance);
}

//////////////////////////////////////////////////////////////////////////
AudioProxyId CEntityComponentAudio::CreateAuxAudioProxy()
{
	AudioProxyId nAudioProxyLocalID = INVALID_AUDIO_PROXY_ID;

	if ((m_pEntity->GetFlagsExtended() & ENTITY_FLAG_EXTENDED_AUDIO_LISTENER) == 0)
	{
		IAudioProxy* const pIAudioProxy = gEnv->pAudioSystem->GetFreeAudioProxy();

		if (pIAudioProxy != nullptr)
		{
#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
			if (m_audioProxyIDCounter == std::numeric_limits<AudioProxyId>::max())
			{
				CryFatalError("<Audio> Exceeded numerical limits during CEntityAudioProxy::CreateAudioProxy!");
			}
			else if (m_pEntity == nullptr)
			{
				CryFatalError("<Audio> nullptr entity pointer during CEntityAudioProxy::CreateAudioProxy!");
			}

			CryFixedStringT<MAX_AUDIO_OBJECT_NAME_LENGTH> sFinalName(m_pEntity->GetName());
			size_t const nNumAuxAudioProxies = m_mapAuxAudioProxies.size();

			if (nNumAuxAudioProxies > 0)
			{
				// First AuxAudioProxy is not explicitly identified, it keeps the entity's name.
				// All additionally AuxaudioProxies however are being explicitly identified.
				sFinalName.Format("%s_auxaudioproxy_#%" PRISIZE_T, m_pEntity->GetName(), nNumAuxAudioProxies + 1);
			}

			pIAudioProxy->Initialize(sFinalName.c_str());
#else
			pIAudioProxy->Initialize(nullptr);
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE

			pIAudioProxy->SetPosition(m_pEntity->GetWorldPos());
			pIAudioProxy->SetOcclusionType(eAudioOcclusionType_Ignore);
			pIAudioProxy->SetCurrentEnvironments(m_pEntity->GetId());

			m_mapAuxAudioProxies.insert(TAudioProxyPair(++m_audioProxyIDCounter, SAudioProxyWrapper(pIAudioProxy)));
			nAudioProxyLocalID = m_audioProxyIDCounter;
		}
	}

	return nAudioProxyLocalID;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentAudio::RemoveAuxAudioProxy(AudioProxyId const audioProxyId)
{
	bool bSuccess = false;

	if (audioProxyId != DEFAULT_AUDIO_PROXY_ID)
	{
		TAuxAudioProxies::iterator iter(m_mapAuxAudioProxies.find(audioProxyId));

		if (iter != m_mapAuxAudioProxies.end())
		{
			iter->second.pIAudioProxy->Release();
			m_mapAuxAudioProxies.erase(iter);
			bSuccess = true;
		}
		else
		{
			gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "<Audio> AuxAudioProxy with ID '%u' not found during CEntityAudioProxy::RemoveAuxAudioProxy (%s)!", audioProxyId, m_pEntity->GetEntityTextDescription().c_str());
			assert(false);
		}
	}
	else
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_ERROR, VALIDATOR_FLAG_AUDIO, 0, "<Audio> Trying to remove the default AudioProxy during CEntityAudioProxy::RemoveAuxAudioProxy (%s)!", m_pEntity->GetEntityTextDescription().c_str());
		assert(false);
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
CEntityComponentAudio::TAudioProxyPair& CEntityComponentAudio::GetAuxAudioProxyPair(AudioProxyId const audioProxyId)
{
	TAuxAudioProxies::iterator const iter(m_mapAuxAudioProxies.find(audioProxyId));

	if (iter != m_mapAuxAudioProxies.end())
	{
		return *iter;
	}

	return s_nullAudioProxyPair;
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentAudio::SetEnvironmentAmountInternal(IEntity const* const pIEntity, float const amount) const
{
	// If the passed-in entity is our parent we skip it.
	// Meaning we do not apply our own environment to ourselves.
	if (pIEntity != nullptr && m_pEntity != nullptr && pIEntity != m_pEntity)
	{
		auto pIEntityAudioComponent = pIEntity->GetComponent<IEntityAudioComponent>();

		if ((pIEntityAudioComponent != nullptr) && (m_audioEnvironmentId != INVALID_AUDIO_ENVIRONMENT_ID))
		{
			// Only set the audio-environment-amount on the entities that already have an AudioProxy.
			// Passing INVALID_AUDIO_PROXY_ID to address all auxiliary AudioProxies on pEntityAudioProxy.
			CRY_ASSERT(amount >= 0.0f && amount <= 1.0f);
			pIEntityAudioComponent->SetEnvironmentAmount(m_audioEnvironmentId, amount, INVALID_AUDIO_PROXY_ID);
		}
	}
}
