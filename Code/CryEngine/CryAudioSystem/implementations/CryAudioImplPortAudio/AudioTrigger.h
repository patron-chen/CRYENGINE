// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ATLEntityData.h>
#include <portaudio.h>

enum EPortAudioEventType
{
	ePortAudioEventType_None = 0,
	ePortAudioEventType_Start,
	ePortAudioEventType_Stop,
};

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
class CAudioTrigger final : public IAudioTrigger
{
public:

	explicit CAudioTrigger(
	  uint32 const _pathId,
	  int const _numLoops,
	  double const _sampleRate,
	  EPortAudioEventType const _eventType,
	  CryFixedStringT<512> const& _filePath,
	  PaStreamParameters const& _streamParameters)
		: pathId(_pathId)
		, numLoops(_numLoops)
		, sampleRate(_sampleRate)
		, eventType(_eventType)
		, filePath(_filePath)
		, streamParameters(_streamParameters)
	{}

	virtual ~CAudioTrigger() override = default;

	CAudioTrigger(CAudioTrigger const&) = delete;
	CAudioTrigger& operator=(CAudioTrigger const&) = delete;

	uint32 const               pathId;
	int const                  numLoops;
	double const               sampleRate;
	EPortAudioEventType const  eventType;
	CryFixedStringT<512> const filePath;
	PaStreamParameters const   streamParameters;
};
}
}
}
