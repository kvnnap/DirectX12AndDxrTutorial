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

			// for each vertex in face
			for (size_t v = 0; v < vertexCountForFace; ++v) {
				int vertexIndex = shape.mesh.indices[index + v].vertex_index;
				size_t vertexLocation = 3 * static_cast<size_t>(vertexIndex);
				vertices[shapeNum].push_back(DirectX::XMFLOAT3(
					attr.vertices[vertexLocation],
					attr.vertices[vertexLocation + 1],
					attr.vertices[vertexLocation + 2]));
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

	for (const auto& material : materials) {
		this->materials.push_back(Material{ DirectX::XMFLOAT4(
			material.diffuse[0],material.diffuse[1],material.diffuse[2], 1.f
		) });
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

const vector<Material>& Engine::Scene::getMaterials() const
{
	return materials;
}
