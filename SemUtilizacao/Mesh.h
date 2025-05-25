// Mesh.h
#pragma once // Garante que este arquivo de cabeçalho seja incluído apenas uma vez

#include <string> // Para usar std::string
#include <vector> // Para usar std::vector

#include <GLFW/glfw3.h> // Inclui a biblioteca GLFW para tipos como GLuint, GLfloat
#include <glm/glm.hpp>	// Inclui a biblioteca GLM para tipos como glm::vec3

#include "Shader.h"		// Inclui a definição da classe Shader
#include "Vertex.h"		// Inclui a definição da struct Vertex
#include "Material.h" // Inclui a definição da struct Material

// Declaração da classe Mesh, que representa um objeto 3D carregado de um arquivo
class Mesh
{
public:
	// Atributos de transformação da malha
	glm::vec3 scale;		// Vetor de escala (x, y, z)
	glm::vec3 position; // Vetor de posição no espaço 3D (x, y, z)
	glm::vec3 rotation; // Vetor que define o eixo de rotação
	glm::vec3 angle;		// Vetor de ângulos de rotação (em graus, em torno dos eixos x, y, z)

	// Caminhos para os arquivos de dados da malha
	std::string objFilePath;		// Caminho para o arquivo .obj (geometria)
	std::string mtlFilePath;		// Caminho para o arquivo .mtl (material)
	std::string *pathToTexture; // Ponteiro para o caminho do arquivo de textura (obtido do material)

	// Atributos OpenGL
	GLuint VAO;										// Identificador do Vertex Array Object
	std::vector<Vertex> vertices; // Vetor contendo os dados de todos os vértices da malha
	Material material;						// Objeto Material contendo as propriedades de material da malha
	GLuint textureId;							// Identificador da textura OpenGL
	Shader *shader;								// Ponteiro para o objeto Shader usado para renderizar esta malha

	// Protótipo para obter os dados dos vértices como um array de float
	GLfloat *getVerticesArrayPointer(std::vector<Vertex> vertices);

public: // Métodos públicos da classe Mesh
	// Métodos de configuração (setup)
	std::vector<Vertex> setupVertices(); // Carrega os vértices do arquivo .obj
	GLuint setupVAO();									 // Configura o Vertex Array Object e Vertex Buffer Object
	Material setupMaterial();						 // Carrega as propriedades do material do arquivo .mtl
	GLuint setupTexture();							 // Carrega e configura a textura

	// Método de inicialização principal da malha
	void initialize(Shader *shader, std::string objFilePath, std::string mtlFilePath, glm::vec3 scale, glm::vec3 position, glm::vec3 rotation, glm::vec3 angle);

	// Métodos de ciclo de vida da renderização
	void render();								 // Desenha/renderiza a malha
	void updateModel();						 // Atualiza a matriz de modelo da malha (transformações)
	void updateMaterialUniforms(); // Envia as propriedades do material para o shader
	void deleteVAO();							 // Deleta o VAO da malha, liberando recursos

	// Getters
	std::vector<Vertex> getVertices() { return this->vertices; } // Retorna uma cópia do vetor de vértices

	// Setters para modificar os atributos de transformação e shader
	void setScale(glm::vec3 scale) { this->scale = scale; }							// Define a escala
	void setPosition(glm::vec3 position) { this->position = position; } // Define a posição
	void setRotation(glm::vec3 rotation) { this->rotation = rotation; } // Define o eixo de rotação
	void setAngle(glm::vec3 angle) { this->angle = angle; }							// Define os ângulos de rotação
	void setShader(Shader *shader) { this->shader = shader; }						// Define o shader

	// Método para imprimir informações da malha (para depuração)
	void toString();
};