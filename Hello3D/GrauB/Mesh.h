#pragma once

#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Shader.h"
#include "Vertex.h"
#include "Material.h"

class Mesh {
public:
	glm::vec3 scale;
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 angle;
	std::string objFilePath;
	std::string mtlFilePath;
	std::string* pathToTexture;

	GLuint VAO;
	std::vector<Vertex> vertices;
	Material material;
	GLuint textureId;
	Shader* shader;

	GLfloat* getVerticesArrayPointer(std::vector<Vertex> vertices);
public:
	std::vector<Vertex> setupVertices();
	GLuint setupVAO();
	Material setupMaterial();
	GLuint setupTexture();

	void initialize(Shader* shader, std::string objFilePath, std::string mtlFilePath, glm::vec3 scale, glm::vec3 position, glm::vec3 rotation, glm::vec3 angle);
	void render();
	void updateModel();
	void updateMaterialUniforms();
	void deleteVAO();

	std::vector<Vertex> getVertices() { return this->vertices; }
	void setScale(glm::vec3 scale) { this->scale = scale; }
	void setPosition(glm::vec3 position) { this->position = position; }
	void setRotation(glm::vec3 rotation) { this->rotation = rotation; }
	void setAngle(glm::vec3 angle) { this->angle = angle; }
	void setShader(Shader* shader) { this->shader = shader; }

	void toString();
};
