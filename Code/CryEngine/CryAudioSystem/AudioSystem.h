// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATL.h"
#include <CrySystem/TimeValue.h>
#include <CryThreading/IThreadManager.h>

// Forward declarations.
class CAudioSystem;
class CAudioProxy;

class CAudioThread final : public IThread
{
public:

	CAudioThread() = default;
	CAudioThread(CAudioThread const&) = delete;
	CAudioThread(CAudioThread&&) = delete;
	CAudioThread& operator=(CAudioThread const&) = delete;
	CAudioThread& operator=(CAudioThread&&) = delete;

	void          Init(CAudioSystem* const pAudioSystem);

	// IThread
	virtual void ThreadEntry();
	// ~IThread

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork();

	bool IsActive();
	void Activate();
	void Deactivate();

private:

	CAudioSystem* m_pAudioSystem = nullptr;
	volatile bool m_bQuit = false;
};

class CAudioSystem final : public IAudioSystem, ISystemEventListener
{
public:

	CAudioSystem();
	virtual ~CAudioSystem() override;

	CAudioSystem(CAudioSystem const&) = delete;
	CAudioSystem(CAudioSystem&&) = delete;
	CAudioSystem& operator=(CAudioSystem const&) = delete;
	CAudioSystem& operator=(CAudioSystem&&) = delete;

	// IAudioSystem
	virtual bool          Initialize() override;
	virtual void          Release() override;
	virtual void          PushRequest(SAudioRequest const& audioRequest) override;
	virtual void          AddRequestListener(void (* func)(SAudioRequestInfo const* const), void* const pObjectToListenTo, EAudioRequestType const requestType = eAudioRequestType_AudioAllRequests, AudioEnumFlagsType const specificRequestMask = ALL_AUDIO_REQUEST_SPECIFIC_TYPE_FLAGS) override;
	virtual void          RemoveRequestListener(void (* func)(SAudioRequestInfo const* const), void* const pObjectToListenTo) override;

	virtual void          ExternalUpdate() override;

	virtual bool          GetAudioTriggerId(char const* const szAudioTriggerName, AudioControlId& audioTriggerId) const override;
	virtual bool          GetAudioRtpcId(char const* const szAudioRtpcName, AudioControlId& audioRtpcId) const override;
	virtual bool          GetAudioSwitchId(char const* const szAudioSwitchName, AudioControlId& audioSwitchId) const override;
	virtual bool          GetAudioSwitchStateId(AudioControlId const audioSwitchId, char const* const szSwitchStateName, AudioSwitchStateId& audioSwitchStateId) const override;
	virtual bool          GetAudioPreloadRequestId(char const* const szAudioPreloadRequestName, AudioPreloadRequestId& audioPreloadRequestId) const override;
	virtual bool          GetAudioEnvironmentId(char const* const szAudioEnvironmentName, AudioEnvironmentId& audioEnvironmentId) const override;

	virtual CATLListener* CreateAudioListener() override;
	virtual void          ReleaseAudioListener(CATLListener* pListener) override;

	virtual void          OnCVarChanged(ICVar* const pCvar)  override {}
	virtual char const*   GetConfigPath() const override;

	virtual IAudioProxy*  GetFreeAudioProxy() override;
	virtual void          GetAudioFileData(char const* const szFilename, SAudioFileData& audioFileData) override;
	virtual void          GetAudioTriggerData(AudioControlId const audioTriggerId, SAudioTriggerData& audioTriggerData) override;
	virtual void          SetAllowedThreadId(threadID id) override { m_allowedThreadId = id; }
	// ~IAudioSystem

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	void InternalUpdate();
	void FreeAudioProxy(IAudioProxy* const pIAudioProxy);

private:

	typedef std::deque<CAudioRequestInternal, STLSoundAllocator<CAudioRequestInternal>> TAudioRequests;
	typedef std::vector<CAudioProxy*, STLSoundAllocator<CAudioProxy*>>                  TAudioProxies;

	void        PushRequestInternal(CAudioRequestInternal const& request);
	void        UpdateTime();
	bool        ProcessRequests(TAudioRequests& requestQueue);
	void        ProcessRequest(CAudioRequestInternal& request);
	bool        ExecuteSyncCallbacks(TAudioRequests& requestQueue);
	void        ExtractSyncCallbacks(TAudioRequests& requestQueue, TAudioRequests& syncCallbacksQueue);
	uint32      MoveAudioRequests(TAudioRequests& from, TAudioRequests& to);

	static void OnAudioEvent(SAudioRequestInfo const* const pAudioRequestInfo);

	bool         m_bSystemInitialized;
	CTimeValue   m_lastUpdateTime;
	float        m_deltaTime;
	CAudioThread m_mainAudioThread;
	threadID     m_allowedThreadId;

	enum EAudioRequestQueueType : AudioEnumFlagsType
	{
		eAudioRequestQueueType_Asynch = 0,
		eAudioRequestQueueType_Synch  = 1,

		eAudioRequestQueueType_Count
	};

	enum EAudioRequestQueuePriority : AudioEnumFlagsType
	{
		eAudioRequestQueuePriority_High   = 0,
		eAudioRequestQueuePriority_Normal = 1,
		eAudioRequestQueuePriority_Low    = 2,

		eAudioRequestQueuePriority_Count
	};

	enum EAudioRequestQueueIndex : AudioEnumFlagsType
	{
		eAudioRequestQueueIndex_One = 0,
		eAudioRequestQueueIndex_Two = 1,

		eAudioRequestQueueIndex_Count
	};

	TAudioRequests                              m_requestQueues[eAudioRequestQueueType_Count][eAudioRequestQueuePriority_Count][eAudioRequestQueueIndex_Count];
	TAudioRequests                              m_syncCallbacks;
	TAudioRequests                              m_syncCallbacksPending;
	TAudioRequests                              m_internalRequests[eAudioRequestQueueIndex_Count];

	CAudioTranslationLayer                      m_atl;

	CryEvent                                    m_mainEvent;
	CryCriticalSection                          m_mainCS;
	CryCriticalSection                          m_syncCallbacksPendingCS;
	CryCriticalSection                          m_internalRequestsCS[eAudioRequestQueueIndex_Count];

	TAudioProxies                               m_audioProxies;
	TAudioProxies                               m_audioProxiesToBeFreed;

	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> m_configPath;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	void DrawAudioDebugData();
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};
