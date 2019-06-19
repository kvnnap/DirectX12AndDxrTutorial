#pragma once

#include <stdint.h>
namespace IO {
	class IKeyboardWriter {
	public:
		virtual ~IKeyboardWriter() {};

		virtual void pressKey(uint8_t key) = 0;
		virtual void depressKey(uint8_t key) = 0;
	};
}