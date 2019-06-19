#pragma once
#include <exception>
#include <string>

namespace Exception {

	class Exception :
		public std::exception
	{
	public:
		Exception(const std::string& fileName, int lineNumber, const std::string& functionName, const std::string& reason = "");
		virtual ~Exception();

		const char* what() const override;

	protected:
		std::string whatBuffer;
	};

}

#define ThrowException(reason) (throw Exception::Exception(__FILE__, __LINE__, __func__, reason))