#pragma once

#include <string>

#include <GLFW/glfw3.h>

// Um struct auxiliar que agrupa os atributos de luz e de textura
struct Material {
	GLfloat kaR, kaG, kaB;   // Ka
	GLfloat kdR, kdG, kdB;   // Kd
	GLfloat ksR, ksG, ksB;   // Ks
	GLfloat ns;
	std::string pathToTexture; // map_Kd
};