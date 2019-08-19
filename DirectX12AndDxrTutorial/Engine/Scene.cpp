#include "Scene.h"

#include "Exception/Exception.h"

#include "Libraries/tinyobjloader/tiny_obj_loader.h"
#include "Libraries/stb/stb_image.h"

using namespace std;
using namespace Engine;

void Engine::Scene::loadScene(const string& pathToObj)
{
	using namespace  tinyobj;

	// may be redundant
	std::vector<std::vector<DirectX::XMFLOAT3>> vertices;
	texVertices.clear();
	faceAttributes.clear();
	lights.clear();
	materials.clear();
	textures.clear();

	attrib_t attr = {};
	vector<shape_t> shapes;
	vector<material_t> materials;
	string warn, err;
	LoadObj(&attr, &shapes, &materials, &warn, &err, pathToObj.c_str());

	int diffTexId = 0;
	for (const auto& material : materials) {
		int currentDiffTexId = material.diffuse_texname.length() ? diffTexId++ : -1;
		if (currentDiffTexId >= 0) {
			textures.emplace_back(material.diffuse_texname);
		}

		this->materials.push_back(Shaders::Material{
			DirectX::XMFLOAT4(material.diffuse[0], material.diffuse[1], material.diffuse[2], 1.f),
			DirectX::XMFLOAT4(material.emission[0],material.emission[1],material.emission[2], 0.f),
			currentDiffTexId
		});
	}

	vertices.resize(shapes.size());
	size_t totalFaceCount = 0;
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

				int texIndex = shape.mesh.indices[index + v].texcoord_index;
				if (texIndex == -1) {
					texVertices.emplace_back(0.f, 0.f);
				}
				else {
					size_t texLocation = 2 * static_cast<size_t>(texIndex);
					// invert texture vertically as is commonly done in OBJ
					DirectX::XMFLOAT2 texVertex = DirectX::XMFLOAT2(attr.texcoords[texLocation], 1.f - attr.texcoords[texLocation + 1]);
					texVertices.push_back(texVertex);
				}
			}

			if (isEmissive) {
				areaLight.instanceIndex = shapeNum;
				areaLight.primitiveId = totalFaceCount;
				areaLight.materialId = materialId;
				areaLight.intensity = DirectX::XMVectorSet(1.f, 1.f, 1.f, 1.f);
				//areaLight.intensity = DirectX::XMVectorScale(areaLight.intensity, 0.1f);
				lights.push_back(areaLight);
			}

			index += vertexCountForFace;
			++faceNum;
			++totalFaceCount;
		}

		// for each face
		for (const auto& materialId : shape.mesh.material_ids) {
			faceAttributes.push_back({ static_cast<std::uint32_t>(materialId) });
		}

		// Initialise shape object
		this->shapes.emplace_back(shape.name, move(vertices[shapeNum]));

		++shapeNum;
	}
}

void Engine::Scene::transformLightPosition(const DirectX::XMMATRIX& mat)
{
	for (auto& light : lights) {
		/*for (std::size_t i = 0; i < std::size(light.a); ++i) {
			light.a[i] = DirectX::XMVector3Transform(light.a[i], mat);
		}*/
	}
}

void Engine::Scene::flattenGroups()
{
	auto verts = getFlattenedVertices();
	shapes.clear();
	shapes.emplace_back("Flattened Shape", move(verts));
}

std::vector<DirectX::XMFLOAT3> Engine::Scene::getFlattenedVertices() const
{
	std::vector<DirectX::XMFLOAT3> verts;
	for (const auto& shape : shapes) {
		const std::vector<DirectX::XMFLOAT3>& v = shape.getVertices();
		verts.insert(verts.end(), v.begin(), v.end());
	}

	return verts;
}

const std::vector<DirectX::XMFLOAT2>& Engine::Scene::getTextureVertices() const
{
	return texVertices;
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

const std::vector<Engine::Texture>& Engine::Scene::getTextures() const
{
	return textures;
}

const std::vector<Shape>& Engine::Scene::getShapes() const
{
	return shapes;
}

Shape& Engine::Scene::getShape(std::size_t index)
{
	return shapes[index];
}
