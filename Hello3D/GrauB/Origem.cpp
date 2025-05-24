#define STB_IMAGE_IMPLEMENTATION
#include "../../Common/include/stb_image.h"
#include <iostream>
#include <string>
#include <assert.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include "Shader.h"

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

//GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Vertex {
	float x, y, z;		// Positioning
	float s, t;			// Texture
	float r, g, b;		// Color
	float nx, ny, nz;   // Normal
};

struct Material {
	float kaR, kaG, kaB;   // Ka
	float kdR, kdG, kdB;   // Kd
	float ksR, ksG, ksB;   // Ks
	float ns;
	std::string textureName; // map_Kd
};

struct GlobalConfig {
	glm::vec3 lightPos, lightColor;
	glm::vec3 cameraPos, cameraFront;
	GLfloat fov, nearPlane, farPlane, sensitivity, cameraSpeed;
};

struct Mesh {
	std::string name;
	std::string objFilePath, mtlFilePath;
	glm::vec3 scale, position, rotation, angle;
	GLuint incrementalAngle;

	std::vector<Vertex> vertices;
	GLuint VAO;
	Material material;
	GLuint textureID;
};

struct BezierCurve {
	std::string name;
	std::vector<glm::vec3> controlPoints;
	GLuint pointsPerSegment;
	glm::vec4 color;
	glm::vec3 orbit;
	GLfloat radius;

	GLuint VAO;
	GLuint controlPointsVAO;
	std::vector<glm::vec3> curvePoints;
};

// Protзtipos das funушes
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void readSceneFile(std::string sceneFilePath, std::unordered_map<std::string, Mesh>* meshes, std::vector<std::string>* meshList, std::unordered_map<std::string, BezierCurve>* bezierCurves, GlobalConfig* globalConfig);
GLuint setupTexture(std::string path);
std::vector<Vertex> setupObj(std::string path);
Material setupMtl(std::string path);
int setupGeometry(std::vector<Vertex>& vertices);
std::vector<glm::vec3> generateCircleControlPoints(glm::vec3 referencePoint, float radius);
GLuint generateControlPointsBuffer(std::vector <glm::vec3> controlPoints);
BezierCurve createBezierCurve(std::vector <glm::vec3> controlPoints, int pointsPerSegment);

// Globals
GlobalConfig globalConfig;
const GLuint WIDTH = 1000, HEIGHT = 1000;

// Camera
glm::vec3 cameraUp = glm::vec3(0.0, 1.0, 0.0);
bool firstMouse = true;
float lastX, lastY;
float pitch = 0.0, yaw = -90.0;
bool moveW = false, moveA = false, moveS = false, moveD = false;

// Vertices & rendering
int i = 0, j = 0;
float incrementalAngle = 0.0f;

GLuint currentlySelectedMesh = -1;
GLfloat selectedMeshScale = 1.0f;
GLfloat selectedMeshAngle = 0.0f;
glm::vec2 selectedMeshPosition = glm::vec2(0.0f, 0.0f);

// Debugging
GLuint showCurves = 1;

// Funусo MAIN
int main()
{
	// Inicializaусo da GLFW
	glfwInit();

	// Criaусo da janela GLFW
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Leitor de Cena", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// Fazendo o registro da funусo de callback para a janela GLFW
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

	// GLAD: carrega todos os ponteiros d funушes da OpenGL
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;

	}

	// Obtendo as informaушes de versсo
	const GLubyte* renderer = glGetString(GL_RENDERER); /* get renderer string */
	const GLubyte* version = glGetString(GL_VERSION); /* version as a string */
	std::cout << "Renderer: " << renderer << std::endl;
	std::cout << "OpenGL version supported " << version << std::endl;

	// Definindo as dimensшes da viewport com as mesmas dimensшes da janela da aplicaусo
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

	// Instanciando os objetos da cena

	std::unordered_map<std::string, Mesh> meshes = std::unordered_map<std::string, Mesh>();
	std::vector<std::string> meshList = std::vector<std::string>();
	std::unordered_map<std::string, BezierCurve> bezierCurves = std::unordered_map<std::string, BezierCurve>();
	readSceneFile("../Scene.txt", &meshes, &meshList, &bezierCurves, &globalConfig);

	// Shader de mesh
	Shader objectShader = Shader("../shaders/Object.vs", "../shaders/Object.fs");
	glUseProgram(objectShader.getId());

	// Texture
	glUniform1i(glGetUniformLocation(objectShader.getId(), "tex"), 0);

	// Camera
	glm::mat4 view = glm::lookAt(globalConfig.cameraPos, globalConfig.cameraFront, cameraUp);
	glm::mat4 projection = glm::perspective(glm::radians(globalConfig.fov), (float)width / (float)height, globalConfig.nearPlane, globalConfig.farPlane);
	glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	// Light
	glUniform3f(glGetUniformLocation(objectShader.getId(), "lightPos"), globalConfig.lightPos.x, globalConfig.lightPos.y, globalConfig.lightPos.z);
	glUniform3f(glGetUniformLocation(objectShader.getId(), "lightColor"), globalConfig.lightColor.r, globalConfig.lightColor.g, globalConfig.lightColor.b);

	// Shader das curvas
	Shader lineShader = Shader("../shaders/Line.vs", "../shaders/Line.fs");

	glUseProgram(lineShader.getId());

	// Camera
	glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	glEnable(GL_DEPTH_TEST);

	// Loop da aplicaусo - "game loop"
	while (!glfwWindowShouldClose(window))
	{
		// Checa se houveram eventos de input (key pressed, mouse moved etc.) e chama as funушes de callback correspondentes
		glfwPollEvents();

		// Limpa o buffer de cor
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glPointSize(10);

		// Shaders & uniforms

		glUseProgram(objectShader.getId());

		glm::mat4 view = glm::lookAt(globalConfig.cameraPos, globalConfig.cameraPos + globalConfig.cameraFront, cameraUp);
		glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));

		if (moveW) globalConfig.cameraPos += globalConfig.cameraFront * globalConfig.cameraSpeed;
		if (moveA) globalConfig.cameraPos -= glm::normalize(glm::cross(globalConfig.cameraFront, cameraUp)) * globalConfig.cameraSpeed;
		if (moveS) globalConfig.cameraPos -= globalConfig.cameraFront * globalConfig.cameraSpeed;
		if (moveD) globalConfig.cameraPos += glm::normalize(glm::cross(globalConfig.cameraFront, cameraUp)) * globalConfig.cameraSpeed;

		glUniform3f(glGetUniformLocation(objectShader.getId(), "cameraPos"), globalConfig.cameraPos.x, globalConfig.cameraPos.y, globalConfig.cameraPos.z);

		// Logica de renderizacao e de meshes

		Mesh planeta = meshes.find("Planeta")->second;
		BezierCurve orbitaTerra = bezierCurves.find("OrbitaTerra")->second;
		planeta.position = orbitaTerra.curvePoints[j];
		meshes["Planeta"] = planeta;

		Mesh lua = meshes.find("Lua")->second;
		BezierCurve orbitaLua = bezierCurves.find("OrbitaLua")->second;
		lua.position = orbitaLua.curvePoints[i] + planeta.position;
		meshes["Lua"] = lua;

		for (auto& m : meshes) {
			Mesh mesh = m.second;
			bool isSelected = meshList.at(currentlySelectedMesh % meshList.size()) == m.first && currentlySelectedMesh != -1;
			GLuint shaderId = objectShader.getId();
			glm::mat4 model = glm::mat4(1);

			glm::vec3 position = mesh.position;
			if (isSelected) position = position + glm::vec3(selectedMeshPosition.x, selectedMeshPosition.y, 0.0f);
			model = glm::translate(model, position);

			glm::vec3 angle = mesh.incrementalAngle ? glm::vec3(incrementalAngle) : mesh.angle;
			if (isSelected && selectedMeshAngle != 0.0f) angle = angle * glm::vec3(selectedMeshAngle, selectedMeshAngle, selectedMeshAngle);
			model = glm::rotate(model, glm::radians(angle.x), mesh.rotation);
			model = glm::rotate(model, glm::radians(angle.y), mesh.rotation);
			model = glm::rotate(model, glm::radians(angle.z), mesh.rotation);

			glm::vec3 scale = mesh.scale;
			if (isSelected) scale = scale * glm::vec3(selectedMeshScale, selectedMeshScale, selectedMeshScale);
			model = glm::scale(model, scale);

			// Model
			glUniformMatrix4fv(glGetUniformLocation(shaderId, "model"), 1, FALSE, glm::value_ptr(model));

			// Material uniforms
			glUniform1f(glGetUniformLocation(shaderId, "kaR"), mesh.material.kaR);
			glUniform1f(glGetUniformLocation(shaderId, "kaG"), mesh.material.kaG);
			glUniform1f(glGetUniformLocation(shaderId, "kaB"), mesh.material.kaB);
			glUniform1f(glGetUniformLocation(shaderId, "kdR"), mesh.material.kdR);
			glUniform1f(glGetUniformLocation(shaderId, "kdG"), mesh.material.kdG);
			glUniform1f(glGetUniformLocation(shaderId, "kdB"), mesh.material.kdB);
			glUniform1f(glGetUniformLocation(shaderId, "ksR"), mesh.material.ksR);
			glUniform1f(glGetUniformLocation(shaderId, "ksG"), mesh.material.ksG);
			glUniform1f(glGetUniformLocation(shaderId, "ksB"), mesh.material.ksB);
			glUniform1f(glGetUniformLocation(shaderId, "ns"), mesh.material.ns);

			if (isSelected) glUniform3f(glGetUniformLocation(objectShader.getId(), "extraColor"), 0.3f, 0.5f, 0.9f);
			else glUniform3f(glGetUniformLocation(objectShader.getId(), "extraColor"), 0.0f, 0.0f, 0.0f);

			if (m.first == "Sol") glUniform1i(glGetUniformLocation(objectShader.getId(), "skipLighting"), 1);
			else glUniform1i(glGetUniformLocation(objectShader.getId(), "skipLighting"), 0);

			glBindVertexArray(mesh.VAO);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, mesh.textureID);
			glDrawArrays(GL_TRIANGLES, 0, mesh.vertices.size());
			glBindVertexArray(0);
		}

		// Renderizacao de curvas
		if (showCurves) {
			glUseProgram(lineShader.getId());

			glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));

			for (auto& b : bezierCurves) {
				BezierCurve bezierCurve = b.second;
				//cout << "Rendering: " << b.first << "\n";
				GLuint shaderId = lineShader.getId();

				glUniform4f(glGetUniformLocation(shaderId, "finalColor"), bezierCurve.color.r, bezierCurve.color.g, bezierCurve.color.b, bezierCurve.color.a);

				// Render curve
				glBindVertexArray(bezierCurve.VAO);
				glDrawArrays(GL_LINE_STRIP, 0, bezierCurve.curvePoints.size());
				glBindVertexArray(0);

				glUniform4f(glGetUniformLocation(shaderId, "finalColor"), 1.0f, 1.0f, 0.0f, 1.0f);

				// Render individual control points
				glBindVertexArray(bezierCurve.controlPointsVAO);
				glDrawArrays(GL_POINTS, 0, bezierCurve.controlPoints.size());
				glBindVertexArray(0);

				glUniform4f(glGetUniformLocation(shaderId, "finalColor"), 0.0f, 1.0f, 0.0f, 1.0f);

				// Render lines between the control points
				glBindVertexArray(bezierCurve.controlPointsVAO);
				glDrawArrays(GL_LINE_STRIP, 0, bezierCurve.controlPoints.size());
				glBindVertexArray(0);
			}
		}

		// Valores i e j sao usados para fazer a revolucao da lua e do planeta
		i = (i + 36) % orbitaLua.curvePoints.size();
		j = (j + 5) % orbitaTerra.curvePoints.size();
		// Valor de angle ж usado para rotacionar os objetos
		if (incrementalAngle < 360.0f) incrementalAngle += 0.1f;
		else incrementalAngle = 0.0f;

		// Troca os buffers da tela
		glfwSwapBuffers(window);
	}

	// Pede pra OpenGL desalocar os buffers
	for (auto& m : meshes) glDeleteVertexArrays(1, &m.second.VAO);
	for (auto& b : bezierCurves) glDeleteVertexArrays(1, &b.second.VAO);

	// Finaliza a execuусo da GLFW, limpando os recursos alocados por ela
	glfwTerminate();
	return 0;
}

void readSceneFile(std::string sceneFilePath, std::unordered_map<std::string, Mesh>* meshes, std::vector<std::string>* meshList, std::unordered_map<std::string, BezierCurve>* bezierCurves, GlobalConfig* globalConfig) {
	std::ifstream file(sceneFilePath);
	std::string line;

	if (!file.is_open()) {
		std::cerr << "Failed to open file " << sceneFilePath << std::endl;
		return;
	}

	std::string objectType;
	std::string name;

	// Global config attributes
	glm::vec3 lightPos, lightColor;
	glm::vec3 cameraPos, cameraFront;
	GLfloat fov, nearPlane, farPlane, sensitivity, cameraSpeed;

	// Mesh attributes
	std::string objFilePath, mtlFilePath;
	glm::vec3 scale, position, rotation, angle;
	GLuint incrementalAngle = false;

	// Bezier curve attributes
	std::vector<glm::vec3> tempControlPoints;
	GLuint pointsPerSegment = 0;
	glm::vec4 color;
	glm::vec3 orbit;
	GLfloat radius;
	GLuint usingOrbit = false;

	while (getline(file, line)) {
		std::istringstream ss(line);
		std::string type;

		ss >> type;

		if (type == "Type") ss >> objectType >> name;
		// GlobalConfig
		else if (type == "LightPos" && objectType == "GlobalConfig") ss >> lightPos.x >> lightPos.y >> lightPos.z;
		else if (type == "LightColor" && objectType == "GlobalConfig") ss >> lightColor.r >> lightColor.g >> lightColor.b;
		else if (type == "CameraPos" && objectType == "GlobalConfig") ss >> cameraPos.x >> cameraPos.y >> cameraPos.z;
		else if (type == "CameraFront" && objectType == "GlobalConfig") ss >> cameraFront.x >> cameraFront.y >> cameraFront.z;
		else if (type == "Fov" && objectType == "GlobalConfig") ss >> fov;
		else if (type == "NearPlane" && objectType == "GlobalConfig") ss >> nearPlane;
		else if (type == "FarPlane" && objectType == "GlobalConfig") ss >> farPlane;
		else if (type == "Sensitivity" && objectType == "GlobalConfig") ss >> sensitivity;
		else if (type == "CameraSpeed" && objectType == "GlobalConfig") ss >> cameraSpeed;
		// Mesh
		else if (type == "Obj" && objectType == "Mesh") ss >> objFilePath;
		else if (type == "Mtl" && objectType == "Mesh") ss >> mtlFilePath;
		else if (type == "Scale" && objectType == "Mesh") ss >> scale.x >> scale.y >> scale.z;
		else if (type == "Position" && objectType == "Mesh") ss >> position.x >> position.y >> position.z;
		else if (type == "Rotation" && objectType == "Mesh") ss >> rotation.x >> rotation.y >> rotation.z;
		else if (type == "Angle" && objectType == "Mesh") ss >> angle.x >> angle.y >> angle.z;
		else if (type == "IncrementalAngle" && objectType == "Mesh") ss >> incrementalAngle;
		// Bezier curve
		else if (type == "ControlPoint" && objectType == "BezierCurve") {
			glm::vec3 controlPoint;
			ss >> controlPoint.x >> controlPoint.y >> controlPoint.z;

			tempControlPoints.push_back(controlPoint);
			usingOrbit = false;
		}
		else if (type == "PointsPerSegment" && objectType == "BezierCurve") ss >> pointsPerSegment;
		else if (type == "Color" && objectType == "BezierCurve") ss >> color.r >> color.g >> color.b >> color.a;
		else if (type == "Orbit" && objectType == "BezierCurve") {
			ss >> orbit.x >> orbit.y >> orbit.z;
			usingOrbit = true;
		}
		else if (type == "Radius" && objectType == "BezierCurve") ss >> radius;
		else if (type == "End") {
			if (objectType == "GlobalConfig") {
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
			else if (objectType == "Mesh") {
				Mesh mesh;

				std::vector<Vertex> vertices = setupObj(objFilePath);
				GLuint VAO = setupGeometry(vertices);
				Material material = setupMtl(mtlFilePath);
				GLuint textureID = setupTexture(material.textureName);

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

				//std::cout << "Created: " << objFilePath;
				meshes->insert(make_pair(name, mesh));
				meshList->push_back(name);
			}
			else if (objectType == "BezierCurve") {
				BezierCurve bezierCurve;
				std::vector<glm::vec3> controlPoints = std::vector<glm::vec3>();

				if (usingOrbit) controlPoints = generateCircleControlPoints(orbit, radius);
				else controlPoints = tempControlPoints;
				bezierCurve = createBezierCurve(controlPoints, pointsPerSegment);
				GLuint controlPointsVAO = generateControlPointsBuffer(controlPoints);

				bezierCurve.name = name;
				bezierCurve.controlPoints = controlPoints;
				bezierCurve.color = color;
				bezierCurve.pointsPerSegment = pointsPerSegment;
				if (usingOrbit) bezierCurve.orbit = orbit;
				if (usingOrbit) bezierCurve.radius = radius;
				bezierCurve.controlPointsVAO = controlPointsVAO;

				bezierCurves->insert(make_pair(name, bezierCurve));
				tempControlPoints.clear();
			}
		}
	}

	file.close();
	return;
}

std::vector<Vertex> setupObj(std::string path) {
	std::vector<Vertex> vertices;
	std::ifstream file(path);
	std::string line;
	std::vector<glm::vec3> temp_positions;
	std::vector<glm::vec2> temp_texcoords;
	std::vector<glm::vec3> temp_normals;

	if (!file.is_open()) {
		std::cerr << "Failed to open file" << path << std::endl;
		return vertices;
	}

	while (getline(file, line)) {
		std::istringstream ss(line);
		std::string type;
		ss >> type;

		if (type == "v") {
			glm::vec3 position;
			ss >> position.x >> position.y >> position.z;
			temp_positions.push_back(position);
		}
		else if (type == "vt") {
			glm::vec2 texcoord;
			ss >> texcoord.x >> texcoord.y;
			temp_texcoords.push_back(texcoord);
		}
		else if (type == "vn") {
			glm::vec3 normal;
			ss >> normal.x >> normal.y >> normal.z;
			temp_normals.push_back(normal);
		}
		else if (type == "f") {
			std::string vertex1, vertex2, vertex3;
			ss >> vertex1 >> vertex2 >> vertex3;
			int vIndex[3], tIndex[3], nIndex[3];

			for (int i = 0; i < 3; i++) {
				std::string vertex = (i == 0) ? vertex1 : (i == 1) ? vertex2 : vertex3;
				size_t pos1 = vertex.find('/');
				size_t pos2 = vertex.find('/', pos1 + 1);

				vIndex[i] = stoi(vertex.substr(0, pos1)) - 1;
				tIndex[i] = stoi(vertex.substr(pos1 + 1, pos2 - pos1 - 1)) - 1;
				nIndex[i] = stoi(vertex.substr(pos2 + 1)) - 1;
			}

			for (int i = 0; i < 3; i++) {
				Vertex vertex;
				vertex.x = temp_positions[vIndex[i]].x;
				vertex.y = temp_positions[vIndex[i]].y;
				vertex.z = temp_positions[vIndex[i]].z;

				vertex.s = temp_texcoords[tIndex[i]].x;
				vertex.t = temp_texcoords[tIndex[i]].y;

				vertex.nx = temp_normals[nIndex[i]].x;
				vertex.ny = temp_normals[nIndex[i]].y;
				vertex.nz = temp_normals[nIndex[i]].z;

				vertices.push_back(vertex);
			}
		}
	}

	file.close();
	return vertices;
}

Material setupMtl(std::string path) {
	std::string texturePath;
	std::ifstream file(path);
	std::string line;
	Material material;

	if (!file.is_open()) {
		std::cerr << "Failed to open file" << path << std::endl;
		return material;
	}

	while (getline(file, line)) {
		std::istringstream ss(line);
		std::string type;
		ss >> type;

		if (type == "Ka") {
			ss >> material.kaR >> material.kaG >> material.kaB;
		}
		else if (type == "Kd") {
			ss >> material.kdR >> material.kdG >> material.kdB;
		}
		else if (type == "Ks") {
			ss >> material.ksR >> material.ksG >> material.ksB;
		}
		else if (type == "Ns") {
			ss >> material.ns;
		}
		else if (type == "map_Kd") {
			ss >> material.textureName;
		}
	}

	file.close();
	return material;
}

GLuint setupTexture(std::string filename) {
	GLuint textureId;
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_set_flip_vertically_on_load(true);

	int width, height, nrChannels;
	unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrChannels, 0);
	if (data) {
		if (nrChannels == 3) //jpg, bmp
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		}
		else //png
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
		glGenerateMipmap(GL_TEXTURE_2D); // geraусo do mipmap
	}
	else {
		std::cout << "Failed to load texture" << std::endl;

	}

	stbi_image_free(data);

	return textureId;
}

// Funусo de callback de teclado - sз pode ter uma instРncia (deve ser estрtica se
// estiver dentro de uma classe) - ╔ chamada sempre que uma tecla for pressionada
// ou solta via GLFW
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	if (key == GLFW_KEY_W && action == GLFW_PRESS) moveW = true;
	if (key == GLFW_KEY_W && action == GLFW_RELEASE) moveW = false;
	if (key == GLFW_KEY_A && action == GLFW_PRESS) moveA = true;
	if (key == GLFW_KEY_A && action == GLFW_RELEASE) moveA = false;
	if (key == GLFW_KEY_S && action == GLFW_PRESS) moveS = true;
	if (key == GLFW_KEY_S && action == GLFW_RELEASE) moveS = false;
	if (key == GLFW_KEY_D && action == GLFW_PRESS) moveD = true;
	if (key == GLFW_KEY_D && action == GLFW_RELEASE) moveD = false;
	if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
		selectedMeshScale = 1.0f;
		selectedMeshAngle = 0.0f;
		selectedMeshPosition = glm::vec2(0.0f, 0.0f);
		currentlySelectedMesh++;
	}
	if (key == GLFW_KEY_1 && action == GLFW_PRESS) selectedMeshScale += 0.2f;
	if (key == GLFW_KEY_2 && action == GLFW_PRESS) selectedMeshScale -= 0.2f;
	if (key == GLFW_KEY_3 && action == GLFW_PRESS) selectedMeshAngle += 0.2f;
	if (key == GLFW_KEY_4 && action == GLFW_PRESS) selectedMeshAngle -= 0.2f;
	if (key == GLFW_KEY_UP && action == GLFW_PRESS) selectedMeshPosition.y += 0.3f;
	if (key == GLFW_KEY_DOWN && action == GLFW_PRESS) selectedMeshPosition.y -= 0.3f;
	if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) selectedMeshPosition.x += 0.3f;
	if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) selectedMeshPosition.x -= 0.3f;
	if (key == GLFW_KEY_F1 && action == GLFW_PRESS) showCurves = !showCurves;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float offsetx = xpos - lastX;
	float offsety = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	offsetx *= globalConfig.sensitivity;
	offsety *= globalConfig.sensitivity;

	pitch += offsety;
	yaw += offsetx;

	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	globalConfig.cameraFront = glm::normalize(front);
}

// Esta funусo estр bastante harcoded - objetivo ж criar os buffers que armazenam a 
// geometria de um triРngulo
// Apenas atributo coordenada nos vжrtices
// 1 VBO com as coordenadas, VAO com apenas 1 ponteiro para atributo
// A funусo retorna o identificador do VAO
int setupGeometry(std::vector<Vertex>& vertices) {
	GLuint VBO, VAO;

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// Positioning
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	// Texture
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	// Color
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(5 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	// Normal
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(8 * sizeof(GLfloat)));
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return VAO;
}

/**
	Gera pontos de controle que quando combinados, gerarсo um cьrculo ao redor do ponto de referЖncia
	("referencePoint") com raio "radius". O ponto de referencia ж um valor no espaуo 3d (x, y, z)
*/
std::vector<glm::vec3> generateCircleControlPoints(glm::vec3 referencePoint, float radius) {
	std::vector <glm::vec3> controlPoints;

	float refX = referencePoint.x;
	float refY = referencePoint.y;
	float refZ = referencePoint.z;

	// Como estou usando curvas de Bezier para fazer o cьrculo, alguns cрlculos precisam ser feitos
	// Basicamente, 4 pontos sсo criados a partir do ponto de referЖncia para formar um quadrado. A distРncia
	// do ponto de ref ж o radius. Esse quadrado serр usado para criar o cьrculo.
	// Depois que tenho os 4 pontos de controle, crio 2 pontos novos para cada 
	// (um posicionado "antes" e outro "depois" do ponto de controle). Esses pontos sсo usados como pontos
	// auxiliares para direcionar a curva. Assim, para criar o cьrculo, o primeiro e o Щltimo ponto sсo os pontos de controle
	// P0, P1, P2 e P3 e os pontos do meio que direcionam a curva. Assim, nessa ordem, sсo criados 4 curvas que formam os 4 "quadrantes"
	// ou segmentos do cьrculo que quero gerar. 

	// Esse valor ж um valor especьfico que, quando multiplicado pelo raio que estou usando, darр as curvas que mais se parecerсo
	// com os 4 segmentos de um cьrculo. Fonte: https://stackoverflow.com/questions/1734745/how-to-create-circle-with-b%C3%A9zier-curves
	float auxControlPoint = 0.552284749831f * radius;

	glm::vec3 P0 = glm::vec3(refX + radius, refY, refZ + radius);   // Top (vжrtice do quadrado)
	glm::vec3 P0Aux0 = glm::vec3(P0.x - auxControlPoint, P0.y, P0.z + auxControlPoint);
	glm::vec3 P0Aux1 = glm::vec3(P0.x + auxControlPoint, P0.y, P0.z - auxControlPoint);

	glm::vec3 P1 = glm::vec3(refX + radius, refY, refZ - radius);    // Left
	glm::vec3 P1Aux0 = glm::vec3(P1.x + auxControlPoint, P1.y, P1.z + auxControlPoint);
	glm::vec3 P1Aux1 = glm::vec3(P1.x - auxControlPoint, P1.y, P1.z - auxControlPoint);

	glm::vec3 P2 = glm::vec3(refX - radius, refY, refZ - radius);   // Bottom
	glm::vec3 P2Aux0 = glm::vec3(P2.x + auxControlPoint, P2.y, P2.z - auxControlPoint);
	glm::vec3 P2Aux1 = glm::vec3(P2.x - auxControlPoint, P2.y, P2.z + auxControlPoint);

	glm::vec3 P3 = glm::vec3(refX - radius, refY, refZ + radius);   // Right
	glm::vec3 P3Aux0 = glm::vec3(P3.x - auxControlPoint, P3.y, P3.z - auxControlPoint);
	glm::vec3 P3Aux1 = glm::vec3(P3.x + auxControlPoint, P3.y, P3.z + auxControlPoint);

	// Primeira curva do cьrculo (P0 a P1)...
	controlPoints.push_back(P0);
	controlPoints.push_back(P0Aux1);
	controlPoints.push_back(P1Aux0);
	controlPoints.push_back(P1);

	controlPoints.push_back(P1Aux1);
	controlPoints.push_back(P2Aux0);
	controlPoints.push_back(P2);

	controlPoints.push_back(P2Aux1);
	controlPoints.push_back(P3Aux0);
	controlPoints.push_back(P3);

	controlPoints.push_back(P3Aux1);
	controlPoints.push_back(P0Aux0);
	controlPoints.push_back(P0);

	return controlPoints;
}

GLuint generateControlPointsBuffer(std::vector<glm::vec3> controlPoints) {

	GLuint VBO, VAO;

	//Geraусo do identificador do VBO
	glGenBuffers(1, &VBO);

	//Faz a conexсo (vincula) do buffer como um buffer de array
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	//Envia os dados do array de floats para o buffer da OpenGl
	glBufferData(GL_ARRAY_BUFFER, controlPoints.size() * sizeof(GLfloat) * 3, controlPoints.data(), GL_STATIC_DRAW);

	//Geraусo do identificador do VAO (Vertex Array Object)
	glGenVertexArrays(1, &VAO);

	// Vincula (bind) o VAO primeiro, e em seguida  conecta e seta o(s) buffer(s) de vжrtices
	// e os ponteiros para os atributos 
	glBindVertexArray(VAO);

	//Atributo posiусo (x, y, z)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	// Observe que isso ж permitido, a chamada para glVertexAttribPointer registrou o VBO como o objeto de buffer de vжrtice 
	// atualmente vinculado - para que depois possamos desvincular com seguranуa
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Desvincula o VAO (ж uma boa prрtica desvincular qualquer buffer ou array para evitar bugs medonhos)
	glBindVertexArray(0);

	return VAO;
}

BezierCurve createBezierCurve(std::vector <glm::vec3> controlPoints, int pointsPerSegment) {

	glm::mat4 M = glm::mat4(
		-1, 3, -3, 1,
		3, -6, 3, 0,
		-3, 3, 0, 0,
		1, 0, 0, 0
	);

	BezierCurve bezierCurve;
	GLuint VAO;
	std::vector<glm::vec3> curvePoints;
	float step = 1.0 / (float)pointsPerSegment;
	float t = 0;
	int nControlPoints = controlPoints.size();

	for (int i = 0; i < nControlPoints - 3; i += 3)
	{
		for (float t = 0.0; t <= 1.0; t += step)
		{
			glm::vec3 p;

			glm::vec4 T(t * t * t, t * t, t, 1);

			glm::vec3 P0 = controlPoints[i];
			glm::vec3 P1 = controlPoints[i + 1];
			glm::vec3 P2 = controlPoints[i + 2];
			glm::vec3 P3 = controlPoints[i + 3];

			glm::mat4x3 G(P0, P1, P2, P3);

			p = G * M * T;  //---------

			curvePoints.push_back(p);
		}
	}


	//Gera o VAO
	GLuint VBO;

	//Geraусo do identificador do VBO
	glGenBuffers(1, &VBO);

	//Faz a conexсo (vincula) do buffer como um buffer de array
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	//Envia os dados do array de floats para o buffer da OpenGl
	glBufferData(GL_ARRAY_BUFFER, curvePoints.size() * sizeof(GLfloat) * 3, curvePoints.data(), GL_STATIC_DRAW);

	//Geraусo do identificador do VAO (Vertex Array Object)
	glGenVertexArrays(1, &VAO);

	// Vincula (bind) o VAO primeiro, e em seguida  conecta e seta o(s) buffer(s) de vжrtices
	// e os ponteiros para os atributos 
	glBindVertexArray(VAO);

	//Atributo posiусo (x, y, z)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	// Observe que isso ж permitido, a chamada para glVertexAttribPointer registrou o VBO como o objeto de buffer de vжrtice 
	// atualmente vinculado - para que depois possamos desvincular com seguranуa
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Desvincula o VAO (ж uma boa prрtica desvincular qualquer buffer ou array para evitar bugs medonhos)
	glBindVertexArray(0);

	bezierCurve.VAO = VAO;
	bezierCurve.curvePoints = curvePoints;

	return bezierCurve;
}