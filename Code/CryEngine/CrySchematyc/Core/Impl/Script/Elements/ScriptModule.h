// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Schematyc/Script/Elements/IScriptModule.h>
#include <Schematyc/SerializationUtils/MultiPassSerializer.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc
{
class CScriptModule : public CScriptElementBase<IScriptModule>, public CMultiPassSerializer
{
public:

	CScriptModule();
	CScriptModule(const SGUID& guid, const char* szName);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement
};
} // Schematyc
