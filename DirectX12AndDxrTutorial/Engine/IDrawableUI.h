#pragma once

namespace Engine {
	class IDrawableUI {
	public:
		virtual ~IDrawableUI() = default;

		virtual void drawUI() = 0;
		virtual bool hasChanged() const = 0;
	};
}