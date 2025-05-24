// Shader.cpp
#include "Shader.h" // Inclui o arquivo de cabeçalho da classe Shader

#include <string>		// Para usar std::string
#include <fstream>	// Para operações de E/S de arquivos (ifstream)
#include <sstream>	// Para usar std::stringstream (streams de string)
#include <iostream> // Para entrada/saída padrão (std::cout)

// Construtor da classe Shader
// Lê os códigos fonte do vertex e fragment shader de arquivos, compila-os e os vincula a um programa shader.
Shader::Shader(const std::string vertexShaderPath, const std::string fragmentShaderPath)
{

	std::string vertexCode;		 // String para armazenar o código do vertex shader
	std::string fragmentCode;	 // String para armazenar o código do fragment shader
	std::ifstream vShaderFile; // Stream de arquivo para o vertex shader
	std::ifstream fShaderFile; // Stream de arquivo para o fragment shader

	// Configura os streams de arquivo para lançar exceções em caso de erros de leitura (badbit)
	vShaderFile.exceptions(std::ifstream::badbit);
	fShaderFile.exceptions(std::ifstream::badbit);
	try
	{
		// Abre os arquivos de shader
		vShaderFile.open(vertexShaderPath);
		fShaderFile.open(fragmentShaderPath);
		std::stringstream vShaderStream, fShaderStream; // Streams de string para ler o conteúdo dos arquivos

		// Lê o conteúdo dos arquivos para os streams de string
		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();

		// Fecha os manipuladores de arquivo
		vShaderFile.close();
		fShaderFile.close();

		// Converte os streams de string para std::string
		vertexCode = vShaderStream.str();
		fragmentCode = fShaderStream.str();
	}
	catch (std::ifstream::failure e) // Captura exceções de falha na leitura do arquivo
	{
		std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
		// Adicionar tratamento de erro mais robusto aqui, como lançar uma exceção ou definir um estado de erro.
	}

	const GLchar *vShaderCode = vertexCode.c_str();		// Converte o código do vertex shader para um array de caracteres C-style
	const GLchar *fShaderCode = fragmentCode.c_str(); // Converte o código do fragment shader para um array de caracteres C-style

	// Compilação do Vertex Shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER); // Cria um objeto shader do tipo GL_VERTEX_SHADER
	glShaderSource(vertexShader, 1, &vShaderCode, NULL);		// Define o código fonte para o objeto shader
																													// 1: número de strings de código fonte
																													// &vShaderCode: ponteiro para o array de strings de código
																													// NULL: array de comprimentos de string (NULL significa que as strings são terminadas em nulo)
	glCompileShader(vertexShader);													// Compila o shader

	GLint success;																						// Variável para verificar o sucesso da compilação/linkagem
	GLchar infoLog[512];																			// Array de caracteres para armazenar o log de informações (mensagens de erro)
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success); // Verifica o status da compilação do vertex shader
	if (!success)																							// Se a compilação falhou
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog); // Obtém o log de informações
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
							<< infoLog << std::endl; // Imprime a mensagem de erro
	}

	// Compilação do Fragment Shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER); // Cria um objeto shader do tipo GL_FRAGMENT_SHADER
	glShaderSource(fragmentShader, 1, &fShaderCode, NULL);			// Define o código fonte
	glCompileShader(fragmentShader);														// Compila o shader

	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success); // Verifica o status da compilação do fragment shader
	if (!success)																								// Se a compilação falhou
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog); // Obtém o log de informações
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
							<< infoLog << std::endl; // Imprime a mensagem de erro
	}

	// Criação e Linkagem do Programa Shader
	this->id = glCreateProgram();				// Cria um objeto de programa shader e armazena seu ID no membro 'id' da classe
	glAttachShader(id, vertexShader);		// Anexa o vertex shader compilado ao programa
	glAttachShader(id, fragmentShader); // Anexa o fragment shader compilado ao programa
	glLinkProgram(id);									// Linka os shaders anexados para criar um programa executável na GPU

	glGetProgramiv(id, GL_LINK_STATUS, &success); // Verifica o status da linkagem do programa
	if (!success)
	{																							 // Se a linkagem falhou
		glGetProgramInfoLog(id, 512, NULL, infoLog); // Obtém o log de informações
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
							<< infoLog << std::endl; // Imprime a mensagem de erro
	}

	// Após a linkagem bem-sucedida, os objetos shader individuais não são mais necessários e podem ser deletados
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

// Método para definir a uniforme de textura (para a unidade de textura 0)
void Shader::setTextureUniform()
{
	// Define o valor da variável uniforme "tex" no shader para 0.
	// Isso significa que a uniforme "tex" (do tipo sampler2D, por exemplo) usará a textura vinculada à GL_TEXTURE0.
	glUniform1i(glGetUniformLocation(this->id, "tex"), 0);
}