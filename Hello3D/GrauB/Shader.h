#pragma once

#include <string>

#include <glad/glad.h>

class Shader {
private:
	GLuint id;

public:
	Shader(std::string vertexShaderPath, std::string fragmentShaderPath);

	void setTextureUniform();

	GLuint getId() { return id; }
};