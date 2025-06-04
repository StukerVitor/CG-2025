// File: main.cpp
#include "GeometryObjects.hpp"

// *** BIBLIOTECAS PADRÃO *** ------------------------------------------------
#include <iostream>
#include <string>
#include <cassert>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Shader.h"

// ============================================================================
// ESTRUTURAS DE DADOS AUXILIARES (para renderização e lógica do aplicativo)
// ============================================================================

// Configuração global do aplicativo (luz, câmera, parâmetros de fog, etc.)
struct GlobalConfig {
    glm::vec3 lightPos, lightColor;   // Posição e cor da luz
    glm::vec3 cameraPos, cameraFront; // Posição e direção da câmera
    GLfloat   fov, nearPlane, farPlane; // Projeção perspectiva
    GLfloat   sensitivity, cameraSpeed; // Sensibilidade e velocidade da câmera

    // Requisito 1: Parâmetros de atenuação e fog
    float   attConstant, attLinear, attQuadratic; // Fatores de atenuação
    glm::vec3 fogColor;                            // Cor do nevoeiro
    float   fogStart, fogEnd;                      // Distâncias de início e fim do nevoeiro
};

// Estrutura para curvas B-Spline
struct BSplineCurve {
    std::string name;
    std::vector<glm::vec3> controlPoints; // Pontos de controle
    std::vector<glm::vec3> curvePoints;   // Pontos discretizados da curva
    GLuint pointsPerSegment;
    glm::vec4 color;
    GLuint VAO;
    GLuint controlPointsVAO;
};

// ============================================================================
// PROTÓTIPOS DE FUNÇÕES
// ============================================================================

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

void readSceneFile(const std::string&,
    std::unordered_map<std::string, Object3D>*,
    std::vector<std::string>*,
    std::unordered_map<std::string, BSplineCurve>*,
    GlobalConfig*);

std::vector<glm::vec3> generateBSplinePoints(const std::vector<glm::vec3>& controlPoints, int pointsPerSegment);
GLuint generateControlPointsBuffer(std::vector<glm::vec3> controlPoints);
BSplineCurve createBSplineCurve(std::vector<glm::vec3> controlPoints, int pointsPerSegment);
void generateTrackMesh(const std::vector<glm::vec3> centerPoints, float trackWidth,
    std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);
void exportAnimationPoints(const std::vector<glm::vec3>& points, const std::string& filename);
void generateSceneFile(const std::string& trackObj, const std::string& carObj,
    const std::string& animFile, const std::string& sceneFile,
    const std::vector<glm::vec3>& controlPoints);
float computeAngleBetweenPoints(const glm::vec3& p1, const glm::vec3& p2);

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

GlobalConfig globalConfig;
const GLuint WIDTH = 1000, HEIGHT = 1000;
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
bool      firstMouse = true;
float     lastX, lastY, pitch = 0.0f, yaw = -90.0f;
bool      moveW = false, moveA = false, moveS = false, moveD = false;
bool      editorMode = true;                     // Requisito 2a: Modo editor ativo inicialmente
std::vector<glm::vec3> editorControlPoints;      // Requisito 2a: Pontos de controle clicados
int       animationIndex = 0;                    // Índice para animação do carro
float     trackWidth = 1.0f;                     // Largura da pista
GLuint    showCurves = 1;

// Variáveis que antes ficavam dentro do main ficam agora globais:
std::unordered_map<std::string, Object3D> meshes;                // Agora armazena Object3D
std::vector<std::string>                  meshList;              // Agora global
std::unordered_map<std::string, BSplineCurve> bSplineCurves;      // Agora global

double lastFrameTime = 0.0;
float  animAccumulator = 0.0f;              // Quanto tempo já acumulou para animação
const float STEP_TIME = 1.0f / 30.0f;       // 30 frames de animação por segundo

// ============================================================================
// SHADERS (modelo de iluminação completo: ambiente + difusa + especular + atenuação + fog)
// ============================================================================

const char* vertexShaderSource = R"glsl(
#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 3) in vec3 aNormal;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragPos     = vec3(model * vec4(aPos, 1.0));
    Normal      = mat3(transpose(inverse(model))) * aNormal;
    TexCoord    = aTexCoord;
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 450 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 cameraPos;
uniform float kaR, kaG, kaB;
uniform float kdR, kdG, kdB;
uniform float ksR, ksG, ksB;
uniform float ns;
uniform vec3 fogColor;
uniform float fogStart, fogEnd;
uniform float attConstant, attLinear, attQuadratic;
uniform sampler2D tex;

void main() {
    // Ambient
    vec3 ambient = vec3(kaR, kaG, kaB) * lightColor;

    // Diffuse
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = vec3(kdR, kdG, kdB) * diff * lightColor;

    // Specular
    vec3 viewDir    = normalize(cameraPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), ns);
    vec3 specular   = vec3(ksR, ksG, ksB) * spec * lightColor;

    // Atenuação
    float distance    = length(lightPos - FragPos);
    float attenuation = 1.0 / (attConstant + attLinear * distance + attQuadratic * distance * distance);

    // Combina iluminação
    vec3 lighting = (ambient + diffuse + specular) * attenuation;

    // Cor da textura
    vec4 texColor = texture(tex, TexCoord);

    // Fog
    float distToCamera = length(cameraPos - FragPos);
    float fogFactor    = clamp((fogEnd - distToCamera) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 finalColor    = mix(fogColor, lighting * texColor.rgb, fogFactor);

    FragColor = vec4(finalColor, texColor.a);
}
)glsl";

// ============================================================================
// FUNÇÃO PRINCIPAL
// ============================================================================

int main() {
    // Inicialização do GLFW e GLAD
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Modelador de Pistas e Visualizador 3D", nullptr, nullptr);
    assert(window && "Falha ao criar janela GLFW");
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Requisito 2a: Cursor visível no editor

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Falha ao inicializar GLAD\n";
        return -1;
    }
    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);

    // Compilação dos shaders
    Shader objectShader(vertexShaderSource, fragmentShaderSource, true);
    Shader lineShader("../shaders/Line.vs", "../shaders/Line.fs");

    // Configuração inicial da câmera e luz (valores padrão)
    globalConfig.cameraPos = glm::vec3(0.0f, 5.0f, 10.0f);
    globalConfig.cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    globalConfig.lightPos = glm::vec3(10.0f, 10.0f, 10.0f);
    globalConfig.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    globalConfig.fov = 45.0f;
    globalConfig.nearPlane = 0.1f;
    globalConfig.farPlane = 100.0f;
    globalConfig.sensitivity = 0.1f;
    globalConfig.cameraSpeed = 0.05f;
    globalConfig.attConstant = 1.0f;
    globalConfig.attLinear = 0.09f;
    globalConfig.attQuadratic = 0.032f;
    globalConfig.fogColor = glm::vec3(0.5f, 0.5f, 0.5f);
    globalConfig.fogStart = 5.0f;
    globalConfig.fogEnd = 50.0f;

    lastFrameTime = glfwGetTime();
    animAccumulator = 0.0f;

    // Loop principal
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPointSize(10);

        double now = glfwGetTime();
        float  deltaTime = static_cast<float>(now - lastFrameTime);
        lastFrameTime = now;
        animAccumulator += deltaTime;

        // Atualiza câmera e projeção
        glm::mat4 view = glm::lookAt(globalConfig.cameraPos,
            globalConfig.cameraPos + globalConfig.cameraFront,
            cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(globalConfig.fov),
            static_cast<float>(WIDTH) / HEIGHT,
            globalConfig.nearPlane,
            globalConfig.farPlane);

        if (editorMode) {
            // Requisito 2a: Desenha pontos de controle no editor 2D (plano XY)
            glUseProgram(lineShader.getId());
            glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            if (!editorControlPoints.empty()) {
                GLuint VAO = generateControlPointsBuffer(editorControlPoints);
                glUniform4f(glGetUniformLocation(lineShader.getId(), "finalColor"), 1.0f, 1.0f, 0.0f, 1.0f);
                glBindVertexArray(VAO);
                glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(editorControlPoints.size()));
                glBindVertexArray(0);
                glDeleteVertexArrays(1, &VAO);
            }
        }
        else {
            // Movimentação de câmera durante animação
            if (moveW)
                globalConfig.cameraPos += globalConfig.cameraFront * globalConfig.cameraSpeed;
            if (moveA)
                globalConfig.cameraPos -= glm::normalize(glm::cross(globalConfig.cameraFront, cameraUp)) * globalConfig.cameraSpeed;
            if (moveS)
                globalConfig.cameraPos -= globalConfig.cameraFront * globalConfig.cameraSpeed;
            if (moveD)
                globalConfig.cameraPos += glm::normalize(glm::cross(globalConfig.cameraFront, cameraUp)) * globalConfig.cameraSpeed;

            // Requisito 3: Visualizador 3D
            glUseProgram(objectShader.getId());
            glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3fv(glGetUniformLocation(objectShader.getId(), "lightPos"), 1, glm::value_ptr(globalConfig.lightPos));
            glUniform3fv(glGetUniformLocation(objectShader.getId(), "lightColor"), 1, glm::value_ptr(globalConfig.lightColor));
            glUniform3fv(glGetUniformLocation(objectShader.getId(), "cameraPos"), 1, glm::value_ptr(globalConfig.cameraPos));
            glUniform3fv(glGetUniformLocation(objectShader.getId(), "fogColor"), 1, glm::value_ptr(globalConfig.fogColor));
            glUniform1f(glGetUniformLocation(objectShader.getId(), "fogStart"), globalConfig.fogStart);
            glUniform1f(glGetUniformLocation(objectShader.getId(), "fogEnd"), globalConfig.fogEnd);
            glUniform1f(glGetUniformLocation(objectShader.getId(), "attConstant"), globalConfig.attConstant);
            glUniform1f(glGetUniformLocation(objectShader.getId(), "attLinear"), globalConfig.attLinear);
            glUniform1f(glGetUniformLocation(objectShader.getId(), "attQuadratic"), globalConfig.attQuadratic);

            // Desenha todas as Object3D carregadas
            float lastCarYaw = 0.0f;
            for (auto& pair : meshes) {
                Object3D& obj = pair.second;
                glm::mat4  model(1.0f);

                // Requisito 3d: Animação do carro
                if (obj.name == "Carro" && obj.animationPositions.size() >= 3) {
                    int N = static_cast<int>(obj.animationPositions.size());
                    int idx = animationIndex % N;
                    int prevIdx = (idx - 1 + N) % N;
                    int nextIdx = (idx + 1) % N;

                    glm::vec3 prev = obj.animationPositions[prevIdx];
                    glm::vec3 next = obj.animationPositions[nextIdx];
                    glm::vec3 dir = glm::normalize(next - prev);

                    if (glm::length(dir) > 1e-6f) {
                        // Cálculo do yaw para alinhar o carro com a tangente
                        float rawYaw = atan2(dir.x, dir.z);

                        // Ajuste para evitar pulos maiores que π
                        float delta = rawYaw - lastCarYaw;
                        if (delta > glm::pi<float>()) rawYaw -= 2.0f * glm::pi<float>();
                        else if (delta < -glm::pi<float>()) rawYaw += 2.0f * glm::pi<float>();

                        lastCarYaw = rawYaw;

                        // Posiciona e rotaciona
                        model = glm::translate(model, obj.animationPositions[idx]);
                        model = glm::rotate(model, rawYaw, glm::vec3(0, 1, 0));
                    }
                    else {
                        // fallback: apenas posiciona
                        model = glm::translate(model, obj.animationPositions[idx]);
                    }
                }
                else {
                    model = glm::translate(model, obj.position);
                }

                // Aplica rotações em X, Y, Z
                model = glm::rotate(model, glm::radians(obj.angle.x), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(obj.angle.y), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, glm::radians(obj.angle.z), glm::vec3(0.0f, 0.0f, 1.0f));
                // Aplica escala
                model = glm::scale(model, obj.scale);

                glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "model"), 1, GL_FALSE, glm::value_ptr(model));
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kaR"), obj.material.kaR);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kaG"), obj.material.kaG);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kaB"), obj.material.kaB);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kdR"), obj.material.kdR);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kdG"), obj.material.kdG);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kdB"), obj.material.kdB);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "ksR"), obj.material.ksR);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "ksG"), obj.material.ksG);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "ksB"), obj.material.ksB);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "ns"), obj.material.ns);

                // Renderiza o Object3D
                GLuint vao = obj.getMesh().VAO;
                size_t vertCount = obj.getMesh().vertices.size();
                glBindVertexArray(vao);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, obj.textureID);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertCount));
                glBindVertexArray(0);
            }

            // Desenha curvas B-Spline
            if (showCurves) {
                glUseProgram(lineShader.getId());
                glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
                glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));
                for (const auto& pair : bSplineCurves) {
                    const BSplineCurve& bc = pair.second;
                    glUniform4fv(glGetUniformLocation(lineShader.getId(), "finalColor"), 1, glm::value_ptr(bc.color));
                    glBindVertexArray(bc.VAO);
                    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(bc.curvePoints.size()));
                    glBindVertexArray(0);

                    // Pontos de controle em amarelo
                    glUniform4f(glGetUniformLocation(lineShader.getId(), "finalColor"), 1.0f, 1.0f, 0.0f, 1.0f);
                    glBindVertexArray(bc.controlPointsVAO);
                    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(bc.controlPoints.size()));
                    glBindVertexArray(0);
                }
            }

            // Atualiza a animação do carro
            if (!meshes["Carro"].animationPositions.empty()) {
                while (animAccumulator >= STEP_TIME) {
                    animationIndex = (animationIndex + 1) % meshes["Carro"].animationPositions.size();
                    animAccumulator -= STEP_TIME;
                }
            }
        }

        glfwSwapBuffers(window);
    }

    // Liberação de recursos
    for (const auto& pair : meshes) {
        glDeleteVertexArrays(1, &pair.second.getMesh().VAO);
    }
    for (const auto& pair : bSplineCurves) {
        glDeleteVertexArrays(1, &pair.second.VAO);
        glDeleteVertexArrays(1, &pair.second.controlPointsVAO);
    }
    glfwTerminate();
    return 0;
}

// ============================================================================
// CALLBACKS DE INPUT E LÓGICA DE MOUSE
// ============================================================================

// Requisito 2a: Captura de cliques do mouse para pontos de controle no editor 2D
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (editorMode && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // Obtem a matriz de projeção e view atuais
        glm::mat4 view = glm::lookAt(globalConfig.cameraPos,
            globalConfig.cameraPos + globalConfig.cameraFront,
            cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(globalConfig.fov),
            static_cast<float>(WIDTH) / HEIGHT,
            globalConfig.nearPlane,
            globalConfig.farPlane);

        // Converte coordenadas da tela para NDC
        float x_ndc = (2.0f * static_cast<float>(xpos)) / WIDTH - 1.0f;
        float y_ndc = 1.0f - (2.0f * static_cast<float>(ypos)) / HEIGHT;

        // Coordenadas de clipe
        glm::vec4 clipCoords = glm::vec4(x_ndc, y_ndc, -1.0f, 1.0f);

        // Inversão da projeção
        glm::vec4 eyeCoords = glm::inverse(projection) * clipCoords;
        eyeCoords.z = -1.0f;
        eyeCoords.w = 0.0f;

        // Inversão da view para obter direção no mundo
        glm::vec3 rayDir = glm::normalize(glm::vec3(glm::inverse(view) * eyeCoords));
        glm::vec3 rayOrigin = globalConfig.cameraPos;

        // Interseção do raio com o plano XY (z = 0)
        float t = -rayOrigin.z / rayDir.z;
        glm::vec3 intersectPoint = rayOrigin + t * rayDir;

        editorControlPoints.push_back(intersectPoint);
    }
}

// Requisito 2b: Geração de curva B-Spline a partir dos pontos de controle
std::vector<glm::vec3> generateBSplinePoints(const std::vector<glm::vec3>& controlPoints, int pointsPerSegment) {
    std::vector<glm::vec3> curvePoints;
    if (controlPoints.size() < 4)
        return curvePoints; // Mínimo 4 pontos para B-Spline cúbica

    const glm::mat4 BSplineMatrix(
        -1.0f / 6.0f, 3.0f / 6.0f, -3.0f / 6.0f, 1.0f / 6.0f,
        3.0f / 6.0f, -6.0f / 6.0f, 3.0f / 6.0f, 0.0f,
        -3.0f / 6.0f, 0.0f, 3.0f / 6.0f, 0.0f,
        1.0f / 6.0f, 4.0f / 6.0f, 1.0f / 6.0f, 0.0f
    );

    float step = 1.0f / pointsPerSegment;
    for (size_t i = 0; i + 3 < controlPoints.size(); ++i) {
        for (float t = 0.0f; t <= 1.0f; t += step) {
            glm::vec4 T(t * t * t, t * t, t, 1.0f);
            glm::mat4x3 G(controlPoints[i],
                controlPoints[i + 1],
                controlPoints[i + 2],
                controlPoints[i + 3]);
            curvePoints.push_back(G * BSplineMatrix * T);
        }
    }
    return curvePoints;
}

// Requisito 2c, 2d: Geração das curvas interna e externa da pista
void generateTrackMesh(const std::vector<glm::vec3> centerPoints,
                       float trackWidth,
                       std::vector<Vertex>& vertices,
                       std::vector<unsigned int>& indices)
{
    vertices.clear();
    indices.clear();
    const float halfWidth = trackWidth * 0.5f;
    size_t n = centerPoints.size();
    if (n < 2) return;

    // 1) Pre‐compute “inner” e “outer” offsets no plano XY
    std::vector<glm::vec3> innerPoints(n), outerPoints(n);
    for (size_t i = 0; i < n; ++i) {
        glm::vec3 A = centerPoints[i];
        glm::vec3 B = centerPoints[(i + 1) % n];

        float dx = B.x - A.x;
        float dy = B.y - A.y;
        float theta = atan2(dy, dx);
        // ângulo perpendicular em relação ao segmento AB
        float alpha = (dx < 0.0f)
            ? theta - glm::radians(90.0f)
            : theta + glm::radians(90.0f);

        float cosA = cos(alpha);
        float sinA = sin(alpha);
        glm::vec3 offset(cosA * halfWidth, sinA * halfWidth, 0.0f);

        innerPoints[i] = A + offset;  // borda interna
        outerPoints[i] = A - offset;  // borda externa
    }

    // 2) Para cada segmento, crie um “quad” com 4 vértices (v0,v1,v2,v3)
    //    e 2 triângulos, usando sempre as mesmas UVs: (0,0),(1,0),(1,1),(0,1).
    //    Normal fixa = (0,0,1).
    for (size_t i = 0; i < n; ++i) {
        size_t next = (i + 1) % n;

        glm::vec3 A = outerPoints[i];
        glm::vec3 B = outerPoints[next];
        glm::vec3 C = innerPoints[i];
        glm::vec3 D = innerPoints[next];

        // normal “para cima” (já que track está em Z=0)
        glm::vec3 upNormal(0.0f, 0.0f, 1.0f);

        // constrói 4 vértices do quad (sempre na ordem C, A, B, D):
        Vertex v0 = { C.x, C.y, C.z,  0.0f, 0.0f,  upNormal.x, upNormal.y, upNormal.z }; // inner[i]
        Vertex v1 = { A.x, A.y, A.z,  1.0f, 0.0f,  upNormal.x, upNormal.y, upNormal.z }; // outer[i]
        Vertex v2 = { B.x, B.y, B.z,  1.0f, 1.0f,  upNormal.x, upNormal.y, upNormal.z }; // outer[i+1]
        Vertex v3 = { D.x, D.y, D.z,  0.0f, 1.0f,  upNormal.x, upNormal.y, upNormal.z }; // inner[i+1]

        // índice base antes de inserir esses 4 vértices
        unsigned int base = static_cast<unsigned int>(vertices.size());
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);

        // 1º triângulo: (v0=C, v3=D, v1=A)
        indices.push_back(base + 0);
        indices.push_back(base + 3);
        indices.push_back(base + 1);

        // 2º triângulo: (v1=A, v3=D, v2=B)
        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }
}



// Requisito 2i: Exportação dos pontos de animação
void exportAnimationPoints(const std::vector<glm::vec3>& points, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Falha ao criar " << filename << '\n';
        return;
    }

    for (const auto& p : points) {
        // Troca Y↔Z para alinhar com o chão XZ
        file << p.x << ' ' << 0.0f << ' ' << p.y << '\n';
    }
    file.close();
}


// Requisito 2j: Geração do arquivo de cena
void generateSceneFile(const std::string& trackObj, const std::string& carObj, const std::string& animFile,
    const std::string& sceneFile, const std::vector<glm::vec3>& controlPoints)
{
    std::ofstream file(sceneFile);
    file << "Type GlobalConfig Config\n"
        << "LightPos 2.0 10.0 2.0\n"
        << "LightColor 1.0 1.0 1.0\n"
        << "CameraPos 0.0 5.0 10.0\n"
        << "CameraFront 0.0 0.0 -1.0\n"
        << "Fov 45.0\n"
        << "NearPlane 0.1\n"
        << "FarPlane 100.0\n"
        << "Sensitivity 0.1\n"
        << "CameraSpeed 0.008\n"
        << "AttConstant 0.2\n"
        << "AttLinear 0.02\n"
        << "AttQuadratic 0.005\n"
        << "FogColor 0.5 0.5 0.5\n"
        << "FogStart 5.0\n"
        << "FogEnd 50.0\n"
        << "End\n"
        << "Type Mesh Track\n"
        << "Obj " << trackObj << "\n"
        << "Mtl track.mtl\n"
        << "Scale 1.0 1.0 1.0\n"
        << "Position 0.0 0.0 0.0\n"
        << "Rotation 0.0 1.0 0.0\n"
        << "Angle 0.0 0.0 0.0\n"
        << "IncrementalAngle 0\n"
        << "End\n"
        << "Type Mesh Carro\n"
        << "Obj " << carObj << "\n"
        << "Mtl car.mtl\n"
        << "Scale 0.5 0.5 0.5\n"
        << "Position 0.0 0.0 0.0\n"
        << "Rotation 0.0 1.0 0.0\n"
        << "Angle 0.0 0.0 0.0\n"
        << "IncrementalAngle 0\n"
        << "AnimationFile " << animFile << "\n"
        << "End\n"
        << "Type BSplineCurve Curve1\n";
    for (const auto& cp : controlPoints)
    {
        file << "ControlPoint " << cp.x << " " << cp.y << " " << cp.z << "\n";
    }
    file << "PointsPerSegment 100\n"
        << "Color 1.0 0.0 0.0 1.0\n"
        << "End\n";
    file.close();
}

// Requisito 3: Leitura do arquivo de cena e pontos de animação
void readSceneFile(const std::string& sceneFilePath,
    std::unordered_map<std::string, Object3D>* meshes,
    std::vector<std::string>* meshList,
    std::unordered_map<std::string, BSplineCurve>* bSplineCurves,
    GlobalConfig* globalConfig)
{
    std::ifstream file(sceneFilePath);
    std::string line;
    if (!file.is_open())
    {
        std::cerr << "Falha ao abrir o arquivo " << sceneFilePath << std::endl;
        return;
    }

    std::string objectType, name, objFilePath, mtlFilePath, animFile;
    glm::vec3 scale{ 1.0f }, position{ 0.0f }, rotation{ 0.0f }, angle{ 0.0f };
    GLuint incrementalAngle = false;
    std::vector<glm::vec3> tempControlPoints;
    GLuint pointsPerSegment = 0;
    glm::vec4 color{ 1.0f };

    while (getline(file, line))
    {
        std::istringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "Type")
            ss >> objectType >> name;
        else if (type == "LightPos")
            ss >> globalConfig->lightPos.x >> globalConfig->lightPos.y >> globalConfig->lightPos.z;
        else if (type == "LightColor")
            ss >> globalConfig->lightColor.r >> globalConfig->lightColor.g >> globalConfig->lightColor.b;
        else if (type == "CameraPos")
            ss >> globalConfig->cameraPos.x >> globalConfig->cameraPos.y >> globalConfig->cameraPos.z;
        else if (type == "CameraFront")
            ss >> globalConfig->cameraFront.x >> globalConfig->cameraFront.y >> globalConfig->cameraFront.z;
        else if (type == "Fov")
            ss >> globalConfig->fov;
        else if (type == "NearPlane")
            ss >> globalConfig->nearPlane;
        else if (type == "FarPlane")
            ss >> globalConfig->farPlane;
        else if (type == "Sensitivity")
            ss >> globalConfig->sensitivity;
        else if (type == "CameraSpeed")
            ss >> globalConfig->cameraSpeed;
        else if (type == "AttConstant")
            ss >> globalConfig->attConstant;
        else if (type == "AttLinear")
            ss >> globalConfig->attLinear;
        else if (type == "AttQuadratic")
            ss >> globalConfig->attQuadratic;
        else if (type == "FogColor")
            ss >> globalConfig->fogColor.r >> globalConfig->fogColor.g >> globalConfig->fogColor.b;
        else if (type == "FogStart")
            ss >> globalConfig->fogStart;
        else if (type == "FogEnd")
            ss >> globalConfig->fogEnd;
        else if (type == "Obj")
            ss >> objFilePath;
        else if (type == "Mtl")
            ss >> mtlFilePath;
        else if (type == "Scale")
            ss >> scale.x >> scale.y >> scale.z;
        else if (type == "Position")
            ss >> position.x >> position.y >> position.z;
        else if (type == "Rotation")
            ss >> rotation.x >> rotation.y >> rotation.z;
        else if (type == "Angle")
            ss >> angle.x >> angle.y >> angle.z;
        else if (type == "IncrementalAngle")
            ss >> incrementalAngle;
        else if (type == "AnimationFile")
            ss >> animFile;
        else if (type == "ControlPoint")
        {
            glm::vec3 cp;
            ss >> cp.x >> cp.y >> cp.z;
            tempControlPoints.push_back(cp);
        }
        else if (type == "PointsPerSegment")
            ss >> pointsPerSegment;
        else if (type == "Color")
            ss >> color.r >> color.g >> color.b >> color.a;
        else if (type == "End")
        {
            if (objectType == "GlobalConfig")
            {
                // Já preenchido diretamente
            }
            else if (objectType == "Mesh")
            {
                // Cria o Object3D usando o construtor que lê .obj e .mtl
                Object3D obj(
                    /*_name=*/           name,
                    /*_objPath=*/        objFilePath,
                    /*_mtlPath=*/        mtlFilePath,
                    /*scale_=*/          scale,
                    /*pos_=*/            position,
                    /*rot_=*/            rotation,
                    /*ang_=*/            angle,
                    /*incAng=*/          incrementalAngle
                );

                // Carrega pontos de animação (mesmo código de antes):
                if (!animFile.empty()) {
                    std::ifstream anim(animFile);
                    std::string animLine;
                    while (std::getline(anim, animLine)) {
                        std::istringstream ass(animLine);
                        glm::vec3 pos;
                        ass >> pos.x >> pos.y >> pos.z;
                        obj.animationPositions.push_back(pos);
                    }
                    anim.close();
                }

                // Insere no mapa global:
                meshes->insert({ name, obj });
                meshList->push_back(name);
            }
            else if (objectType == "BSplineCurve")
            {
                BSplineCurve bc = createBSplineCurve(tempControlPoints, pointsPerSegment);
                bc.name = name;
                bc.controlPoints = tempControlPoints;
                bc.color = color;
                bc.pointsPerSegment = pointsPerSegment;
                bc.controlPointsVAO = generateControlPointsBuffer(tempControlPoints);
                bSplineCurves->insert(std::make_pair(name, bc));
                tempControlPoints.clear();
            }
        }
    }
    file.close();
}

// Função key_callback ajustada
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
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
    // Requisito 2: Alternar entre editor e visualizador
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        if (editorMode && !editorControlPoints.empty())
        {
            // Gera curva B-Spline, malha e arquivos
            std::vector<glm::vec3> curvePoints = generateBSplinePoints(editorControlPoints, 50);
            std::vector<Vertex> vertices;
            std::vector<unsigned int> indices;
            generateTrackMesh(curvePoints, trackWidth, vertices, indices);

            // Cria um novo vetor de Vertex “trackVerts” aplicando Y↔Z
            std::vector<Vertex> trackVerts;
            trackVerts.reserve(vertices.size());
            for (const auto& v : vertices) {
                // Posição: (x, z, y)
                // Normal: (nx, nz, ny)
                trackVerts.push_back({
                    v.x, v.z, v.y,    // swapped y<->z
                    v.s, v.t,
                    v.nx, v.nz, v.ny  // swapped normal y<->z
                    });
            }
            
            // Constrói diretamente o Mesh de “track”:
            Mesh trackMesh(trackVerts, indices, /*groupName=*/"track", /*mtlName=*/"");

            // Grava “track.obj” com OBJWriter:
            OBJWriter objWriter;
            objWriter.write(trackMesh, "track.obj");

            exportAnimationPoints(curvePoints, "animation.txt");
            generateSceneFile("track.obj", "car.obj", "animation.txt", "Scene.txt", editorControlPoints);
            readSceneFile("Scene.txt", &meshes, &meshList, &bSplineCurves, &globalConfig); // Agora acessa as globais
            editorMode = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!editorMode)
    {
        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }
        float offsetX = static_cast<float>(xpos - lastX) * globalConfig.sensitivity;
        float offsetY = static_cast<float>(lastY - ypos) * globalConfig.sensitivity;
        lastX = xpos;
        lastY = ypos;
        pitch += offsetY;
        yaw += offsetX;
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        globalConfig.cameraFront = glm::normalize(front);
    }
}

GLuint generateControlPointsBuffer(std::vector<glm::vec3> controlPoints)
{
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, controlPoints.size() * sizeof(glm::vec3), controlPoints.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return VAO;
}

BSplineCurve createBSplineCurve(std::vector<glm::vec3> controlPoints, int pointsPerSegment)
{
    BSplineCurve bc;
    bc.curvePoints = generateBSplinePoints(controlPoints, pointsPerSegment);
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, bc.curvePoints.size() * sizeof(glm::vec3), bc.curvePoints.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    bc.VAO = VAO;
    return bc;
}

// Requisito 3: Cálculo do ângulo de rotação do carro
float computeAngleBetweenPoints(const glm::vec3& p1, const glm::vec3& p2)
{
    glm::vec3 dir = p2 - p1; // direção no solo
    return glm::degrees(atan2(dir.z, dir.x));
}
