// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySystem/ICryPlugin.h>

#include "Schematyc/FundamentalTypes.h"
#include "Schematyc/Utils/Delegate.h"

namespace Schematyc
{
// Forward declare interfaces.
struct ICompiler;
struct IScriptView;
struct IEnvRegistrar;
struct IEnvRegistry;
struct ILog;
struct ILogRecorder;
struct IObject;
struct IRuntimeRegistry;
struct IScriptRegistry;
struct ISerializationContext;
struct ISettingsManager;
struct ITimerSystem;
struct IUpdateScheduler;
struct IValidatorArchive;
// Forward declare structures.
struct SGUID;
struct SObjectParams;
struct SSerializationContextParams;
struct SValidatorArchiveParams;
// Forward declare classes.
class CRuntimeParams;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(IScriptView)
DECLARE_SHARED_POINTERS(ISerializationContext)
DECLARE_SHARED_POINTERS(IValidatorArchive)

typedef CDelegate<SGUID ()> GUIDGenerator;
} // Schematyc

struct ICrySchematycCore : public ICryPlugin
{
	CRYINTERFACE_DECLARE(ICrySchematycCore, 0x041b8bda35d74341, 0xbde7f0ca69be2595)

	virtual void                                SetGUIDGenerator(const Schematyc::GUIDGenerator& guidGenerator) = 0;
	virtual Schematyc::SGUID                    CreateGUID() const = 0;

	virtual const char*                         GetRootFolder() const = 0;
	virtual const char*                         GetScriptsFolder() const = 0;     // #SchematycTODO : Do we really need access to this outside script registry?
	virtual const char*                         GetSettingsFolder() const = 0;    // #SchematycTODO : Do we really need access to this outside env registry?
	virtual bool                                IsExperimentalFeatureEnabled(const char* szFeatureName) const = 0;

	virtual Schematyc::IEnvRegistry&            GetEnvRegistry() = 0;
	virtual Schematyc::IScriptRegistry&         GetScriptRegistry() = 0;
	virtual Schematyc::IRuntimeRegistry&        GetRuntimeRegistry() = 0;
	virtual Schematyc::ICompiler&               GetCompiler() = 0;
	virtual Schematyc::ILog&                    GetLog() = 0;
	virtual Schematyc::ILogRecorder&            GetLogRecorder() = 0;
	virtual Schematyc::ISettingsManager&        GetSettingsManager() = 0;
	virtual Schematyc::IUpdateScheduler&        GetUpdateScheduler() = 0;
	virtual Schematyc::ITimerSystem&            GetTimerSystem() = 0;

	virtual Schematyc::IValidatorArchivePtr     CreateValidatorArchive(const Schematyc::SValidatorArchiveParams& params) const = 0;
	virtual Schematyc::ISerializationContextPtr CreateSerializationContext(const Schematyc::SSerializationContextParams& params) const = 0;
	virtual Schematyc::IScriptViewPtr           CreateScriptView(const Schematyc::SGUID& scopeGUID) const = 0;

	virtual Schematyc::IObject*                 CreateObject(const Schematyc::SObjectParams& params) = 0;
	virtual Schematyc::IObject*                 GetObject(Schematyc::ObjectId objectId) = 0;
	virtual void                                DestroyObject(Schematyc::ObjectId objectId) = 0;
	virtual void                                SendSignal(Schematyc::ObjectId objectId, const Schematyc::SGUID& signalGUID, Schematyc::CRuntimeParams& params) = 0;
	virtual void                                BroadcastSignal(const Schematyc::SGUID& signalGUID, Schematyc::CRuntimeParams& params) = 0;

	virtual void                                RefreshLogFileSettings() = 0;
	virtual void                                RefreshEnv() = 0;
};

