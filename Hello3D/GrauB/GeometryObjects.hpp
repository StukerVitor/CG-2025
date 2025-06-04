#ifndef GEOMETRYOBJECTS_HPP
#define GEOMETRYOBJECTS_HPP

#define STB_IMAGE_IMPLEMENTATION
#include "../../Common/include/stb_image.h"

#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <numeric>
#include <vector>
#include <fstream>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <array>
#include <glad/glad.h>

// ----------------------------------------------------------------------------
// AUXILIARY STRUCTS
// ----------------------------------------------------------------------------

struct Vec2 {
    float u, v;
    bool operator==(const Vec2& other) const {
        return u == other.u && v == other.v;
    }
};

struct Vec3 {
    float x, y, z;
    bool operator==(const Vec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct Face {
    std::vector<Vec3> verts;
    std::vector<Vec3> norms;
    std::vector<Vec2> texts;

    void addVert(const Vec3& v) { verts.push_back(v); }
    void addNorm(const Vec3& n) { norms.push_back(n); }
    void addText(const Vec2& t) { texts.push_back(t); }
};

struct Group {
    std::string name;
    std::string mtlName;
    std::vector<Face> faces;

    Group(const std::string& _name, const std::string& _mtl)
        : name(_name), mtlName(_mtl) {
    }

    void addFace(const Face& f) { faces.push_back(f); }
};

// ----------------------------------------------------------------------------
// “Vertex” holds exactly what the GPU needs per‐vertex.
// ----------------------------------------------------------------------------

struct Vertex {
    float x, y, z;    // posição
    float s, t;       // UV
    float nx, ny, nz; // normal
};

// ----------------------------------------------------------------------------
// “Material” holds Ka, Kd, Ks, Ns, and the texture filename.
// ----------------------------------------------------------------------------

struct Material {
    float kaR = 0.0f, kaG = 0.0f, kaB = 0.0f;
    float kdR = 0.0f, kdG = 0.0f, kdB = 0.0f;
    float ksR = 0.0f, ksG = 0.0f, ksB = 0.0f;
    float ns = 0.0f;
    std::string textureName;
};

// ----------------------------------------------------------------------------
// Helper function: Load a ".mtl" into a Material.
// ----------------------------------------------------------------------------

static Material setupMtl(const std::string& path)
{
    Material material{};
    std::ifstream file(path);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Falha ao abrir o arquivo MTL: " << path << std::endl;
        return material;
    }

    while (std::getline(file, line)) {
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

// ----------------------------------------------------------------------------
// Helper function: Create a texture from a filename.
// ----------------------------------------------------------------------------

static GLuint setupTexture(const std::string& filename)
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
    unsigned char* data = stbi_load(filename.c_str(), &w, &h, &channels, 0);
    if (data) {
        GLenum fmt = (channels == 3 ? GL_RGB : GL_RGBA);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cerr << "Falha ao carregar a textura: " << filename << std::endl;
    }
    stbi_image_free(data);
    return texId;
}

// ----------------------------------------------------------------------------
// Helper function: Build a VAO/VBO from an interleaved vector of Vertex.
// ----------------------------------------------------------------------------

static GLuint setupGeometry(const std::vector<Vertex>& vertices)
{
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // posição (x,y,z) → location 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // UV (s,t) → location 1
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // normal (nx,ny,nz) → location 3  ← aqui é a correção!
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return VAO;
}

// ----------------------------------------------------------------------------
// Mesh: trata apenas dos dados geométricos “puros”:
//   • vertices (vec3)  
//   • normals  (vec3)  
//   • mappings (vec2)  
//   • groups   (cada Group contém nome e mtlName, e Faces compostas de vec3 + vec2)  
//   • VAO      (montado com base em um vetor intercalado de Vertex)  
// ----------------------------------------------------------------------------

struct Mesh {
    std::vector<Vec3>   vertices;
    std::vector<Vec2>   mappings;
    std::vector<Vec3>   normals;
    std::vector<Group>  groups;
    GLuint              VAO = 0;

    Mesh() = default;

    /// Construtor recebendo dados geométricos completos, incluindo grupos:
    Mesh(const std::vector<Vec3>& verts,
        const std::vector<Vec2>& maps,
        const std::vector<Vec3>& norms,
        const std::vector<Group>& groups_)
        : vertices(verts), mappings(maps), normals(norms), groups(groups_)
    {
        // Montar um vetor intercalado de Vertex para configurar o VAO
        std::vector<Vertex> interleaved;
        interleaved.reserve(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i) {
            Vertex v;
            v.x = vertices[i].x;
            v.y = vertices[i].y;
            v.z = vertices[i].z;
            v.s = mappings[i].u;
            v.t = mappings[i].v;
            v.nx = normals[i].x;
            v.ny = normals[i].y;
            v.nz = normals[i].z;
            interleaved.push_back(v);
        }
        VAO = setupGeometry(interleaved);
    }

    /// Construtor que recebe um vetor intercalado de Vertex + índices
    ///    (usa um único Group “default”).  
    Mesh(const std::vector<Vertex>& interleavedVerts,
        const std::vector<unsigned int>& indices,
        const std::string& groupName = "",
        const std::string& mtlName = "")
    {
        // 1) Cria vetores paralelos (Vec3, Vec2, Vec3) a partir de cada Vertex
        vertices.reserve(interleavedVerts.size());
        mappings.reserve(interleavedVerts.size());
        normals.reserve(interleavedVerts.size());
        for (const auto& v : interleavedVerts) {
            vertices.push_back(Vec3{ v.x,  v.y,  v.z });
            mappings.push_back(Vec2{ v.s,  v.t });
            normals.push_back(Vec3{ v.nx, v.ny, v.nz });
        }

        // 2) Se não houver índices, assume que cada trio consecutivo é um triângulo
        std::vector<unsigned int> idxs = indices;
        if (idxs.empty()) {
            idxs.resize(interleavedVerts.size());
            std::iota(idxs.begin(), idxs.end(), 0u);
        }

        // 3) Cria um Group único (“default” ou com nome/mtl passados)
        groups.emplace_back(groupName, mtlName);
        Group* currentGroup = &groups.back();

        // 4) Agrupa cada triângulo de acordo com os índices em Face
        for (size_t i = 0; i + 2 < idxs.size(); i += 3) {
            Face face;
            for (int k = 0; k < 3; ++k) {
                unsigned int idx = idxs[i + k];
                face.addVert(vertices[idx]);
                face.addText(mappings[idx]);
                face.addNorm(normals[idx]);
            }
            currentGroup->addFace(face);
        }

        // 5) Monta VAO a partir do vetor intercalado recebido
        VAO = setupGeometry(interleavedVerts);
    }
};

// ----------------------------------------------------------------------------
// Object3D: mantém “tudo o resto” (nome, paths, material, transformação, etc.),
// faz parsing do .obj em dados para o Mesh e carrega o .mtl.
// ----------------------------------------------------------------------------

struct Object3D {
    std::string            name;
    std::string            objFilePath;
    std::string            mtlFilePath;
    Mesh                   mesh;
    glm::vec3              scale = glm::vec3(1.0f);
    glm::vec3              position = glm::vec3(0.0f);
    glm::vec3              rotation = glm::vec3(0.0f);
    glm::vec3              angle = glm::vec3(0.0f);
    GLuint                 incrementalAngle = 0;
    Material               material;
    GLuint                 textureID = 0;
    std::vector<glm::vec3> animationPositions;

    Object3D() = default;

    /// Construtor “tudo-em-um”:
    ///   faz parsing de OBJ (criando sempre um grupo “default”),
    ///   extrai posições, UVs, normais e grupos,
    ///   constrói o Mesh e carrega o MTL.
    Object3D(const std::string& _name,
        const std::string& objPath,
        const std::string& mtlPath,
        const glm::vec3& scale_ = glm::vec3(1.0f),
        const glm::vec3& pos_ = glm::vec3(0.0f),
        const glm::vec3& rot_ = glm::vec3(0.0f),
        const glm::vec3& ang_ = glm::vec3(0.0f),
        GLuint              incAng = 0)
        : name(_name)
        , objFilePath(objPath)
        , mtlFilePath(mtlPath)
        , scale(scale_)
        , position(pos_)
        , rotation(rot_)
        , angle(ang_)
        , incrementalAngle(incAng)
    {
        // ❶ Buffers temporários para leitura bruta de OBJ
        std::ifstream file(objFilePath);
        if (!file.is_open()) {
            std::cerr << "Falha ao abrir o arquivo OBJ: " << objFilePath << std::endl;
            return;
        }

        std::string line;
        std::vector<glm::vec3> raw_positions;
        std::vector<glm::vec2> raw_texcoords;
        std::vector<glm::vec3> raw_normals;

        // Dados finais a serem passados ao Mesh
        std::vector<Vec3> positions;
        std::vector<Vec2> texcoords;
        std::vector<Vec3> normals;
        std::vector<Group> groups;

        // ① Cria um grupo “default” (nome = nome do objeto, sem material)
        groups.emplace_back(name, "");
        Group* currentGroup = &groups.back();

        // ② Leitura linha a linha
        while (std::getline(file, line)) {
            std::istringstream ss(line);
            std::string type;
            ss >> type;

            if (type == "v") {
                glm::vec3 pos;
                ss >> pos.x >> pos.y >> pos.z;
                raw_positions.push_back(pos);
            }
            else if (type == "vt") {
                glm::vec2 uv;
                ss >> uv.x >> uv.y;
                raw_texcoords.push_back(uv);
            }
            else if (type == "vn") {
                glm::vec3 n;
                ss >> n.x >> n.y >> n.z;
                raw_normals.push_back(n);
            }
            else if (type == "usemtl") {
                std::string mtl;
                ss >> mtl;
                // Cria novo grupo com nome = material
                groups.emplace_back(mtl, mtl);
                currentGroup = &groups.back();
            }
            else if (type == "f") {
                // “triangle-fan” para decompor polígono em triângulos
                std::vector<std::string> tokens;
                std::string tok;
                while (ss >> tok) tokens.push_back(tok);
                if (tokens.size() < 3) continue;

                for (size_t i = 1; i + 1 < tokens.size(); ++i) {
                    Face face;
                    std::array<std::string, 3> idxs = {
                        tokens[0], tokens[i], tokens[i + 1]
                    };

                    for (int k = 0; k < 3; ++k) {
                        const std::string& vertStr = idxs[k];
                        int vIdx = -1, tIdx = -1, nIdx = -1;
                        size_t firstSlash = vertStr.find('/');
                        size_t secondSlash = (firstSlash == std::string::npos
                            ? std::string::npos
                            : vertStr.find('/', firstSlash + 1));

                        if (firstSlash == std::string::npos) {
                            // Apenas posição
                            vIdx = std::stoi(vertStr) - 1;
                        }
                        else if (secondSlash == std::string::npos) {
                            // v/t
                            vIdx = std::stoi(vertStr.substr(0, firstSlash)) - 1;
                            tIdx = std::stoi(vertStr.substr(firstSlash + 1)) - 1;
                        }
                        else if (secondSlash == firstSlash + 1) {
                            // v//n
                            vIdx = std::stoi(vertStr.substr(0, firstSlash)) - 1;
                            nIdx = std::stoi(vertStr.substr(secondSlash + 1)) - 1;
                        }
                        else {
                            // v/t/n
                            vIdx = std::stoi(vertStr.substr(0, firstSlash)) - 1;
                            tIdx = std::stoi(vertStr.substr(firstSlash + 1,
                                secondSlash - firstSlash - 1)) - 1;
                            nIdx = std::stoi(vertStr.substr(secondSlash + 1)) - 1;
                        }

                        Vec3 pV{ 0.0f, 0.0f, 0.0f };
                        Vec2 tV{ 0.0f, 0.0f };
                        Vec3 nV{ 0.0f, 0.0f, 0.0f };

                        if (vIdx >= 0 && vIdx < static_cast<int>(raw_positions.size())) {
                            const auto& rp = raw_positions[vIdx];
                            pV = Vec3{ rp.x, rp.y, rp.z };
                        }
                        if (tIdx >= 0 && tIdx < static_cast<int>(raw_texcoords.size())) {
                            const auto& rt = raw_texcoords[tIdx];
                            tV = Vec2{ rt.x, rt.y };
                        }
                        if (nIdx >= 0 && nIdx < static_cast<int>(raw_normals.size())) {
                            const auto& rn = raw_normals[nIdx];
                            nV = Vec3{ rn.x, rn.y, rn.z };
                        }

                        // Adiciona aos vetores finais alinhados
                        positions.push_back(pV);
                        texcoords.push_back(tV);
                        normals.push_back(nV);

                        // Adiciona ao Face
                        face.addVert(pV);
                        face.addText(tV);
                        face.addNorm(nV);
                    }

                    // Adiciona Face ao grupo atual
                    currentGroup->addFace(face);
                }
            }
            // ignora outras linhas (o, g, s, mtllib, etc.)
        }

        file.close();

        // ❷ Constrói o Mesh “geométrico” com todos os grupos
        mesh = Mesh(positions, texcoords, normals, groups);

        // ❸ Carrega “.mtl” e seta o textureID
        material = setupMtl(mtlFilePath);
        textureID = setupTexture(material.textureName);
    }

    // Retorna mesh mutável
    Mesh& getMesh() { return mesh; }
    // Retorna mesh constante (para chamadas em objetos const)
    const Mesh& getMesh() const { return mesh; }
};

// ----------------------------------------------------------------------------
// OBJWriter / Object3DWriter
// ----------------------------------------------------------------------------

class OBJWriter {
public:
    // Agora recebe apenas o Mesh e um nome de arquivo para escrever o .obj
    void write(const Mesh& mesh, const std::string& filename) {
        std::ofstream f(filename);
        if (!f.is_open()) {
            assert(false && "Não conseguiu criar .obj");
        }

        // 1) Escreve as posições (v x y z)
        for (const auto& v : mesh.vertices) {
            f << "v " << v.x << " " << v.y << " " << v.z << "\n";
        }
        // 2) Escreve as UVs (vt u v)
        for (const auto& uv : mesh.mappings) {
            f << "vt " << uv.u << " " << uv.v << "\n";
        }
        // 3) Escreve as normais (vn nx ny nz)
        for (const auto& n : mesh.normals) {
            f << "vn " << n.x << " " << n.y << " " << n.z << "\n";
        }

        // 4) Para cada Group, escreve "usemtl" e depois as faces
        for (const auto& grp : mesh.groups) {
            // Se o grupo tiver mtlName vazio, ignora o usemtl (opcional)
            if (!grp.mtlName.empty()) {
                f << "usemtl " << grp.mtlName << "\n";
            }
            for (const auto& face : grp.faces) {
                f << "f ";
                for (size_t k = 0; k < face.verts.size(); ++k) {
                    // Encontrar índices baseados nos vetores globais:
                    int vi = static_cast<int>(
                        std::find(mesh.vertices.begin(),
                            mesh.vertices.end(),
                            face.verts[k])
                        - mesh.vertices.begin()) + 1;
                    int ti = static_cast<int>(
                        std::find(mesh.mappings.begin(),
                            mesh.mappings.end(),
                            face.texts[k])
                        - mesh.mappings.begin()) + 1;
                    int ni = static_cast<int>(
                        std::find(mesh.normals.begin(),
                            mesh.normals.end(),
                            face.norms[k])
                        - mesh.normals.begin()) + 1;

                    f << vi << "/" << ti << "/" << ni << " ";
                }
                f << "\n";
            }
        }

        f.close();
    }
};

class Object3DWriter {
public:
    // Continua recebendo Object3D&
    void write(Object3D& obj) {
        // Gera um nome de arquivo baseado no nome do objeto
        std::string outObj = obj.name + ".obj";

        // Chama OBJWriter apenas com o Mesh contido em Object3D
        OBJWriter w;
        w.write(obj.getMesh(), outObj);
    }
};


#endif // GEOMETRYOBJECTS_HPP
