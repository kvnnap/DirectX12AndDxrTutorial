#pragma once

#define NOMINMAX
#include <Windows.h>

#include <string>

class WindowClass
{
public:
	WindowClass(const std::string& className, WNDPROC wndProc);
	~WindowClass();

	const std::string& getClassName() const;

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	std::string className;
};

