#pragma once

#include <GLFW/glfw3.h>

// Um struct auxiliar para facilitar a criacao dos floats com os atributos dos vertices
struct Vertex
{
	GLfloat x, y, z;		// Positioning
	GLfloat s, t;				// Texture
	GLfloat r, g, b;		// Color
	GLfloat nx, ny, nz; // Normal
};