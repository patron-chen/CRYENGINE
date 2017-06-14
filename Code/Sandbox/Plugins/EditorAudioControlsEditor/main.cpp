// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>
#include <IEditor.h>
#include <IPlugin.h>
#include <IEditorClassFactory.h>
#include "AudioControlsEditorPlugin.h"

IEditor* g_pEditor;
IEditor* GetIEditor() { return g_pEditor; }

//------------------------------------------------------------------
PLUGIN_API IPlugin* CreatePluginInstance(PLUGIN_INIT_PARAM* pInitParam)
{
	g_pEditor = pInitParam->pIEditor;
	ISystem* pSystem = pInitParam->pIEditor->GetSystem();
	ModuleInitISystem(pSystem, "AudioControlsEditor");
	return new CAudioControlsEditorPlugin(g_pEditor);
}

//------------------------------------------------------------------
extern "C" IMAGE_DOS_HEADER __ImageBase;
HINSTANCE g_hInstance = (HINSTANCE)&__ImageBase;
