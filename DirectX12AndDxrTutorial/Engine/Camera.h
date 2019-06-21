#pragma once

#include <DirectXMath.h>

namespace Engine {
	class Camera
	{
	public:
		Camera(const DirectX::XMVECTOR& position, const DirectX::XMVECTOR& direction, float nearWidth, float nearHeight, float nearZ, float farZ);
		virtual ~Camera() = default;

		// Methods
		void lookTo(const DirectX::XMVECTOR& direction, const DirectX::XMVECTOR& up = DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f));
		void incrementPosition(const DirectX::XMVECTOR& deltaPosition);
		void incrementPosition(float dx, float dy, float dz);
		void incrementPositionAlongDirection(float dx, float dy);

		void incrementDirection(float rotationY, float rotationZ);

		const DirectX::XMVECTOR& getPosition() const;
		const DirectX::XMVECTOR& getDirection() const;
		void setPosition(const DirectX::XMVECTOR& position);
		void setDirection(const DirectX::XMVECTOR& direction);

		const DirectX::XMMATRIX& getViewMatrix() const;
		const DirectX::XMMATRIX& getPerspectiveMatrix() const;
		DirectX::XMMATRIX getViewPerspectiveMatrix() const;

	private:
		// methods
		void recalculateViewMatrix();
		DirectX::XMVECTOR getCrossVector() const;

		// Camera vectors
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR up;

		float nearWidth;
		float nearHeight;
		float nearZ;
		float farZ;

		// Matrices
		DirectX::XMMATRIX viewMatrix;
		DirectX::XMMATRIX perspectiveMatrix;
	};
}


