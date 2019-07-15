#include "Window.h"

#include <algorithm>

#include "Exception/Exception.h"

using namespace std;
using namespace UI;

// Static init
WindowClass Window::wndClass = WindowClass("DX12TutorialWindowClass", Window::WndProcSetup);

LRESULT CALLBACK Window::WndProcSetup(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
	if (msg == WM_NCCREATE) {
		CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
		Window* window = reinterpret_cast<Window*>(createStruct->lpCreateParams);
		
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Window::WndProcThunk));

		return window->wndProc(hwnd, msg, wParam, lParam);
	}
	else {
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

LRESULT CALLBACK Window::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
	Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	return window->wndProc(hwnd, msg, wParam, lParam);
}

std::optional<int> Window::ProcessMessages()
{
	MSG msg = {};
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return static_cast<int>(msg.wParam);

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return nullopt;
}

LRESULT Window::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	bool handled = std::any_of(callbacks.begin(), callbacks.end(), [hwnd, msg, wParam, lParam](const WNDCALLBACKFN& fn) -> bool {
		return fn(hwnd, msg, wParam, lParam) != 0;
	});

	if (handled) {
		return handled;
	}

	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (keyboardWriter && !(lParam & (1 << 30))) {
			keyboardWriter->pressKey(static_cast<uint8_t>(wParam));
		}
		break;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		if (keyboardWriter) {
			keyboardWriter->depressKey(static_cast<uint8_t>(wParam));
		}
		break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Window::cleanup()
{
	if (hWnd) {
		DestroyWindow(hWnd);
		hWnd = HWND();
	}
}

Window::Window(const string& windowName, int width, int height, IO::IKeyboardWriter* keyboardWriter)
	: hWnd(), keyboardWriter ( keyboardWriter )
{
	auto dwClass = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;

	RECT rect = {};
	rect.right = width;
	rect.bottom = height;
	if (!AdjustWindowRect(&rect, dwClass, false)) {
		ThrowException("Cannot adjust client area");
	}
	
	HINSTANCE hInstance = GetModuleHandle(NULL);

	hWnd = CreateWindowEx(NULL,
		wndClass.getClassName().c_str(),
		windowName.c_str(),
		dwClass,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
		NULL,
		NULL,
		hInstance,
		this);

	if (!hWnd)
	{
		cleanup();
		ThrowException("Error creating window");
	}

	ShowWindow(hWnd, SW_SHOW);
	
}

Window::~Window()
{
	cleanup();
}

HWND Window::getHandle() const
{
	return hWnd;
}

void UI::Window::setWindowName(const std::string& windowName) const
{
	SetWindowText(hWnd, windowName.c_str());
}

void Window::addWndProcCallback(WNDCALLBACKFN a)
{
	callbacks.push_back(move(a));
}
