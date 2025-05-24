// Material.h
#pragma once // Garante que este arquivo de cabeçalho seja incluído apenas uma vez durante a compilação

#include <string> // Necessário para usar std::string

#include <GLFW/glfw3.h> // Inclui a biblioteca GLFW para funcionalidades relacionadas a OpenGL

// Um struct auxiliar que agrupa os atributos de luz e de textura de um material
struct Material
{
	GLfloat kaR, kaG, kaB;		 // Ka: Componente ambiente da cor do material (Vermelho, Verde, Azul)
	GLfloat kdR, kdG, kdB;		 // Kd: Componente difusa da cor do material (Vermelho, Verde, Azul)
	GLfloat ksR, ksG, ksB;		 // Ks: Componente especular da cor do material (Vermelho, Verde, Azul)
	GLfloat ns;								 // Ns: Expoente especular (brilho)
	std::string pathToTexture; // map_Kd: Caminho para o arquivo de textura associado ao material
};