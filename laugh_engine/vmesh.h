#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"    
#include "assimp/postprocess.h"
#include "assimp/cimport.h"
#include "VManager.h"

#include "tiny_gltf_loader.h"
#include "gltf_loader.h"

#define DIFF_IRRADIANCE_MAP_SIZE 32
#define SPEC_IRRADIANCE_MAP_SIZE 512


struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texCoord;

	bool operator==(const Vertex& other) const
	{
		return pos == other.pos &&
			normal == other.normal &&
			texCoord == other.texCoord;
	}

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3, {});

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, normal);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}
};

struct BBox
{
	glm::vec3 min;
	glm::vec3 max;

	BBox() : min(std::numeric_limits<float>::max()), max(-std::numeric_limits<float>::max()) {}

	BBox(const glm::vec3 &_min, const glm::vec3 &_max)
		: min(_min), max(_max) {}
};

namespace std
{
	template<> struct hash<Vertex>
	{
		size_t operator()(Vertex const& vertex) const
		{
			using namespace rj::helper_functions;
			size_t seed = 0;
			hash_combine(seed, vertex.pos);
			hash_combine(seed, vertex.normal);
			hash_combine(seed, vertex.texCoord);
			return seed;
		}
	};
}

namespace rj
{
	namespace helper_functions
	{
		extern std::unordered_map<gli::format, VkFormat> gliFormat2VkFormatTable;

		gli::format chooseFormat(uint32_t componentType, uint32_t componentCount);

		void loadMeshIntoHostBuffers(const std::string &modelFileName,
			std::vector<Vertex> &hostVerts, std::vector<uint32_t> &hostIndices,
			glm::vec3 *minPos = nullptr, glm::vec3 *maxPos = nullptr);

		void loadTexture2DFromBinaryData(ImageWrapper *pTexRet, VManager *pManager, const void *pixels,
			uint32_t width, uint32_t height, gli::format gliformat, uint32_t mipLevels = 1, bool createSampler = true);

		void loadTexture2D(ImageWrapper *pTexRet, VManager *pManager, const std::string &fn, bool createSampler = true);

		void loadCubemap(ImageWrapper *pTexRet, VManager *pManager, const std::string &fn, bool createSampler = true);
	}
}

enum MaterialType
{
	MATERIAL_TYPE_HDR_PROBE = 0,
	MATERIAL_TYPE_FSCHLICK_DGGX_GSMITH,
	MATERIAL_TYPE_COUNT
};

typedef uint32_t MaterialType_t;

struct PerModelUniformBuffer
{
	glm::mat4 M;
	glm::mat4 M_invTrans;
};

class VMesh
{
public:
	const static uint32_t numMapsPerMesh = 6;

	rj::VManager *pVulkanManager;

	PerModelUniformBuffer *uPerModelInfo = nullptr;
	bool uniformDataChanged = true;
	
	glm::vec3 worldPosition{ 0.f, 0.f, 0.f };
	glm::quat worldRotation{ glm::vec3(0.f, 0.f, 0.f) }; // Euler angles to quaternion
	float scale = 1.f;
	BBox bounds;

	rj::helper_functions::BufferWrapper vertexBuffer;
	rj::helper_functions::BufferWrapper indexBuffer;

	rj::helper_functions::ImageWrapper albedoMap;
	rj::helper_functions::ImageWrapper normalMap;
	rj::helper_functions::ImageWrapper roughnessMap;
	rj::helper_functions::ImageWrapper metalnessMap;
	rj::helper_functions::ImageWrapper aoMap;
	rj::helper_functions::ImageWrapper emissiveMap;

	MaterialType_t materialType = MATERIAL_TYPE_FSCHLICK_DGGX_GSMITH;


	static void loadFromGLTF(std::vector<VMesh> &retMeshes, rj::VManager *pManager, const std::string &gltfFileName,
		const std::string &version = "1.0")
	{
		using namespace rj::helper_functions;

		if (version == "1.0")
		{
			// Parse glTF
			tinygltf::Scene scene;
			tinygltf::TinyGLTFLoader loader;
			std::string err;
			bool status;

			std::string ext = rj::helper_functions::getFileExtension(gltfFileName);
			if (ext == "gltf")
			{
				status = loader.LoadASCIIFromFile(&scene, &err, gltfFileName);
			}
			else if (ext == "glb")
			{
				status = loader.LoadBinaryFromFile(&scene, &err, gltfFileName);
			}
			else
			{
				throw std::invalid_argument("invalid input file name");
			}

			if (!status)
			{
				throw std::runtime_error("failed to parse glTF: " + err);
			}

			// Meshes
			std::unordered_map<std::string, std::vector<const tinygltf::Primitive *>> mat2meshes;
			const auto &meshes = scene.meshes;
			const auto &materials = scene.materials;
			const auto &textures = scene.textures;
			const auto &images = scene.images;

			for (const auto &nameMeshPair : meshes)
			{
				const auto &name = nameMeshPair.first;
				const auto &mesh = nameMeshPair.second;

				for (const auto &prim : mesh.primitives)
				{
					const auto &matName = prim.material;
					mat2meshes[matName].emplace_back(&prim);
				}
			}

			for (const auto &matMeshes : mat2meshes)
			{
				retMeshes.emplace_back(pManager);
				auto &retMesh = retMeshes.back();

				// Textures
				const auto &material = materials.at(matMeshes.first);
				{
					const auto &texName = material.values.at("baseColorTexture").string_value;
					const auto &tex = textures.at(texName);
					const auto &image = images.at(tex.source);

					auto gliFormat = chooseFormat(tex.type, image.component);
					loadTexture2DFromBinaryData(&retMesh.albedoMap, pManager, image.image.data(), image.width, image.height, gliFormat, image.levelCount);
				}
				{
					const auto &texName = material.values.at("normalTexture").string_value;
					const auto &tex = textures.at(texName);
					const auto &image = images.at(tex.source);

					auto gliFormat = chooseFormat(tex.type, image.component);
					loadTexture2DFromBinaryData(&retMesh.normalMap, pManager, image.image.data(), image.width, image.height, gliFormat, image.levelCount);
				}
				{
					const auto &texName = material.values.at("roughnessTexture").string_value;
					const auto &tex = textures.at(texName);
					const auto &image = images.at(tex.source);

					auto gliFormat = chooseFormat(tex.type, image.component);
					loadTexture2DFromBinaryData(&retMesh.roughnessMap, pManager, image.image.data(), image.width, image.height, gliFormat, image.levelCount);
				}
				{
					const auto &texName = material.values.at("metallicTexture").string_value;
					const auto &tex = textures.at(texName);
					const auto &image = images.at(tex.source);

					auto gliFormat = chooseFormat(tex.type, image.component);
					loadTexture2DFromBinaryData(&retMesh.metalnessMap, pManager, image.image.data(), image.width, image.height, gliFormat, image.levelCount);
				}
				if (material.values.find("aoTexture") != material.values.end())
				{
					const auto &texName = material.values.at("aoTexture").string_value;
					const auto &tex = textures.at(texName);
					const auto &image = images.at(tex.source);

					auto gliFormat = chooseFormat(tex.type, image.component);
					loadTexture2DFromBinaryData(&retMesh.aoMap, pManager, image.image.data(), image.width, image.height, gliFormat, image.levelCount);
				}
				if (material.values.find("emissiveTexture") != material.values.end())
				{
					const auto &texName = material.values.at("emissiveTexture").string_value;
					const auto &tex = textures.at(texName);
					const auto &image = images.at(tex.source);

					auto gliFormat = chooseFormat(tex.type, image.component);
					loadTexture2DFromBinaryData(&retMesh.emissiveMap, pManager, image.image.data(), image.width, image.height, gliFormat, image.levelCount);
				}

				// Geometry
				std::vector<Vertex> hostVertices;
				std::vector<uint32_t> hostIndices;
				uint32_t vertOffset = 0;
				uint32_t indexOffset = 0;
				const auto &accessors = scene.accessors;
				const auto &bufferViews = scene.bufferViews;
				const auto &buffers = scene.buffers;

				for (const auto &pMeshPrim : matMeshes.second)
				{
					const auto &mesh = *pMeshPrim;
					const auto &posAccessor = accessors.at(mesh.attributes.at("POSITION"));
					const auto &nrmAccessor = accessors.at(mesh.attributes.at("NORMAL"));
					const auto &uvAccessor = accessors.at(mesh.attributes.at("TEXCOORD_0"));
					const auto &idxAccessor = accessors.at(mesh.indices);
					uint32_t numVertices = static_cast<uint32_t>(posAccessor.count);
					hostVertices.resize(hostVertices.size() + numVertices);
					uint32_t numIndices = static_cast<uint32_t>(idxAccessor.count);
					hostIndices.resize(hostIndices.size() + numIndices);

					// Postions
					{
						const auto &bufferView = bufferViews.at(posAccessor.bufferView);
						const auto &buffer = buffers.at(bufferView.buffer);
						size_t offset = bufferView.byteOffset + posAccessor.byteOffset;
						size_t stride = posAccessor.byteStride;
						assert(stride >= 12);
						assert(posAccessor.count == numVertices);
						const auto *data = &buffer.data[offset];

						for (uint32_t i = 0; i < numVertices; ++i)
						{
							const float *pos = reinterpret_cast<const float *>(&data[stride * i]);
							hostVertices[vertOffset + i].pos = glm::vec3(pos[0], pos[1], pos[2]);

							retMesh.bounds.max = glm::max(retMesh.bounds.max, hostVertices[vertOffset + i].pos);
							retMesh.bounds.min = glm::min(retMesh.bounds.max, hostVertices[vertOffset + i].pos);
						}
					}
					// Normal
					{
						const auto &bufferView = bufferViews.at(nrmAccessor.bufferView);
						const auto &buffer = buffers.at(bufferView.buffer);
						size_t offset = bufferView.byteOffset + nrmAccessor.byteOffset;
						size_t stride = nrmAccessor.byteStride;
						assert(stride >= 12);
						assert(nrmAccessor.count == numVertices);
						const auto *data = &buffer.data[offset];

						for (uint32_t i = 0; i < numVertices; ++i)
						{
							const float *nrm = reinterpret_cast<const float *>(&data[stride * i]);
							hostVertices[vertOffset + i].normal = glm::vec3(nrm[0], nrm[1], nrm[2]);
						}
					}
					// Texture coordinates
					{
						const auto &bufferView = bufferViews.at(uvAccessor.bufferView);
						const auto &buffer = buffers.at(bufferView.buffer);
						size_t offset = bufferView.byteOffset + uvAccessor.byteOffset;
						size_t stride = uvAccessor.byteStride;
						assert(stride >= 8);
						assert(uvAccessor.count == numVertices);
						const auto *data = &buffer.data[offset];

						for (uint32_t i = 0; i < numVertices; ++i)
						{
							const float *uv = reinterpret_cast<const float *>(&data[stride * i]);
							hostVertices[vertOffset + i].texCoord = glm::vec2(uv[0], 1.f - uv[1]);
						}
					}
					// Indices
					{
						const auto &bufferView = bufferViews.at(idxAccessor.bufferView);
						const auto &buffer = buffers.at(bufferView.buffer);
						size_t offset = bufferView.byteOffset + idxAccessor.byteOffset;
						const uint16_t *data = reinterpret_cast<const uint16_t *>(&buffer.data[offset]);

						for (uint32_t i = 0; i < numIndices; ++i)
						{
							hostIndices[indexOffset + i] = vertOffset + static_cast<uint32_t>(data[i]);
						}
					}

					vertOffset += numVertices;
					indexOffset += numIndices;
				}

				// create vertex buffer
				retMesh.vertexBuffer = {};
				retMesh.vertexBuffer.size = sizeof(hostVertices[0]) * hostVertices.size();
				retMesh.vertexBuffer.buffer = pManager->createBuffer(retMesh.vertexBuffer.size,
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

				pManager->transferHostDataToBuffer(retMesh.vertexBuffer.buffer, retMesh.vertexBuffer.size, hostVertices.data());

				// create index buffer
				retMesh.indexBuffer = {};
				retMesh.indexBuffer.size = sizeof(hostIndices[0]) * hostIndices.size();
				retMesh.indexBuffer.buffer = pManager->createBuffer(retMesh.indexBuffer.size,
					VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

				pManager->transferHostDataToBuffer(retMesh.indexBuffer.buffer, retMesh.indexBuffer.size, hostIndices.data());
			}
		}
		else
		{
			rj::GLTFScene scene;
			rj::GLTFLoader loader;
			loader.load(&scene, gltfFileName);

			for (const auto &mesh : scene.meshes)
			{
				retMeshes.emplace_back(pManager);
				auto &retMesh = retMeshes.back();
				auto gliFormat = gli::FORMAT_RGBA8_UNORM_PACK8;

				// Textures
				loadTexture2DFromBinaryData(&retMesh.albedoMap, pManager, mesh.albedoMap.pixels.data(),
					mesh.albedoMap.width, mesh.albedoMap.height, gliFormat, mesh.albedoMap.levelCount);

				loadTexture2DFromBinaryData(&retMesh.normalMap, pManager, mesh.normalMap.pixels.data(),
					mesh.normalMap.width, mesh.normalMap.height, gliFormat, mesh.normalMap.levelCount);

				loadTexture2DFromBinaryData(&retMesh.roughnessMap, pManager, mesh.roughnessMap.pixels.data(),
					mesh.roughnessMap.width, mesh.roughnessMap.height, gliFormat, mesh.roughnessMap.levelCount);

				loadTexture2DFromBinaryData(&retMesh.metalnessMap, pManager, mesh.metallicMap.pixels.data(),
					mesh.metallicMap.width, mesh.metallicMap.height, gliFormat, mesh.metallicMap.levelCount);

				if (!mesh.aoMap.pixels.empty())
				{
					loadTexture2DFromBinaryData(&retMesh.aoMap, pManager, mesh.aoMap.pixels.data(),
						mesh.aoMap.width, mesh.aoMap.height, gliFormat, mesh.aoMap.levelCount);
				}

				if (!mesh.emissiveMap.pixels.empty())
				{
					loadTexture2DFromBinaryData(&retMesh.emissiveMap, pManager, mesh.emissiveMap.pixels.data(),
						mesh.emissiveMap.width, mesh.emissiveMap.height, gliFormat, mesh.emissiveMap.levelCount);
				}

				// Geometry
				uint32_t vertCount = static_cast<uint32_t>(mesh.positions.size() / 3);
				std::vector<Vertex> hostVertices(vertCount);

				for (int i = 0; i < vertCount; ++i)
				{
					Vertex &vert = hostVertices[i];
					vert.pos = glm::vec3(mesh.positions[3 * i], mesh.positions[3 * i + 1], mesh.positions[3 * i + 2]);
					vert.normal = glm::vec3(mesh.normals[3 * i], mesh.normals[3 * i + 1], mesh.normals[3 * i + 2]);
					vert.texCoord = glm::vec2(mesh.texCoords[i << 1], mesh.texCoords[(i << 1) + 1]);

					retMesh.bounds.max = glm::max(retMesh.bounds.max, vert.pos);
					retMesh.bounds.min = glm::min(retMesh.bounds.min, vert.pos);
				}

				// create vertex buffer
				retMesh.vertexBuffer = {};
				retMesh.vertexBuffer.size = sizeof(hostVertices[0]) * hostVertices.size();
				retMesh.vertexBuffer.buffer = pManager->createBuffer(retMesh.vertexBuffer.size,
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

				pManager->transferHostDataToBuffer(retMesh.vertexBuffer.buffer, retMesh.vertexBuffer.size, hostVertices.data());

				// create index buffer
				retMesh.indexBuffer = {};
				retMesh.indexBuffer.size = sizeof(mesh.indices[0]) * mesh.indices.size();
				retMesh.indexBuffer.buffer = pManager->createBuffer(retMesh.indexBuffer.size,
					VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

				pManager->transferHostDataToBuffer(retMesh.indexBuffer.buffer, retMesh.indexBuffer.size, mesh.indices.data());
			}
		}
	}


	VMesh(rj::VManager *pManager) : pVulkanManager(pManager)
	{
		albedoMap.image = std::numeric_limits<uint32_t>::max();
		normalMap.image = std::numeric_limits<uint32_t>::max();
		roughnessMap.image = std::numeric_limits<uint32_t>::max();
		metalnessMap.image = std::numeric_limits<uint32_t>::max();
		aoMap.image = std::numeric_limits<uint32_t>::max();
		emissiveMap.image = std::numeric_limits<uint32_t>::max();
	}

	void load(
		const std::string &modelFileName,
		const std::string &albedoMapName = "",
		const std::string &normalMapName = "",
		const std::string &roughnessMapName = "",
		const std::string &metalnessMapName = "",
		const std::string &aoMapName = "",
		const std::string &emissiveMapName = "")
	{
		using namespace rj::helper_functions;

		// load textures
		if (albedoMapName != "")
		{
			loadTexture2D(&albedoMap, pVulkanManager, albedoMapName);
		}
		if (normalMapName != "")
		{
			loadTexture2D(&normalMap, pVulkanManager, normalMapName);
		}
		if (roughnessMapName != "")
		{
			loadTexture2D(&roughnessMap, pVulkanManager, roughnessMapName);
		}
		if (metalnessMapName != "")
		{
			loadTexture2D(&metalnessMap, pVulkanManager, metalnessMapName);
		}
		if (aoMapName != "")
		{
			loadTexture2D(&aoMap, pVulkanManager, aoMapName);
		}
		if (emissiveMapName != "")
		{
			loadTexture2D(&emissiveMap, pVulkanManager, emissiveMapName);
		}

		// load mesh
		std::vector<Vertex> hostVerts;
		std::vector<uint32_t> hostIndices;
		glm::vec3 minPos, maxPos;
		loadMeshIntoHostBuffers(modelFileName, hostVerts, hostIndices, &minPos, &maxPos);
		bounds.min = minPos;
		bounds.max = maxPos;

		// create vertex buffer
		vertexBuffer = {};
		vertexBuffer.size = sizeof(hostVerts[0]) * hostVerts.size();
		vertexBuffer.buffer = pVulkanManager->createBuffer(vertexBuffer.size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		pVulkanManager->transferHostDataToBuffer(vertexBuffer.buffer, vertexBuffer.size, hostVerts.data());

		// create index buffer
		indexBuffer = {};
		indexBuffer.size = sizeof(hostIndices[0]) * hostIndices.size();
		indexBuffer.buffer = pVulkanManager->createBuffer(indexBuffer.size,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		pVulkanManager->transferHostDataToBuffer(indexBuffer.buffer, indexBuffer.size, hostIndices.data());
	}

	virtual void updateHostUniformBuffer()
	{
		assert(uPerModelInfo);
		if (!uniformDataChanged) return;
		uPerModelInfo->M = glm::translate(glm::mat4_cast(worldRotation) * glm::scale(glm::mat4(), glm::vec3(scale)), worldPosition);
		uPerModelInfo->M_invTrans = glm::transpose(glm::inverse(uPerModelInfo->M));
		uniformDataChanged = false;
	}
};

class Skybox : public VMesh
{
public:
	rj::helper_functions::ImageWrapper radianceMap; // unfiltered map
	rj::helper_functions::ImageWrapper specularIrradianceMap;
	glm::vec3 diffuseSHCoefficients[9];

	bool specMapReady = false;
	bool shouldSaveSpecMap = false;

	Skybox(rj::VManager *pManager) :
		VMesh{ pManager }
	{
		materialType = MATERIAL_TYPE_HDR_PROBE;
	}

	void load(
		const std::string &modelFileName,
		const std::string &radianceMapName,
		const std::string &specMapName,
		const std::string &diffuseSHName)
	{
		using namespace rj::helper_functions;

		if (radianceMapName != "")
		{
			loadCubemap(&radianceMap, pVulkanManager, radianceMapName);
		}
		else
		{
			throw std::invalid_argument("radiance map required but not provided.");
		}

		if (specMapName != "")
		{
			loadCubemap(&specularIrradianceMap, pVulkanManager, specMapName);
			specMapReady = true;
		}
		else
		{
			uint32_t mipLevels = static_cast<uint32_t>(floor(log2f(SPEC_IRRADIANCE_MAP_SIZE) + 0.5f)) + 1;

			specularIrradianceMap.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			specularIrradianceMap.width = specularIrradianceMap.height = SPEC_IRRADIANCE_MAP_SIZE;
			specularIrradianceMap.depth = 1;
			specularIrradianceMap.mipLevelCount = mipLevels;
			specularIrradianceMap.layerCount = 6;

			specularIrradianceMap.image =
				pVulkanManager->createImageCube(specularIrradianceMap.width, specularIrradianceMap.height, specularIrradianceMap.format,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mipLevels);

			// Used for sampling read in shaders
			specularIrradianceMap.imageViews.resize(mipLevels + 1);
			specularIrradianceMap.imageViews[0] = pVulkanManager->createImageViewCube(specularIrradianceMap.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels);

			// Used for rendering
			for (uint32_t level = 0; level < mipLevels; ++level)
			{
				specularIrradianceMap.imageViews[level + 1] =
					pVulkanManager->createImageViewCube(specularIrradianceMap.image, VK_IMAGE_ASPECT_COLOR_BIT, level);
			}

			specularIrradianceMap.samplers.resize(1);
			specularIrradianceMap.samplers[0] = pVulkanManager->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				0.f, static_cast<float>(mipLevels - 1), 0.f, VK_TRUE, 16.f);

			shouldSaveSpecMap = true;
		}

		if (diffuseSHName != "")
		{
			loadSHCoefficients(diffuseSHName);
		}
		else
		{
			computeSHCoefficients(radianceMapName, rj::helper_functions::getBaseDir(radianceMapName) + "/Diffuse_SH.bin");
		}

		VMesh::load(modelFileName);
	}

private:
	void computeSHCoefficients(const std::string &radianceMapName, const std::string &saveFileName = "")
	{
		gli::texture_cube rm(gli::load(radianceMapName));
		if (rm.empty()) throw std::runtime_error("Failed to load: " + radianceMapName);

		uint32_t width = rm.extent().x;
		uint32_t height = rm.extent().y;
		float pixelArea = (1.f / float(width)) * (1.f / float(height));
		memset(diffuseSHCoefficients, 0, sizeof(diffuseSHCoefficients));

		for (uint32_t faceIdx = 0; faceIdx < 6; ++faceIdx)
		{
			const glm::vec4 *rgba = reinterpret_cast<const glm::vec4 *>(rm.data(0, faceIdx, 0));
			glm::vec3 faceNrm = getFaceNormal(faceIdx);

			for (uint32_t py = 0; py < height; ++py)
			{
				for (uint32_t px = 0; px < width; ++px)
				{
					glm::vec3 wi = getWorldDir(faceIdx, px, py, width, height);
					float dist2 = glm::dot(wi, wi);
					wi = glm::normalize(wi);
					float dw = pixelArea * glm::dot(faceNrm, -wi) / dist2; // differential solid angle
					glm::vec3 L = glm::vec3(rgba[py * width + px]);

					diffuseSHCoefficients[0] += L * 0.282095f * dw; // l = m = 0
					diffuseSHCoefficients[1] += L * 0.488603f * wi.y * dw; // l = 1, m = -1
					diffuseSHCoefficients[2] += L * 0.488603f * wi.z * dw; // l = 1, m = 0
					diffuseSHCoefficients[3] += L * 0.488603f * wi.x * dw; // l = 1, m = 1
					diffuseSHCoefficients[4] += L * 1.092548f * wi.x * wi.y * dw; // l = 2, m = -2
					diffuseSHCoefficients[5] += L * 1.092548f * wi.y * wi.z * dw; // l = 2, m = -1
					diffuseSHCoefficients[6] += L * 0.315392f * (3.f * wi.z * wi.z - 1.f) * dw; // l = 2, m = 0
					diffuseSHCoefficients[7] += L * 1.092548f * wi.x * wi.z * dw; // l = 2, m = 1
					diffuseSHCoefficients[8] += L * 0.546274f * (wi.x * wi.x - wi.y * wi.y) * dw; // l = 2, m = 2
				}
			}
		}

		if (saveFileName != "")
		{
			std::ofstream fs(saveFileName, std::ofstream::out | std::ofstream::binary);
			if (fs.is_open())
			{
				fs.write(reinterpret_cast<const char *>(diffuseSHCoefficients), sizeof(diffuseSHCoefficients));
				fs.close();
			}
			else
			{
				throw std::runtime_error("Unable to open file: " + saveFileName);
			}
		}
	}

	glm::vec3 getFaceNormal(uint32_t faceIdx) const
	{
		glm::vec3 result(0.f);
		result[faceIdx >> 1] = (faceIdx & 1) ? 1.f : -1.f;
		return result;
	}

	glm::vec3 getWorldDir(uint32_t faceIdx, uint32_t px, uint32_t py, uint32_t width, uint32_t height) const
	{
		glm::vec2 pixelSize = 1.f / glm::vec2(static_cast<float>(width), static_cast<float>(height));
		glm::vec2 uv = glm::vec2(static_cast<float>(px) + 0.5f, static_cast<float>(py) + 0.5f) * pixelSize;
		uv.y = 1.f - uv.y;
		uv -= 0.5f; // [-0.5, 0.5]

		switch (faceIdx)
		{
		case 0: // +x
			return glm::vec3(0.5f, uv.y, -uv.x); // DDS uses left-handed system. Need to flip z
		case 1: // -x
			return glm::vec3(-0.5f, uv.y, uv.x);
		case 2: // +y
			return glm::vec3(uv.x, 0.5f, -uv.y);
		case 3: // -y
			return glm::vec3(uv.x, -0.5f, uv.y);
		case 4: // +z
			return glm::vec3(uv.x, uv.y, 0.5f);
		case 5: // -z
			return glm::vec3(-uv.x, uv.y, -0.5f);
		default:
			throw std::runtime_error("Invalid face index");
		}
	}

	void loadSHCoefficients(const std::string &fn)
	{
		std::ifstream fs(fn, std::ifstream::in | std::ifstream::binary);

		if (fs.is_open())
		{
			fs.seekg(0, std::ifstream::end);
			size_t fileSize = fs.tellg();
			assert(fileSize == sizeof(diffuseSHCoefficients));
			fs.seekg(0, std::ifstream::beg);
			fs.read(reinterpret_cast<char *>(diffuseSHCoefficients), fileSize);
			fs.close();
		}
		else
		{
			throw std::runtime_error("Invalid file name: " + fn);
		}
	}
};