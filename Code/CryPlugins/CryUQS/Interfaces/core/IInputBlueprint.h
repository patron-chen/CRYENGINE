// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace core
	{

		//===================================================================================
		//
		// ITextualInputBlueprint
		//
		// - used by a loader to build a preliminary input hierarchy for a blueprint from an abstract data source (e. g. XML, JSON, UI)
		// - that input hierarchy cannot be used directly, it needs to get validated and resolved into a InputBlueprint
		//
		//===================================================================================

		struct ITextualInputBlueprint
		{
			virtual                                       ~ITextualInputBlueprint() {}

			virtual const char*                           GetParamName() const = 0;
			virtual const char*                           GetFuncName() const = 0;
			virtual const char*                           GetFuncReturnValueLiteral() const = 0;
			virtual const char*                           GetAddReturnValueToDebugRenderWorldUponExecution() const = 0;

			virtual void                                  SetParamName(const char* szParamName) = 0;
			virtual void                                  SetFuncName(const char* szFuncName) = 0;
			virtual void                                  SetFuncReturnValueLiteral(const char* szValue) = 0;
			virtual void                                  SetAddReturnValueToDebugRenderWorldUponExecution(const char* szAddReturnValueToDebugRenderWorldUponExecution) = 0;

			virtual ITextualInputBlueprint&               AddChild(const char* paramName, const char* funcName, const char* funcReturnValueLiteral, const char* addReturnValueToDebugRenderWorldUponExecution) = 0;
			virtual size_t                                GetChildCount() const = 0;
			virtual const ITextualInputBlueprint&         GetChild(size_t index) const = 0;
			virtual const ITextualInputBlueprint*         FindChildByParamName(const char* paramName) const = 0;

			virtual void                                  SetSyntaxErrorCollector(datasource::SyntaxErrorCollectorUniquePtr ptr) = 0;
			virtual datasource::ISyntaxErrorCollector*    GetSyntaxErrorCollector() const = 0;     // called while resolving a blueprint from its textual representation into the "in-memory" representation
		};

	}
}
