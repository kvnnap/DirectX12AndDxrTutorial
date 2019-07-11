#pragma once

#include <string>
#include <vector>

#include <DirectXMath.h>

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
		const std::vector<Shaders::AreaLight>& getLights() const;
		const std::vector<Shaders::Material>& getMaterials() const;

	private:

		std::vector<std::vector<DirectX::XMFLOAT3>> vertices;

		std::vector<Shaders::FaceAttributes> faceAttributes;
		std::vector<Shaders::AreaLight> lights;

		std::vector<Shaders::Material> materials;
	};
}