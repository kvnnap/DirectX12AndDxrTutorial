#pragma once

#include "Exception.h"
#include "Engine/DxgiInfoManager.h"

namespace Exception {
	class DxgiInfoException :
		public Exception
	{
	public:
		DxgiInfoException(const std::string& fileName, int lineNumber, const std::string& functionName, const Engine::DxgiInfoManager& infoManager);
		virtual ~DxgiInfoException() = default;
	};
}

#define ThrowDxgiInfoExceptionIfFailed(expr) { infoManager.set(); expr; if (infoManager.hasMessages()) { throw Exception::DxgiInfoException(__FILE__, __LINE__, __func__, infoManager); } }