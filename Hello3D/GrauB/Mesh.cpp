#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../Common/include/stb_image.h"
#include "Mesh.h"
#include "Vertex.h"
#include "Material.h"

void Mesh::initialize(Shader *shader, std::string objFilePath, std::string mtlFilePath, glm::vec3 scale, glm::vec3 position, glm::vec3 rotation, glm::vec3 angle)
{
	this->shader = shader;

	this->objFilePath = objFilePath;
	this->mtlFilePath = mtlFilePath;

	this->scale = scale;
	this->position = position;
	this->rotation = rotation;
	this->angle = angle;
	this->pathToTexture = &material.pathToTexture;
}

void Mesh::render()
{
	glBindVertexArray(this->VAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, this->textureId);
	glDrawArrays(GL_TRIANGLES, 0, this->vertices.size());
	glBindVertexArray(0);
}

void Mesh::updateModel()
{
	glm::mat4 model = glm::mat4(1);

	model = glm::translate(model, this->position);

	model = glm::rotate(model, glm::radians(this->angle.x), this->rotation);
	model = glm::rotate(model, glm::radians(this->angle.y), this->rotation);
	model = glm::rotate(model, glm::radians(this->angle.z), this->rotation);

	model = glm::scale(model, this->scale);

	glUniformMatrix4fv(glGetUniformLocation(shader->getId(), "model"), 1, FALSE, glm::value_ptr(model));
}

void Mesh::updateMaterialUniforms()
{
	glUniform1f(glGetUniformLocation(shader->getId(), "kaR"), this->material.kaR);
	glUniform1f(glGetUniformLocation(shader->getId(), "kaG"), this->material.kaG);
	glUniform1f(glGetUniformLocation(shader->getId(), "kaB"), this->material.kaB);
	glUniform1f(glGetUniformLocation(shader->getId(), "kdR"), this->material.kdR);
	glUniform1f(glGetUniformLocation(shader->getId(), "kdG"), this->material.kdG);
	glUniform1f(glGetUniformLocation(shader->getId(), "kdB"), this->material.kdB);
	glUniform1f(glGetUniformLocation(shader->getId(), "ksR"), this->material.ksR);
	glUniform1f(glGetUniformLocation(shader->getId(), "ksG"), this->material.ksG);
	glUniform1f(glGetUniformLocation(shader->getId(), "ksB"), this->material.ksB);
	glUniform1f(glGetUniformLocation(shader->getId(), "ns"), this->material.ns);
}

void Mesh::deleteVAO()
{
	glDeleteVertexArrays(1, &this->VAO);
}

std::vector<Vertex> Mesh::setupVertices()
{
	std::vector<Vertex> vertices;
	std::ifstream file(objFilePath);
	std::string line;
	std::vector<glm::vec3> tempPositions;
	std::vector<glm::vec2> tempTexcoords;
	std::vector<glm::vec3> tempNormals;

	if (!file.is_open())
	{
		std::cerr << "Failed to open file " << objFilePath << std::endl;
		return vertices;
	}

	while (getline(file, line))
	{
		std::istringstream ss(line);
		std::string type;
		ss >> type;

		if (type == "v")
		{
			glm::vec3 position;
			ss >> position.x >> position.y >> position.z;
			tempPositions.push_back(position);
		}
		else if (type == "vt")
		{
			glm::vec2 texcoord;
			ss >> texcoord.x >> texcoord.y;
			tempTexcoords.push_back(texcoord);
		}
		else if (type == "vn")
		{
			glm::vec3 normal;
			ss >> normal.x >> normal.y >> normal.z;
			tempNormals.push_back(normal);
		}
		else if (type == "f")
		{
			std::string vertex1, vertex2, vertex3;
			ss >> vertex1 >> vertex2 >> vertex3;
			int vIndex[3], tIndex[3], nIndex[3];

			for (int i = 0; i < 3; i++)
			{
				std::string vertex = (i == 0) ? vertex1 : (i == 1) ? vertex2
																													 : vertex3;
				size_t pos1 = vertex.find('/');
				size_t pos2 = vertex.find('/', pos1 + 1);

				vIndex[i] = std::stoi(vertex.substr(0, pos1)) - 1;
				tIndex[i] = std::stoi(vertex.substr(pos1 + 1, pos2 - pos1 - 1)) - 1;
				nIndex[i] = std::stoi(vertex.substr(pos2 + 1)) - 1;
			}

			for (int i = 0; i < 3; i++)
			{
				Vertex vertex;
				vertex.x = tempPositions[vIndex[i]].x;
				vertex.y = tempPositions[vIndex[i]].y;
				vertex.z = tempPositions[vIndex[i]].z;

				vertex.s = tempTexcoords[tIndex[i]].x;
				vertex.t = tempTexcoords[tIndex[i]].y;

				vertex.nx = tempNormals[nIndex[i]].x;
				vertex.ny = tempNormals[nIndex[i]].y;
				vertex.nz = tempNormals[nIndex[i]].z;

				vertices.push_back(vertex);
			}
		}
	}

	file.close();
	this->vertices = vertices;
	return vertices;
}

GLuint Mesh::setupVAO()
{
	GLuint VBO, VAO;

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// Positioning
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)0);
	glEnableVertexAttribArray(0);

	// Texture
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	// Color
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)(5 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	// Normal
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)(8 * sizeof(GLfloat)));
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	this->VAO = VAO;
	return VAO;
}

Material Mesh::setupMaterial()
{
	Material material;
	std::string texturePath;
	std::ifstream file(mtlFilePath);
	std::string line;

	if (!file.is_open())
	{
		std::cerr << "Failed to open file " << mtlFilePath << std::endl;
		return material;
	}

	while (getline(file, line))
	{
		std::istringstream ss(line);
		std::string type;
		ss >> type;

		if (type == "Ka")
		{
			ss >> material.kaR >> material.kaG >> material.kaB;
		}
		else if (type == "Kd")
		{
			ss >> material.kdR >> material.kdG >> material.kdB;
		}
		else if (type == "Ks")
		{
			ss >> material.ksR >> material.ksG >> material.ksB;
		}
		else if (type == "Ns")
		{
			ss >> material.ns;
		}
		else if (type == "map_Kd")
		{
			ss >> material.pathToTexture;
		}
	}
	this->material = material;
	std::cout << material.pathToTexture;
	file.close();
	return material;
}

GLuint Mesh::setupTexture()
{
	GLuint textureId;

	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_set_flip_vertically_on_load(true);

	int width, height, nrChannels;
	unsigned char *data = stbi_load(pathToTexture->c_str(), &width, &height, &nrChannels, 0);

	if (data)
	{
		// jpg, bmp
		if (nrChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		// png
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture " << std::endl;
	}
	this->textureId = textureId;
	stbi_image_free(data);

	return textureId;
}

GLfloat *Mesh::getVerticesArrayPointer(std::vector<Vertex> vertices)
{
	std::vector<GLfloat> tempVector;

	for (Vertex vertex : vertices)
	{
		tempVector.push_back(vertex.x);
		tempVector.push_back(vertex.y);
		tempVector.push_back(vertex.z);
		tempVector.push_back(vertex.s);
		tempVector.push_back(vertex.t);
		tempVector.push_back(vertex.r);
		tempVector.push_back(vertex.g);
		tempVector.push_back(vertex.b);
		tempVector.push_back(vertex.nx);
		tempVector.push_back(vertex.ny);
		tempVector.push_back(vertex.nz);
	}

	return tempVector.data();
}

void Mesh::toString()
{
	std::cout
			<< "Scale: " << this->scale.x << " " << this->scale.y << " " << this->scale.z << " " << "\n"
			<< "Position: " << this->position.x << " " << this->position.y << " " << this->position.z << " " << "\n"
			<< "Rotation: " << this->position.x << " " << this->position.y << " " << this->position.z << " " << "\n"
			<< "Angle: " << this->angle.x << " " << this->angle.y << " " << this->angle.z << " " << "\n";
}