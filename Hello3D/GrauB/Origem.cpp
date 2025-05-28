// Objetivo: Implementar um modelador de pistas de corrida e visualizador de modelos 3D
// conforme especificado no documento CGR-TrabalhoGB-completo-3.pdf, incluindo editor de
// pistas 2D, geração de curvas B-Spline, malha poligonal da pista, exportação para OBJ,
// e animação de um carro no visualizador 3D com modelo de iluminação completo.

// *** IMPLEMENTAÇÃO DA STB_IMAGE *** -----------------------------------------
#define STB_IMAGE_IMPLEMENTATION
#include "../../Common/include/stb_image.h"

// *** BIBLIOTECAS PADRÃO *** ------------------------------------------------
#include <iostream>
#include <string>
#include <assert.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Shader.h"

// ============================================================================
// ESTRUTURAS DE DADOS
// ============================================================================

struct Vertex
{
    float x, y, z;    // Posição do vértice
    float s, t;       // Coordenadas de textura
    float nx, ny, nz; // Vetor normal
};

struct Material
{
    float kaR, kaG, kaB;     // Coeficiente ambiente
    float kdR, kdG, kdB;     // Coeficiente difuso
    float ksR, ksG, ksB;     // Coeficiente especular
    float ns;                // Expoente especular
    std::string textureName; // Nome do arquivo de textura
};

struct GlobalConfig
{
    glm::vec3 lightPos, lightColor;   // Posição e cor da luz
    glm::vec3 cameraPos, cameraFront; // Posição e direção da câmera
    GLfloat fov, nearPlane, farPlane; // Projeção perspectiva
    GLfloat sensitivity, cameraSpeed; // Sensibilidade e velocidade da câmera
    // Requisito 1: Adicionando parâmetros para atenuação e fog
    float attConstant, attLinear, attQuadratic; // Fatores de atenuação
    glm::vec3 fogColor;                         // Cor do nevoeiro
    float fogStart, fogEnd;                     // Distâncias de início e fim do nevoeiro
};

struct Mesh
{
    std::string name;
    std::string objFilePath, mtlFilePath;
    glm::vec3 scale, position, rotation;
    glm::vec3 angle;
    GLuint incrementalAngle;
    std::vector<Vertex> vertices;
    GLuint VAO;
    Material material;
    GLuint textureID;
    // Requisito 1: Array de transformações para animação
    std::vector<glm::vec3> animationPositions; // Posições para cada passo de animação
};

struct BSplineCurve
{
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
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
void readSceneFile(const std::string sceneFilePath, std::unordered_map<std::string, Mesh> *meshes,
                   std::vector<std::string> *meshList, std::unordered_map<std::string, BSplineCurve> *bSplineCurves,
                   GlobalConfig *globalConfig);
GLuint setupTexture(const std::string path);
std::vector<Vertex> setupObj(const std::string path);
Material setupMtl(const std::string path);
int setupGeometry(std::vector<Vertex> &vertices);
std::vector<glm::vec3> generateBSplinePoints(const std::vector<glm::vec3> &controlPoints, int pointsPerSegment);
GLuint generateControlPointsBuffer(const std::vector<glm::vec3> controlPoints);
BSplineCurve createBSplineCurve(const std::vector<glm::vec3> controlPoints, int pointsPerSegment);
void generateTrackMesh(std::vector<glm::vec3> centerPoints, float trackWidth, std::vector<Vertex> &vertices,
                       std::vector<unsigned int> &indices);
void exportTrackToOBJ(const std::vector<Vertex> &vertices, const std::vector<unsigned int> &indices,
                      const std::string &filename);
void exportAnimationPoints(const std::vector<glm::vec3> &points, const std::string &filename);
float computeAngleBetweenPoints(const glm::vec3 &p1, const glm::vec3 &p2);

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================
GlobalConfig globalConfig;
const GLuint WIDTH = 1000, HEIGHT = 1000;
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
bool firstMouse = true;
float lastX, lastY, pitch = 0.0f, yaw = -90.0f;
bool moveW = false, moveA = false, moveS = false, moveD = false;
bool editorMode = true;                     // Requisito 2a: Modo editor ativo inicialmente
std::vector<glm::vec3> editorControlPoints; // Requisito 2a: Pontos de controle clicados
int animationIndex = 0;                     // Índice para animação do carro
float trackWidth = 2.0f;                    // Largura da pista
GLuint showCurves = 1;

// Adicionando as variáveis que antes estavam no main como globais
std::unordered_map<std::string, Mesh> meshes;                // Agora global
std::vector<std::string> meshList;                           // Agora global
std::unordered_map<std::string, BSplineCurve> bSplineCurves; // Agora global

double lastFrameTime = glfwGetTime();
float  animAccumulator = 0.0f;          // quanto tempo já acumulou
const  float STEP_TIME = 1.0f / 30.0f; // 30 pontos da animação por segundo

// ============================================================================
// SHADERS
// ============================================================================
// Requisito 1: Shader com modelo de iluminação completo (ambiente + difusa + especular + atenuação + fog)
const char *vertexShaderSource = R"glsl(
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
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
}
)glsl";

const char *fragmentShaderSource = R"glsl(
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
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = vec3(kdR, kdG, kdB) * diff * lightColor;

    // Specular
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), ns);
    vec3 specular = vec3(ksR, ksG, ksB) * spec * lightColor;

    // Attenuation
    float distance = length(lightPos - FragPos);
    float attenuation = 1.0 / (attConstant + attLinear * distance + attQuadratic * distance * distance);

    // Combine lighting
    vec3 lighting = (ambient + diffuse + specular) * attenuation;

    // Texture
    vec4 texColor = texture(tex, TexCoord);

    // Fog
    float distToCamera = length(cameraPos - FragPos);
    float fogFactor = clamp((fogEnd - distToCamera) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 finalColor = mix(fogColor, lighting * texColor.rgb, fogFactor);

    FragColor = vec4(finalColor, texColor.a);
}
)glsl";

// ============================================================================
// FUNÇÃO PRINCIPAL
// ============================================================================
int main()
{
    // Inicialização do GLFW e GLAD
    glfwInit();
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Modelador de Pistas e Visualizador 3D", nullptr, nullptr);
    assert(window && "Falha ao criar janela GLFW");
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Requisito 2a: Cursor visível no editor
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
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

    // Requisito 2a: Modo editor para clicar pontos de controle
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPointSize(10);

        double now = glfwGetTime();
        float  deltaTime = static_cast<float>(now - lastFrameTime);
        lastFrameTime = now;
        animAccumulator += deltaTime;

        // Atualiza câmera
        glm::mat4 view = glm::lookAt(globalConfig.cameraPos, globalConfig.cameraPos + globalConfig.cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(globalConfig.fov),
                                                static_cast<float>(WIDTH) / HEIGHT,
                                                globalConfig.nearPlane, globalConfig.farPlane);

        if (editorMode)
        {
            // Requisito 2a: Desenha pontos de controle no editor 2D (plano XY)
            glUseProgram(lineShader.getId());
            glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            if (!editorControlPoints.empty())
            {
                GLuint VAO = generateControlPointsBuffer(editorControlPoints);
                glUniform4f(glGetUniformLocation(lineShader.getId(), "finalColor"), 1.0f, 1.0f, 0.0f, 1.0f);
                glBindVertexArray(VAO);
                glDrawArrays(GL_POINTS, 0, editorControlPoints.size());
                glBindVertexArray(0);
                glDeleteVertexArrays(1, &VAO);
            }
        }
        else
        {
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

            // Desenha malhas
            for (auto &pair : meshes)
            {
                Mesh &mesh = pair.second;
                glm::mat4 model(1.0f);

                // Requisito 3d: Animação do carro
                if (mesh.name == "Carro" && !mesh.animationPositions.empty())
                {
                    mesh.position = mesh.animationPositions[animationIndex % mesh.animationPositions.size()];
                    model = glm::translate(model, mesh.position);
                    if (animationIndex + 1 < mesh.animationPositions.size())
                    {
                        float angle = computeAngleBetweenPoints(
                            mesh.animationPositions[animationIndex % mesh.animationPositions.size()],
                            mesh.animationPositions[(animationIndex + 1) % mesh.animationPositions.size()]);
                        model = glm::rotate(model, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
                    }
                }
                else
                {
                    model = glm::translate(model, mesh.position);
                }
                model = glm::rotate(model, glm::radians(mesh.angle.x), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(mesh.angle.y), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, glm::radians(mesh.angle.z), glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, mesh.scale);

                glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "model"), 1, GL_FALSE, glm::value_ptr(model));
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

                glBindVertexArray(mesh.VAO);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mesh.textureID);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.vertices.size()));
                glBindVertexArray(0);
            }

            // Desenha curvas B-Spline
            if (showCurves)
            {
                glUseProgram(lineShader.getId());
                glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
                glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));
                for (const auto &pair : bSplineCurves)
                {
                    const BSplineCurve &bc = pair.second;
                    glUniform4fv(glGetUniformLocation(lineShader.getId(), "finalColor"), 1, glm::value_ptr(bc.color));
                    glBindVertexArray(bc.VAO);
                    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(bc.curvePoints.size()));
                    glBindVertexArray(0);

                    glUniform4f(glGetUniformLocation(lineShader.getId(), "finalColor"), 1.0f, 1.0f, 0.0f, 1.0f);
                    glBindVertexArray(bc.controlPointsVAO);
                    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(bc.controlPoints.size()));
                    glBindVertexArray(0);
                }
            }

            // Atualiza animação
            while (animAccumulator >= STEP_TIME)          // avança só quando passar o step
            {
                animationIndex =
                    (animationIndex + 1) % meshes["Carro"].animationPositions.size();
                animAccumulator -= STEP_TIME;
            }
        }

        glfwSwapBuffers(window);
    }

    // Liberação de recursos
    for (const auto &pair : meshes)
        glDeleteVertexArrays(1, &pair.second.VAO);
    for (const auto &pair : bSplineCurves)
        glDeleteVertexArrays(1, &pair.second.VAO);
    glfwTerminate();
    return 0;
}

// Requisito 2a: Captura de cliques do mouse para pontos de controle no editor 2D
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (editorMode && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        // Converte coordenadas de tela para coordenadas do mundo (plano XY)
        float x = (2.0f * xpos) / WIDTH - 1.0f;
        float y = 1.0f - (2.0f * ypos) / HEIGHT;
        // Ajusta para o plano XY (z = 0)
        editorControlPoints.push_back(glm::vec3(x * 10.0f, y * 10.0f, 0.0f));
    }
}

// Requisito 2b: Geração de curva B-Spline a partir dos pontos de controle
std::vector<glm::vec3> generateBSplinePoints(const std::vector<glm::vec3> &controlPoints, int pointsPerSegment)
{
    std::vector<glm::vec3> curvePoints;
    if (controlPoints.size() < 4)
        return curvePoints; // Mínimo 4 pontos para B-Spline cúbica

    const glm::mat4 BSplineMatrix(
        -1.0f / 6.0f, 3.0f / 6.0f, -3.0f / 6.0f, 1.0f / 6.0f,
        3.0f / 6.0f, -6.0f / 6.0f, 3.0f / 6.0f, 0.0f,
        -3.0f / 6.0f, 0.0f, 3.0f / 6.0f, 0.0f,
        1.0f / 6.0f, 4.0f / 6.0f, 1.0f / 6.0f, 0.0f);

    float step = 1.0f / pointsPerSegment;
    for (size_t i = 0; i < controlPoints.size() - 3; ++i)
    {
        for (float t = 0.0f; t <= 1.0f; t += step)
        {
            glm::vec4 T(t * t * t, t * t, t, 1.0f);
            glm::mat4x3 G(controlPoints[i], controlPoints[i + 1], controlPoints[i + 2], controlPoints[i + 3]);
            curvePoints.push_back(G * BSplineMatrix * T);
        }
    }
    return curvePoints;
}

// Requisito 2c, 2d: Geração das curvas interna e externa da pista
void generateTrackMesh(const std::vector<glm::vec3> centerPoints,
                       float trackWidth,
                       std::vector<Vertex> &vertices,
                       std::vector<unsigned int> &indices)
{
    vertices.clear();
    indices.clear();
    const float half = trackWidth * 0.5f;

    for (size_t i = 0; i < centerPoints.size(); ++i)
    {
        glm::vec3 p = centerPoints[i];
        glm::vec3 p2 = centerPoints[(i + 1) % centerPoints.size()];

        // vetor tangente e normal (plano XY)
        glm::vec3 t = glm::normalize(p2 - p);
        glm::vec3 n = glm::normalize(glm::vec3(-t.y, t.x, 0.0f));

        glm::vec3 inner = p - n * half;
        glm::vec3 outer = p + n * half;
        glm::vec3 inner2 = p2 - n * half;
        glm::vec3 outer2 = p2 + n * half;

        // normal da face (0,0,1) -> depois do swap vira (0,1,0)
        glm::vec3 faceN = glm::vec3(0.0f, 0.0f, 1.0f);

        Vertex v[4] = {
            {inner.x, inner.y, inner.z, 0.0f, 0.0f, faceN.x, faceN.y, faceN.z},
            {outer.x, outer.y, outer.z, 1.0f, 0.0f, faceN.x, faceN.y, faceN.z},
            {inner2.x, inner2.y, inner2.z, 0.0f, 1.0f, faceN.x, faceN.y, faceN.z},
            {outer2.x, outer2.y, outer2.z, 1.0f, 1.0f, faceN.x, faceN.y, faceN.z}};

        // adiciona vértices
        size_t base = vertices.size();
        vertices.insert(vertices.end(), std::begin(v), std::end(v));

        // dois triângulos
        indices.insert(indices.end(),
                       {static_cast<unsigned>(base + 0), static_cast<unsigned>(base + 1), static_cast<unsigned>(base + 2),
                        static_cast<unsigned>(base + 2), static_cast<unsigned>(base + 1), static_cast<unsigned>(base + 3)});
    }
}

// Requisito 2h: Exportação da malha da pista para OBJ
void exportTrackToOBJ(const std::vector<Vertex> &vertices, const std::vector<unsigned int> &indices,
                      const std::string &filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Falha ao criar arquivo OBJ " << filename << std::endl;
        return;
    }

    // Requisito 2h: Inversão de Y com Z (plano XZ como chão)
    for (const auto &v : vertices)
    {
        file << "v " << v.x << " " << v.z << " " << v.y << "\n";
    }
    for (const auto &v : vertices)
    {
        file << "vt " << v.s << " " << v.t << "\n";
    }
    for (const auto &v : vertices)
    {
        file << "vn " << v.nx << " " << v.ny << " " << v.nz << "\n";
    }
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        file << "f " << (indices[i] + 1) << "/" << (indices[i] + 1) << "/" << (indices[i] + 1) << " "
             << (indices[i + 1] + 1) << "/" << (indices[i + 1] + 1) << "/" << (indices[i + 1] + 1) << " "
             << (indices[i + 2] + 1) << "/" << (indices[i + 2] + 1) << "/" << (indices[i + 2] + 1) << "\n";
    }
    file.close();
}

// Requisito 2i: Exportação dos pontos de animação
void exportAnimationPoints(const std::vector<glm::vec3> &points,
                           const std::string &filename)
{
    std::ofstream file(filename);
    if (!file)
    {
        std::cerr << "Falha ao criar " << filename << '\n';
        return;
    }

    for (const auto &p : points)
        //  swap Y→Z para alinhar com o chão XZ
        file << p.x << ' ' << 0.0f << ' ' << p.y << '\n';
}

// Requisito 2j: Geração do arquivo de cena
void generateSceneFile(const std::string &trackObj, const std::string &carObj, const std::string &animFile,
                       const std::string &sceneFile, const std::vector<glm::vec3> &controlPoints)
{
    std::ofstream file(sceneFile);
    file << "Type GlobalConfig Config\n"
         << "LightPos 10.0 10.0 10.0\n"
         << "LightColor 1.0 1.0 1.0\n"
         << "CameraPos 0.0 5.0 10.0\n"
         << "CameraFront 0.0 0.0 -1.0\n"
         << "Fov 45.0\n"
         << "NearPlane 0.1\n"
         << "FarPlane 100.0\n"
         << "Sensitivity 0.1\n"
         << "CameraSpeed 0.05\n"
         << "AttConstant 1.0\n"
         << "AttLinear 0.09\n"
         << "AttQuadratic 0.032\n"
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
    for (const auto &cp : controlPoints)
    {
        file << "ControlPoint " << cp.x << " " << cp.y << " " << cp.z << "\n";
    }
    file << "PointsPerSegment 50\n"
         << "Color 1.0 0.0 0.0 1.0\n"
         << "End\n";
    file.close();
}

// Requisito 3: Leitura do arquivo de cena e pontos de animação
void readSceneFile(std::string sceneFilePath, std::unordered_map<std::string, Mesh> *meshes,
                   std::vector<std::string> *meshList, std::unordered_map<std::string, BSplineCurve> *bSplineCurves,
                   GlobalConfig *globalConfig)
{
    std::ifstream file(sceneFilePath);
    std::string line;
    if (!file.is_open())
    {
        std::cerr << "Falha ao abrir o arquivo " << sceneFilePath << std::endl;
        return;
    }

    std::string objectType, name, objFilePath, mtlFilePath, animFile;
    glm::vec3 scale{1.0f}, position{0.0f}, rotation{0.0f}, angle{0.0f};
    GLuint incrementalAngle = false;
    std::vector<glm::vec3> tempControlPoints;
    GLuint pointsPerSegment = 0;
    glm::vec4 color{1.0f};

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
                Mesh mesh;
                mesh.name = name;
                mesh.objFilePath = objFilePath;
                mesh.mtlFilePath = mtlFilePath;
                mesh.vertices = setupObj(objFilePath);
                mesh.VAO = setupGeometry(mesh.vertices);
                mesh.material = setupMtl(mtlFilePath);
                mesh.textureID = setupTexture(mesh.material.textureName);
                mesh.scale = scale;
                mesh.position = position;
                mesh.rotation = rotation;
                mesh.angle = angle;
                mesh.incrementalAngle = incrementalAngle;

                // Requisito 3a: Carrega pontos de animação
                if (!animFile.empty())
                {
                    std::ifstream anim(animFile);
                    std::string animLine;
                    while (getline(anim, animLine))
                    {
                        std::istringstream ass(animLine);
                        glm::vec3 pos;
                        ass >> pos.x >> pos.y >> pos.z;
                        mesh.animationPositions.push_back(pos);
                    }
                    anim.close();
                }

                meshes->insert(std::make_pair(name, mesh));
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

// Funções de suporte (mantidas do código original, ajustadas conforme necessário)
std::vector<Vertex> setupObj(std::string path)
{
    std::vector<Vertex> vertices;
    std::ifstream file(path);
    std::string line;
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

        if (type == "v")
        {
            glm::vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            temp_positions.push_back(pos);
        }
        else if (type == "vt")
        {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            temp_texcoords.push_back(uv);
        }
        else if (type == "vn")
        {
            glm::vec3 n;
            ss >> n.x >> n.y >> n.z;
            temp_normals.push_back(n);
        }
        else if (type == "f")
        {
            std::string v1, v2, v3;
            ss >> v1 >> v2 >> v3;
            int vIdx[3], tIdx[3], nIdx[3];
            std::string verts[3] = {v1, v2, v3};
            for (int i = 0; i < 3; ++i)
            {
                size_t p1 = verts[i].find('/');
                size_t p2 = verts[i].find('/', p1 + 1);
                vIdx[i] = std::stoi(verts[i].substr(0, p1)) - 1;
                tIdx[i] = std::stoi(verts[i].substr(p1 + 1, p2 - p1 - 1)) - 1;
                nIdx[i] = std::stoi(verts[i].substr(p2 + 1)) - 1;
            }
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

GLuint setupTexture(std::string filename)
{
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_set_flip_vertically_on_load(true);
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

// Função key_callback ajustada
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
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
            exportTrackToOBJ(vertices, indices, "track.obj");
            exportAnimationPoints(curvePoints, "animation.txt");
            generateSceneFile("track.obj", "car.obj", "animation.txt", "Scene.txt", editorControlPoints);
            readSceneFile("Scene.txt", &meshes, &meshList, &bSplineCurves, &globalConfig); // Agora acessa as globais
            editorMode = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
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

int setupGeometry(std::vector<Vertex> &vertices)
{
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return VAO;
}

GLuint generateControlPointsBuffer(std::vector<glm::vec3> controlPoints)
{
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, controlPoints.size() * sizeof(glm::vec3), controlPoints.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid *)0);
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    bc.VAO = VAO;
    return bc;
}

// Requisito 3: Cálculo do ângulo de rotação do carro
float computeAngleBetweenPoints(const glm::vec3 &p1, const glm::vec3 &p2)
{
    glm::vec3 dir = p2 - p1; // direção no solo
    return glm::degrees(atan2(dir.z, dir.x));
}