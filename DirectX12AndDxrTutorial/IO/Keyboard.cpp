#include "Keyboard.h"

using namespace IO;

Keyboard::Keyboard() {}
Keyboard::~Keyboard() {}

void IO::Keyboard::pressKey(uint8_t key)
{
	if (!cSet[key]) {
		cSet[key] = !bSet[key];
	}

	bSet.set(key);
}

void IO::Keyboard::depressKey(uint8_t key)
{
	if (!cSet[key]) {
		cSet[key] = bSet[key];
	}

	bSet.reset(key);
}

bool IO::Keyboard::isKeyPressed(uint8_t key)
{
	return bSet[key];
}

bool IO::Keyboard::hasKeyChanged(uint8_t key)
{
	bool state = cSet[key];
	cSet.reset(key);
	return state;
}

bool IO::Keyboard::anyKeyPressed()
{
	return bSet.any();
}
