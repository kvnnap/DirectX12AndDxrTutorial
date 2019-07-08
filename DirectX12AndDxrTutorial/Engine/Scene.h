#pragma once

#include <string>
#include <vector>

#include <DirectXMath.h>

namespace Engine {
	class Scene {
	public:
		Scene() = default;
		virtual ~Scene() = default;

		std::vector<DirectX::XMFLOAT3> loadScene(const std::string& pathToObj);
	};
}