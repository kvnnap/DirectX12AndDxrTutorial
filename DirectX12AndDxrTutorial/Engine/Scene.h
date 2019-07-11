#pragma once

#include <string>
#include <vector>

#include <DirectXMath.h>
#include "Material.h"

#include "../Shaders/RTShaders.hlsli"

namespace Engine {
	class Scene {
	public:
		Scene() = default;
		virtual ~Scene() = default;

		void loadScene(const std::string& pathToObj);

		const std::vector<std::vector<DirectX::XMFLOAT3>>& getVertices() const;
		const std::vector<DirectX::XMFLOAT3>& getVertices(size_t materialId) const;
		const std::vector<Shaders::FaceAttributes>& getFaceAttributes() const;
		const std::vector<Material>& getMaterials() const;

	private:

		std::vector<std::vector<DirectX::XMFLOAT3>> vertices;

		std::vector<Shaders::FaceAttributes> faceAttributes;

		std::vector<Material> materials;
	};
}