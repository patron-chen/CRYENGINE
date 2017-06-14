// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#pragma once

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/Forward.h>
#include <Schematyc/Script/IScriptElement.h>
#include <Schematyc/SerializationUtils/ISerializationContext.h>
#include <Schematyc/Utils/Delegate.h>

namespace Schematyc
{
// Forward declare structures.
struct SScriptInputElement;
// Forward declare classes.
class CScript;

typedef CDelegate<void (IScriptElement&)> ScriptElementSerializeCallback;

class CScriptInputElementSerializer
{
public:

	CScriptInputElementSerializer(IScriptElement& element, ESerializationPass serializationPass, const ScriptElementSerializeCallback& callback = ScriptElementSerializeCallback());

	void Serialize(Serialization::IArchive& archive);

private:

	IScriptElement&                m_element;
	ESerializationPass             m_serializationPass;
	ScriptElementSerializeCallback m_callback;
};

typedef std::vector<SScriptInputElement>  ScriptInputElements;
typedef std::vector<SScriptInputElement*> ScriptInputElementPtrs;

struct SScriptInputElement
{
	SScriptInputElement();

	void Serialize(Serialization::IArchive& archive);

	Serialization::SBlackBox blackBox;
	IScriptElementPtr        ptr;
	ScriptInputElements      children;
	ScriptInputElementPtrs   dependencies;
	uint32                   sortPriority;
};

struct SScriptInputBlock
{
	SGUID               guid;
	SGUID               scopeGUID;
	SScriptInputElement rootElement;
};

typedef std::vector<SScriptInputBlock> ScriptInputBlocks;

class CScriptLoadSerializer
{
public:

	CScriptLoadSerializer(SScriptInputBlock& inputBlock, const ScriptElementSerializeCallback& callback = ScriptElementSerializeCallback());

	void Serialize(Serialization::IArchive& archive);

private:

	SScriptInputBlock&             m_inputBlock;
	ScriptElementSerializeCallback m_callback;
};

class CScriptSaveSerializer
{
public:

	CScriptSaveSerializer(CScript& script, const ScriptElementSerializeCallback& callback = ScriptElementSerializeCallback());

	void Serialize(Serialization::IArchive& archive);

private:

	CScript&                       m_script;
	ScriptElementSerializeCallback m_callback;
};

class CScriptCopySerializer
{
public:

	CScriptCopySerializer(IScriptElement& root, const ScriptElementSerializeCallback& callback = ScriptElementSerializeCallback());

	void Serialize(Serialization::IArchive& archive);

private:

	IScriptElement&                m_root;
	ScriptElementSerializeCallback m_callback;
};

class CScriptPasteSerializer
{
public:

	CScriptPasteSerializer(SScriptInputBlock& inputBlock, const ScriptElementSerializeCallback& callback = ScriptElementSerializeCallback());

	void Serialize(Serialization::IArchive& archive);

private:

	SScriptInputBlock&             m_inputBlock;
	ScriptElementSerializeCallback m_callback;
};

void UnrollScriptInputElementsRecursive(ScriptInputElementPtrs& output, SScriptInputElement& element);
bool SortScriptInputElementsByDependency(ScriptInputElementPtrs& elements);
} // Schematyc
