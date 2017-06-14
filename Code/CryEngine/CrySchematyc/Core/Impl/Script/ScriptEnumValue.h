// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <Schematyc/Reflection/Reflection.h>
#include <Schematyc/FundamentalTypes.h>

namespace Schematyc
{
// Forward declare interfaces.
struct IScriptEnum;
// Forward declare classes.
class CCommonTypeInfo;

// Script enumeration value.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptEnumValue
{
public:

	CScriptEnumValue(const IScriptEnum* pEnum);
	CScriptEnumValue(const CScriptEnumValue& rhs);

	bool         Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel);
	void         ToString(IString& output) const;

	static SGUID ReflectSchematycType(CTypeInfo<CScriptEnumValue>& typeInfo);

private:

	const IScriptEnum* m_pEnum;       // #SchematycTODO : Wouldn't it be safer to reference by GUID?
	uint32             m_constantIdx; // #SchematycTODO : Wouldn't it be safer to store a string?
};

bool Serialize(Serialization::IArchive& archive, CScriptEnumValue& value, const char* szName, const char* szLabel);
} // Schematyc
