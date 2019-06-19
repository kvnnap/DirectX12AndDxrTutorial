#pragma once

#include <stdint.h>
namespace IO {
	class IKeyboardReader {
	public:
		virtual ~IKeyboardReader() {};

		virtual bool isKeyPressed(uint8_t key) = 0;
		virtual bool hasKeyChanged(uint8_t key) = 0;
	};
}