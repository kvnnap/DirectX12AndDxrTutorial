#pragma once

#define NOMINMAX
#include <Windows.h>

#include "Exception.h"

namespace Exception {
	class WindowException :
		public Exception
	{
	public:
		WindowException(const std::string& fileName, int lineNumber, const std::string& functionName, HRESULT hr);
		virtual ~WindowException();
	};
}

#define ThrowWindowException(hr) (throw Exception::WindowException(__FILE__, __LINE__, __func__, hr))
#define WinThrowIfFailed(expr) if(FAILED(hr = (expr))) ThrowWindowException(hr)
#define GFXTHROWIFFAILED(expr) WinThrowIfFailed(expr)