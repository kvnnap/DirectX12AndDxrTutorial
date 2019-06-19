#include "Window/Window.h"
#include "IO/Keyboard.h"
#include "Engine/Graphics.h"

#include <memory>

#pragma once

class App
{
public:
	App();
	virtual ~App() = default;

	int execute() noexcept;

private:
	int localExecute();
	
	void processFrame();
	float getValueIfPressed(char keyPressed, float deltaUnits) const;

	uint64_t frameCounter;
	long long msec;

	std::unique_ptr<UI::Window> window;
	std::unique_ptr<IO::Keyboard> keyboard;
	std::unique_ptr<Engine::Graphics> graphics;
};

