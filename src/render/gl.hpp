#pragma once
// gl.hpp — wrapper minimo su OpenGL 3.3 core via QOpenGLExtraFunctions.
//
// Qt 6.10 ha rimosso QOpenGLContext::versionFunctions<T>(): le funzioni GL 3.0+
// (VAO, FBO, istanze) arrivano da context()->extraFunctions().

#include <QOpenGLExtraFunctions>
#include <QString>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace w3::glx {

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
};

class Program {
public:
    bool build(QOpenGLExtraFunctions* gl, const QString& vertexPath, const QString& fragmentPath,
               QString* error);
    void destroy(QOpenGLExtraFunctions* gl);
    void use(QOpenGLExtraFunctions* gl) const;

    void set1f(QOpenGLExtraFunctions* gl, const char* name, float value);
    void set2f(QOpenGLExtraFunctions* gl, const char* name, float a, float b);
    void set3f(QOpenGLExtraFunctions* gl, const char* name, float a, float b, float c);
    void set4f(QOpenGLExtraFunctions* gl, const char* name, float a, float b, float c, float d);
    void setInt(QOpenGLExtraFunctions* gl, const char* name, int value);
    void setMat4(QOpenGLExtraFunctions* gl, const char* name, const float* columnMajor);

    GLuint id() const { return id_; }

private:
    GLint uniform(QOpenGLExtraFunctions* gl, const char* name);

    GLuint id_ = 0;
    std::unordered_map<std::string, GLint> uniforms_;
};

struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLsizei indexCount = 0;
};

GpuMesh upload(QOpenGLExtraFunctions* gl, const std::vector<Vertex>& vertices,
               const std::vector<std::uint32_t>& indices);
void destroy(QOpenGLExtraFunctions* gl, GpuMesh& mesh);

}  // namespace w3::glx
