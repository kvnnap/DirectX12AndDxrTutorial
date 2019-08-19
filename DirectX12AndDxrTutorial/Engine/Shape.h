#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>

namespace Engine {

	class Shape
	{
	public:
		
		Shape(const std::string& name, std::vector<DirectX::XMFLOAT3>&& vertices);

		void setPosition(float x, float y, float z);
		void setRotation(float x, float y, float z);
		void setScale(float x, float y, float z);

		const std::string& getName() const;
		const std::vector<DirectX::XMFLOAT3>& getVertices() const;
		DirectX::XMFLOAT3X4 getTransform() const;

	private:
		std::string name;
		std::vector<DirectX::XMFLOAT3> vertices;

		// Belos is the world position
		DirectX::XMFLOAT3 worldPosition;
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 rotation;
		DirectX::XMFLOAT3 scale;
	};

}