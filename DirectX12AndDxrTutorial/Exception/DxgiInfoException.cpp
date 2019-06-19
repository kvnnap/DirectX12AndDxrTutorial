#include <sstream>
#include "DxgiInfoException.h"

using namespace std;
using namespace Engine;
using namespace Exception;

DxgiInfoException::DxgiInfoException(const std::string& fileName, int lineNumber, const std::string& functionName, const DxgiInfoManager& infoManager)
	: Exception(fileName, lineNumber, functionName)
{
	ostringstream ss;

	ss << whatBuffer;

	auto messages = infoManager.getMessages();

	for (string message : messages) {
		ss << message << endl;
	}

	whatBuffer = ss.str();
}
