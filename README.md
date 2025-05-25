# Documentação – **Carregador de Cena 3D com OpenGL e Curvas de Bézier**

> Versão 1.1 – 25 mai 2025

---

## Índice

1. [Introdução](#introdução)
2. [Estrutura do Projeto](#estrutura-do-projeto)
3. [Pipeline de Execução](#pipeline-de-execução)
4. [Estruturas de Dados](#estruturas-de-dados)
5. [Formato do Arquivo `Scene.txt`](#formato-do-arquivo-scenetxt)
6. [Carregamento de Malhas OBJ](#carregamento-de-malhas-obj)
7. [Carregamento de Materiais MTL](#carregamento-de-materiais-mtl)
8. [Texturas e stb_image](#texturas-e-stb_image)
9. [Curvas de Bézier](#curvas-de-bézier)
10. [Inicialização OpenGL](#inicialização-opengl)
11. [Sistema de Câmera](#sistema-de-câmera)
12. [Interação e Seleção de Objetos](#interação-e-seleção-de-objetos)
13. [Loop de Renderização](#loop-de-renderização)
14. [Programas de Shader](#programas-de-shader)
15. [Glossário](#glossário)

---

## Introdução

Este documento descreve **em profundidade** a arquitetura, o fluxo de dados e o funcionamento interno de um _loader_ de cenas 3D que:

- Lê um _script_ de texto (`Scene.txt`) descrevendo **malhas OBJ**, **materiais MTL**, **curvas de Bézier** e **configurações globais**.
- Renderiza o resultado usando **OpenGL 3.3 Core** com **iluminação Phong** e texturas.
- Oferece **controles de câmera em primeira pessoa**, seleção de objetos com manipulação de posição/escala/rotação via teclado e exibição opcional das curvas.

---

## Estrutura do Projeto

```text
ProjectRoot/
├─ assets/               # OBJ, MTL, texturas, Scene.txt
├─ shaders/              # Programas GLSL
│  ├─ Object.vs
│  ├─ Object.fs
│  ├─ Line.vs
│  └─ Line.fs
├─ src/
│  ├─ main.cpp           # ponto de entrada
│  └─ Shader.h           # utilitário de compilação de shaders
└─ external/
   ├─ glad/              # loader de extensões
   ├─ glfw/              # builds estáticos/dinâmicos
   ├─ glm/               # matemática
   └─ stb/               # stb_image.h
```

### Principais Arquivos

| Arquivo        | Função                                                                                                                  |
| -------------- | ----------------------------------------------------------------------------------------------------------------------- |
| `main.cpp`     | Contém **todo o core**: parsing de cena, criação de buffers, _game loop_, entrada e liberação.                          |
| `Shader.h`     | Classe RAII pequena que compila/linka GLSL a partir de arquivos e expõe `use()` / `getId()`.                            |
| `Object.vs/fs` | Shader para malhas com iluminação Phong + textura.                                                                      |
| `Line.vs/fs`   | Shader simplificado para curvas (linha + pontos).                                                                       |
| `Scene.txt`    | Script que descreve entidades. É **a única forma** de entrada de conteúdo, dispensando formatação binária proprietária. |

---

## Pipeline de Execução

```text
┌─────────────┐
│ main()      │
└────┬────────┘
     │ init GLFW/GLAD
     ▼
┌─────────────┐
│ readScene   │───► cria Mesh/Bezier/Config
└────┬────────┘
     │ setup shaders, textures, VAOs
     ▼
┌─────────────┐
│ renderLoop  │ (60 FPS)
│  ├─ processInput
│  ├─ updateCamera
│  ├─ animateObjects
│  ├─ drawMeshes
│  └─ drawCurves
└────┬────────┘
     │ cleanup
     ▼
┌─────────────┐
│ exit        │
└─────────────┘
```

- **CPU‑side**: prepara dados, detecta input
- **GPU‑side**: executa _vertex + fragment shaders_, z‑buffer, rasteriza

---

## Estruturas de Dados

### Diagrama Simplificado

```text
+----------------+           +-----------------+
|  GlobalConfig  |           |   BezierCurve   |
| lightPos       |<----+---->| controlPoints[] |
| cameraPos      |     |     | curvePoints[]   |
| ...            |     |     | VAO             |
+----------------+     |     +-----------------+
                       |
                       |1
                       |
                       |N
              +--------v-------+
              |     Mesh       |
              | name           |
              | vertices[]     |
              | VAO, textureID |
              | Material       |
              +----------------+
```

### Campos Relevantes

- **`Vertex`**
  - `vec3 position`, `vec2 texCoord`, `vec3 normal` (a cor é opcional, mas facilita _debug_).
- **`Material`**
  - Valores Ka/Kd/Ks (RGB) e expoente `Ns` (shininess).
  - `textureName` guarda **apenas** o _basename_; o gerenciador de texturas acrescenta caminho.
- **`Mesh`**
  - Flags de rotação contínua (`incrementalAngle`) permitem animações simples **sem** shaders de _skinning_.
- **`BezierCurve`**
  - Oferece **duas** formas de construção: pontos dados ou círculo gerado via aproximação cúbica.
- **`GlobalConfig`**
  - Todos os valores são carregados **antes** do primeiro _draw_; não há recarregamento.

---

## Formato do Arquivo `Scene.txt`

O parser aceita **qualquer ordem** de blocos, mas cada bloco deve iniciar com `Type` e terminar com `End`.

#### Propriedades de `GlobalConfig`

| Propriedade   | Valor (exemplo) | Descrição                     |
| ------------- | --------------- | ----------------------------- |
| `LightPos`    | `0 10 0`        | posição xyz da luz principal  |
| `LightColor`  | `1 1 1`         | cor RGB (0‑1)                 |
| `CameraPos`   | `0 2 6`         | posição inicial da câmera     |
| `CameraFront` | `0 0 -1`        | direção inicial               |
| `Fov`         | `45`            | campo de visão (graus)        |
| `NearPlane`   | `0.1`           | plano de recorte próximo      |
| `FarPlane`    | `100`           | plano de recorte distante     |
| `Sensitivity` | `0.08`          | sens. do mouse                |
| `CameraSpeed` | `0.05`          | velocidade base (unid./frame) |

#### Propriedades de `Mesh`

| Propriedade        | Exemplo          | Obs.                                                                |
| ------------------ | ---------------- | ------------------------------------------------------------------- |
| `Obj`              | `assets/sol.obj` | aceita caminho relativo                                             |
| `Mtl`              | `assets/sol.mtl` | se omitido → material básico cinza                                  |
| `Scale`            | `0.5 0.5 0.5`    | escala XYZ                                                          |
| `Position`         | `0 0 0`          | posição inicial                                                     |
| `Rotation`         | `0 1 0`          | eixo normalizado de rotação                                         |
| `Angle`            | `0 0 0`          | ângulo fixo (graus) para cada eixo                                  |
| `IncrementalAngle` | `1`              | 1 = soma `+0.1°` por frame (ver variável global `incrementalAngle`) |

#### Propriedades de `BezierCurve`

| Propriedade        | Exemplo     | Descrição                                            |
| ------------------ | ----------- | ---------------------------------------------------- |
| `ControlPoint`     | `1 0 0`     | múltiplas linhas → curva composta                    |
| `Orbit` + `Radius` | `0 0 0` `4` | gera pontos automaticamente (substitui ControlPoint) |
| `PointsPerSegment` | `60`        | amostragem (quanto maior, mais suave)                |
| `Color`            | `0 0 1 1`   | RGBA (valores 0‑1)                                   |

---

## Carregamento de Malhas OBJ

### Regras de Parsing

- **Linhas suportadas**: `v`, `vt`, `vn`, `f`.
- O parser **não** trata:
  - faces com mais de 3 vértices (técnica futura: _Ear Clipping_).
  - negativos ou offsets relativos (`-1`).
- Cada `f` cria **três** structs `Vertex` (triângulo).
- Os índices OBJ são **1‑based**; o código converte para **0‑based**.

```cpp
size_t p1 = verts[i].find('/');
size_t p2 = verts[i].find('/', p1 + 1);
vIdx[i] = std::stoi(verts[i].substr(0, p1)) - 1;
tIdx[i] = std::stoi(verts[i].substr(p1 + 1, p2 - p1 - 1)) - 1;
nIdx[i] = std::stoi(verts[i].substr(p2 + 1)) - 1;
```

---

## Carregamento de Materiais MTL

- Apenas **um** material é lido por arquivo; múltiplos `newmtl` não são suportados.
- Campos suportados: `Ka`, `Kd`, `Ks`, `Ns`, `map_Kd`.
- O valor `Ns` é passado como `float ns` e usado diretamente em `pow()` do fragment shader.

---

## Texturas e stb_image

1. `stbi_set_flip_vertically_on_load(true)` – OpenGL espera origem no **canto inferior esquerdo**.
2. `glTexParameteri(..., GL_LINEAR_MIPMAP_LINEAR)` garante _trilinear filtering_.
3. Quando `channels == 4`, o formato vira `GL_RGBA`.

---

## Curvas de Bézier

### Matemática

A posição em _t_ para uma curva cúbica é:

\[
B(t) = \mathbf{G} \cdot \mathbf{M} \cdot
\begin{bmatrix} t^3 \\ t^2 \\ t \\ 1 \end{bmatrix},
\qquad 0 \le t \le 1
\]

onde \(\mathbf{G}\) contém **4 pontos de controle**, e \(\mathbf{M}\) é a matriz de base Bézier:

\[
\mathbf{M} =
\begin{bmatrix}
-1 & 3 & -3 & 1 \\
3 & -6 & 3 & 0 \\
-3 & 3 & 0 & 0 \\
1 & 0 & 0 & 0
\end{bmatrix}
\]

### Aproximação de Círculo

A constante \(k = 0.5522847498\cdots\) aproxima um quarto de circunferência usando um único segmento cúbico.

```cpp
const float k = 0.552284749831f * radius; // 4 * ((√2 - 1) / 3)
```

O gerador produz **12 control points** = 4 segmentos (360°).

---

## Inicialização OpenGL

- **GLFW** cria janela + contexto; versão mínima **3.3**.
- **GLAD** é gerado para `core` profile 3.3 (evita funções _deprecated_).
- **Depth Test** (`glEnable(GL_DEPTH_TEST)`) + _back‑face culling_ opcional.

Para depuração, ative **mensagens de debug**:

```cpp
glEnable(GL_DEBUG_OUTPUT);
glDebugMessageCallback(myGLDebugCallback, nullptr);
```

---

## Sistema de Câmera

- Estilo **FPS**: posição (`cameraPos`) + vetor frente (`cameraFront`), up fixo `(0,1,0)`.
- Euler Angles → converte para vetor direção a cada movimento de mouse.
- Sensibilidade escalona deslocamentos:

```cpp
front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
front.y = sin(glm::radians(pitch));
front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
```

---

## Interação e Seleção de Objetos

| Tecla     | Função                                       |
| --------- | -------------------------------------------- |
| `Tab`     | Cicla por meshes (`meshList`)                |
| `1 / 2`   | aumenta/diminui **escala** do selecionado    |
| `3 / 4`   | gira em torno do eixo definido em `Rotation` |
| `← ↑ → ↓` | desloca no plano **XY**                      |
| `F1`      | _toggle_ curvas                              |

Internamente, o índice `currentlySelectedMesh` é incrementado **mod** `meshList.size()`.  
A cor extra `(0.3, 0.5, 0.9)` é adicionada no fragment shader para “highlight”.

---

## Loop de Renderização

1. **Input** – `glfwPollEvents`
2. **Limpeza** – `glClearColor` + `glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)`
3. **Atualizações**
   - Câmera
   - Posições de órbita (`i`, `j`)
   - `incrementalAngle`
4. **Desenho de malhas**
5. **Desenho de curvas** (se `showCurves`)
6. **SwapBuffers**

---

## Programas de Shader

### `Object.vs`

```glsl
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec3 aNormal;

uniform mat4 model, view, projection;
out vec2 vUV;
out vec3 vNormal;
out vec3 vFragPos;

void main() {
    vUV       = aUV;
    vNormal   = mat3(transpose(inverse(model))) * aNormal;
    vFragPos  = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * vec4(vFragPos, 1.0);
}
```

### `Object.fs`

- Implementa **Phong**:

\[
I = k_a I_a + k_d (\mathbf{L}\cdot\mathbf{N}) I_l + k_s (\max(\mathbf{R}\cdot\mathbf{V}, 0))^{N_s} I_l
\]

- Campo `skipLighting` evita custo desnecessário para objetos “auto‑iluminados” (ex.: Sol).

### `Line.vs/fs`

Cor fixa vinda de `uniform vec4 finalColor`.

---

## Glossário

| Termo      | Definição concisa                                                     |
| ---------- | --------------------------------------------------------------------- |
| **VAO**    | _Vertex Array Object_ – guarda ponteiros de atributo de vértice.      |
| **VBO**    | _Vertex Buffer Object_ – memória de vértices na GPU.                  |
| **Bezier** | Curva paramétrica polinomial de grau *n* – aqui, cúbica.              |
| **Phong**  | Modelo de iluminação empírico com termos ambiente, difuso, especular. |
| **UV**     | Coordenadas 2D que mapeiam vértices 3D para texturas 2D.              |

---
