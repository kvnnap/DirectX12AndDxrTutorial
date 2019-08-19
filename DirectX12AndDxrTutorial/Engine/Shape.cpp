#include "Shape.h"

#include <limits>
#include <algorithm>

using namespace std;
using namespace Engine;
using namespace DirectX;

Engine::Shape::Shape(const std::string& name, std::vector<XMFLOAT3>&& vertices)
	: name(name), vertices(move(vertices)), position{}, rotation{}, scale{1.f, 1.f, 1.f}
{
	constexpr float maxFloat = std::numeric_limits<float>::max();
	constexpr float minFloat = -maxFloat;
	
	XMVECTOR min = DirectX::XMVectorSet(maxFloat, maxFloat, maxFloat, 0.f);
	XMVECTOR max = DirectX::XMVectorSet(minFloat, minFloat, minFloat, 0.f);

	// Initialise position
	for (const auto& vertex : this->vertices) {
		min = DirectX::XMVectorMin(min, DirectX::XMVectorSet(vertex.x, vertex.y, vertex.z, 0.f));
		max = DirectX::XMVectorMax(max, DirectX::XMVectorSet(vertex.x, vertex.y, vertex.z, 0.f));
	}

	XMVECTOR position = 0.5f * (min + max);
	XMVECTOR worldPosition = -position;

	XMStoreFloat3(&this->position, position);
	XMStoreFloat3(&this->worldPosition, worldPosition);
}

void Engine::Shape::setPosition(float x, float y, float z)
{
	position = { x, y, z };
}

void Engine::Shape::setRotation(float x, float y, float z)
{
	rotation = { x, y, z };
}

void Engine::Shape::setScale(float x, float y, float z)
{
	scale = { x, y, z };
}

const std::string& Engine::Shape::getName() const
{
	return name;
}

const std::vector<DirectX::XMFLOAT3>& Engine::Shape::getVertices() const
{
	return vertices;
}

DirectX::XMFLOAT3X4 Engine::Shape::getTransform() const
{
	DirectX::XMMATRIX matrix = 
		  DirectX::XMMatrixTranslation(worldPosition.x, worldPosition.y, worldPosition.z)
		* DirectX::XMMatrixScaling(scale.x, scale.y, scale.z)
		* DirectX::XMMatrixRotationX(rotation.x)
		* DirectX::XMMatrixRotationY(rotation.y)
		* DirectX::XMMatrixRotationZ(rotation.z)
		* DirectX::XMMatrixTranslation(position.x, position.y, position.z)
		;

	XMFLOAT3X4 mat;
	XMStoreFloat3x4(&mat, matrix);
	return mat;
}
