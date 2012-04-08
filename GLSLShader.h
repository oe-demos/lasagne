#ifndef _LASAGNE_GLSL_SHADER_H_
#define _LASAGNE_GLSL_SHADER_H_H_

#include <string>
#include <iostream>
#include <vector>
#include <stdexcept>

class GLSLShader {
private:
    GLuint shaderProgram;
    GLuint vertexShaderId, fragmentShaderId;

private:
    GLuint LoadShader(std::vector<std::string> source, int type){
        GLuint shader = glCreateShader(type);
        

        // Fill the shader source to GLchar
        unsigned int size = source.size();
        const GLchar** shaderBits = new const GLchar*[size];
        for (unsigned int i = 0; i < source.size(); ++i)
            shaderBits[i] = source[i].c_str();

        glShaderSource(shader, size, shaderBits, NULL);

        // Compile shader
        glCompileShader(shader);

        GLint compiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled == 0) {
            std::cout << "Failed compiling shader program" << std::endl;
            GLsizei bufsize;
            const int maxBufSize = 100;
            char buffer[maxBufSize];
            glGetShaderInfoLog(shader, maxBufSize, &bufsize, buffer);
            std::cout << "compile errors: " << buffer << std::endl;
            throw runtime_error("Failed compiling shader program.");
        }
            
        return shader;
    }

public:
    GLSLShader() 
        : shaderProgram(0), vertexShaderId(0), fragmentShaderId(0) {}

    GLSLShader(const std::string& vert, const std::string& frag) {
        shaderProgram = glCreateProgram();
        
        std::vector<std::string> vertexBits(1);
        vertexBits[0] = vert;
        vertexShaderId = LoadShader(vertexBits, GL_VERTEX_SHADER);
        glAttachShader(shaderProgram, vertexShaderId);

        std::vector<std::string> fragmentBits(1);
        fragmentBits[0] = frag;
        fragmentShaderId = LoadShader(fragmentBits, GL_FRAGMENT_SHADER);
        glAttachShader(shaderProgram, fragmentShaderId);

        glLinkProgram(shaderProgram);
        GLint linked;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked);
        
        GLint infologLength = 0, charsWritten = 0;
        GLchar* infoLog;
        glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &infologLength);
        if (infologLength > 0) {
            infoLog = (GLchar *)malloc(infologLength);
            if (infoLog == NULL) {
                std::cout << "Could not allocate InfoLog buffer" << std::endl;
                return;
            }
            glGetProgramInfoLog(shaderProgram, infologLength, &charsWritten, infoLog);
            std::cout << "Program InfoLog:\n \"" << infoLog << "\"" << std::endl;
            free(infoLog);
        }
    }
    
    void Bind() {
        glUseProgram(shaderProgram);
    }

    int GetUniformLocation(string name) {
        return glGetUniformLocation(shaderProgram, name.c_str());
    }

    void SetUniform(string name, float value) {
        glUniform1f(GetUniformLocation(name), value);
    }
    
};

#endif
