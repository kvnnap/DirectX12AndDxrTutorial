#include "App.h"
#include "Exception/Exception.h"
#include "Engine/Graphics.h"
#include "Engine/RTGraphics.h"
#include "scene/scene.h"
#include "scene/wavefront_loader.h"
#include "core/renderer/opengl_renderer.h"

#define NOMINMAX
#include <Windows.h>
#include <string>
#include <iostream>
#include <sstream>
#include <cmath>
#include <chrono>

using namespace std;
using namespace UI;
using feanor::io::Keyboard;
using feanor::io::Mouse;
using feanor::anvil::Anvil;
using feanor::anvil::scene::Scene;
using feanor::anvil::scene::WavefrontLoader;
using feanor::anvil::renderer::OpenGLRenderer;
using feanor::anvil::visualisation::BasicVisualiser;

App::App() : frameCounter(), fpsFrameCounter(), msec(), fpsMSec(), anvil(Anvil::getInstance())
{}

App::~App()
{
	anvil.clear();
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int App::execute() noexcept
{
	string err;

	try
	{
		//scene.loadScene("sibenik.obj");
		//scene.loadScene("SunTempleModel_v2.obj");
		//scene.loadScene("tarxien_temple.obj");
		string sceneFileName = "CornellBox-Original.obj";
		keyboard = make_unique<Keyboard>();
		mouse = make_unique<Mouse>();
		window = make_unique<Window>("DX12 & DXR Tutorial", 1080, 720, keyboard.get(), mouse.get());
		renderer = make_unique<Engine::RTGraphics>(window->getHandle(), mouse.get());
		renderer->setDebugMode(true);
		renderer->init(sceneFileName);

		//Anvil
		auto scene = make_shared<Scene>();
		WavefrontLoader().loadScene(*scene, sceneFileName);
		auto glRenderer = make_shared<OpenGLRenderer>();
		glRenderer->setScene(scene);
		glRenderer->render();

		auto basicVisualiser = make_shared<BasicVisualiser>(glRenderer, scene);
		anvil.addSystem(basicVisualiser);

		window->addWndProcCallback(ImGui_ImplWin32_WndProcHandler);

		return localExecute();
	}
	catch (Exception::Exception e) {
		err = "App Exception: \n";
		err += e.what();
	}
	catch (std::exception e) {
		err = "Standard Exception: \n";
		err += e.what();
	}
	catch (...) {
		err = "Unknown Exception\n";
	}

	cout << err << endl;
	return EXIT_FAILURE;
}

int App::localExecute()
{
	while (true)
	{
		if (auto exitCode = Window::ProcessMessages()) {
			return *exitCode;
		}

		processFrame();
	}
}

void App::processFrame()
{
	using namespace std::chrono;
	milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
	const auto msLong = ms.count();
	long long deltaMs = msec == 0 ? 0 : msLong - msec;
	msec = msLong;

	// fps stuff
	fpsMSec = fpsMSec == 0 ? msLong : fpsMSec;
	// end fps stuff

	renderer->clearBuffer(
		keyboard->isKeyPressed('R') ? 1.f : 0.f,
		keyboard->isKeyPressed('G') ? 1.f : 0.f,
		keyboard->isKeyPressed('B') ? 1.f : 0.f);
	
	Engine::Camera& camera = renderer->getCamera();
	const float unitsPerSec = 3.f;
	float deltaUnits = deltaMs / 1000.f * unitsPerSec;

	camera.incrementPositionAlongDirection(getValueIfPressed('D', -deltaUnits), getValueIfPressed('W', deltaUnits));
	camera.incrementPositionAlongDirection(getValueIfPressed('A', deltaUnits), getValueIfPressed('S', -deltaUnits));

	deltaUnits = deltaMs / 1000.f;
	camera.incrementDirection(getValueIfPressed('L', -deltaUnits), getValueIfPressed('I', -deltaUnits));
	camera.incrementDirection(getValueIfPressed('J', deltaUnits), getValueIfPressed('K', deltaUnits));
	
	bool clear = keyboard->anyKeyPressed();
	renderer->draw(msLong, clear);

	renderer->endFrame();

	//// run game code
	
	if (clear) {
		frameCounter = 0;
	}
	
	++frameCounter;
	++fpsFrameCounter;

	if (msLong - fpsMSec >= 1000) {
		float fps = (float)fpsFrameCounter / (msLong - fpsMSec) * 1000.f;
		fpsFrameCounter = fpsMSec = 0;
		ostringstream oss;
		oss << "DX12 & DXR Tutorial - Total frames: " << frameCounter <<  " FPS: " << fps << endl;
		window->setWindowName(oss.str());
	}
}

float App::getValueIfPressed(char keyPressed, float value) const
{
	return keyboard->isKeyPressed(keyPressed) ? value : 0.f;
}
