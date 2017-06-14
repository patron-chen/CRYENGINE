// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NodeGraphClipboard.h"

#include "GraphViewModel.h"
#include "GraphNodeItem.h"

#include <Schematyc/SerializationUtils/SerializationUtils.h>

#pragma optimize("", off)

namespace CrySchematycEditor {

CNodeGraphClipboard::CNodeGraphClipboard(CryGraphEditor::CNodeGraphViewModel& model)
	: CClipboardItemCollection(model)
{}

void CNodeGraphClipboard::SaveNodeDataToXml(CryGraphEditor::CAbstractNodeItem& node, Serialization::IArchive& archive)
{
	CNodeItem& nodeItem = static_cast<CNodeItem&>(node);
	const Schematyc::IScriptGraphNode& scriptGraphNode = nodeItem.GetScriptElement();

	Schematyc::CStackString typeGuidString;
	Schematyc::GUID::ToString(typeGuidString, scriptGraphNode.GetTypeGUID());
	archive(typeGuidString, "typeGUID");
	archive(CopySerialize(scriptGraphNode), "dataBlob");
}

CryGraphEditor::CAbstractNodeItem* CNodeGraphClipboard::RestoreNodeFromXml(Serialization::IArchive& archive)
{
	CNodeGraphViewModel* pModel = static_cast<CNodeGraphViewModel*>(GetModel());
	if (pModel)
	{
		Schematyc::CStackString typeGuidString;
		archive(typeGuidString, "typeGUID");

		const Schematyc::SGUID typeGuid = Schematyc::GUID::FromString(typeGuidString.c_str());
		CNodeItem* pNodeItem = pModel->CreateNode(typeGuid);
		if (pNodeItem)
		{
			Schematyc::IScriptGraphNode& scriptGraphNode = pNodeItem->GetScriptElement();
			archive(PasteSerialize(scriptGraphNode), "dataBlob");
			// TODO: We should not have to do this here!
			scriptGraphNode.ProcessEvent(Schematyc::SScriptEvent(Schematyc::EScriptEventId::EditorPaste));
			// ~TODO

			pNodeItem->Refresh(true);
			return pNodeItem;
		}
	}
	return nullptr;
}

}
