// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

#include "Schematyc/Script/IScriptElement.h"
#include "Schematyc/Script/Elements/IScriptBase.h"
#include "Schematyc/Script/Elements/IScriptClass.h"
#include "Schematyc/Utils/GUID.h"
#include "Schematyc/Utils/IString.h"

namespace Schematyc
{
namespace ScriptUtils
{
inline const IScriptClass* GetClass(const IScriptElement& scriptElement)
{
	for (const IScriptElement* pScriptElement = &scriptElement; pScriptElement; pScriptElement = pScriptElement->GetParent())
	{
		const IScriptClass* pScriptClass = DynamicCast<IScriptClass>(pScriptElement);
		if (pScriptClass)
		{
			return pScriptClass;
		}
	}
	return nullptr;
}

inline const IScriptBase* GetBase(const IScriptClass& scriptClass)
{
	for (const IScriptElement* pScriptElement = scriptClass.GetFirstChild(); pScriptElement; pScriptElement = pScriptElement->GetNextSibling())
	{
		const IScriptBase* pScriptBase = DynamicCast<IScriptBase>(pScriptElement);
		if (pScriptBase)
		{
			return pScriptBase;
		}
	}
	return nullptr;
}

inline void QualifyName(IString& output, const IScriptElement& element, EScriptElementType scopeType = EScriptElementType::Root)
{
	output.clear();
	for (const IScriptElement* pScope = &element; pScope; pScope = pScope->GetParent())
	{
		if (pScope->GetElementType() == scopeType)
		{
			break;
		}
		else
		{
			if (!output.empty())
			{
				output.insert(0, "::");
			}
			output.insert(0, pScope->GetName());
		}
	}
}
} // ScriptUtils
} // Schematyc
