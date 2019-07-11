#include "Scene.h"

#include "Exception/Exception.h"

#include "Libraries/tinyobjloader/tiny_obj_loader.h"

using namespace std;
using namespace Engine;

void Engine::Scene::loadScene(const string& pathToObj)
{
	using namespace  tinyobj;

	vertices.clear();
	materials.clear();

	attrib_t attr = {};
	vector<shape_t> shapes;
	vector<material_t> materials;
	string warn, err;
	LoadObj(&attr, &shapes, &materials, &warn, &err, pathToObj.c_str());

	for (const auto& material : materials) {
		this->materials.push_back(Shaders::Material{
			DirectX::XMFLOAT4(material.diffuse[0], material.diffuse[1], material.diffuse[2], 1.f),
			DirectX::XMFLOAT4(material.emission[0],material.emission[1],material.emission[2], 0.f),
		});
	}

	vertices.resize(shapes.size());
	size_t shapeNum = 0;

	// for each shape
	for (const auto& shape : shapes) {
		size_t index = 0;
		size_t faceNum = 0;
		// for each face
		for (const auto& vertexCountForFace : shape.mesh.num_face_vertices) {
			if (vertexCountForFace != 3) {
				ThrowException("Not loading a triangle");
			}
			
			const int materialId = shape.mesh.material_ids[faceNum];

			const auto& em = this->materials[materialId].emission;
			bool isEmissive = !(em.x == em.y && em.y == em.z && em.z == em.w && em.w == 0.f);
			Shaders::AreaLight areaLight = {};

			// for each vertex in face
			for (size_t v = 0; v < vertexCountForFace; ++v) {
				int vertexIndex = shape.mesh.indices[index + v].vertex_index;
				size_t vertexLocation = 3 * static_cast<size_t>(vertexIndex);

				DirectX::XMFLOAT3 vertex = DirectX::XMFLOAT3(
					attr.vertices[vertexLocation],
					attr.vertices[vertexLocation + 1],
					attr.vertices[vertexLocation + 2]);

				vertices[shapeNum].push_back(vertex);

				if (isEmissive) {
					areaLight.a[v] = DirectX::XMFLOAT4(vertex.x, vertex.y, vertex.z, 1.f);
				}
			}

			if (isEmissive) {
				areaLight.materialId = materialId;
				areaLight.intensity = 1.f;
				lights.push_back(areaLight);
			}

			index += vertexCountForFace;
			++faceNum;
		}

		// for each face
		for (const auto& materialId : shape.mesh.material_ids) {
			faceAttributes.push_back({ static_cast<std::uint32_t>(materialId) });
		}

		++shapeNum;
	}
}

const std::vector<std::vector<DirectX::XMFLOAT3>>& Engine::Scene::getVertices() const
{
	return vertices;
}

const std::vector<DirectX::XMFLOAT3>& Engine::Scene::getVertices(size_t materialId) const
{
	return vertices.at(materialId);
}

const std::vector<Shaders::FaceAttributes>& Engine::Scene::getFaceAttributes() const
{
	return faceAttributes;
}

const std::vector<Shaders::AreaLight>& Engine::Scene::getLights() const
{
	return lights;
}

const vector<Shaders::Material>& Engine::Scene::getMaterials() const
{
	return materials;
}
