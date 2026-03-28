#pragma once

#include <abstract_renderer.h>

#include <QColor>
#include <QMatrix4x4>
#include <QVector3D>
#include <QQuaternion>

#include <vector>
#include <cstdint>

struct cuboid;
class cuboid_manager;

class cuboid_renderer final : public abstract_renderer {
    public:
    static constexpr quint32 uniform_size = 80;

    static constexpr float default_alpha = 0.15f; //opacity while not selected
    static constexpr float highlight_alpha = 0.25f; //opacity while selected
    static constexpr float wireframe_alpha = 1.0f;

    static constexpr quint32 vertices_per_cuboid = 24;
    static constexpr quint32 indices_per_cuboid_face = 36;
    static constexpr quint32 indices_per_cuboid_edge = 24;

    cuboid_renderer();
    ~cuboid_renderer() override;

    void initialize(QRhi *rhi, QRhiRenderPassDescriptor *render_pass, const QSize &viewport_size) override;
    void render(QRhiCommandBuffer *command_buffer, const QSize &viewport_size, orbital_camera *camera) override;
    void release_resources() override;

    void set_cuboid_manager(cuboid_manager *manager) { cmngr = manager; }
    void set_geometry_changed() { geometry_changed = true; }

    private:
    struct cuboid_vertex {
        float position[3];
        float normal[3];
        float color[4];
    };

    void build_face_pipeline();
    void build_edge_pipeline();
    void build_geometry();
    void upload_uniform(const QSize &viewport_size, orbital_camera *camera);

    void generate_cuboid_vertices(const cuboid &c, bool is_selected, std::vector<cuboid_vertex> &output_vertices) const;
    void generate_cuboid_face_indices(uint32_t base_vertex, std::vector<uint32_t> &output_indices) const;
    void generate_cuboid_edge_indices(uint32_t base_vertex, std::vector<uint32_t> &output_indices) const;

    cuboid_manager *cmngr = nullptr;

    std::unique_ptr<QRhiGraphicsPipeline> face_pipeline;
    std::unique_ptr<QRhiGraphicsPipeline> edge_pipeline;

    std::unique_ptr<QRhiBuffer> vertex_buffer;
    std::unique_ptr<QRhiBuffer> face_index_buffer;
    std::unique_ptr<QRhiBuffer> edge_index_buffer;

    quint32 vertex_buffer_capacity = 0;
    quint32 face_index_buffer_capacity = 0;
    quint32 edge_index_buffer_capacity = 0;

    quint32 face_index_count = 0;
    quint32 edge_index_count = 0;

    QMatrix4x4 last_mvp_matrix;

    bool geometry_changed = true;
    bool uniform_changed = true;
};