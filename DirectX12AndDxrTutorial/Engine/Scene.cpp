#include "Scene.h"

#include "Exception/Exception.h"

#include "Libraries/tinyobjloader/tiny_obj_loader.h"

using namespace std;
using namespace Engine;

vector<DirectX::XMFLOAT3> Engine::Scene::loadScene(const string& pathToObj)
{
	using namespace  tinyobj;

	attrib_t attr = {};
	vector<shape_t> shapes;
	vector<material_t> materials;
	string warn, err;
	LoadObj(&attr, &shapes, &materials, &warn, &err, pathToObj.c_str());

	vector<DirectX::XMFLOAT3> verts;
	// for each shape
	for (const auto& shape : shapes) {
		size_t index = 0;

		// for each face
		for (const auto& vertexCountForFace : shape.mesh.num_face_vertices) {
			if (vertexCountForFace != 3) {
				ThrowException("Not loading a triangle");
			}

			// for each vertex in face
			for (size_t v = 0; v < vertexCountForFace; ++v) {
				int vertexIndex = shape.mesh.indices[index + v].vertex_index;
				size_t vertexLocation = 3 * static_cast<size_t>(vertexIndex);
				verts.push_back(DirectX::XMFLOAT3(
					attr.vertices[vertexLocation],
					attr.vertices[vertexLocation + 1],
					attr.vertices[vertexLocation + 2]));
			}

			index += vertexCountForFace;
		}
	}

	return verts;
}
