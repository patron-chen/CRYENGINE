// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Schematyc/FundamentalTypes.h"
#include "Schematyc/Script/IScriptElement.h"

namespace Schematyc
{
// Forward declare classes.
class CAnyConstPtr;

struct IScriptSignal : public IScriptElementBase<EScriptElementType::Signal>
{
	virtual ~IScriptSignal() {}

	virtual const char*  GetAuthor() const = 0;
	virtual const char*  GetDescription() const = 0;
	virtual uint32       GetInputCount() const = 0;
	virtual SGUID        GetInputGUID(uint32 inputIdx) const = 0;
	virtual const char*  GetInputName(uint32 inputIdx) const = 0;
	virtual SElementId   GetInputTypeId(uint32 inputIdx) const = 0;
	virtual CAnyConstPtr GetInputData(uint32 inputIdx) const = 0;
};
} // Schematyc
