// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <Schematyc/FundamentalTypes.h>
#include <Schematyc/Reflection/Reflection.h>

namespace Schematyc
{
// Forward declare interfaces.
struct IScriptStruct;
// Forward declare classes.
class CAnyValue;
class CCommonTypeInfo;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(CAnyValue)

// Script structure value.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptStructValue
{
private:

	typedef std::map<string, CAnyValuePtr> FieldMap;   // #SchematycTODO : Replace map with vector to preserve order!

public:

	CScriptStructValue(const IScriptStruct* pStruct);
	CScriptStructValue(const CScriptStructValue& rhs);

	void         Serialize(Serialization::IArchive& archive);

	static SGUID ReflectSchematycType(CTypeInfo<CScriptStructValue>& typeInfo);

private:

	void Refresh();

private:

	const IScriptStruct* m_pStruct;   // #SchematycTODO : Wouldn't it be safer to reference by GUID?
	FieldMap             m_fields;
};
} // Schematyc
