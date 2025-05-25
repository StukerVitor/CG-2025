// Mesh.cpp
#include <vector>		// Para usar std::vector (arrays dinâmicos)
#include <string>		// Para usar std::string
#include <fstream>	// Para operações de entrada/saída de arquivos (ifstream)
#include <sstream>	// Para usar std::istringstream (streams de string)
#include <iostream> // Para entrada/saída padrão (std::cout, std::cerr)

#include <glad/glad.h>									// Biblioteca GLAD para carregar ponteiros de funções OpenGL. Deve ser incluído antes de GLFW.
#include <glm/glm.hpp>									// Biblioteca GLM para matemática gráfica (vetores, matrizes)
#include <glm/gtc/matrix_transform.hpp> // Para transformações de matriz (translate, rotate, scale)
#include <glm/gtc/type_ptr.hpp>					// Para converter tipos GLM para ponteiros que a OpenGL possa usar

#include "../../Common/include/stb_image.h" // Biblioteca para carregamento de imagens (texturas)
#include "Mesh.h"														// Inclui o arquivo de cabeçalho da classe Mesh
#include "Vertex.h"													// Inclui a definição da struct Vertex
#include "Material.h"												// Inclui a definição da struct Material

// Método para inicializar os atributos da malha (Mesh)
void Mesh::initialize(Shader *shader, std::string objFilePath, std::string mtlFilePath, glm::vec3 scale, glm::vec3 position, glm::vec3 rotation, glm::vec3 angle)
{
	this->shader = shader; // Define o shader que será usado para renderizar esta malha

	this->objFilePath = objFilePath; // Define o caminho para o arquivo .obj (geometria)
	this->mtlFilePath = mtlFilePath; // Define o caminho para o arquivo .mtl (material)

	this->scale = scale;													 // Define o vetor de escala da malha
	this->position = position;										 // Define o vetor de posição da malha
	this->rotation = rotation;										 // Define o eixo de rotação da malha
	this->angle = angle;													 // Define os ângulos de rotação em torno dos eixos X, Y, Z
	this->pathToTexture = &material.pathToTexture; // Define um ponteiro para o caminho da textura do material da malha
}

// Método para renderizar a malha
void Mesh::render()
{
	glBindVertexArray(this->VAO);													// Ativa o Vertex Array Object (VAO) da malha
	glActiveTexture(GL_TEXTURE0);													// Ativa a unidade de textura 0
	glBindTexture(GL_TEXTURE_2D, this->textureId);				// Vincula a textura da malha à unidade de textura ativa
	glDrawArrays(GL_TRIANGLES, 0, this->vertices.size()); // Desenha os triângulos da malha. O número de vértices é dado por this->vertices.size()
	glBindVertexArray(0);																	// Desativa o VAO
}

// Método para atualizar a matriz de modelo (model matrix) da malha e enviá-la ao shader
void Mesh::updateModel()
{
	glm::mat4 model = glm::mat4(1); // Cria uma matriz de modelo identidade (4x4)

	model = glm::translate(model, this->position); // Aplica a translação à matriz de modelo

	// Aplica as rotações à matriz de modelo em torno dos eixos X, Y e Z
	// glm::radians converte ângulos de graus para radianos
	model = glm::rotate(model, glm::radians(this->angle.x), this->rotation); // Rotação em torno do eixo X (usando 'this->rotation' como eixo, o que pode ser um erro se a intenção era rotar em X, Y, Z globais/locais)
	model = glm::rotate(model, glm::radians(this->angle.y), this->rotation); // Rotação em torno do eixo Y
	model = glm::rotate(model, glm::radians(this->angle.z), this->rotation); // Rotação em torno do eixo Z

	model = glm::scale(model, this->scale); // Aplica a escala à matriz de modelo

	// Envia a matriz de modelo atualizada para o shader
	// glGetUniformLocation obtém a localização da variável uniforme "model" no shader
	// glUniformMatrix4fv envia os dados da matriz (4x4, float) para essa localização
	glUniformMatrix4fv(glGetUniformLocation(shader->getId(), "model"), 1, FALSE, glm::value_ptr(model));
}

// Método para atualizar as uniformes do material no shader
void Mesh::updateMaterialUniforms()
{
	// Envia cada componente do material (Ka, Kd, Ks, Ns) para as uniformes correspondentes no shader
	glUniform1f(glGetUniformLocation(shader->getId(), "kaR"), this->material.kaR); // Componente ambiente vermelha
	glUniform1f(glGetUniformLocation(shader->getId(), "kaG"), this->material.kaG); // Componente ambiente verde
	glUniform1f(glGetUniformLocation(shader->getId(), "kaB"), this->material.kaB); // Componente ambiente azul
	glUniform1f(glGetUniformLocation(shader->getId(), "kdR"), this->material.kdR); // Componente difusa vermelha
	glUniform1f(glGetUniformLocation(shader->getId(), "kdG"), this->material.kdG); // Componente difusa verde
	glUniform1f(glGetUniformLocation(shader->getId(), "kdB"), this->material.kdB); // Componente difusa azul
	glUniform1f(glGetUniformLocation(shader->getId(), "ksR"), this->material.ksR); // Componente especular vermelha
	glUniform1f(glGetUniformLocation(shader->getId(), "ksG"), this->material.ksG); // Componente especular verde
	glUniform1f(glGetUniformLocation(shader->getId(), "ksB"), this->material.ksB); // Componente especular azul
	glUniform1f(glGetUniformLocation(shader->getId(), "ns"), this->material.ns);	 // Expoente especular (brilho)
}

// Método para deletar o Vertex Array Object (VAO) da malha
void Mesh::deleteVAO()
{
	glDeleteVertexArrays(1, &this->VAO); // Deleta o VAO, liberando recursos da GPU
}

// Método para carregar e configurar os vértices a partir de um arquivo .obj
std::vector<Vertex> Mesh::setupVertices()
{
	std::vector<Vertex> vertices;					// Vetor para armazenar os vértices finais da malha
	std::ifstream file(objFilePath);			// Abre o arquivo .obj para leitura
	std::string line;											// String para armazenar cada linha lida do arquivo
	std::vector<glm::vec3> tempPositions; // Vetor para armazenar temporariamente as posições dos vértices (v)
	std::vector<glm::vec2> tempTexcoords; // Vetor para armazenar temporariamente as coordenadas de textura (vt)
	std::vector<glm::vec3> tempNormals;		// Vetor para armazenar temporariamente os vetores normais (vn)

	// Verifica se o arquivo foi aberto com sucesso
	if (!file.is_open())
	{
		std::cerr << "Failed to open file " << objFilePath << std::endl; // Exibe mensagem de erro
		return vertices;																								 // Retorna um vetor de vértices vazio
	}

	// Lê o arquivo linha por linha
	while (getline(file, line))
	{
		std::istringstream ss(line); // Cria um stream de string a partir da linha lida
		std::string type;						 // String para armazenar o tipo de dado na linha (v, vt, vn, f)
		ss >> type;									 // Lê o tipo de dado

		// Se a linha define uma posição de vértice (v)
		if (type == "v")
		{
			glm::vec3 position;														// Cria um vetor de 3 floats para a posição
			ss >> position.x >> position.y >> position.z; // Lê as coordenadas x, y, z
			tempPositions.push_back(position);						// Adiciona a posição ao vetor temporário
		}
		// Se a linha define uma coordenada de textura (vt)
		else if (type == "vt")
		{
			glm::vec2 texcoord;								 // Cria um vetor de 2 floats para a coordenada de textura
			ss >> texcoord.x >> texcoord.y;		 // Lê as coordenadas s, t (ou u, v)
			tempTexcoords.push_back(texcoord); // Adiciona a coordenada de textura ao vetor temporário
		}
		// Se a linha define um vetor normal (vn)
		else if (type == "vn")
		{
			glm::vec3 normal;												// Cria um vetor de 3 floats para o normal
			ss >> normal.x >> normal.y >> normal.z; // Lê as coordenadas nx, ny, nz
			tempNormals.push_back(normal);					// Adiciona o normal ao vetor temporário
		}
		// Se a linha define uma face (f)
		else if (type == "f")
		{
			std::string vertex1, vertex2, vertex3; // Strings para armazenar os dados de cada vértice da face
			ss >> vertex1 >> vertex2 >> vertex3;	 // Lê os dados dos três vértices da face (formato v/t/n)
			int vIndex[3], tIndex[3], nIndex[3];	 // Arrays para armazenar os índices de posição, textura e normal para cada vértice da face

			// Processa cada um dos três vértices da face
			for (int i = 0; i < 3; i++)
			{
				std::string vertexData = (i == 0) ? vertex1 : (i == 1) ? vertex2
																															 : vertex3; // Seleciona os dados do vértice atual
				size_t pos1 = vertexData.find('/');																// Encontra a primeira barra '/'
				size_t pos2 = vertexData.find('/', pos1 + 1);											// Encontra a segunda barra '/'

				// Extrai os índices de posição, textura e normal e converte para inteiros
				// Os índices em arquivos .obj são baseados em 1, então subtraímos 1 para torná-los baseados em 0
				vIndex[i] = std::stoi(vertexData.substr(0, pos1)) - 1;
				tIndex[i] = std::stoi(vertexData.substr(pos1 + 1, pos2 - pos1 - 1)) - 1;
				nIndex[i] = std::stoi(vertexData.substr(pos2 + 1)) - 1;
			}

			// Cria os objetos Vertex para cada vértice da face e os adiciona ao vetor 'vertices'
			for (int i = 0; i < 3; i++)
			{
				Vertex vertex; // Cria um novo objeto Vertex
				// Define a posição do vértice usando o índice correspondente
				vertex.x = tempPositions[vIndex[i]].x;
				vertex.y = tempPositions[vIndex[i]].y;
				vertex.z = tempPositions[vIndex[i]].z;

				// Define as coordenadas de textura do vértice usando o índice correspondente
				vertex.s = tempTexcoords[tIndex[i]].x;
				vertex.t = tempTexcoords[tIndex[i]].y;

				// Define o vetor normal do vértice usando o índice correspondente
				vertex.nx = tempNormals[nIndex[i]].x;
				vertex.ny = tempNormals[nIndex[i]].y;
				vertex.nz = tempNormals[nIndex[i]].z;

				vertices.push_back(vertex); // Adiciona o vértice configurado ao vetor final
			}
		}
	}

	file.close();							 // Fecha o arquivo .obj
	this->vertices = vertices; // Armazena os vértices carregados no membro 'vertices' da classe
	return vertices;					 // Retorna o vetor de vértices
}

// Método para configurar o Vertex Array Object (VAO) e o Vertex Buffer Object (VBO)
GLuint Mesh::setupVAO()
{
	GLuint VBO, VAO; // Declara identificadores para VBO e VAO

	glGenBuffers(1, &VBO);							// Gera 1 buffer e armazena seu ID em VBO
	glBindBuffer(GL_ARRAY_BUFFER, VBO); // Vincula o VBO como o buffer de array atual
	// Envia os dados dos vértices para o VBO vinculado
	// vertices.size() * sizeof(Vertex) é o tamanho total dos dados em bytes
	// &vertices[0] é um ponteiro para o início dos dados dos vértices
	// GL_STATIC_DRAW indica que os dados serão modificados raramente
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO); // Gera 1 VAO e armazena seu ID em VAO
	glBindVertexArray(VAO);			// Vincula o VAO como o VAO ativo

	// Configura os atributos dos vértices no VAO

	// Atributo de Posição (layout location = 0 no shader)
	// 0: índice do atributo
	// 3: número de componentes por atributo (x, y, z)
	// GL_FLOAT: tipo de dado dos componentes
	// GL_FALSE: não normalizar os dados
	// sizeof(Vertex): stride - tamanho de um vértice completo em bytes
	// (GLvoid*)0: offset - deslocamento do primeiro componente deste atributo no início do vértice
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)0);
	glEnableVertexAttribArray(0); // Habilita o atributo de vértice no índice 0

	// Atributo de Textura (layout location = 1 no shader)
	// 1: índice do atributo
	// 2: número de componentes por atributo (s, t)
	// GL_FLOAT: tipo de dado
	// GL_FALSE: não normalizar
	// sizeof(Vertex): stride
	// (GLvoid*)(3 * sizeof(GLfloat)): offset - deslocamento após os 3 floats da posição (x, y, z)
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1); // Habilita o atributo de vértice no índice 1

	// Atributo de Cor (layout location = 2 no shader)
	// Se a cor não vier do .obj, este atributo pode não ter dados significativos ou usar valores padrão.
	// 2: índice do atributo
	// 3: número de componentes por atributo (r, g, b)
	// GL_FLOAT: tipo de dado
	// GL_FALSE: não normalizar
	// sizeof(Vertex): stride
	// (GLvoid*)(5 * sizeof(GLfloat)): offset - deslocamento após os 3 floats da posição e 2 floats da textura
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)(5 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2); // Habilita o atributo de vértice no índice 2

	// Atributo Normal (layout location = 3 no shader)
	// 3: índice do atributo
	// 3: número de componentes por atributo (nx, ny, nz)
	// GL_FLOAT: tipo de dado
	// GL_FALSE: não normalizar
	// sizeof(Vertex): stride
	// (GLvoid*)(8 * sizeof(GLfloat)): offset - deslocamento após posição (3), textura (2) e cor (3) (assumindo que a cor tem 3 floats)
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)(8 * sizeof(GLfloat))); // Errata: O offset deveria ser (5 * sizeof(GLfloat)) se a cor não estivesse sendo usada ou (5+3 = 8) se a cor fosse usada. O struct Vertex.h mostra r,g,b.
	glEnableVertexAttribArray(3);																																			// Habilita o atributo de vértice no índice 3

	glBindBuffer(GL_ARRAY_BUFFER, 0); // Desvincula o VBO (o VAO ainda mantém a referência ao VBO)
	glBindVertexArray(0);							// Desvincula o VAO

	this->VAO = VAO; // Armazena o ID do VAO no membro da classe
	return VAO;			 // Retorna o ID do VAO
}

// Método para carregar e configurar o material a partir de um arquivo .mtl
Material Mesh::setupMaterial()
{
	Material material;							 // Cria um objeto Material para armazenar os dados lidos
	std::string texturePath;				 // String para armazenar o caminho da textura (não usada diretamente aqui, mas o material.pathToTexture sim)
	std::ifstream file(mtlFilePath); // Abre o arquivo .mtl para leitura
	std::string line;								 // String para armazenar cada linha lida do arquivo

	// Verifica se o arquivo foi aberto com sucesso
	if (!file.is_open())
	{
		std::cerr << "Failed to open file " << mtlFilePath << std::endl; // Exibe mensagem de erro
		return material;																								 // Retorna um objeto Material padrão/vazio
	}

	// Lê o arquivo linha por linha
	while (getline(file, line))
	{
		std::istringstream ss(line); // Cria um stream de string a partir da linha lida
		std::string type;						 // String para armazenar o tipo de propriedade do material (Ka, Kd, Ks, Ns, map_Kd)
		ss >> type;									 // Lê o tipo de propriedade

		// Se a linha define a componente ambiente (Ka)
		if (type == "Ka")
		{
			ss >> material.kaR >> material.kaG >> material.kaB; // Lê os valores R, G, B da componente ambiente
		}
		// Se a linha define a componente difusa (Kd)
		else if (type == "Kd")
		{
			ss >> material.kdR >> material.kdG >> material.kdB; // Lê os valores R, G, B da componente difusa
		}
		// Se a linha define a componente especular (Ks)
		else if (type == "Ks")
		{
			ss >> material.ksR >> material.ksG >> material.ksB; // Lê os valores R, G, B da componente especular
		}
		// Se a linha define o expoente especular (Ns)
		else if (type == "Ns")
		{
			ss >> material.ns; // Lê o valor do expoente especular
		}
		// Se a linha define o mapa de textura difusa (map_Kd)
		else if (type == "map_Kd")
		{
			ss >> material.pathToTexture; // Lê o caminho para o arquivo de textura
		}
	}
	this->material = material;					 // Armazena o material carregado no membro da classe
	std::cout << material.pathToTexture; // Imprime o caminho da textura (para depuração)
	file.close();												 // Fecha o arquivo .mtl
	return material;										 // Retorna o material carregado
}

// Método para configurar a textura
GLuint Mesh::setupTexture()
{
	GLuint textureId; // Declara um identificador para a textura

	glGenTextures(1, &textureId);						 // Gera 1 textura e armazena seu ID em textureId
	glBindTexture(GL_TEXTURE_2D, textureId); // Vincula a textura como a textura 2D ativa

	// Define os parâmetros de empacotamento (wrapping) da textura
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Repete a textura na coordenada S (equivalente a X ou U)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // Repete a textura na coordenada T (equivalente a Y ou V)
	// Define os parâmetros de filtragem da textura
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Filtragem linear para minificação (quando a textura é menor na tela)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Filtragem linear para magnificação (quando a textura é maior na tela)

	stbi_set_flip_vertically_on_load(true); // Instrui a stb_image a inverter a imagem verticalmente ao carregar (comum porque OpenGL espera coordenadas de textura com origem no canto inferior esquerdo)

	int width, height, nrChannels; // Variáveis para armazenar largura, altura e número de canais da imagem
	// Carrega a imagem da textura do arquivo usando stb_image
	// pathToTexture é um ponteiro para string, então ->c_str() é usado
	unsigned char *data = stbi_load(pathToTexture->c_str(), &width, &height, &nrChannels, 0);

	// Verifica se os dados da imagem foram carregados com sucesso
	if (data)
	{
		// Se a imagem tem 3 canais (RGB - ex: JPG, BMP)
		if (nrChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		// Se a imagem tem 4 canais (RGBA - ex: PNG)
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D); // Gera mipmaps para a textura (melhora a qualidade e performance em objetos distantes)
	}
	else
	{
		std::cout << "Failed to load texture " << std::endl; // Exibe mensagem de erro se o carregamento falhar
	}
	this->textureId = textureId; // Armazena o ID da textura no membro da classe
	stbi_image_free(data);			 // Libera a memória alocada por stbi_load para os dados da imagem

	return textureId; // Retorna o ID da textura
}

// Método para obter um ponteiro para um array de GLfloat contendo todos os dados dos vértices
// Este método não é usado nas outras partes do código fornecido e pode ser para depuração ou uma funcionalidade legada/alternativa.
GLfloat *Mesh::getVerticesArrayPointer(std::vector<Vertex> vertices)
{
	std::vector<GLfloat> tempVector; // Vetor temporário para armazenar os floats

	// Itera sobre cada vértice no vetor de entrada
	for (Vertex vertex : vertices)
	{
		// Adiciona cada componente do vértice (posição, textura, cor, normal) ao vetor tempVector
		tempVector.push_back(vertex.x);
		tempVector.push_back(vertex.y);
		tempVector.push_back(vertex.z);
		tempVector.push_back(vertex.s);
		tempVector.push_back(vertex.t);
		tempVector.push_back(vertex.r); // Componente vermelha da cor
		tempVector.push_back(vertex.g); // Componente verde da cor
		tempVector.push_back(vertex.b); // Componente azul da cor
		tempVector.push_back(vertex.nx);
		tempVector.push_back(vertex.ny);
		tempVector.push_back(vertex.nz);
	}

	// Retorna um ponteiro para os dados brutos do vetor tempVector
	// Importante: este ponteiro se torna inválido se tempVector for destruído ou realocado.
	// O chamador deve ter cuidado com o ciclo de vida deste ponteiro.
	return tempVector.data();
}

// Método para imprimir informações da malha (escala, posição, rotação, ângulo) no console
void Mesh::toString()
{
	std::cout
			<< "Scale: " << this->scale.x << " " << this->scale.y << " " << this->scale.z << " " << "\n"						 // Imprime a escala
			<< "Position: " << this->position.x << " " << this->position.y << " " << this->position.z << " " << "\n" // Imprime a posição
			<< "Rotation: " << this->position.x << " " << this->position.y << " " << this->position.z << " " << "\n" // Erro: Imprimindo 'position' em vez de 'rotation'
			<< "Angle: " << this->angle.x << " " << this->angle.y << " " << this->angle.z << " " << "\n";						 // Imprime o ângulo
}