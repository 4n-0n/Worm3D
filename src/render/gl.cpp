#include "render/gl.hpp"

#include <QFile>

#include <cstddef>

namespace w3::glx {
namespace {

QString readResource(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("shader non trovato nel resource system: %1").arg(path);
        }
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

GLuint compile(QOpenGLExtraFunctions* gl, GLenum type, const QString& source, const QString& label,
               QString* error) {
    const GLuint shader = gl->glCreateShader(type);
    const QByteArray utf8 = source.toUtf8();
    const char* data = utf8.constData();
    gl->glShaderSource(shader, 1, &data, nullptr);
    gl->glCompileShader(shader);

    GLint ok = GL_FALSE;
    gl->glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint length = 0;
        gl->glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        QByteArray log(length > 1 ? length : 1, '\0');
        gl->glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
        if (error) {
            *error = QStringLiteral("compilazione %1 fallita: %2")
                         .arg(label, QString::fromUtf8(log));
        }
        gl->glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

bool Program::build(QOpenGLExtraFunctions* gl, const QString& vertexPath, const QString& fragmentPath,
                    QString* error) {
    const QString vertexSource = readResource(vertexPath, error);
    if (vertexSource.isEmpty()) {
        return false;
    }
    const QString fragmentSource = readResource(fragmentPath, error);
    if (fragmentSource.isEmpty()) {
        return false;
    }

    const GLuint vs = compile(gl, GL_VERTEX_SHADER, vertexSource, vertexPath, error);
    if (vs == 0) {
        return false;
    }
    const GLuint fs = compile(gl, GL_FRAGMENT_SHADER, fragmentSource, fragmentPath, error);
    if (fs == 0) {
        gl->glDeleteShader(vs);
        return false;
    }

    const GLuint program = gl->glCreateProgram();
    gl->glAttachShader(program, vs);
    gl->glAttachShader(program, fs);
    gl->glLinkProgram(program);
    gl->glDeleteShader(vs);
    gl->glDeleteShader(fs);

    GLint ok = GL_FALSE;
    gl->glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint length = 0;
        gl->glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        QByteArray log(length > 1 ? length : 1, '\0');
        gl->glGetProgramInfoLog(program, log.size(), nullptr, log.data());
        if (error) {
            *error = QStringLiteral("link fallito (%1 + %2): %3")
                         .arg(vertexPath, fragmentPath, QString::fromUtf8(log));
        }
        gl->glDeleteProgram(program);
        return false;
    }

    destroy(gl);
    id_ = program;
    return true;
}

void Program::destroy(QOpenGLExtraFunctions* gl) {
    if (id_ != 0) {
        gl->glDeleteProgram(id_);
        id_ = 0;
    }
    uniforms_.clear();
}

void Program::use(QOpenGLExtraFunctions* gl) const { gl->glUseProgram(id_); }

GLint Program::uniform(QOpenGLExtraFunctions* gl, const char* name) {
    const auto it = uniforms_.find(name);
    if (it != uniforms_.end()) {
        return it->second;
    }
    const GLint location = gl->glGetUniformLocation(id_, name);
    uniforms_.emplace(name, location);
    return location;
}

// Le uniform assenti vengono ignorate (loc == -1): uno shader puo' essere
// semplificato senza far crashare il renderer.
void Program::set1f(QOpenGLExtraFunctions* gl, const char* name, float value) {
    const GLint location = uniform(gl, name);
    if (location >= 0) gl->glUniform1f(location, value);
}

void Program::set2f(QOpenGLExtraFunctions* gl, const char* name, float a, float b) {
    const GLint location = uniform(gl, name);
    if (location >= 0) gl->glUniform2f(location, a, b);
}

void Program::set3f(QOpenGLExtraFunctions* gl, const char* name, float a, float b, float c) {
    const GLint location = uniform(gl, name);
    if (location >= 0) gl->glUniform3f(location, a, b, c);
}

void Program::set4f(QOpenGLExtraFunctions* gl, const char* name, float a, float b, float c,
                    float d) {
    const GLint location = uniform(gl, name);
    if (location >= 0) gl->glUniform4f(location, a, b, c, d);
}

void Program::setInt(QOpenGLExtraFunctions* gl, const char* name, int value) {
    const GLint location = uniform(gl, name);
    if (location >= 0) gl->glUniform1i(location, value);
}

void Program::setMat4(QOpenGLExtraFunctions* gl, const char* name, const float* columnMajor) {
    const GLint location = uniform(gl, name);
    if (location >= 0) gl->glUniformMatrix4fv(location, 1, GL_FALSE, columnMajor);
}

GpuMesh upload(QOpenGLExtraFunctions* gl, const std::vector<Vertex>& vertices,
               const std::vector<std::uint32_t>& indices) {
    GpuMesh mesh;
    if (vertices.empty() || indices.empty()) {
        return mesh;
    }

    gl->glGenVertexArrays(1, &mesh.vao);
    gl->glBindVertexArray(mesh.vao);

    gl->glGenBuffers(1, &mesh.vbo);
    gl->glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    gl->glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                     vertices.data(), GL_STATIC_DRAW);

    gl->glGenBuffers(1, &mesh.ibo);
    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
    gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                     indices.data(), GL_STATIC_DRAW);

    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, px)));
    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, nx)));

    gl->glBindVertexArray(0);
    mesh.indexCount = static_cast<GLsizei>(indices.size());
    return mesh;
}

void destroy(QOpenGLExtraFunctions* gl, GpuMesh& mesh) {
    if (mesh.ibo != 0) gl->glDeleteBuffers(1, &mesh.ibo);
    if (mesh.vbo != 0) gl->glDeleteBuffers(1, &mesh.vbo);
    if (mesh.vao != 0) gl->glDeleteVertexArrays(1, &mesh.vao);
    mesh = {};
}

}  // namespace w3::glx
