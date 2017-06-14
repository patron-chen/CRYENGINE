// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Schematyc/Script/IScriptElement.h"

namespace Schematyc
{
// Forward declare structures.
struct SGUID;

enum class EScriptSignalReceiverType
{
	Unknown = 0,
	EnvSignal,
	ScriptSignal,
	ScriptTimer,
	Universal // #SchematycTODO : All signal receivers should be universal.
};

struct IScriptSignalReceiver : public IScriptElementBase<EScriptElementType::SignalReceiver>
{
	virtual ~IScriptSignalReceiver() {}

	virtual EScriptSignalReceiverType GetType() const = 0;
	virtual SGUID                     GetSignalGUID() const = 0;
};
} // Schematyc
