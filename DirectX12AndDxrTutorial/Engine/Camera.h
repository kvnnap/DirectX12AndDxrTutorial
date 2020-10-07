#pragma once

#include <DirectXMath.h>

#include "IDrawableUI.h"

#include "reflection/reflect.h"

namespace Engine {
	class Camera
		: public IDrawableUI
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
		const DirectX::XMVECTOR& getUp() const;
		void setPosition(const DirectX::XMVECTOR& position);
		void setDirection(const DirectX::XMVECTOR& direction);

		const DirectX::XMMATRIX& getViewMatrix() const;
		const DirectX::XMMATRIX& getPerspectiveMatrix() const;
		DirectX::XMMATRIX getViewPerspectiveMatrix() const;

		// UI
		void drawUI() override;
		bool hasChanged() const override;

		// Thin lens stuff
		void setFocalLength(float focalLength);
		void setFNumber(float fNumber);
		void setFocalPlaneDistance(float focalPlaneDistance);

		bool isThinLensEnabled() const;
		float getFocalLength() const;
		float getFNumber() const;
		float getFocalPlaneDistance() const;
		float getMagnification() const;
		float getApertureSize() const;
		float getFocusPointDistance() const;

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

		bool changed;
		bool thinLensEnabled;
		// Thin lens stuff - things that user will change..
		float focalLength; // In the camera world, this is distance of film plane to lens (nodal point on lens). It affects zoom
						   // Can also be seen as filmPlaneDistance
		float fNumber; // This is what is used to change aperture size in camera.. 
		float focalPlaneDistance; // At `focalPlaneDistance` distance, the focal plane is found (plane where all points are in focus)
								  // Can also be seen as objectPlaneDistance

		REFLECT(Camera)
	};
}


