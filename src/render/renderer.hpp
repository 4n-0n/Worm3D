#pragma once
// renderer.hpp — disegna il mondo: cielo, griglia cartesiana, mesh istanziate,
// aloni additivi. Non conosce input ne' HUD: riceve World + Camera.

#include "core/world.hpp"
#include "render/gl.hpp"
#include "render/mesh.hpp"

#include <QString>

#include <vector>

namespace w3 {

struct Camera {
    Vec3 position{0.0f, 2.0f, -14.0f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float fov = 58.0f;
};

struct RenderStats {
    int instances = 0;
    int drawCalls = 0;
};

class Renderer {
public:
    static constexpr int kShapeCount = static_cast<int>(MeshShape::Count);

    bool init(QOpenGLExtraFunctions* gl, QString* error);
    void shutdown(QOpenGLExtraFunctions* gl);
    void render(QOpenGLExtraFunctions* gl, const World& world, const Camera& camera, int pixelWidth,
                int pixelHeight);

    const RenderStats& stats() const { return stats_; }
    bool showGrid = true;
    // In prima persona la testa non si disegna: la camera e' al suo interno.
    void setFirstPerson(bool value) { firstPerson_ = value; }

private:
    struct Instance {
        float pos[3];
        float scale[3];
        float color[4];  // rgb + emissive
        float rot[2];    // yaw, pitch
    };

    struct GlowInstance {
        float pos[3];
        float size;
        float color[4];
    };

    void add(MeshShape shape, Vec3 position, Vec3 scale, Vec3 color, float emissive, float yaw = 0.0f,
             float pitch = 0.0f);
    void addGlow(Vec3 position, float size, Vec3 color, float alpha);
    void buildScene(const World& world);
    void setupInstanceAttributes(QOpenGLExtraFunctions* gl, GLuint vao);
    void setupGlowAttributes(QOpenGLExtraFunctions* gl);
    void drawBatches(QOpenGLExtraFunctions* gl);
    void drawGrid(QOpenGLExtraFunctions* gl, const float* viewProjection, Vec3 camPos);
    void drawLines(QOpenGLExtraFunctions* gl, const float* viewProjection);
    void drawGlows(QOpenGLExtraFunctions* gl, const float* viewProjection, const Camera& camera);

    glx::Program sceneProg_;
    glx::Program gridProg_;
    glx::Program skyProg_;
    glx::Program glowProg_;
    glx::Program lineProg_;

    glx::GpuMesh meshes_[kShapeCount];
    GLuint instanceVbo_ = 0;
    GLuint quadVao_ = 0;
    GLuint quadVbo_ = 0;
    GLuint glowVao_ = 0;
    GLuint glowVbo_ = 0;
    GLuint skyVao_ = 0;
    GLuint lineVao_ = 0;
    GLuint lineVbo_ = 0;
    GLsizei lineVertexCount_ = 0;

    std::vector<Instance> batches_[kShapeCount];
    std::vector<GlowInstance> glows_;
    std::vector<GlowInstance> glowUpload_;
    std::vector<Vec3> dust_;
    RenderStats stats_;
    bool firstPerson_ = false;
};

}  // namespace w3
