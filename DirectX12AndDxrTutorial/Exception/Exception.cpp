#include "Exception.h"
#include <sstream>

using namespace std;
//using namespace Exception;

Exception::Exception::Exception(const string& fileName, int lineNumber, const string& functionName, const string& reason)
{
	ostringstream ss;

	if (!reason.empty()) {
		ss << "Reason:\t" << reason << endl;
	}

	ss  << "FileName:\t" << fileName << endl
		<< "Line:\t" << lineNumber << endl
		<< "Fn:\t" << functionName << endl;
	
	whatBuffer = ss.str();
}

Exception::Exception::~Exception() {}

const char* Exception::Exception::what() const
{
	return whatBuffer.c_str();
}
