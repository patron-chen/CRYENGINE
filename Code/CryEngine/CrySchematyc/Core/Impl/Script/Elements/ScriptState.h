// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Schematyc/Script/Elements/IScriptState.h>
#include <Schematyc/SerializationUtils/MultiPassSerializer.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc
{
class CScriptState : public CScriptElementBase<IScriptState>, public CMultiPassSerializer
{
public:

	CScriptState();
	CScriptState(const SGUID& guid, const char* szName);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

	// IScriptState
	virtual SGUID GetPartnerGUID() const override;
	// ~IScriptState

protected:

	// CMultiPassSerializer
	virtual void LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

private:

	SGUID m_partnerGUID;
};
} // Schematyc
