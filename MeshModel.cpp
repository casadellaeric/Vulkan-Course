#include "MeshModel.h"

namespace VkCourse {

	MeshModel::MeshModel()
	{
	}

	MeshModel::MeshModel(const std::vector<Mesh>& meshList)
	{
		m_meshes = meshList;
		m_model = glm::mat4(1.f);
	}

	MeshModel::~MeshModel()
	{
	}

	std::vector<std::string> MeshModel::load_materials(const aiScene* scene)
	{
		// Create 1:1 sized list of textures
		std::vector<std::string> textureList(scene->mNumMaterials, "");

		// Copy file name if it exists
		for (size_t i = 0; i < scene->mNumMaterials; ++i)
		{
			aiMaterial* material{ scene->mMaterials[i] };
			
			// Get the diffuse texture
			if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				aiString path;
				if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == aiReturn_SUCCESS)
				{
					// Cut off any directory information already present
					size_t idx{ std::string(path.data).rfind("\\") };
					std::string fileName{ std::string(path.data).substr(idx + 1) };

					textureList[i] = fileName;
				}
			}
		}

		return textureList;
	}

	std::vector<Mesh> MeshModel::load_node(VkPhysicalDevice physicalDevice, VkDevice device, 
		VkQueue transferQueue, VkCommandPool transferCommandPool, 
		aiNode* node, const aiScene* scene, const std::vector<size_t>& materialsToTextures)
	{
		std::vector<Mesh> meshList{};

		// Go through each mesh at this node, create it and add to the list
		for (size_t i = 0; i < node->mNumMeshes; ++i)
		{
			// The scene contains all meshes and nodes contain references to those meshes
			meshList.push_back(load_mesh(physicalDevice, device, transferQueue, transferCommandPool,
				scene->mMeshes[node->mMeshes[i]], scene, materialsToTextures));
		}

		// Go through each children node, load it and append meshes to this node's list
		for (size_t i = 0; i < node->mNumChildren; ++i)
		{
			std::vector<Mesh> childMeshList{ load_node(physicalDevice, device, transferQueue, transferCommandPool,
				node->mChildren[i], scene, materialsToTextures) };
			meshList.insert(meshList.end(), childMeshList.begin(), childMeshList.end());
		}

		return meshList;
	}

	Mesh MeshModel::load_mesh(VkPhysicalDevice physicalDevice, VkDevice device, 
		VkQueue transferQueue, VkCommandPool transferCommandPool, 
		aiMesh* mesh, const aiScene* scene, const std::vector<size_t>& materialsToTextures)
	{
		std::vector<Vertex> vertices{};
		std::vector<uint32_t> indices{};

		vertices.resize(mesh->mNumVertices);

		// Copy each vertex to vertices vector
		for (size_t i = 0; i < mesh->mNumVertices; ++i)
		{
			vertices[i].position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
			
			if (mesh->mTextureCoords[0] != nullptr)
			{
				vertices[i].texCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
			}
			else 
			{
				vertices[i].texCoords = { 0.f, 0.f };
			}

			// (not in use by current shaders)
			//vertices[i].color = { 1.f, 1.f, 1.f };
		}

		// Iterate over indices and copy
		for (size_t i = 0; i < mesh->mNumFaces; ++i)
		{
			aiFace face{ mesh->mFaces[i] };
			for (size_t j = 0; j < face.mNumIndices; ++j)
			{
				indices.push_back(face.mIndices[j]);
			}
		}

		// Mesh
		return { physicalDevice, device, transferQueue, transferCommandPool, &vertices, &indices, materialsToTextures[mesh->mMaterialIndex] };
	}

	size_t MeshModel::get_mesh_count()
	{
		return m_meshes.size();
	}

	Mesh& MeshModel::get_mesh(size_t index)
	{
		if (index >= m_meshes.size())
		{
			throw std::runtime_error("Attempted to access invalid mesh index!");
		}
		return m_meshes[index];
	}

	glm::mat4& MeshModel::get_model_matrix()
	{
		return m_model;
	}

	void MeshModel::set_model(const glm::mat4& model)
	{
		m_model = model;
	}

	void MeshModel::destroy_mesh_model()
	{
		for (auto& mesh : m_meshes)
		{
			mesh.destroy_buffers();
		}
	}

}