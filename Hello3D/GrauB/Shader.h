// Shader.h
#pragma once // Garante que este arquivo de cabeçalho seja incluído apenas uma vez durante a compilação

#include <string> // Necessário para usar std::string

#include <glad/glad.h> // Inclui a biblioteca GLAD para funcionalidades OpenGL (como GLuint)

// Declaração da classe Shader
class Shader
{
private:
	GLuint id; // Membro privado para armazenar o ID do programa shader OpenGL

public:
	// Construtor que recebe os caminhos para os arquivos de vertex e fragment shader
	Shader(std::string vertexShaderPath, std::string fragmentShaderPath);

	// Método para configurar a uniforme de textura no shader (para a unidade de textura 0)
	void setTextureUniform();

	// Método getter para obter o ID do programa shader
	GLuint getId() { return id; }
};