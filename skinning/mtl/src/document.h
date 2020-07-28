//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#ifndef SKINNING_DOCUMENT_H_
#define SKINNING_DOCUMENT_H_

#include <string>
#include <vector>
#include <Metal/Metal.h>
#include <simd/simd.h>
#include <tiny_gltf.h>

//----------------------------------------------------------------------------------------------------------------------

struct Skin;

//----------------------------------------------------------------------------------------------------------------------

struct Primitive {
    id<MTLBuffer> vertex_buffer = nil;
    id<MTLBuffer> index_buffer = nil;
    uint64_t draw_count = 0u;
};

//----------------------------------------------------------------------------------------------------------------------

struct Mesh {
    std::string name;
    std::vector<Primitive> primitives;
};

//----------------------------------------------------------------------------------------------------------------------

struct Node {
    std::string name;
    Skin* skin = nullptr;
    Mesh* mesh = nullptr;
    Node* parent = nullptr;
    std::vector<Node*> children;
    simd_quatf rotation;
    simd_float3 scale;
    simd_float3 translation;
    simd_float4x4 matrix;
};

//----------------------------------------------------------------------------------------------------------------------

struct Skin {
    std::string name;
    simd_float4x4 inverse_bind_matrix;
    Node* skeleton = nullptr;
    std::vector<Node*> joints;
};

//----------------------------------------------------------------------------------------------------------------------

struct Scene {
    std::string name;
    std::vector<Node*> nodes;
};

//----------------------------------------------------------------------------------------------------------------------

struct Animation {
    struct Sampler {
        std::vector<float> times;
        std::vector<uint8_t> values;
    };

    struct Channel {
        Sampler* sampler;
        Node* target;
        std::string path;
    };

    std::string name;
    std::vector<Sampler> samplers;
    std::vector<Channel> channels;
    double time = 0;
};

//----------------------------------------------------------------------------------------------------------------------

class Document final {
public:
    Document(id<MTLDevice> device, id<MTLCommandQueue> command_queue, const tinygltf::Model& model);

    void Update(double delta_time);

    void Render(id<MTLRenderCommandEncoder> render_encoder);

private:
    id<MTLBuffer> RecordUploadData(id<MTLDevice> device,
                                   id<MTLCommandBuffer> command_buffer, id<MTLBlitCommandEncoder> blit_encoder,
                                   void* data, size_t size);

    void Update(double delta_time, Animation* animation);

    void Render(id<MTLRenderCommandEncoder> render_encoder, Node* node);

private:
    std::vector<Mesh> meshes_;
    std::vector<Node> nodes_;
    std::vector<Skin> skins_;
    std::vector<Scene> scenes_;
    Scene* default_scene_;
    std::vector<Animation> animations_;
    id<MTLRenderPipelineState> pso_;
    simd_float4x4 projection_matrix_;
    simd_float4x4 view_matrix_;
};

//----------------------------------------------------------------------------------------------------------------------

#endif // SKINNING_DOCUMENT_H_
