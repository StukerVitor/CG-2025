// File: GeometryObjects.hpp
#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <glm/glm.hpp>             // apenas para glm::mat4
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//
// ─────────────────────────────────────────────────────────────────────────────
// Vec2: armazenar coordenadas de textura (coord : float[2])
// ─────────────────────────────────────────────────────────────────────────────
class Vec2 {
private:
    std::array<float, 2> coord_{ 0.0f, 0.0f };

public:
    Vec2() = default;
    Vec2(float x, float y) : coord_{ x, y } {}

    float x() const { return coord_[0]; }
    float y() const { return coord_[1]; }

    void set(float x, float y) {
        coord_[0] = x;
        coord_[1] = y;
    }
};

//
// ─────────────────────────────────────────────────────────────────────────────
// Vec3: armazenar posição ou vetor normal (coord : float[3])
// ─────────────────────────────────────────────────────────────────────────────
class Vec3 {
private:
    std::array<float, 3> coord_{ 0.0f, 0.0f, 0.0f };

public:
    Vec3() = default;
    Vec3(float x, float y, float z) : coord_{ x, y, z } {}

    float x() const { return coord_[0]; }
    float y() const { return coord_[1]; }
    float z() const { return coord_[2]; }

    void set(float x, float y, float z) {
        coord_[0] = x;
        coord_[1] = y;
        coord_[2] = z;
    }
};

//
// ─────────────────────────────────────────────────────────────────────────────
// Face:
//   – verts  : std::vector<int>  (índices para vertices)
//   – norms  : std::vector<int>  (índices para normais)
//   – texts  : std::vector<int>  (índices para coordenadas de textura)
// ─────────────────────────────────────────────────────────────────────────────
class Face {
private:
    std::vector<int> verts_;
    std::vector<int> norms_;
    std::vector<int> texts_;

public:
    Face() = default;

    Face(const std::vector<int>& verts,
        const std::vector<int>& norms,
        const std::vector<int>& texts)
        : verts_{ verts }, norms_{ norms }, texts_{ texts }
    {
    }

    // getters
    const std::vector<int>& getVerts() const { return verts_; }
    const std::vector<int>& getNorms() const { return norms_; }
    const std::vector<int>& getTexts() const { return texts_; }

    // setters
    void setVerts(const std::vector<int>& v) { verts_ = v; }
    void setNorms(const std::vector<int>& n) { norms_ = n; }
    void setTexts(const std::vector<int>& t) { texts_ = t; }

    // métodos auxiliares para adicionar índices
    void addVertIndex(int i) { verts_.push_back(i); }
    void addNormIndex(int i) { norms_.push_back(i); }
    void addTextIndex(int i) { texts_.push_back(i); }
};

//
// ─────────────────────────────────────────────────────────────────────────────
// Group:
//   – name     : std::string
//   – material : std::string
//   ◇── faces  : std::vector<Face> (0..*)
// ─────────────────────────────────────────────────────────────────────────────
class Group {
private:
    std::string name_;
    std::string material_;
    std::vector<Face> faces_;

public:
    Group() = default;
    Group(const std::string& name, const std::string& material)
        : name_{ name }, material_{ material }
    {
    }

    // getters
    const std::string& getName() const { return name_; }
    const std::string& getMaterial() const { return material_; }
    const std::vector<Face>& getFaces() const { return faces_; }

    // setters
    void setName(const std::string& n) { name_ = n; }
    void setMaterial(const std::string& m) { material_ = m; }

    // adicionar Face
    void addFace(const Face& f) { faces_.push_back(f); }
};

//
// ─────────────────────────────────────────────────────────────────────────────
// Mesh:
//   ◇── groups    : std::vector<Group> (0..*)
//   vertex ►     : std::vector<Vec3>  (0..*)
//   normals ►    : std::vector<Vec3>  (0..*)
//   mappings ►   : std::vector<Vec2>  (0..*)
// ─────────────────────────────────────────────────────────────────────────────
class Mesh {
public:
    Mesh() = default;

    // Nome do mesh (opcional, mas frequentemente útil)
    std::string name;

    // atributos “básicos” do UML
    std::vector<Vec3> vertices;  // vertex ► (0..*) → Vec3
    std::vector<Vec3> normals;   // normals ► (0..*) → Vec3
    std::vector<Vec2> mappings;  // mappings ► (0..*) → Vec2
    std::vector<Group> groups;   // ◇── groups (0..*) → Group

    // getters (se desejar manter imutável externamente)
    const std::vector<Vec3>& getVertices() const { return vertices; }
    const std::vector<Vec3>& getNormals()  const { return normals; }
    const std::vector<Vec2>& getMappings() const { return mappings; }
    const std::vector<Group>& getGroups()   const { return groups; }

    // setters/auxiliares
    void addVertex(const Vec3& v) { vertices.push_back(v); }
    void addNormal(const Vec3& n) { normals.push_back(n); }
    void addMapping(const Vec2& uv) { mappings.push_back(uv); }
    void addGroup(const Group& g) { groups.push_back(g); }
};

//
// ─────────────────────────────────────────────────────────────────────────────
// Object3D:
//   – transform : glm::mat4
//   ◇── mesh    : Mesh (1..1)
// ─────────────────────────────────────────────────────────────────────────────
class Object3D {
private:
    glm::mat4 transform_{ 1.0f };
    Mesh mesh_;

public:
    Object3D() = default;

    // getters/setters de transform
    void setTransform(const glm::mat4& t) { transform_ = t; }
    const glm::mat4& getTransform() const { return transform_; }

    // acesso ao Mesh
    Mesh& getMesh() { return mesh_; }
    const Mesh& getMesh() const { return mesh_; }
};

//
// ─────────────────────────────────────────────────────────────────────────────
// OBJWriter:
//   + write(mesh : Mesh) : void
//   Responsável por exportar um Mesh em formato OBJ.
// ─────────────────────────────────────────────────────────────────────────────
class OBJWriter {
public:
    OBJWriter() = default;
    ~OBJWriter() = default;

    void write(const Mesh& mesh) {
        // Exemplo de implementação básica (omitir aqui):
        // 1) Para cada v em mesh.vertices:   escreve “v x y z\n”
        // 2) Para cada vt em mesh.mappings:   escreve “vt u v\n”
        // 3) Para cada vn em mesh.normals:    escreve “vn nx ny nz\n”
        // 4) Para cada Group em mesh.groups:
        //       escreve “g <group.getName()>\n”
        //       escreve “usemtl <group.getMaterial()>\n”
        //       para cada Face f em group.getFaces():
        //         escreve “f v1/t1/n1 v2/t2/n2 v3/t3/n3\n”
        // IMPLEMENTAÇÃO REAL DEVE SUBSTITUIR ESTE PSEUDO‐CÓDIGO.
    }
};

//
// ─────────────────────────────────────────────────────────────────────────────
// Object3DWriter:
//   + write(obj : Object3D) : void
//   Encapsula a exportação de um Object3D (aplica transform e chama OBJWriter).
// ─────────────────────────────────────────────────────────────────────────────
class Object3DWriter {
public:
    Object3DWriter() = default;
    ~Object3DWriter() = default;

    void write(const Object3D& obj) {
        // Se necessário, poderia usar obj.getTransform() para modificar vértices.
        // Aqui apenas encaminha a malha para o OBJWriter:
        OBJWriter w;
        w.write(obj.getMesh());
    }
};
