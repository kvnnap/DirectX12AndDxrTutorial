#pragma once

#include <string>
#include <vector>

#include <DirectXMath.h>
#include "Engine/Texture.h"

#include "../Shaders/RTShaders.hlsli"

namespace Engine {
	class Scene {
	public:
		Scene() = default;
		virtual ~Scene() = default;

		void loadScene(const std::string& pathToObj);

		// Transform virtual light sources' position
		void transformLightPosition(const DirectX::XMMATRIX& mat);

		// Flatten shapes found into obj into one big shape
		void flattenGroups();

		const std::vector<std::vector<DirectX::XMFLOAT3>>& getVertices() const;
		const std::vector<DirectX::XMFLOAT2>& getTextureVertices() const;
		const std::vector<DirectX::XMFLOAT3>& getVertices(size_t materialId) const;
		const std::vector<Shaders::FaceAttributes>& getFaceAttributes() const;
		const std::vector<Shaders::AreaLight>& getLights() const;
		const std::vector<Shaders::Material>& getMaterials() const;
		const std::vector<Engine::Texture>& getTextures() const;
	
	private:

		std::vector<std::vector<DirectX::XMFLOAT3>> vertices;
		std::vector<DirectX::XMFLOAT2> texVertices;

		std::vector<Shaders::FaceAttributes> faceAttributes;
		std::vector<Shaders::AreaLight> lights;
		
		std::vector<Engine::Texture> textures;

		std::vector<Shaders::Material> materials;
	};
}