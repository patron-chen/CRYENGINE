// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityAudioComponent.h"

#include <CryEntitySystem/IEntityComponent.h>
#include <CryEntitySystem/IEntity.h>
#include <Schematyc/Entity/EntityUtils.h>
#include <Schematyc/Entity/EntityClasses.h>
#include <Schematyc/Types/ResourceTypes.h>

#include "AutoRegister.h"
#include "STDModules.h"

namespace Schematyc
{
bool CEntityAudioComponent::Init()
{
	//the audio callbacks we are interested in, all related to executeTrigger, #TODO: Check if it`s worth constantly registering/unregister on ExecuteTrigger/TriggerFinished
	gEnv->pAudioSystem->AddRequestListener(&CEntityAudioComponent::OnAudioCallback, this, eAudioRequestType_AudioCallbackManagerRequest, eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance);
	gEnv->pAudioSystem->AddRequestListener(&CEntityAudioComponent::OnAudioCallback, this, eAudioRequestType_AudioObjectRequest, eAudioObjectRequestType_ExecuteTrigger);

	IEntity& entity = EntityUtils::GetEntity(*this);
	m_pAudioProxy = entity.GetOrCreateComponent<IEntityAudioComponent>();
	if (!m_pAudioProxy)
	{
		CRY_ASSERT_MESSAGE(m_pAudioProxy, "Could not create audio proxy");
		return false;
	}

	if (m_audioProxyId != INVALID_AUDIO_PROXY_ID && m_audioProxyId != DEFAULT_AUDIO_PROXY_ID)
	{
		m_pAudioProxy->RemoveAuxAudioProxy(m_audioProxyId);  //#TODO: for now this is a workaround, because there are scenarios where 'Init' is called twice without a 'Shutdown' in between.
	}

	const Vec3 offset = Schematyc::CComponent::GetTransform().GetTranslation();
	if (!IsEquivalent(offset, Vec3(ZERO)))  //only create an aux-proxy when needed (if there is an offset specified)
	{
		m_audioProxyId = m_pAudioProxy->CreateAuxAudioProxy();
		m_pAudioProxy->SetAuxAudioProxyOffset(Matrix34(IDENTITY, offset), m_audioProxyId);
	}
	else
	{
		m_audioProxyId = DEFAULT_AUDIO_PROXY_ID;
	}

	return true;
}

void CEntityAudioComponent::Shutdown()
{
	gEnv->pAudioSystem->RemoveRequestListener(nullptr, this);  //remove all listener-callback-functions from this object

	CRY_ASSERT_MESSAGE(m_pAudioProxy, "Audio proxy was not created");
	if (m_audioProxyId != INVALID_AUDIO_PROXY_ID && m_audioProxyId != DEFAULT_AUDIO_PROXY_ID)
	{
		m_pAudioProxy->RemoveAuxAudioProxy(m_audioProxyId);
	}
	m_audioProxyId = INVALID_AUDIO_PROXY_ID;
}

void CEntityAudioComponent::Register(IEnvRegistrar& registrar)
{
	CEnvRegistrationScope scope = registrar.Scope(g_entityClassGUID);
	{
		auto pComponent = SCHEMATYC_MAKE_ENV_COMPONENT(CEntityAudioComponent, "Audio");
		pComponent->SetAuthor(g_szCrytek);
		pComponent->SetDescription("Entity audio component");
		pComponent->SetIcon("icons:schematyc/entity_audio_component.ico");
		pComponent->SetFlags({ Schematyc::EEnvComponentFlags::Transform, Schematyc::EEnvComponentFlags::Attach });
		scope.Register(pComponent);

		CEnvRegistrationScope componentScope = registrar.Scope(pComponent->GetGUID());
		// Functions
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAudioComponent::ExecuteTrigger, "0D58AF22-775A-4FBE-BC5C-3A7CE250EF98"_schematyc_guid, "ExecuteAudioTrigger");
			pFunction->SetAuthor(g_szCrytek);
			pFunction->SetDescription("Executes a trigger");
			pFunction->BindInput(1, 'sta', "StartTrigger");
			pFunction->BindOutput(2, 'inst', "InstanceId");
			pFunction->BindOutput(3, 'id', "TriggerId");
			componentScope.Register(pFunction);
		}

		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAudioComponent::StopTrigger, "E4016C26-87E9-4880-8BAE-D8D39E974AFC"_schematyc_guid, "StopAudioTrigger");
			pFunction->SetAuthor(g_szCrytek);
			pFunction->SetDescription("Stops a trigger");
			pFunction->BindInput(1, 'sto', "StopTrigger");
			componentScope.Register(pFunction);
		}

		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAudioComponent::SetParameter, "FBE1DD7C-57C1-46CE-89A1-3612CFD017E4"_schematyc_guid, "SetAudioParameter");
			pFunction->SetAuthor(g_szCrytek);
			pFunction->SetDescription("Sets a parameter to a specific value");
			pFunction->BindInput(1, 'par', "Parameter");
			pFunction->BindInput(2, 'val', "Value");
			componentScope.Register(pFunction);
		}

		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityAudioComponent::SetSwitchState, "7ABA1505-527C-4882-9399-716C0E43FFCD"_schematyc_guid, "SetAudioSwitch");
			pFunction->SetAuthor(g_szCrytek);
			pFunction->SetDescription("Sets a switch to a specific state");
			pFunction->BindInput(1, 'swi', "SwitchAndState");
			componentScope.Register(pFunction);
		}

		// Signals
		{
			auto pSignal = SCHEMATYC_MAKE_ENV_SIGNAL_TYPE(SAudioTriggerFinishedSignal, "AudioTriggerFinishedSignal");
			pSignal->SetAuthor(g_szCrytek);
			pSignal->SetDescription("Sent when an audio trigger finishes");
			componentScope.Register(pSignal);
		}
	}
}

void CEntityAudioComponent::ExecuteTrigger(const SAudioTriggerSerializeHelper trigger, uint32& _instanceId, uint32& _triggerId)
{
	if (m_pAudioProxy && trigger.m_triggerId != INVALID_AUDIO_CONTROL_ID)
	{
		_triggerId = static_cast<uint32>(trigger.m_triggerId);
		static uint32 currentInstanceId = 1;
		SAudioCallBackInfo callbackInfo(this, (void*)static_cast<UINT_PTR>(currentInstanceId), (void*)static_cast<UINT_PTR>(_triggerId), eAudioRequestFlags_PriorityNormal | eAudioRequestFlags_SyncFinishedCallback);
		if (m_pAudioProxy->ExecuteTrigger(trigger.m_triggerId, m_audioProxyId, callbackInfo))
		{
			_instanceId = currentInstanceId++;
		}
	}
	else
	{
		_triggerId = 0;
		_instanceId = 0;
	}
}

void CEntityAudioComponent::StopTrigger(const SAudioTriggerSerializeHelper trigger)
{
	if (m_pAudioProxy && trigger.m_triggerId != INVALID_AUDIO_CONTROL_ID)
	{
		m_pAudioProxy->StopTrigger(trigger.m_triggerId, m_audioProxyId);
	}
}

void CEntityAudioComponent::SetParameter(const SAudioParameterSerializeHelper parameter, float value)
{
	if (m_pAudioProxy && parameter.m_parameterId != INVALID_AUDIO_CONTROL_ID)
	{
		m_pAudioProxy->SetRtpcValue(parameter.m_parameterId, value, m_audioProxyId);
	}
}

void CEntityAudioComponent::SetSwitchState(const SAudioSwitchWithStateSerializeHelper switchAndState)
{
	if (m_pAudioProxy && switchAndState.m_switchId != INVALID_AUDIO_CONTROL_ID && switchAndState.m_switchStateId != INVALID_AUDIO_CONTROL_ID)
	{
		m_pAudioProxy->SetSwitchState(switchAndState.m_switchId, switchAndState.m_switchStateId, m_audioProxyId);
	}
}

SGUID CEntityAudioComponent::ReflectSchematycType(CTypeInfo<CEntityAudioComponent>& typeInfo)
{
	return "7E792283-20BB-4D18-B3DD-08ADF38C92BE"_schematyc_guid;
}

void CEntityAudioComponent::OnAudioCallback(SAudioRequestInfo const* const pAudioRequestInfo)
{
	uint32 instanceId = (uint32)reinterpret_cast<UINT_PTR>(pAudioRequestInfo->pUserData);
	uint32 triggerId = (uint32)reinterpret_cast<UINT_PTR>(pAudioRequestInfo->pUserDataOwner);
	CEntityAudioComponent* pAudioComp = static_cast<CEntityAudioComponent*>(pAudioRequestInfo->pOwner);

	if (pAudioRequestInfo->requestResult == eAudioRequestResult_Failure)  //failed to start/finish
	{
		pAudioComp->GetObject().ProcessSignal(SAudioTriggerFinishedSignal(instanceId, triggerId, false));
	}
	else if (pAudioRequestInfo->audioRequestType == eAudioRequestType_AudioCallbackManagerRequest  //finished successful
	         && pAudioRequestInfo->specificAudioRequest == eAudioCallbackManagerRequestType_ReportFinishedTriggerInstance)
	{
		pAudioComp->GetObject().ProcessSignal(SAudioTriggerFinishedSignal(instanceId, triggerId, true));
	}
}

SGUID SAudioTriggerSerializeHelper::ReflectSchematycType(CTypeInfo<SAudioTriggerSerializeHelper>& typeInfo)
{
	return "C5DE4974-ECAB-4D6F-A93D-02C1F5C55C31"_schematyc_guid;
}

SGUID SAudioParameterSerializeHelper::ReflectSchematycType(CTypeInfo<SAudioParameterSerializeHelper>& typeInfo)
{
	return "5287D8F9-7638-41BB-BFDD-2F5B47DEEA07"_schematyc_guid;
}

SGUID SAudioSwitchWithStateSerializeHelper::ReflectSchematycType(CTypeInfo<SAudioSwitchWithStateSerializeHelper>& typeInfo)
{
	return "9DB56B33-57FE-4E97-BED2-F0BBD3012967"_schematyc_guid;
}

SGUID CEntityAudioComponent::SAudioTriggerFinishedSignal::ReflectSchematycType(CTypeInfo<SAudioTriggerFinishedSignal>& typeInfo)
{
	typeInfo.AddMember(&SAudioTriggerFinishedSignal::m_instanceId, 'inst', "InstanceId", "TriggerId");
	typeInfo.AddMember(&SAudioTriggerFinishedSignal::m_triggerId, 'id', "TriggerId", "TriggerId");
	typeInfo.AddMember(&SAudioTriggerFinishedSignal::m_bSuccess, 'res', "Result", "Result");

	return "A16A29CB-8E39-42C0-88C2-33FED1680545"_schematyc_guid;
}

CEntityAudioComponent::SAudioTriggerFinishedSignal::SAudioTriggerFinishedSignal(uint32 instanceId, uint32 triggerId, bool bSuccess)
	: m_instanceId(instanceId)
	, m_triggerId(triggerId)
	, m_bSuccess(bSuccess)
{
}

void SAudioTriggerSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(Serialization::AudioTrigger<string>(m_triggerName), "triggerName", "^Name");

	if (archive.isInput())
	{
		gEnv->pAudioSystem->GetAudioTriggerId(m_triggerName.c_str(), m_triggerId);
	}
}

void SAudioParameterSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(Serialization::AudioRTPC<string>(m_parameterName), "parameter", "^Name");

	if (archive.isInput())
	{
		gEnv->pAudioSystem->GetAudioRtpcId(m_parameterName.c_str(), m_parameterId);
	}
}

void SAudioSwitchWithStateSerializeHelper::Serialize(Serialization::IArchive& archive)
{
	archive(Serialization::AudioSwitch<string>(m_switchName), "switchName", "SwitchName");
	archive(Serialization::AudioSwitchState<string>(m_switchStateName), "stateName", "StateName");

	if (archive.isInput())
	{
		gEnv->pAudioSystem->GetAudioSwitchId(m_switchName.c_str(), m_switchId);
		gEnv->pAudioSystem->GetAudioSwitchStateId(m_switchId, m_switchStateName.c_str(), m_switchStateId);
	}
}
} //Schematyc

SCHEMATYC_AUTO_REGISTER(&Schematyc::CEntityAudioComponent::Register)
