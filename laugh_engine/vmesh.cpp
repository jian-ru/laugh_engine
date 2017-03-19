#include "vmesh.h"


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

			throw std::runtime_error("Not able to choose image format");
		}

		void loadMeshIntoHostBuffers(const std::string &modelFileName,
			std::vector<Vertex> &hostVerts, std::vector<uint32_t> &hostIndices,
			glm::vec3 *minPos, glm::vec3 *maxPos)
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
			uint32_t width, uint32_t height, gli::format gliformat, uint32_t mipLevels, bool createSampler)
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

		void loadTexture2D(ImageWrapper *pTexRet, VManager *pManager, const std::string &fn, bool createSampler)
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

		void loadCubemap(ImageWrapper *pTexRet, VManager *pManager, const std::string &fn, bool createSampler)
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