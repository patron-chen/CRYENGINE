// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityClassProperties.h"

#include <Schematyc/Types/ResourceTypes.h>

namespace Schematyc
{
SEntityClassProperties::SEntityClassProperties()
	: icon("editor/objecticons/schematyc.bmp")
	, bHideInEditor(false)
	, bTriggerAreas(true)
{}

void SEntityClassProperties::Serialize(Serialization::IArchive& archive)
{
	archive(SerializationUtils::ObjectIconPath(icon), "icon", "Icon");
	archive.doc("Icon");
	archive(bHideInEditor, "bHideInEditor", "Hide In Editor");
	archive.doc("Hide entity class in editor");
	archive(bTriggerAreas, "bTriggerAreas", "Trigger Areas");
	archive.doc("Entity can enter and trigger areas");
}
} // Schematyc
