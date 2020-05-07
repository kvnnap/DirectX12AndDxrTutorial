#pragma once

#include <stdint.h>
#include <bitset>
#include "IKeyboardWriter.h"
#include "IKeyboardReader.h"

namespace IO {

	class Keyboard
		: public IKeyboardWriter, public IKeyboardReader
	{
	public:
		virtual ~Keyboard() = default;

		void pressKey(uint8_t key) override;
		void depressKey(uint8_t key) override;

		bool isKeyPressed(uint8_t key) override;
		bool hasKeyChanged(uint8_t key) override;
		bool anyKeyPressed() override;
		
	private:
		// The current state of the keyboard
		std::bitset<256> bSet;

		// Track key changes - Will be reset when change state is read
		std::bitset<256> cSet;
	};

}
