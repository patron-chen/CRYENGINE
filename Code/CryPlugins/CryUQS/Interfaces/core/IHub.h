// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace core
	{

		//===================================================================================
		//
		// EHubEvent
		//
		//===================================================================================

		enum class EHubEvent
		{
			RegisterYourFactoriesNow,
			LoadQueryBlueprintLibrary
		};

		//===================================================================================
		//
		// IHubEventListener
		//
		//===================================================================================

		struct IHubEventListener
		{
			virtual           ~IHubEventListener() {}
			virtual void      OnUQSHubEvent(EHubEvent ev) = 0;
		};

		//===================================================================================
		//
		// IHub
		//
		//===================================================================================

		struct IHub
		{
			virtual                                                    ~IHub() {}
			virtual void                                               RegisterHubEventListener(IHubEventListener* listener) = 0;
			virtual void                                               Update() = 0;
			virtual IQueryFactoryDatabase&                             GetQueryFactoryDatabase() = 0;
			virtual IItemFactoryDatabase&                              GetItemFactoryDatabase() = 0;
			virtual IFunctionFactoryDatabase&                          GetFunctionFactoryDatabase() = 0;
			virtual IGeneratorFactoryDatabase&                         GetGeneratorFactoryDatabase() = 0;
			virtual IInstantEvaluatorFactoryDatabase&                  GetInstantEvaluatorFactoryDatabase() = 0;
			virtual IDeferredEvaluatorFactoryDatabase&                 GetDeferredEvaluatorFactoryDatabase() = 0;
			virtual IQueryBlueprintLibrary&                            GetQueryBlueprintLibrary() = 0;
			virtual IQueryManager&                                     GetQueryManager() = 0;
			virtual IQueryHistoryManager&                              GetQueryHistoryManager() = 0;
			virtual IUtils&                                            GetUtils() = 0;
			virtual IEditorService&                                    GetEditorService() = 0;
			virtual IItemSerializationSupport&                         GetItemSerializationSupport() = 0;

			// TODO pavloi 2016.04.07: maybe editor library provider doesn't really belong here.
			// Instead, we should use other means to pass pointer from game to editor (like, register
			// provider as game extension CRYINTERFACE and query it by GUID - same as Hub).
			virtual datasource::IEditorLibraryProvider*                GetEditorLibraryProvider() = 0;
			virtual void                                               SetEditorLibraryProvider(datasource::IEditorLibraryProvider* pProvider) = 0;
		};

	}
}
