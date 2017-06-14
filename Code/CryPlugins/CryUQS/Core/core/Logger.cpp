// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Logger.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace uqs
{
	namespace core
	{

		//===================================================================================
		//
		// CLogger
		//
		//===================================================================================

		int CLoggerIndentation::s_indentLevel;

		CLoggerIndentation::CLoggerIndentation()
		{
			++s_indentLevel;
		}

		CLoggerIndentation::~CLoggerIndentation()
		{
			assert(s_indentLevel > 0);
			--s_indentLevel;
		}

		int CLoggerIndentation::GetCurrentIndentLevel()
		{
			return s_indentLevel;
		}

		//===================================================================================
		//
		// CLogger
		//
		//===================================================================================

		void CLogger::Printf(const char* fmt, ...)
		{
			va_list args;
			char text[1024];

			va_start(args, fmt);
			cry_vsprintf(text, fmt, args);
			va_end(args);

			const int indentLevel = CLoggerIndentation::GetCurrentIndentLevel();
			CryLog("%*s%s", indentLevel * 4, "", text);
		}

	}
}
