#pragma once
#define NOMINMAX
#include <Windows.h>
#include <string>
#include <optional>
#include <vector>
#include <functional>

#include "WindowClass.h"
#include "IO/IKeyboardWriter.h"

namespace UI {
	class Window
	{
	public:
		static std::optional<int> ProcessMessages();

		Window(const std::string& windowName, int width, int height, IO::IKeyboardWriter* keyboardWriter = nullptr);
		virtual ~Window();

		HWND getHandle() const;


		using WNDCALLBACKFN = std::function<LRESULT(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)>;
		void addWndProcCallback(WNDCALLBACKFN fn);


		using WNDCALLBACKPT = LRESULT(CALLBACK*)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	private:
		static LRESULT CALLBACK WndProcSetup(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
		static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

		static WindowClass wndClass;

		LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
		void cleanup();

		std::vector<WNDCALLBACKFN> callbacks;

		HWND hWnd;

		IO::IKeyboardWriter* keyboardWriter;
	};
}
