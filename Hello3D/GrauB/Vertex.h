// Vertex.h
#pragma once // Garante que este arquivo de cabeçalho seja incluído apenas uma vez

#include <GLFW/glfw3.h> // Inclui a biblioteca GLFW para tipos como GLfloat

// Um struct auxiliar para facilitar a criação dos floats com os atributos dos vértices
// Esta struct define a estrutura de dados para um único vértice.
struct Vertex
{
	GLfloat x, y, z;		// Atributos de Posição: coordenadas X, Y, Z do vértice no espaço 3D
	GLfloat s, t;				// Atributos de Textura: coordenadas S, T (ou U, V) para mapeamento de textura
											// 's' corresponde à coordenada horizontal da textura, 't' à vertical.
	GLfloat r, g, b;		// Atributos de Cor: componentes Vermelho, Verde, Azul da cor do vértice
											// Pode ser usado para colorir o vértice diretamente, ou modulado com uma textura.
	GLfloat nx, ny, nz; // Atributos de Normal: componentes X, Y, Z do vetor normal do vértice
											// O vetor normal é perpendicular à superfície no ponto do vértice e é usado para cálculos de iluminação.
};