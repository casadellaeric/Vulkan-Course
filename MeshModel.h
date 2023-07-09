#pragma once
#include "Mesh.h"

#include <glm/glm.hpp>

#pragma warning( push )
#pragma warning( disable : 26451 )
#include <assimp/scene.h>
#pragma warning( pop )

#include <vector>

namespace VkCourse {

	class MeshModel
	{
	public:
		MeshModel();
		MeshModel(const std::vector<Mesh>& meshList);

		~MeshModel();

		// -- GETTERS/SETTERS --
		size_t get_mesh_count();
		Mesh& get_mesh(size_t index);

		glm::mat4& get_model_matrix();
		void set_model(const glm::mat4& model);

		void destroy_mesh_model();
		// --				  --

		static std::vector<std::string> load_materials(const aiScene* scene);

		static std::vector<Mesh> load_node(VkPhysicalDevice physicalDevice, VkDevice device,
			VkQueue transferQueue, VkCommandPool transferCommandPool,
			aiNode* node, const aiScene* scene, const std::vector<size_t>& materialsToTextures);

		static Mesh load_mesh(VkPhysicalDevice physicalDevice, VkDevice device,
			VkQueue transferQueue, VkCommandPool transferCommandPool,
			aiMesh* mesh, const aiScene* scene, const std::vector<size_t>& materialsToTextures);


	private:
		std::vector<Mesh> m_meshes{};
		glm::mat4 m_model;
	};

}