// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Schematyc/Script/IScriptElement.h"

namespace Schematyc
{
struct IScriptInterfaceTask : public IScriptElementBase<EScriptElementType::InterfaceTask>
{
	virtual ~IScriptInterfaceTask() {}

	virtual const char* GetAuthor() const = 0;
	virtual const char* GetDescription() const = 0;
};
} // Schematyc
