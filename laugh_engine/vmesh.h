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
#undef max
#undef min

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
		std::unordered_map<gli::format, VkFormat> gliFormat2VkFormatTable =
		{
			{ gli::FORMAT_RGBA8_UNORM_PACK8, VK_FORMAT_R8G8B8A8_UNORM },
			{ gli::FORMAT_RGBA32_SFLOAT_PACK32, VK_FORMAT_R32G32B32A32_SFLOAT },
			{ gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16, VK_FORMAT_BC3_UNORM_BLOCK },
			{ gli::FORMAT_RG32_SFLOAT_PACK32, VK_FORMAT_R32G32_SFLOAT },
			{ gli::FORMAT_RGB8_UNORM_PACK8, VK_FORMAT_R8G8B8_UNORM }
		};

		gli::format chooseFormat(uint32_t componentType, uint32_t componentCount)
		{
			if (componentCount == 3)
			{
				if (componentType == 5121)
				{
					return gli::FORMAT_RGB8_UNORM_PACK8;
				}
			}
			else if (componentCount == 4)
			{
				if (componentType == 5121)
				{
					return gli::FORMAT_RGBA8_UNORM_PACK8;
				}
			}
		}

		void loadMeshIntoHostBuffers(const std::string &modelFileName,
			std::vector<Vertex> &hostVerts, std::vector<uint32_t> &hostIndices,
			glm::vec3 *minPos = nullptr, glm::vec3 *maxPos = nullptr)
		{
			if (minPos) *minPos = glm::vec3(std::numeric_limits<float>::max());
			if (maxPos) *maxPos = glm::vec3(-std::numeric_limits<float>::max());

			Assimp::Importer meshImporter;
			const aiScene *scene = nullptr;

			const uint32_t defaultFlags =
				aiProcess_FlipWindingOrder |
				aiProcess_Triangulate |
				aiProcess_PreTransformVertices |
				aiProcess_GenSmoothNormals;

			scene = meshImporter.ReadFile(modelFileName, defaultFlags);

			std::unordered_map<Vertex, uint32_t> vert2IdxLut;

			for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
			{
				const aiMesh *mesh = scene->mMeshes[i];
				const aiVector3D *vertices = mesh->mVertices;
				const aiVector3D *normals = mesh->mNormals;
				const aiVector3D *texCoords = mesh->mTextureCoords[0];
				const auto *faces = mesh->mFaces;

				if (!normals || !texCoords)
				{
					throw std::runtime_error("model must have normals and uvs.");
				}

				for (uint32_t j = 0; j < mesh->mNumFaces; ++j)
				{
					const aiFace &face = faces[j];

					if (face.mNumIndices != 3) continue;

					for (uint32_t k = 0; k < face.mNumIndices; ++k)
					{
						uint32_t idx = face.mIndices[k];
						const aiVector3D &pos = vertices[idx];
						const aiVector3D &nrm = normals[idx];
						const aiVector3D &texCoord = texCoords[idx];

						Vertex vert =
						{
							glm::vec3(pos.x, pos.y, pos.z),
							glm::vec3(nrm.x, nrm.y, nrm.z),
							glm::vec2(texCoord.x, 1.f - texCoord.y)
						};

						if (minPos) *minPos = glm::min(*minPos, vert.pos);
						if (maxPos) *maxPos = glm::max(*maxPos, vert.pos);

						const auto searchResult = vert2IdxLut.find(vert);
						if (searchResult == vert2IdxLut.end())
						{
							uint32_t newIdx = static_cast<uint32_t>(hostVerts.size());
							vert2IdxLut[vert] = newIdx;
							hostIndices.emplace_back(newIdx);
							hostVerts.emplace_back(vert);
						}
						else
						{
							hostIndices.emplace_back(searchResult->second);
						}
					}
				}
			}
		}

		void loadTexture2DFromBinaryData(ImageWrapper *pTexRet, VManager *pManager, const void *pixels,
			uint32_t width, uint32_t height, gli::format gliformat, uint32_t mipLevels = 1, bool createSampler = true)
		{
			VkFormat format = gliFormat2VkFormatTable[gliformat];
			const auto &formatInfo = g_formatInfoTable[format];

			gli::extent2d extent = { width, height };
			gli::texture2d textureSrc{ gliformat, extent, mipLevels };
			size_t sizeInBytes = compute2DImageSizeInBytes(width, height, formatInfo.blockSize, mipLevels, 1);
			memcpy(textureSrc.data(), pixels, sizeInBytes);

			if (format != VK_FORMAT_R8G8B8A8_UNORM)
			{
				textureSrc = gli::convert(textureSrc, gli::FORMAT_RGBA8_UNORM_PACK8);
				format = VK_FORMAT_R8G8B8A8_UNORM;
			}

			mipLevels = static_cast<uint32_t>(textureSrc.levels());
			sizeInBytes = textureSrc.size();

			pTexRet->width = width;
			pTexRet->height = height;
			pTexRet->depth = 1;
			pTexRet->format = format;
			pTexRet->mipLevelCount = mipLevels;
			pTexRet->layerCount = 1;

			pTexRet->image = pManager->createImage2D(width, height, format,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				mipLevels);

			pManager->transferHostDataToImage(pTexRet->image, sizeInBytes, textureSrc.data(),
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			pTexRet->imageViews.push_back(pManager->createImageView2D(pTexRet->image, VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels));

			if (createSampler)
			{
				pTexRet->samplers.resize(1);
				pTexRet->samplers[0] = pManager->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
					VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
					0.f, float(mipLevels - 1), 0.f, VK_TRUE, 16.f);
			}
		}

		void loadTexture2D(ImageWrapper *pTexRet, VManager *pManager, const std::string &fn, bool createSampler = true)
		{
			std::string ext = getFileExtension(fn);
			if (ext != "ktx" && ext != "dds")
			{
				throw std::runtime_error("texture type ." + ext + " is not supported.");
			}

			gli::texture2d textureSrc(gli::load(fn.c_str()));

			if (textureSrc.empty())
			{
				throw std::runtime_error("cannot load texture.");
			}

			VkFormat format = gliFormat2VkFormatTable.at(textureSrc.format());

			uint32_t width = static_cast<uint32_t>(textureSrc.extent().x);
			uint32_t height = static_cast<uint32_t>(textureSrc.extent().y);
			uint32_t mipLevels = static_cast<uint32_t>(textureSrc.levels());
			size_t sizeInBytes = textureSrc.size();

			pTexRet->width = width;
			pTexRet->height = height;
			pTexRet->depth = 1;
			pTexRet->format = format;
			pTexRet->mipLevelCount = mipLevels;
			pTexRet->layerCount = 1;

			pTexRet->image = pManager->createImage2D(width, height, format,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				mipLevels);

			pManager->transferHostDataToImage(pTexRet->image, sizeInBytes, textureSrc.data(),
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			pTexRet->imageViews.push_back(pManager->createImageView2D(pTexRet->image, VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels));

			if (createSampler)
			{
				pTexRet->samplers.resize(1);
				pTexRet->samplers[0] = pManager->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
					VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
					0.f, float(mipLevels - 1), 0.f, VK_TRUE, 16.f);
			}
		}

		void loadCubemap(ImageWrapper *pTexRet, VManager *pManager, const std::string &fn, bool createSampler = true)
		{
			std::string ext = getFileExtension(fn);
			if (ext != "ktx" && ext != "dds")
			{
				throw std::runtime_error("texture type ." + ext + " is not supported.");
			}

			gli::texture_cube texCube(gli::load(fn.c_str()));

			if (texCube.empty())
			{
				throw std::runtime_error("cannot load texture.");
			}

			VkFormat format = gliFormat2VkFormatTable.at(texCube.format());

			uint32_t width = static_cast<uint32_t>(texCube.extent().x);
			uint32_t height = static_cast<uint32_t>(texCube.extent().y);
			uint32_t mipLevels = static_cast<uint32_t>(texCube.levels());
			size_t sizeInBytes = texCube.size();

			pTexRet->format = format;
			pTexRet->width = width;
			pTexRet->height = height;
			pTexRet->depth = 1;
			pTexRet->mipLevelCount = mipLevels;
			pTexRet->layerCount = 6;

			pTexRet->image = pManager->createImageCube(width, height, format,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mipLevels);

			pManager->transferHostDataToImage(pTexRet->image, sizeInBytes, texCube.data(),
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			pTexRet->imageViews.push_back(pManager->createImageViewCube(pTexRet->image, VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels));

			if (createSampler)
			{
				pTexRet->samplers.resize(1);
				pTexRet->samplers[0] = pManager->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
					VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
					0.f, float(mipLevels - 1), 0.f, VK_TRUE, 16.f);
			}
		}
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
	const static uint32_t numMapsPerMesh = 5;

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

	MaterialType_t materialType = MATERIAL_TYPE_FSCHLICK_DGGX_GSMITH;


	static void loadFromGLTF(std::vector<VMesh> &retMeshes, rj::VManager *pManager, const std::string gltfFileName)
	{
		using namespace rj::helper_functions;

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
				uint32_t numVertices = posAccessor.count;
				hostVertices.resize(hostVertices.size() + numVertices);
				uint32_t numIndices = idxAccessor.count;
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


	VMesh(rj::VManager *pManager) : pVulkanManager(pManager)
	{
		albedoMap.image = std::numeric_limits<uint32_t>::max();
		normalMap.image = std::numeric_limits<uint32_t>::max();
		roughnessMap.image = std::numeric_limits<uint32_t>::max();
		metalnessMap.image = std::numeric_limits<uint32_t>::max();
		aoMap.image = std::numeric_limits<uint32_t>::max();
	}

	void load(
		const std::string &modelFileName,
		const std::string &albedoMapName = "",
		const std::string &normalMapName = "",
		const std::string &roughnessMapName = "",
		const std::string &metalnessMapName = "",
		const std::string &aoMapName = "")
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
	rj::helper_functions::ImageWrapper diffuseIrradianceMap; // TODO: use spherical harmonics instead

	bool specMapReady = false;
	bool diffMapReady = false;
	bool shouldSaveSpecMap = false;
	bool shouldSaveDiffMap = false;

	Skybox(rj::VManager *pManager) :
		VMesh{ pManager }
	{
		materialType = MATERIAL_TYPE_HDR_PROBE;
	}

	void load(
		const std::string &modelFileName,
		const std::string &radianceMapName,
		const std::string &specMapName,
		const std::string &diffuseMapName)
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

		if (diffuseMapName != "")
		{
			loadCubemap(&diffuseIrradianceMap, pVulkanManager, diffuseMapName);
			diffMapReady = true;
		}
		else
		{
			diffuseIrradianceMap.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			diffuseIrradianceMap.width = diffuseIrradianceMap.height = DIFF_IRRADIANCE_MAP_SIZE;
			diffuseIrradianceMap.depth = 1;
			diffuseIrradianceMap.mipLevelCount = 1;
			diffuseIrradianceMap.layerCount = 6;

			diffuseIrradianceMap.image =
				pVulkanManager->createImageCube(diffuseIrradianceMap.width, diffuseIrradianceMap.height, diffuseIrradianceMap.format,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			diffuseIrradianceMap.imageViews.push_back(pVulkanManager->createImageViewCube(diffuseIrradianceMap.image, VK_IMAGE_ASPECT_COLOR_BIT));

			// maxLod == 0.f will cause magFilter to always be used. This is fine if minFilter == maxFilter but
			// need to be careful
			diffuseIrradianceMap.samplers.resize(1);
			diffuseIrradianceMap.samplers[0] = pVulkanManager->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				0.f, 0.f, 0.f, VK_TRUE, 16.f);

			shouldSaveDiffMap = true;
		}

		VMesh::load(modelFileName);
	}
};