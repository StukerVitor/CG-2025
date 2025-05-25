// Objetivo: demonstrar o carregamento de cena a partir de arquivos de texto,
//           renderizar malhas e curvas de Bézier em OpenGL, além de permitir
//           interação com câmera e seleção de objetos.
// Todos os comentários foram revisados quanto à acentuação e ampliados para
// fornecer uma explicação passo a passo do funcionamento do código.
// ============================================================================

// *** IMPLEMENTAÇÃO DA STB_IMAGE *** -----------------------------------------
// Define que o arquivo stb_image.h incluirá apenas a implementação necessária
// nesta tradução‑unit. Deve vir *antes* da inclusão do cabeçalho.
#define STB_IMAGE_IMPLEMENTATION
#include "../../Common/include/stb_image.h"

// *** BIBLIOTECAS PADRÃO *** --------------------------------------------------
#include <iostream>			 // Saída de dados no console
#include <string>				 // Classe std::string
#include <assert.h>			 // Assertivas de depuração
#include <fstream>			 // Manipulação de arquivos
#include <sstream>			 // String streams
#include <vector>				 // Vetores dinâmicos
#include <unordered_map> // Dicionários hash

#include "Shader.h" // Classe utilitária para shaders

// *** OPENGL / FRAMEWORKS *** -------------------------------------------------
#include <glad/glad.h>									// Carregador de funções OpenGL
#include <GLFW/glfw3.h>									// Janela e input
#include <glm/glm.hpp>									// Tipos matemáticos
#include <glm/gtc/matrix_transform.hpp> // Matrizes de transformação
#include <glm/gtc/type_ptr.hpp>					// Conversão p/ ponteiros

// ============================================================================
// ESTRUTURAS DE DADOS
// ============================================================================

struct Vertex
{
	float x, y, z;		// Posição do vértice
	float s, t;				// Coordenadas de textura
	float r, g, b;		// Cor (não utilizada para materiais)
	float nx, ny, nz; // Vetor normal
};

struct Material
{
	// Componentes de material baseados no modelo de iluminação Phong
	float kaR, kaG, kaB;		 // Coeficiente ambiente (Ka)
	float kdR, kdG, kdB;		 // Coeficiente difuso   (Kd)
	float ksR, ksG, ksB;		 // Coeficiente especular (Ks)
	float ns;								 // Expoente especular
	std::string textureName; // Nome do arquivo de textura
};

struct GlobalConfig
{
	// Parámetros de iluminação e câmera globais
	glm::vec3 lightPos, lightColor;		// Posição e cor da luz principal
	glm::vec3 cameraPos, cameraFront; // Posição e direção inicial da câmera
	GLfloat fov, nearPlane, farPlane; // Projeção perspectiva
	GLfloat sensitivity, cameraSpeed; // Sensibilidade do mouse e velocidade da câmera
};

struct Mesh
{
	// Informações de uma malha (objeto) estática
	std::string name;											// Identificador textual
	std::string objFilePath, mtlFilePath; // Arquivos de origem
	glm::vec3 scale, position, rotation;	// Transformações gerais
	glm::vec3 angle;											// Ângulos iniciais (XYZ)
	GLuint incrementalAngle;							// Flag p/ rotação contínua

	std::vector<Vertex> vertices; // Vértices carregados
	GLuint VAO;										// Vertex Array Object
	Material material;						// Material associado
	GLuint textureID;							// ID da textura OpenGL
};

struct BezierCurve
{
	// Representação lógica/visual de uma curva de Bézier composta
	std::string name;											// Identificador textual
	std::vector<glm::vec3> controlPoints; // Pontos de controle
	GLuint pointsPerSegment;							// Resolução por segmento
	glm::vec4 color;											// Cor de renderização
	glm::vec3 orbit;											// Eixo de órbita (não usado)
	GLfloat radius;												// Raio (para gerar órbitas)

	GLuint VAO;													// VAO da curva
	GLuint controlPointsVAO;						// VAO para pontos de controle
	std::vector<glm::vec3> curvePoints; // Pontos discretizados
};

// ============================================================================
// PROTÓTIPOS DE FUNÇÕES
// ============================================================================
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void readSceneFile(const std::string sceneFilePath,
									 std::unordered_map<std::string, Mesh> *meshes,
									 std::vector<std::string> *meshList,
									 std::unordered_map<std::string, BezierCurve> *bezierCurves,
									 GlobalConfig *globalConfig);
GLuint setupTexture(const std::string path);
std::vector<Vertex> setupObj(const std::string path);
Material setupMtl(const std::string path);
int setupGeometry(std::vector<Vertex> &vertices);
std::vector<glm::vec3> generateCircleControlPoints(glm::vec3 referencePoint, float radius);
GLuint generateControlPointsBuffer(const std::vector<glm::vec3> controlPoints);
BezierCurve createBezierCurve(const std::vector<glm::vec3> controlPoints, int pointsPerSegment);

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================
GlobalConfig globalConfig;	// Configurações carregadas da cena
const GLuint WIDTH = 1000;	// Largura inicial da janela
const GLuint HEIGHT = 1000; // Altura inicial da janela

// --- Câmera ------------------------------------------------------------------
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);								 // Vetor "up" fixo
bool firstMouse = true;																					 // Evita saltos na 1ª leitura do mouse
float lastX, lastY;																							 // Últimas posições do cursor
float pitch = 0.0f, yaw = -90.0f;																 // Ângulos iniciais da câmera
bool moveW = false, moveA = false, moveS = false, moveD = false; // Teclas W A S D

// --- Renderização ------------------------------------------------------------
int i = 0, j = 0;							 // Índices para animação de órbitas
float incrementalAngle = 0.0f; // Ângulo global para rotações contínuas

GLuint currentlySelectedMesh = -1;								// Índice do objeto selecionado
GLfloat selectedMeshScale = 1.0f;									// Escala aplicada ao objeto selecionado
GLfloat selectedMeshAngle = 0.0f;									// Rotação adicional do objeto selecionado
glm::vec2 selectedMeshPosition = glm::vec2(0.0f); // Deslocamento XY 2D

// --- Depuração ---------------------------------------------------------------
GLuint showCurves = 1; // 1 = desenha curvas; 0 = esconde

// ============================================================================
// FUNÇÃO PRINCIPAL
// ============================================================================
int main()
{
	// --------------------------------------------------------------------
	// 1) Inicialização da janela e do contexto OpenGL (GLFW + GLAD)
	// --------------------------------------------------------------------
	glfwInit(); // Inicia a GLFW (biblioteca de janelas + input)

	// Cria a janela / contexto – sem especificar hints (padrões)
	GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Leitor de Cena", nullptr, nullptr);
	assert(window && "Falha ao criar janela GLFW");
	glfwMakeContextCurrent(window); // Torna o contexto atual

	// Registra callbacks de teclado e mouse -----------------------------
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);

	// Oculta o cursor e mantém teclas apertadas mesmo após evento
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

	// GLAD – carrega ponteiros de funções OpenGL para este contexto
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cerr << "Falha ao inicializar GLAD\n";
		return -1;
	}

	// Informações do driver ---------------------------------------------
	std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n'
						<< "OpenGL version: " << glGetString(GL_VERSION) << "\n\n";

	// Ajusta o viewport para a resolução do framebuffer -----------------
	int fbWidth, fbHeight;
	glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
	glViewport(0, 0, fbWidth, fbHeight);

	// --------------------------------------------------------------------
	// 2) Carregamento da cena (malhas, curvas, configurações globais)
	// --------------------------------------------------------------------
	std::unordered_map<std::string, Mesh> meshes;							 // Tabela de malhas
	std::vector<std::string> meshList;												 // Lista ordenada p/ seleção
	std::unordered_map<std::string, BezierCurve> bezierCurves; // Curvas Bézier

	readSceneFile("../Scene.txt", &meshes, &meshList, &bezierCurves, &globalConfig);

	// --------------------------------------------------------------------
	// 3) Compilação / Link de Shaders
	// --------------------------------------------------------------------
	Shader objectShader("../shaders/Object.vs", "../shaders/Object.fs");
	Shader lineShader("../shaders/Line.vs", "../shaders/Line.fs");

	// Define a unidade de textura padrão para o shader de objetos --------
	glUseProgram(objectShader.getId());
	glUniform1i(glGetUniformLocation(objectShader.getId(), "tex"), 0);

	// Matrizes de câmera e projeção iniciais ------------------------------
	glm::mat4 view = glm::lookAt(globalConfig.cameraPos, globalConfig.cameraFront, cameraUp);
	glm::mat4 projection = glm::perspective(glm::radians(globalConfig.fov),
																					static_cast<float>(fbWidth) / fbHeight,
																					globalConfig.nearPlane, globalConfig.farPlane);
	glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	// Luz principal -------------------------------------------------------
	glUniform3fv(glGetUniformLocation(objectShader.getId(), "lightPos"), 1, glm::value_ptr(globalConfig.lightPos));
	glUniform3fv(glGetUniformLocation(objectShader.getId(), "lightColor"), 1, glm::value_ptr(globalConfig.lightColor));

	// Shaders para curvas (usa mesma câmera / projeção) -------------------
	glUseProgram(lineShader.getId());
	glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	// Habilita o teste de profundidade (pintar pixels mais próximos) ------
	glEnable(GL_DEPTH_TEST);

	// --------------------------------------------------------------------
	// 4) Loop principal (Game Loop)
	// --------------------------------------------------------------------
	while (!glfwWindowShouldClose(window))
	{
		// 4.1) Processa eventos de input -------------------------------
		glfwPollEvents();

		// 4.2) Limpa color buffer + depth buffer -----------------------
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glPointSize(10); // Tamanho para pontos das curvas

		// 4.3) Atualiza a câmera de acordo com entrada WASD ------------
		if (moveW)
			globalConfig.cameraPos += globalConfig.cameraFront * globalConfig.cameraSpeed;
		if (moveA)
			globalConfig.cameraPos -= glm::normalize(glm::cross(globalConfig.cameraFront, cameraUp)) * globalConfig.cameraSpeed;
		if (moveS)
			globalConfig.cameraPos -= globalConfig.cameraFront * globalConfig.cameraSpeed;
		if (moveD)
			globalConfig.cameraPos += glm::normalize(glm::cross(globalConfig.cameraFront, cameraUp)) * globalConfig.cameraSpeed;

		view = glm::lookAt(globalConfig.cameraPos, globalConfig.cameraPos + globalConfig.cameraFront, cameraUp);

		// 4.4) Renderiza malhas ----------------------------------------
		glUseProgram(objectShader.getId());
		glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniform3fv(glGetUniformLocation(objectShader.getId(), "cameraPos"), 1, glm::value_ptr(globalConfig.cameraPos));

		// --- Atualização de posições de planeta e lua -----------------
		Mesh &planeta = meshes["Planeta"];
		BezierCurve &orbTer = bezierCurves["OrbitaTerra"];
		planeta.position = orbTer.curvePoints[j];

		Mesh &lua = meshes["Lua"];
		BezierCurve &orbLua = bezierCurves["OrbitaLua"];
		lua.position = orbLua.curvePoints[i] + planeta.position; // órbita relativa

		// --- Desenha todas as malhas ----------------------------------
		for (auto &pair : meshes)
		{
			Mesh &mesh = pair.second;
			bool isSelected = (currentlySelectedMesh != -1) && (meshList.at(currentlySelectedMesh % meshList.size()) == pair.first);

			// Matriz Model de cada malha -----------------------------
			glm::mat4 model(1.0f);
			glm::vec3 pos = mesh.position + (isSelected ? glm::vec3(selectedMeshPosition, 0.0f) : glm::vec3(0.0f));
			model = glm::translate(model, pos);

			// Rotação: incremental ou fixa ---------------------------
			glm::vec3 ang = mesh.incrementalAngle ? glm::vec3(incrementalAngle) : mesh.angle;
			if (isSelected && selectedMeshAngle != 0.0f)
				ang *= selectedMeshAngle;
			model = glm::rotate(model, glm::radians(ang.x), mesh.rotation);
			model = glm::rotate(model, glm::radians(ang.y), mesh.rotation);
			model = glm::rotate(model, glm::radians(ang.z), mesh.rotation);

			// Escala --------------------------------------------------
			glm::vec3 scl = mesh.scale * (isSelected ? selectedMeshScale : 1.0f);
			model = glm::scale(model, scl);

			// Envia a matriz Model p/ o shader -----------------------
			glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "model"), 1, GL_FALSE, glm::value_ptr(model));

			// Material ----------------------------------------------
			glUniform1f(glGetUniformLocation(objectShader.getId(), "kaR"), mesh.material.kaR);
			glUniform1f(glGetUniformLocation(objectShader.getId(), "kaG"), mesh.material.kaG);
			glUniform1f(glGetUniformLocation(objectShader.getId(), "kaB"), mesh.material.kaB);
			glUniform1f(glGetUniformLocation(objectShader.getId(), "kdR"), mesh.material.kdR);
			glUniform1f(glGetUniformLocation(objectShader.getId(), "kdG"), mesh.material.kdG);
			glUniform1f(glGetUniformLocation(objectShader.getId(), "kdB"), mesh.material.kdB);
			glUniform1f(glGetUniformLocation(objectShader.getId(), "ksR"), mesh.material.ksR);
			glUniform1f(glGetUniformLocation(objectShader.getId(), "ksG"), mesh.material.ksG);
			glUniform1f(glGetUniformLocation(objectShader.getId(), "ksB"), mesh.material.ksB);
			glUniform1f(glGetUniformLocation(objectShader.getId(), "ns"), mesh.material.ns);

			// Cor extra ao selecionar --------------------------------
			if (isSelected)
				glUniform3f(glGetUniformLocation(objectShader.getId(), "extraColor"), 0.3f, 0.5f, 0.9f);
			else
				glUniform3f(glGetUniformLocation(objectShader.getId(), "extraColor"), 0.0f, 0.0f, 0.0f);

			// Opcional: pular iluminação para o Sol ------------------
			glUniform1i(glGetUniformLocation(objectShader.getId(), "skipLighting"), pair.first == "Sol");

			// Desenho -----------------------------------------------
			glBindVertexArray(mesh.VAO);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, mesh.textureID);
			glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.vertices.size()));
			glBindVertexArray(0);
		}

		// 4.5) Renderiza curvas de Bézier -----------------------------
		if (showCurves)
		{
			glUseProgram(lineShader.getId());
			glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));

			for (const auto &pair : bezierCurves)
			{
				const BezierCurve &bc = pair.second;

				// ----- Curva em linha contínua --------------------
				glUniform4fv(glGetUniformLocation(lineShader.getId(), "finalColor"), 1, glm::value_ptr(bc.color));
				glBindVertexArray(bc.VAO);
				glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(bc.curvePoints.size()));
				glBindVertexArray(0);

				// ----- Pontos de controle -------------------------
				glUniform4f(glGetUniformLocation(lineShader.getId(), "finalColor"), 1.0f, 1.0f, 0.0f, 1.0f);
				glBindVertexArray(bc.controlPointsVAO);
				glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(bc.controlPoints.size()));
				glBindVertexArray(0);

				// ----- Linhas dos pontos de controle -------------
				glUniform4f(glGetUniformLocation(lineShader.getId(), "finalColor"), 0.0f, 1.0f, 0.0f, 1.0f);
				glBindVertexArray(bc.controlPointsVAO);
				glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(bc.controlPoints.size()));
				glBindVertexArray(0);
			}
		}

		// 4.6) Atualiza variáveis de animação -------------------------
		i = (i + 36) % bezierCurves["OrbitaLua"].curvePoints.size();
		j = (j + 5) % bezierCurves["OrbitaTerra"].curvePoints.size();

		incrementalAngle = fmod(incrementalAngle + 0.1f, 360.0f);

		// 4.7) Troca os buffers (double buffering) -------------------
		glfwSwapBuffers(window);
	}

	// --------------------------------------------------------------------
	// 5) Liberação de recursos
	// --------------------------------------------------------------------
	for (const auto &pair : meshes)
		glDeleteVertexArrays(1, &pair.second.VAO);
	for (const auto &pair : bezierCurves)
		glDeleteVertexArrays(1, &pair.second.VAO);

	glfwTerminate(); // Encerra GLFW e libera memória alocada internamente
	return 0;
}

/*****************************************************************************************
 *  readSceneFile()
 *  --------------------------------------------------------------------------------------
 *  Lê um arquivo de cena (.txt ou similar) e, para cada bloco de entidade descrito,
 *  popula as estruturas de configuração global, malhas (Mesh) e curvas de Bézier
 *  utilizadas na aplicação.
 *
 *  Passo a passo geral:
 *    1. Abre o arquivo indicado por sceneFilePath e verifica sucesso.
 *    2. Percorre cada linha do arquivo, identificando o tipo de informação (Type,
 *       LightPos, Obj, ControlPoint, End, etc.).
 *    3. Conforme o tipo, acumula dados temporariamente nas variáveis correspondentes.
 *    4. Quando encontra "End", instancia o objeto adequado (GlobalConfig, Mesh ou
 *       BezierCurve) com os dados coletados e o armazena nas coleções recebidas via
 *       ponteiro.
 *    5. Ao final, fecha o arquivo e retorna.
 *****************************************************************************************/
void readSceneFile(std::string sceneFilePath,
									 std::unordered_map<std::string, Mesh> *meshes,
									 std::vector<std::string> *meshList,
									 std::unordered_map<std::string, BezierCurve> *bezierCurves,
									 GlobalConfig *globalConfig)
{
	std::ifstream file(sceneFilePath);
	std::string line;

	if (!file.is_open())
	{
		std::cerr << "Falha ao abrir o arquivo " << sceneFilePath << std::endl;
		return;
	}

	/* ------------------------------ Variáveis temporárias ------------------------------ */
	std::string objectType; // Armazena o tipo atual (GlobalConfig, Mesh, BezierCurve)
	std::string name;				// Nome do objeto corrente (identificador único)

	// --- Atributos da configuração global ---
	glm::vec3 lightPos{}, lightColor{};
	glm::vec3 cameraPos{}, cameraFront{};
	GLfloat fov{}, nearPlane{}, farPlane{}, sensitivity{}, cameraSpeed{};

	// --- Atributos de Mesh ---
	std::string objFilePath, mtlFilePath;
	glm::vec3 scale{1.0f}, position{0.0f}, rotation{0.0f}, angle{0.0f};
	GLuint incrementalAngle = false;

	// --- Atributos de Bézier ---
	std::vector<glm::vec3> tempControlPoints;
	GLuint pointsPerSegment = 0;
	glm::vec4 color{1.0f};
	glm::vec3 orbit{0.0f};
	GLfloat radius = 1.0f;
	GLuint usingOrbit = false;

	/* ------------------------------- Loop de leitura ---------------------------------- */
	while (getline(file, line))
	{
		std::istringstream ss(line);
		std::string type; // Primeiro token da linha define a “tag”
		ss >> type;

		/* ---------- Identificação de cada campo válido ---------- */
		if (type == "Type")
			ss >> objectType >> name;

		/* ---- Campos da GlobalConfig ---- */
		else if (type == "LightPos" && objectType == "GlobalConfig")
			ss >> lightPos.x >> lightPos.y >> lightPos.z;
		else if (type == "LightColor" && objectType == "GlobalConfig")
			ss >> lightColor.r >> lightColor.g >> lightColor.b;
		else if (type == "CameraPos" && objectType == "GlobalConfig")
			ss >> cameraPos.x >> cameraPos.y >> cameraPos.z;
		else if (type == "CameraFront" && objectType == "GlobalConfig")
			ss >> cameraFront.x >> cameraFront.y >> cameraFront.z;
		else if (type == "Fov" && objectType == "GlobalConfig")
			ss >> fov;
		else if (type == "NearPlane" && objectType == "GlobalConfig")
			ss >> nearPlane;
		else if (type == "FarPlane" && objectType == "GlobalConfig")
			ss >> farPlane;
		else if (type == "Sensitivity" && objectType == "GlobalConfig")
			ss >> sensitivity;
		else if (type == "CameraSpeed" && objectType == "GlobalConfig")
			ss >> cameraSpeed;

		/* ---- Campos de Mesh ---- */
		else if (type == "Obj" && objectType == "Mesh")
			ss >> objFilePath;
		else if (type == "Mtl" && objectType == "Mesh")
			ss >> mtlFilePath;
		else if (type == "Scale" && objectType == "Mesh")
			ss >> scale.x >> scale.y >> scale.z;
		else if (type == "Position" && objectType == "Mesh")
			ss >> position.x >> position.y >> position.z;
		else if (type == "Rotation" && objectType == "Mesh")
			ss >> rotation.x >> rotation.y >> rotation.z;
		else if (type == "Angle" && objectType == "Mesh")
			ss >> angle.x >> angle.y >> angle.z;
		else if (type == "IncrementalAngle" && objectType == "Mesh")
			ss >> incrementalAngle;

		/* ---- Campos da BezierCurve ---- */
		else if (type == "ControlPoint" && objectType == "BezierCurve")
		{
			glm::vec3 cp;
			ss >> cp.x >> cp.y >> cp.z;
			tempControlPoints.push_back(cp);
			usingOrbit = false; // Se definir pontos manualmente, desativa órbita
		}
		else if (type == "PointsPerSegment" && objectType == "BezierCurve")
			ss >> pointsPerSegment;
		else if (type == "Color" && objectType == "BezierCurve")
			ss >> color.r >> color.g >> color.b >> color.a;
		else if (type == "Orbit" && objectType == "BezierCurve")
		{
			ss >> orbit.x >> orbit.y >> orbit.z;
			usingOrbit = true; // Ativa modo de órbita (círculo automático)
		}
		else if (type == "Radius" && objectType == "BezierCurve")
			ss >> radius;

		/* ---------- Fim de um bloco (“End”) ---------- */
		else if (type == "End")
		{
			/* ---- Finaliza e armazena uma GlobalConfig ---- */
			if (objectType == "GlobalConfig")
			{
				globalConfig->lightPos = lightPos;
				globalConfig->lightColor = lightColor;
				globalConfig->cameraPos = cameraPos;
				globalConfig->cameraFront = cameraFront;
				globalConfig->nearPlane = nearPlane;
				globalConfig->farPlane = farPlane;
				globalConfig->fov = fov;
				globalConfig->sensitivity = sensitivity;
				globalConfig->cameraSpeed = cameraSpeed;
			}
			/* ---- Finaliza e armazena uma Mesh ---- */
			else if (objectType == "Mesh")
			{
				Mesh mesh;

				/* 1. Carrega vértices do OBJ para CPU */
				std::vector<Vertex> vertices = setupObj(objFilePath);

				/* 2. Envia geometria para GPU, obtém VAO */
				GLuint VAO = setupGeometry(vertices);

				/* 3. Lê material (.mtl) e textura correspondente */
				Material material = setupMtl(mtlFilePath);
				GLuint textureID = setupTexture(material.textureName);

				/* 4. Preenche estrutura Mesh */
				mesh.name = name;
				mesh.vertices = vertices;
				mesh.VAO = VAO;
				mesh.material = material;
				mesh.textureID = textureID;
				mesh.position = position;
				mesh.rotation = rotation;
				mesh.scale = scale;
				mesh.angle = angle;
				mesh.incrementalAngle = incrementalAngle;

				/* 5. Adiciona aos contêineres globais */
				meshes->insert(std::make_pair(name, mesh));
				meshList->push_back(name);
			}
			/* ---- Finaliza e armazena uma BezierCurve ---- */
			else if (objectType == "BezierCurve")
			{
				/* Se for curva orbital, gera pontos de controle automaticamente */
				std::vector<glm::vec3> controlPoints = usingOrbit
																									 ? generateCircleControlPoints(orbit, radius)
																									 : tempControlPoints;

				/* Cria curva, gera seu VAO, e preenche estrutura */
				BezierCurve bezierCurve = createBezierCurve(controlPoints, pointsPerSegment);
				GLuint controlVAO = generateControlPointsBuffer(controlPoints);

				bezierCurve.name = name;
				bezierCurve.controlPoints = controlPoints;
				bezierCurve.color = color;
				bezierCurve.pointsPerSegment = pointsPerSegment;
				if (usingOrbit)
				{
					bezierCurve.orbit = orbit;
					bezierCurve.radius = radius;
				}
				bezierCurve.controlPointsVAO = controlVAO;

				bezierCurves->insert(std::make_pair(name, bezierCurve));
				tempControlPoints.clear(); // Limpa para o próximo bloco
			}
		}
	}

	file.close();
}

/*****************************************************************************************
 *  setupObj()
 *  --------------------------------------------------------------------------------------
 *  Carrega um arquivo .obj simples (v / vt / vn / f) e devolve um vetor de Vertex pronto
 *  para envio à GPU.
 *  Passo a passo:
 *    1. Abre o arquivo e percorre linha por linha.
 *    2. Armazena posições (v), coordenadas de textura (vt) e normais (vn) em vetores
 *       temporários.
 *    3. Para cada face (f), converte índices v/vt/vn em instâncias de Vertex, preenchendo
 *       posição, texcoord e normal.
 *****************************************************************************************/
std::vector<Vertex> setupObj(std::string path)
{
	std::vector<Vertex> vertices;
	std::ifstream file(path);
	std::string line;

	/* Vetores temporários para dados crus do .obj */
	std::vector<glm::vec3> temp_positions;
	std::vector<glm::vec2> temp_texcoords;
	std::vector<glm::vec3> temp_normals;

	if (!file.is_open())
	{
		std::cerr << "Falha ao abrir o arquivo " << path << std::endl;
		return vertices;
	}

	while (getline(file, line))
	{
		std::istringstream ss(line);
		std::string type;
		ss >> type;

		/* Posições de vértice */
		if (type == "v")
		{
			glm::vec3 pos;
			ss >> pos.x >> pos.y >> pos.z;
			temp_positions.push_back(pos);
		}
		/* Coordenadas de textura */
		else if (type == "vt")
		{
			glm::vec2 uv;
			ss >> uv.x >> uv.y;
			temp_texcoords.push_back(uv);
		}
		/* Normais */
		else if (type == "vn")
		{
			glm::vec3 n;
			ss >> n.x >> n.y >> n.z;
			temp_normals.push_back(n);
		}
		/* Faces (triângulos) */
		else if (type == "f")
		{
			std::string v1, v2, v3;
			ss >> v1 >> v2 >> v3;
			int vIdx[3], tIdx[3], nIdx[3];

			/* Converte cada vértice v/vt/vn para índices inteiros (0-based) */
			std::string verts[3] = {v1, v2, v3};
			for (int i = 0; i < 3; ++i)
			{
				size_t p1 = verts[i].find('/');
				size_t p2 = verts[i].find('/', p1 + 1);
				vIdx[i] = std::stoi(verts[i].substr(0, p1)) - 1;
				tIdx[i] = std::stoi(verts[i].substr(p1 + 1, p2 - p1 - 1)) - 1;
				nIdx[i] = std::stoi(verts[i].substr(p2 + 1)) - 1;
			}

			/* Monta três structs Vertex e adiciona ao vetor final */
			for (int i = 0; i < 3; ++i)
			{
				Vertex v{};
				v.x = temp_positions[vIdx[i]].x;
				v.y = temp_positions[vIdx[i]].y;
				v.z = temp_positions[vIdx[i]].z;
				v.s = temp_texcoords[tIdx[i]].x;
				v.t = temp_texcoords[tIdx[i]].y;
				v.nx = temp_normals[nIdx[i]].x;
				v.ny = temp_normals[nIdx[i]].y;
				v.nz = temp_normals[nIdx[i]].z;
				vertices.push_back(v);
			}
		}
	}
	file.close();
	return vertices;
}

/*****************************************************************************************
 *  setupMtl()
 *  --------------------------------------------------------------------------------------
 *  Lê um arquivo .mtl e devolve uma estrutura Material preenchida.
 *****************************************************************************************/
Material setupMtl(std::string path)
{
	Material material{};
	std::ifstream file(path);
	std::string line;

	if (!file.is_open())
	{
		std::cerr << "Falha ao abrir o arquivo " << path << std::endl;
		return material;
	}

	while (getline(file, line))
	{
		std::istringstream ss(line);
		std::string type;
		ss >> type;

		if (type == "Ka")
			ss >> material.kaR >> material.kaG >> material.kaB;
		else if (type == "Kd")
			ss >> material.kdR >> material.kdG >> material.kdB;
		else if (type == "Ks")
			ss >> material.ksR >> material.ksG >> material.ksB;
		else if (type == "Ns")
			ss >> material.ns;
		else if (type == "map_Kd")
			ss >> material.textureName;
	}
	file.close();
	return material;
}

/*****************************************************************************************
 *  setupTexture()
 *  --------------------------------------------------------------------------------------
 *  Carrega uma textura de disco com stb_image e cria um objeto de textura OpenGL.
 *  Aceita imagens RGB ou RGBA; gera mipmaps automaticamente.
 *****************************************************************************************/
GLuint setupTexture(std::string filename)
{
	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	/* Parâmetros de wrapping e filtragem */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_set_flip_vertically_on_load(true); // Ajusta origem da imagem

	int w, h, channels;
	unsigned char *data = stbi_load(filename.c_str(), &w, &h, &channels, 0);
	if (data)
	{
		GLenum fmt = (channels == 3) ? GL_RGB : GL_RGBA;
		glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cerr << "Falha ao carregar a textura " << filename << std::endl;
	}
	stbi_image_free(data);
	return texId;
}

/*****************************************************************************************
 *  key_callback()
 *  --------------------------------------------------------------------------------------
 *  Função de callback para teclado (GLFW). Atualiza flags de movimento e seleciona meshes
 *  conforme teclas específicas são pressionadas/soltas.
 *****************************************************************************************/
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	/* Flags de movimento da câmera */
	if (key == GLFW_KEY_W && action == GLFW_PRESS)
		moveW = true;
	else if (key == GLFW_KEY_W && action == GLFW_RELEASE)
		moveW = false;
	if (key == GLFW_KEY_A && action == GLFW_PRESS)
		moveA = true;
	else if (key == GLFW_KEY_A && action == GLFW_RELEASE)
		moveA = false;
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
		moveS = true;
	else if (key == GLFW_KEY_S && action == GLFW_RELEASE)
		moveS = false;
	if (key == GLFW_KEY_D && action == GLFW_PRESS)
		moveD = true;
	else if (key == GLFW_KEY_D && action == GLFW_RELEASE)
		moveD = false;

	/* Seleção e manipulação da mesh atual */
	if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
	{
		selectedMeshScale = 1.0f;
		selectedMeshAngle = 0.0f;
		selectedMeshPosition = {0.0f, 0.0f};
		++currentlySelectedMesh;
	}
	if (key == GLFW_KEY_1 && action == GLFW_PRESS)
		selectedMeshScale += 0.2f;
	if (key == GLFW_KEY_2 && action == GLFW_PRESS)
		selectedMeshScale -= 0.2f;
	if (key == GLFW_KEY_3 && action == GLFW_PRESS)
		selectedMeshAngle += 0.2f;
	if (key == GLFW_KEY_4 && action == GLFW_PRESS)
		selectedMeshAngle -= 0.2f;
	if (key == GLFW_KEY_UP && action == GLFW_PRESS)
		selectedMeshPosition.y += 0.3f;
	if (key == GLFW_KEY_DOWN && action == GLFW_PRESS)
		selectedMeshPosition.y -= 0.3f;
	if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS)
		selectedMeshPosition.x += 0.3f;
	if (key == GLFW_KEY_LEFT && action == GLFW_PRESS)
		selectedMeshPosition.x -= 0.3f;

	/* Mostrar/ocultar curvas */
	if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
		showCurves = !showCurves;
}

/*****************************************************************************************
 *  mouse_callback()
 *  --------------------------------------------------------------------------------------
 *  Atualiza o vetor cameraFront (olhar da câmera) de acordo com o deslocamento do mouse.
 *****************************************************************************************/
void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float offsetX = static_cast<float>(xpos - lastX);
	float offsetY = static_cast<float>(lastY - ypos); // Invertido (eixo Y de tela)
	lastX = xpos;
	lastY = ypos;

	offsetX *= globalConfig.sensitivity;
	offsetY *= globalConfig.sensitivity;

	pitch += offsetY;
	yaw += offsetX;

	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	globalConfig.cameraFront = glm::normalize(front);
}

/*****************************************************************************************
 *  setupGeometry()
 *  --------------------------------------------------------------------------------------
 *  Cria VBO + VAO para um vetor de Vertex e devolve o ID do VAO.
 *  Layout dos atributos:
 *    0 -> posição (vec3)      | offset 0
 *    1 -> texcoord (vec2)     | offset 3  * sizeof(float)
 *    2 -> cor (vec3)          | offset 5  * sizeof(float)
 *    3 -> normal (vec3)       | offset 8  * sizeof(float)
 *****************************************************************************************/
int setupGeometry(std::vector<Vertex> &vertices)
{
	GLuint VBO, VAO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER,
							 vertices.size() * sizeof(Vertex),
							 vertices.data(), GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	/* Posição */
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)0);
	glEnableVertexAttribArray(0);

	/* Texcoord */
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
												(GLvoid *)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	/* Cor */
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
												(GLvoid *)(5 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	/* Normal */
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
												(GLvoid *)(8 * sizeof(GLfloat)));
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	return VAO;
}

/*****************************************************************************************
 *  generateCircleControlPoints()
 *  --------------------------------------------------------------------------------------
 *  Gera pontos de controle para desenhar um círculo de raio “radius” ao redor de
 *  referencePoint, usando quatro curvas de Bézier cúbicas.
 *****************************************************************************************/
std::vector<glm::vec3> generateCircleControlPoints(glm::vec3 referencePoint, float radius)
{
	std::vector<glm::vec3> cps;
	const float k = 0.552284749831f * radius; // Constante para aproximar círculo

	/* Pontos principais do “quadrado” circunscrito */
	glm::vec3 P0 = referencePoint + glm::vec3(radius, 0, radius);		// Topo
	glm::vec3 P1 = referencePoint + glm::vec3(radius, 0, -radius);	// Lado direito
	glm::vec3 P2 = referencePoint + glm::vec3(-radius, 0, -radius); // Base
	glm::vec3 P3 = referencePoint + glm::vec3(-radius, 0, radius);	// Lado esquerdo

	/* Pontos auxiliares (tangentes) */
	glm::vec3 P0a = P0 + glm::vec3(k, 0, -k);
	glm::vec3 P0b = P0 + glm::vec3(-k, 0, k);
	glm::vec3 P1a = P1 + glm::vec3(-k, 0, -k);
	glm::vec3 P1b = P1 + glm::vec3(k, 0, k);
	glm::vec3 P2a = P2 + glm::vec3(-k, 0, k);
	glm::vec3 P2b = P2 + glm::vec3(k, 0, -k);
	glm::vec3 P3a = P3 + glm::vec3(k, 0, k);
	glm::vec3 P3b = P3 + glm::vec3(-k, 0, -k);

	/* Quatro segmentos Bézier (cada 4 pontos) */
	cps.insert(cps.end(), {P0, P0a, P1b, P1,
												 P1a, P2b, P2,
												 P2a, P3b, P3,
												 P3a, P0b, P0});
	return cps;
}

/*****************************************************************************************
 *  generateControlPointsBuffer()
 *  --------------------------------------------------------------------------------------
 *  Envia um vetor de glm::vec3 para GPU e devolve o VAO resultante.
 *****************************************************************************************/
GLuint generateControlPointsBuffer(std::vector<glm::vec3> controlPoints)
{
	GLuint VBO, VAO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER,
							 controlPoints.size() * sizeof(glm::vec3),
							 controlPoints.data(), GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
												3 * sizeof(GLfloat), (GLvoid *)0);
	glEnableVertexAttribArray(0);

	/* Limpeza */
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	return VAO;
}

/*****************************************************************************************
 *  createBezierCurve()
 *  --------------------------------------------------------------------------------------
 *  Constrói pontos de uma ou mais curvas de Bézier cúbicas usando matriz de base
 *  (M) e gera VAO para renderização. Devolve estrutura BezierCurve preenchida.
 *****************************************************************************************/
BezierCurve createBezierCurve(std::vector<glm::vec3> controlPoints,
															int pointsPerSegment)
{
	/* Matriz de base de Bézier cúbica */
	const glm::mat4 M(
			-1, 3, -3, 1,
			3, -6, 3, 0,
			-3, 3, 0, 0,
			1, 0, 0, 0);

	std::vector<glm::vec3> curvePoints;
	float step = 1.0f / static_cast<float>(pointsPerSegment);

	/* Para cada segmento de 4 pontos (P0..P3) */
	for (size_t i = 0; i + 3 < controlPoints.size(); i += 3)
	{
		for (float t = 0.0f; t <= 1.0f; t += step)
		{
			glm::vec4 T(t * t * t, t * t, t, 1.0f);
			glm::mat4x3 G(controlPoints[i],
										controlPoints[i + 1],
										controlPoints[i + 2],
										controlPoints[i + 3]);
			curvePoints.push_back(G * M * T);
		}
	}

	/* Envio à GPU (VBO + VAO) */
	GLuint VBO, VAO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER,
							 curvePoints.size() * sizeof(glm::vec3),
							 curvePoints.data(), GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
												3 * sizeof(GLfloat), (GLvoid *)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	/* Preenche struct de retorno */
	BezierCurve bc;
	bc.VAO = VAO;
	bc.curvePoints = curvePoints;
	return bc;
}
