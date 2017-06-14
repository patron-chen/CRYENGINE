// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ATLEntityData.h>
#include <IAudioImpl.h>
#include <AK/SoundEngine/Common/AkTypes.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
typedef std::vector<AkUniqueID, STLSoundAllocator<AkUniqueID>> TAKUniqueIDVector;

struct SAudioObject final : public IAudioObject
{
	typedef std::map<AkAuxBusID, float, std::less<AkAuxBusID>,
	                 STLSoundAllocator<std::pair<AkAuxBusID const, float>>> TEnvironmentImplMap;

	explicit SAudioObject(AkGameObjectID const _id, bool const _bHasPosition)
		: id(_id)
		, bHasPosition(_bHasPosition)
		, bNeedsToUpdateEnvironments(false)
	{}

	virtual ~SAudioObject() override = default;

	SAudioObject(SAudioObject const&) = delete;
	SAudioObject(SAudioObject&&) = delete;
	SAudioObject& operator=(SAudioObject const&) = delete;
	SAudioObject& operator=(SAudioObject&&) = delete;

	AkGameObjectID const id;
	bool const           bHasPosition;
	bool                 bNeedsToUpdateEnvironments;
	TEnvironmentImplMap  environemntImplAmounts;
};

struct SAudioListener final : public IAudioListener
{
	explicit SAudioListener(AkUniqueID const _id)
		: id(_id)
	{}

	virtual ~SAudioListener() override = default;

	SAudioListener(SAudioListener const&) = delete;
	SAudioListener(SAudioListener&&) = delete;
	SAudioListener& operator=(SAudioListener const&) = delete;
	SAudioListener& operator=(SAudioListener&&) = delete;

	AkUniqueID const id;
};

struct SAudioTrigger final : public IAudioTrigger
{
	explicit SAudioTrigger(AkUniqueID const _id)
		: id(_id)
	{}

	virtual ~SAudioTrigger() override = default;

	SAudioTrigger(SAudioTrigger const&) = delete;
	SAudioTrigger(SAudioTrigger&&) = delete;
	SAudioTrigger& operator=(SAudioTrigger const&) = delete;
	SAudioTrigger& operator=(SAudioTrigger&&) = delete;

	AkUniqueID const id;
};

struct SAudioRtpc final : public IAudioRtpc
{
	explicit SAudioRtpc(AkRtpcID const _id, float const _mult, float const _shift)
		: mult(_mult)
		, shift(_shift)
		, id(_id)
	{}

	virtual ~SAudioRtpc() override = default;

	SAudioRtpc(SAudioRtpc const&) = delete;
	SAudioRtpc(SAudioRtpc&&) = delete;
	SAudioRtpc& operator=(SAudioRtpc const&) = delete;
	SAudioRtpc& operator=(SAudioRtpc&&) = delete;

	float const    mult;
	float const    shift;
	AkRtpcID const id;
};

enum EWwiseSwitchType : AudioEnumFlagsType
{
	eWwiseSwitchType_None = 0,
	eWwiseSwitchType_Switch,
	eWwiseSwitchType_State,
	eWwiseSwitchType_Rtpc,
};

struct SAudioSwitchState final : public IAudioSwitchState
{
	explicit SAudioSwitchState(
	  EWwiseSwitchType const _type,
	  AkUInt32 const _switchId,
	  AkUInt32 const _stateId,
	  float const rtpcValue = 0.0f)
		: type(_type)
		, switchId(_switchId)
		, stateId(_stateId)
		, rtpcValue(rtpcValue)
	{}

	virtual ~SAudioSwitchState() override = default;

	SAudioSwitchState(SAudioSwitchState const&) = delete;
	SAudioSwitchState(SAudioSwitchState&&) = delete;
	SAudioSwitchState& operator=(SAudioSwitchState const&) = delete;
	SAudioSwitchState& operator=(SAudioSwitchState&&) = delete;

	EWwiseSwitchType const type;
	AkUInt32 const         switchId;
	AkUInt32 const         stateId;
	float const            rtpcValue;
};

enum EWwiseAudioEnvironmentType : AudioEnumFlagsType
{
	eWwiseAudioEnvironmentType_None = 0,
	eWwiseAudioEnvironmentType_AuxBus,
	eWwiseAudioEnvironmentType_Rtpc,
};

struct SAudioEnvironment final : public IAudioEnvironment
{
	explicit SAudioEnvironment(EWwiseAudioEnvironmentType const _type)
		: type(_type)
	{}

	explicit SAudioEnvironment(EWwiseAudioEnvironmentType const _type, AkAuxBusID const _busId)
		: type(_type)
		, busId(_busId)
	{
		CRY_ASSERT(_type == eWwiseAudioEnvironmentType_AuxBus);
	}

	explicit SAudioEnvironment(
	  EWwiseAudioEnvironmentType const _type,
	  AkRtpcID const _rtpcId,
	  float const _multiplier,
	  float const _shift)
		: type(_type)
		, rtpcId(_rtpcId)
		, multiplier(_multiplier)
		, shift(_shift)
	{
		CRY_ASSERT(_type == eWwiseAudioEnvironmentType_Rtpc);
	}

	virtual ~SAudioEnvironment() override = default;

	SAudioEnvironment(SAudioEnvironment const&) = delete;
	SAudioEnvironment(SAudioEnvironment&&) = delete;
	SAudioEnvironment& operator=(SAudioEnvironment const&) = delete;
	SAudioEnvironment& operator=(SAudioEnvironment&&) = delete;

	EWwiseAudioEnvironmentType const type;

	union
	{
		// Aux Bus implementation
		struct
		{
			AkAuxBusID busId;
		};

		// Rtpc implementation
		struct
		{
			AkRtpcID rtpcId;
			float    multiplier;
			float    shift;
		};
	};
};

struct SAudioEvent final : public IAudioEvent
{
	explicit SAudioEvent(AudioEventId const _id)
		: audioEventState(eAudioEventState_None)
		, id(AK_INVALID_UNIQUE_ID)
		, audioEventId(_id)
	{}

	virtual ~SAudioEvent() override = default;

	SAudioEvent(SAudioEvent const&) = delete;
	SAudioEvent(SAudioEvent&&) = delete;
	SAudioEvent& operator=(SAudioEvent const&) = delete;
	SAudioEvent& operator=(SAudioEvent&&) = delete;

	EAudioEventState   audioEventState;
	AkUniqueID         id;
	AudioEventId const audioEventId;
};

struct SAudioFileEntry final : public IAudioFileEntry
{
	SAudioFileEntry() = default;
	virtual ~SAudioFileEntry() override = default;

	SAudioFileEntry(SAudioFileEntry const&) = delete;
	SAudioFileEntry(SAudioFileEntry&&) = delete;
	SAudioFileEntry& operator=(SAudioFileEntry const&) = delete;
	SAudioFileEntry& operator=(SAudioFileEntry&&) = delete;

	AkBankID bankId = AK_INVALID_BANK_ID;
};

struct SAudioStandaloneFile final : public IAudioStandaloneFile
{
	SAudioStandaloneFile() = default;
	virtual ~SAudioStandaloneFile() override = default;

	SAudioStandaloneFile(SAudioStandaloneFile const&) = delete;
	SAudioStandaloneFile(SAudioStandaloneFile&&) = delete;
	SAudioStandaloneFile& operator=(SAudioStandaloneFile const&) = delete;
	SAudioStandaloneFile& operator=(SAudioStandaloneFile&&) = delete;
};
}
}
}
