#pragma once

#include <fstream>

#include "picojson.h"
#include "gli/gli.hpp"

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

	struct GLTFScene
	{
		std::vector<GLTFMesh> meshes;
	};

	class GLTFLoader
	{
	public:
		void load(GLTFScene *scene, const std::string &fn)
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
			parseBuffers(buffers, rootNode.get("buffers").get<picojson::array>());

			// Parse images
		}

	private:
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

		void parseBufferViews(std::vector<GLTFBufferView> &bvs, const picojson::array &bufferViews)
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

		void parseBuffers(std::vector<GLTFBuffer> &bs, const picojson::array &buffers)
		{
			size_t p = bs.size();
			bs.resize(bs.size() + buffers.size());

			for (const auto &buffer : buffers)
			{
				const auto &fields = buffer.get<picojson::object>();

				auto &b = bs[p++];
				readEntireFile(b, fields.at("uri").get<std::string>());
				if (b.size() != static_cast<size_t>(fields.at("byteLength").get<int64_t>())) throw std::runtime_error("Incorrect buffer byte length");
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
			std::ifstream fs(fn);
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