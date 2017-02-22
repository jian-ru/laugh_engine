#pragma once

#include <fstream>
#include <unordered_set>

#include "picojson.h"
#include "gli/gli.hpp"

#undef max
#undef min

namespace rj
{
	enum GLTFComponentType
	{
		GLTF_BYTE = 5120,
		GLTF_UNSIGNED_BYTE = 5121,
		GLTF_SHORT = 5122,
		GLTF_UNSIGNED_SHORT = 5123,
		GLTF_FLOAT = 5126
	};

	static std::unordered_map<std::string, uint32_t> g_attrType2CompCnt =
	{
		{ "SCALAR", 1 },
		{ "VEC2", 2 },
		{ "VEC3", 3 },
		{ "VEC4", 4 },
		{ "MAT2", 4 },
		{ "MAT3", 9 },
		{ "MAT4", 16 }
	};

	static std::unordered_map<GLTFComponentType, uint32_t> g_compType2ByteSize =
	{
		{ GLTF_BYTE, 1 },
		{ GLTF_UNSIGNED_BYTE, 1 },
		{ GLTF_SHORT, 2 },
		{ GLTF_UNSIGNED_SHORT, 2 },
		{ GLTF_FLOAT, 4 }
	};

	struct GLTFAccessor
	{
		uint32_t bufferView;
		uint32_t byteOffset;
		GLTFComponentType componentType;
		uint32_t count; // number of aggregates (not components)
		std::string type;
	};

	struct GLTFBufferView
	{
		uint32_t buffer;
		uint32_t byteOffset;
		uint32_t byteLength;
	};

	typedef std::vector<char> GLTFBuffer;

	template <typename T>
	class GLTFBufferIterator
	{
	public:
		GLTFBufferIterator(const T *b, const T *e) : cur(b), end(e) {}

		bool hasNext() const { return cur != end; }

		T getNext() { return *cur++; }

	private:
		const T *cur = nullptr;
		const T *end = nullptr;
	};

	struct GLTFImage
	{
		uint32_t width;
		uint32_t height;
		uint32_t component;
		uint32_t levelCount;
		std::vector<char> pixels;
	};

	struct GLTFTexture
	{
		uint32_t sampler;
		uint32_t source; // index to a GLTFImage
	};

	struct GLTFMaterial
	{
		uint32_t albedoTexture = std::numeric_limits<uint32_t>::max();
		uint32_t normalTexture = std::numeric_limits<uint32_t>::max();
		uint32_t roughnessTexture = std::numeric_limits<uint32_t>::max();
		uint32_t metallicTexture = std::numeric_limits<uint32_t>::max();
		uint32_t aoTexture = std::numeric_limits<uint32_t>::max();
		uint32_t emissiveTexture = std::numeric_limits<uint32_t>::max();
	};

	// GLTFMesh is defined as an aggregate of all the geometry of the same material
	struct GLTFMesh
	{
		std::vector<float> positions;
		std::vector<float> normals;
		std::vector<float> texCoords;
		std::vector<uint32_t> indices;

		GLTFImage albedoMap;
		GLTFImage normalMap;
		GLTFImage roughnessMap;
		GLTFImage metallicMap;
		GLTFImage aoMap;
		GLTFImage emissiveMap;
	};

	struct GLTFNode
	{
		std::vector<uint32_t> children;
		uint32_t mesh = std::numeric_limits<uint32_t>::max();
		glm::mat4 local2parent;
	};

	struct GLTFScene
	{
		std::vector<GLTFMesh> meshes;
	};

	class GLTFLoader
	{
	public:
		void load(GLTFScene *scene, const std::string &fn) const
		{
			auto baseDir = getBaseDir(fn);
			auto ext = getExtension(fn);

			if (ext != "gltf") throw std::runtime_error("Not gltf file");

			std::ifstream fs(fn);
			if (!fs.is_open()) throw std::runtime_error("file: " + fn + " not found");

			picojson::value rootNode;
			auto err = picojson::parse(rootNode, fs);
			if (!err.empty()) throw std::runtime_error(err);

			// Parse accessors
			if (!rootNode.contains("accessors") || !rootNode.get("accessors").is<picojson::array>()) throw std::runtime_error("Invalid accessors");
			std::vector<GLTFAccessor> accessors;
			parseAccessors(accessors, rootNode.get("accessors").get<picojson::array>());

			// Parse bufferViews
			if (!rootNode.contains("bufferViews") || !rootNode.get("bufferViews").is<picojson::array>()) throw std::runtime_error("Invalid bufferViews");
			std::vector<GLTFBufferView> bufferViews;
			parseBufferViews(bufferViews, rootNode.get("bufferViews").get<picojson::array>());

			// Parse buffers
			if (!rootNode.contains("buffers") || !rootNode.get("buffers").is<picojson::array>()) throw std::runtime_error("Invalid buffers");
			std::vector<GLTFBuffer> buffers;
			parseBuffers(buffers, rootNode.get("buffers").get<picojson::array>(), baseDir);

			// Parse images
			if (!rootNode.contains("images") || !rootNode.get("images").is<picojson::array>()) throw std::runtime_error("Invalid images");
			std::vector<GLTFImage> images;
			parseImages(images, rootNode.get("images").get<picojson::array>(), baseDir);

			// Parse textures
			if (!rootNode.contains("textures") || !rootNode.get("textures").is<picojson::array>()) throw std::runtime_error("Invalid textures");
			std::vector<GLTFTexture> textures;
			parseTextures(textures, rootNode.get("textures").get<picojson::array>());

			// Parse materials
			if (!rootNode.contains("materials") || !rootNode.get("materials").is<picojson::array>()) throw std::runtime_error("Invalid materials");
			std::vector<GLTFMaterial> materials;
			parseMaterial(materials, rootNode.get("materials").get<picojson::array>());

			// Parse scene hierarchy
			if (!rootNode.contains("nodes") || !rootNode.get("nodes").is<picojson::array>()) throw std::runtime_error("Invalid nodes");
			std::unordered_map<uint32_t, glm::mat4> meshId2Transform;
			parseSceneHierarchy(meshId2Transform, rootNode.get("nodes").get<picojson::array>());

			// Parse meshes
			if (!rootNode.contains("meshes") || !rootNode.get("meshes").is<picojson::array>()) throw std::runtime_error("Invalid meshes");
			parseMeshes(scene->meshes, rootNode.get("meshes").get<picojson::array>(),
				accessors, bufferViews, buffers, images, textures, materials, meshId2Transform);
		}

	private:
		void parseSceneHierarchy(std::unordered_map<uint32_t, glm::mat4> &meshId2Transform, const picojson::array &nodes) const
		{
			std::unordered_set<uint32_t> rootCandidates;
			for (uint32_t i = 0; i < static_cast<uint32_t>(nodes.size()); ++i)
			{
				rootCandidates.insert(i);
			}

			std::vector<GLTFNode> ns(nodes.size());
			uint32_t p = 0;

			for (const auto &node : nodes)
			{
				const auto &fields = node.get<picojson::object>();

				GLTFNode &n = ns[p++];
				if (fields.find("mesh") != fields.end()) n.mesh = static_cast<uint32_t>(fields.at("mesh").get<int64_t>());
				
				glm::vec3 trans(0.f);
				if (fields.find("translation") != fields.end())
				{
					const auto &v3 = fields.at("translation").get<picojson::array>();
					trans = glm::vec3(v3[0].get<double>(), v3[1].get<double>(), v3[2].get<double>());
				}

				glm::quat rot; // (0, 0, 0, 1);
				if (fields.find("rotation") != fields.end())
				{
					const auto &q = fields.at("rotation").get<picojson::array>();
					rot = glm::quat(q[3].get<double>(), q[0].get<double>(), q[1].get<double>(), q[2].get<double>());
				}

				glm::vec3 scale(1.f);
				if (fields.find("scale") != fields.end())
				{
					const auto &v3 = fields.at("scale").get<picojson::array>();
					scale = glm::vec3(v3[0].get<double>(), v3[1].get<double>(), v3[2].get<double>());
				}

				glm::mat4 I; // identity
				n.local2parent = glm::translate(I, trans) * glm::mat4_cast(rot) * glm::scale(I, scale);

				if (fields.find("children") != fields.end())
				{
					const auto &children = fields.at("children").get<picojson::array>();
					for (const auto &childId : children)
					{
						n.children.push_back(static_cast<uint32_t>(childId.get<int64_t>()));

						if (rootCandidates.find(n.children.back()) != rootCandidates.end())
						{
							rootCandidates.erase(n.children.back());
						}
					}
				}
			}

			std::function<void (uint32_t, glm::mat4)> visit = [&visit, &meshId2Transform, &ns](uint32_t root, glm::mat4 T)
			{
				const auto &n = ns[root];
				T *= n.local2parent;
				if (n.mesh != std::numeric_limits<uint32_t>::max()) meshId2Transform[n.mesh] = T;
				for (auto childId : n.children)
				{
					visit(childId, T);
				}
			};

			for (auto rc : rootCandidates)
			{
				visit(rc, glm::mat4());
			}
		}

		void parseMeshes(std::vector<GLTFMesh> &ms, const picojson::array &meshes,
			const std::vector<GLTFAccessor> &accessors, const std::vector<GLTFBufferView> &bufferViews,
			const std::vector<GLTFBuffer> &buffers, const std::vector<GLTFImage> &images,
			const std::vector<GLTFTexture> &textures, const std::vector<GLTFMaterial> &materials,
			const std::unordered_map<uint32_t, glm::mat4> &meshId2Transform) const
		{
			std::unordered_map<uint32_t, uint32_t> mat2mesh;

			for (uint32_t meshId = 0; meshId < meshes.size(); ++meshId)
			{
				const auto &mesh = meshes[meshId];

				glm::mat4 T = meshId2Transform.find(meshId) != meshId2Transform.end() ? meshId2Transform.at(meshId) : glm::mat4();
				glm::mat4 Tit = glm::transpose(glm::inverse(T));
				const auto &prims = mesh.get<picojson::object>().at("primitives").get<picojson::array>();

				for (const auto &prim : prims)
				{
					const auto &fields = prim.get<picojson::object>();
					uint32_t matId = static_cast<uint32_t>(fields.at("material").get<int64_t>());

					if (mat2mesh.find(matId) == mat2mesh.end())
					{
						mat2mesh[matId] = static_cast<uint32_t>(ms.size());
						ms.resize(ms.size() + 1);
					}
					uint32_t meshId = mat2mesh[matId];
					auto &m = ms[meshId];

					const auto &attributes = fields.at("attributes").get<picojson::object>();
					uint32_t posAccId = static_cast<uint32_t>(attributes.at("POSITION").get<int64_t>());
					uint32_t nrmAccId = static_cast<uint32_t>(attributes.at("NORMAL").get<int64_t>());
					uint32_t tcAccId = static_cast<uint32_t>(attributes.at("TEXCOORD_0").get<int64_t>());
					uint32_t idxAccId = static_cast<uint32_t>(fields.at("indices").get<int64_t>());
					uint32_t idxOffset = static_cast<uint32_t>(m.positions.size()) / 3;

					// Positions
					{
						const GLTFAccessor &acc = accessors[posAccId];
						assert(acc.type == "VEC3" && acc.componentType == GLTF_FLOAT);
						const GLTFBufferView &bv = bufferViews[acc.bufferView];
						const GLTFBuffer &buff = buffers[bv.buffer];
						uint32_t offset = acc.byteOffset + bv.byteOffset;
						uint32_t compCount = acc.count * g_attrType2CompCnt[acc.type];
						uint32_t size = compCount * g_compType2ByteSize[acc.componentType];

						uint32_t posStart = static_cast<uint32_t>(m.positions.size());
						m.positions.resize(m.positions.size() + compCount);
						memcpy(&m.positions[posStart], &buff[offset], size);

						glm::vec3 *positions = reinterpret_cast<glm::vec3 *>(&m.positions[posStart]);
						for (uint32_t i = 0; i < acc.count; ++i)
						{
							positions[i] = glm::vec3(T * glm::vec4(positions[i], 1.f));
						}
					}
					// Normals
					{
						const GLTFAccessor &acc = accessors[nrmAccId];
						assert(acc.type == "VEC3" && acc.componentType == GLTF_FLOAT);
						const GLTFBufferView &bv = bufferViews[acc.bufferView];
						const GLTFBuffer &buff = buffers[bv.buffer];
						uint32_t offset = acc.byteOffset + bv.byteOffset;
						uint32_t compCount = acc.count * g_attrType2CompCnt[acc.type];
						uint32_t size = compCount * g_compType2ByteSize[acc.componentType];

						uint32_t nrmStart = static_cast<uint32_t>(m.normals.size());
						m.normals.resize(m.normals.size() + compCount);
						memcpy(&m.normals[nrmStart], &buff[offset], size);

						glm::vec3 *normals = reinterpret_cast<glm::vec3 *>(&m.normals[nrmStart]);
						for (uint32_t i = 0; i < acc.count; ++i)
						{
							normals[i] = glm::normalize(glm::vec3(Tit * glm::vec4(normals[i], 0.f)));
						}
					}
					// Texture coordinates
					{
						const GLTFAccessor &acc = accessors[tcAccId];
						assert(acc.type == "VEC2" && acc.componentType == GLTF_FLOAT);
						const GLTFBufferView &bv = bufferViews[acc.bufferView];
						const GLTFBuffer &buff = buffers[bv.buffer];
						uint32_t offset = acc.byteOffset + bv.byteOffset;
						uint32_t compCount = acc.count * g_attrType2CompCnt[acc.type];
						uint32_t size = compCount * g_compType2ByteSize[acc.componentType];

						uint32_t tcStart = static_cast<uint32_t>(m.texCoords.size());
						m.texCoords.resize(m.texCoords.size() + compCount);
						memcpy(&m.texCoords[tcStart], &buff[offset], size);
					}
					// Indices
					{
						const GLTFAccessor &acc = accessors[idxAccId];
						assert(acc.type == "SCALAR" && acc.componentType == GLTF_UNSIGNED_SHORT);
						const GLTFBufferView &bv = bufferViews[acc.bufferView];
						const GLTFBuffer &buff = buffers[bv.buffer];
						uint32_t offset = acc.byteOffset + bv.byteOffset;
						uint32_t compCount = acc.count * g_attrType2CompCnt[acc.type];
						uint32_t size = compCount * g_compType2ByteSize[acc.componentType];

						GLTFBufferIterator<uint16_t> it(
							reinterpret_cast<const uint16_t *>(&buff[offset]),
							reinterpret_cast<const uint16_t *>(&buff[offset]) + compCount);
						while (it.hasNext())
						{
							m.indices.push_back(idxOffset + static_cast<uint32_t>(it.getNext()));
						}
					}

#define INVALID_VAL std::numeric_limits<uint32_t>::max()
#define IS_VALID(x) ((x) != INVALID_VAL)
					const GLTFMaterial &material = materials[matId];
					uint32_t albedoMapId = textures[material.albedoTexture].source;
					uint32_t normalMapId = textures[material.normalTexture].source;
					uint32_t roughnessMapId = textures[material.roughnessTexture].source;
					uint32_t metallicMapId = textures[material.metallicTexture].source;
					uint32_t aoMapId = IS_VALID(material.aoTexture) ? textures[material.aoTexture].source : INVALID_VAL;
					uint32_t emissiveMapId = IS_VALID(material.emissiveTexture) ? textures[material.emissiveTexture].source : INVALID_VAL;
					m.albedoMap = images[albedoMapId];
					m.normalMap = images[normalMapId];
					m.roughnessMap = images[roughnessMapId];
					m.metallicMap = images[metallicMapId];
					if (IS_VALID(aoMapId)) m.aoMap = images[aoMapId];
					if (IS_VALID(emissiveMapId)) m.emissiveMap = images[emissiveMapId];
#undef IS_VALID
#undef INVALID_VAL
				}
			}
		}

		void parseMaterial(std::vector<GLTFMaterial> &mats, const picojson::array &materials) const
		{
			for (const auto &material : materials)
			{
				const auto &fields = material.get<picojson::object>();
				const auto &pbrTextures = fields.at("pbrMetallicRoughness").get<picojson::object>();

				GLTFMaterial mat;
				mat.albedoTexture = static_cast<uint32_t>(pbrTextures.at("baseColorTexture").get<picojson::object>().at("index").get<int64_t>());
				mat.roughnessTexture = static_cast<uint32_t>(pbrTextures.at("metallicRoughnessTexture").get<picojson::object>().at("index").get<int64_t>());
				mat.metallicTexture = mat.roughnessTexture;
				mat.normalTexture = static_cast<uint32_t>(fields.at("normalTexture").get<picojson::object>().at("index").get<int64_t>());
				if (fields.find("occlusionTexture") != fields.end())
				{
					mat.aoTexture = static_cast<uint32_t>(fields.at("occlusionTexture").get<picojson::object>().at("index").get<int64_t>());
				}
				if (fields.find("emissiveTexture") != fields.end())
				{
					mat.emissiveTexture = static_cast<uint32_t>(fields.at("emissiveTexture").get<picojson::object>().at("index").get<int64_t>());
				}

				mats.emplace_back(mat);
			}
		}

		void parseAccessors(std::vector<GLTFAccessor> &as, const picojson::array &accessors) const
		{
			for (const auto &accessor : accessors)
			{
				const auto &fields = accessor.get<picojson::object>();

				GLTFAccessor a;
				a.bufferView = static_cast<uint32_t>(fields.at("bufferView").get<int64_t>());
				a.byteOffset = static_cast<uint32_t>(fields.at("byteOffset").get<int64_t>());
				a.componentType = static_cast<rj::GLTFComponentType>(fields.at("componentType").get<int64_t>());
				a.count = static_cast<uint32_t>(fields.at("count").get<int64_t>());
				a.type = fields.at("type").get<std::string>();

				as.emplace_back(a);
			}
		}

		void parseBufferViews(std::vector<GLTFBufferView> &bvs, const picojson::array &bufferViews) const
		{
			for (const auto &bufferView : bufferViews)
			{
				const auto &fields = bufferView.get<picojson::object>();

				GLTFBufferView bv;
				bv.buffer = static_cast<uint32_t>(fields.at("buffer").get<int64_t>());
				bv.byteOffset = static_cast<uint32_t>(fields.at("byteOffset").get<int64_t>());
				bv.byteLength = static_cast<uint32_t>(fields.at("byteLength").get<int64_t>());

				bvs.emplace_back(bv);
			}
		}

		void parseBuffers(std::vector<GLTFBuffer> &bs, const picojson::array &buffers, const std::string &baseDir) const
		{
			size_t p = bs.size();
			bs.resize(bs.size() + buffers.size());

			for (const auto &buffer : buffers)
			{
				const auto &fields = buffer.get<picojson::object>();

				auto &b = bs[p++];
				readEntireFile(b, baseDir + "/" + fields.at("uri").get<std::string>());
				if (b.size() != static_cast<size_t>(fields.at("byteLength").get<int64_t>())) throw std::runtime_error("Incorrect buffer byte length");
			}
		}

		void parseImages(std::vector<GLTFImage> &imgs, const picojson::array &images, const std::string &baseDir) const
		{
			size_t p = imgs.size();
			imgs.resize(p + images.size());

			for (const auto &image : images)
			{
				const auto &fields = image.get<picojson::object>();
				auto fn = fields.at("uri").get<std::string>();
				assert(getExtension(fn) == "dds");

				fn = baseDir + "/" + fn;
				gli::texture2d tex2D(gli::load(fn.c_str()));

				assert(tex2D.format() == gli::FORMAT_RGBA8_UNORM_PACK8);

				auto &img = imgs[p++];
				img.width = tex2D.extent().x;
				img.height = tex2D.extent().y;
				img.component = 4; // must be RGBA8 right now
				img.levelCount = tex2D.levels();
				img.pixels.resize(tex2D.size());
				memcpy(&img.pixels[0], tex2D.data(), tex2D.size());
			}
		}

		void parseTextures(std::vector<GLTFTexture> &ts, const picojson::array &textures) const
		{
			for (const auto &texture : textures)
			{
				const auto &fields = texture.get<picojson::object>();

				GLTFTexture t;
				t.sampler = static_cast<uint32_t>(fields.at("sampler").get<int64_t>());
				t.source = static_cast<uint32_t>(fields.at("source").get<int64_t>());

				ts.emplace_back(t);
			}
		}

		std::string getBaseDir(const std::string &fn) const
		{
			size_t pos = fn.find_last_of('/');
			if (pos == std::string::npos) return "";
			return fn.substr(0, pos);
		}

		std::string getExtension(const std::string &fn) const
		{
			size_t pos = fn.find_last_of('.');
			if (pos == std::string::npos) return "";
			return fn.substr(pos + 1);
		}

		void readEntireFile(std::vector<char> &content, const std::string &fn) const
		{
			std::ifstream fs(fn, std::ifstream::binary);
			if (!fs.is_open()) throw std::runtime_error("file: " + fn + " not found");

			fs.seekg(0, std::ifstream::end);
			size_t fileSize = static_cast<size_t>(fs.tellg());
			content.resize(fileSize);

			fs.seekg(0, std::ifstream::beg);
			fs.read(&content[0], static_cast<std::streamsize>(fileSize));
			fs.close();
		}
	};
}