#include "Window/Window.h"
#include "IO/Keyboard.h"
#include "IO/mouse.h"
#include "Engine/IRenderer.h"

#include "core/anvil.h"

#include <memory>

#pragma once

class App
{
public:
	App();
	virtual ~App();

	int execute() noexcept;

private:
	int localExecute();
	
	void processFrame();
	float getValueIfPressed(char keyPressed, float deltaUnits) const;

	uint64_t frameCounter, fpsFrameCounter;
	long long msec, fpsMSec;

	std::unique_ptr<UI::Window> window;
	std::unique_ptr<feanor::io::Keyboard> keyboard;
	std::unique_ptr<feanor::io::Mouse> mouse;
	std::unique_ptr<Engine::IRenderer> renderer;
	feanor::anvil::Anvil& anvil;
};

