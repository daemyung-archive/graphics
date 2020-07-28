//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#include "document.h"

#include <cstddef>
#include <fstream>
#include <algorithm>

namespace {

//----------------------------------------------------------------------------------------------------------------------

constexpr auto kVertexBufferOffset = 16;

//----------------------------------------------------------------------------------------------------------------------

struct Vertex {
    simd_float3 position;
    simd_ushort4 joints;
    simd_float4 weights;
};

//----------------------------------------------------------------------------------------------------------------------

simd_float4x4 simd_translation_matrix(simd_float3 translation) {
    return simd_matrix(simd_make_float4(1.0, 0.0, 0.0, 0.0),
                       simd_make_float4(0.0, 1.0, 0.0, 0.0),
                       simd_make_float4(0.0, 0.0, 1.0, 0.0),
                       simd_make_float4(translation, 1.0));
}

//----------------------------------------------------------------------------------------------------------------------

simd_float4x4 simd_scale_matrix(simd_float3 scale) {
    return simd_matrix(simd_make_float4(scale[0], 0.0, 0.0, 0.0),
                       simd_make_float4(0.0, scale[1], 0.0, 0.0),
                       simd_make_float4(0.0, 0.0, scale[2], 0.0),
                       simd_make_float4(0.0, 0.0, 0.0, 1.0));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename T>
simd_float4x4 simd_make_matrix(const T* data) {
    return simd_matrix(simd_make_float4(static_cast<float>(data[4 * 0 + 0]),
                                        static_cast<float>(data[4 * 0 + 1]),
                                        static_cast<float>(data[4 * 0 + 2]),
                                        static_cast<float>(data[4 * 0 + 3])),
                       simd_make_float4(static_cast<float>(data[4 * 1 + 0]),
                                        static_cast<float>(data[4 * 1 + 1]),
                                        static_cast<float>(data[4 * 1 + 2]),
                                        static_cast<float>(data[4 * 1 + 3])),
                       simd_make_float4(static_cast<float>(data[4 * 2 + 0]),
                                        static_cast<float>(data[4 * 2 + 1]),
                                        static_cast<float>(data[4 * 2 + 2]),
                                        static_cast<float>(data[4 * 2 + 3])),
                       simd_make_float4(static_cast<float>(data[4 * 3 + 0]),
                                        static_cast<float>(data[4 * 3 + 1]),
                                        static_cast<float>(data[4 * 3 + 2]),
                                        static_cast<float>(data[4 * 3 + 3])));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename T>
simd_float4x4 simd_make_matrix(const std::vector<T>& data) {
    return simd_make_matrix(&data[0]);
}

//----------------------------------------------------------------------------------------------------------------------

simd_float4x4 simd_make_orthographic_matrix(float r, float l, float t, float b, float f, float n) {
    return simd_matrix(simd_make_float4(2.0f / (r - l), 0.0f, 0.0f, 0.0f),
                       simd_make_float4(0.0f, 2.0f / (t - b), 0.0f, 0.0f),
                       simd_make_float4(0.0f, 0.0f, -2.0f / (f - n), 0.0f),
                       simd_make_float4(-(r + l) / (r - l), -(t + b) / (t - b), -(f + n) / (f - n), 1.0f));

}

//----------------------------------------------------------------------------------------------------------------------

simd_float4x4 CalcModelMatrix(Node* node) {
    if (!node->parent) {
        return node->matrix;
    }

    return matrix_multiply(CalcModelMatrix(node->parent), node->matrix);
}

//----------------------------------------------------------------------------------------------------------------------

void UpdateMatrix(Node* node) {
    node->matrix = simd_translation_matrix(node->translation);
    node->matrix = matrix_multiply(node->matrix, simd_matrix4x4(node->rotation));
    node->matrix = matrix_multiply(node->matrix, simd_scale_matrix(node->scale));
}

//----------------------------------------------------------------------------------------------------------------------

} // namespace of

//----------------------------------------------------------------------------------------------------------------------

Document::Document(id<MTLDevice> device, id<MTLCommandQueue> command_queue, const tinygltf::Model& model) {
    //
    // Create a command buffer.
    //
    auto command_buffer = [command_queue commandBuffer];

    //
    // Create a blit command encoder for the resource uploading.
    //
    auto blit_encoder = [command_buffer blitCommandEncoder];

    //
    // Resize containers.
    //
    meshes_.resize(model.meshes.size());
    nodes_.resize(model.nodes.size());
    skins_.resize(model.skins.size());
    scenes_.resize(model.scenes.size());
    animations_.resize(model.animations.size());

    //
    // Build meshes.
    //
    for (auto i = 0; i != meshes_.size(); ++i) {
        //
        // Build primitives.
        //
        meshes_[i].primitives.resize(model.meshes[i].primitives.size());

        for (auto j = 0; j != meshes_[i].primitives.size(); ++j) {
            auto& attributes = model.meshes[i].primitives[j].attributes;

            std::vector<Vertex> vertices;

            // Set the position of vertices.
            if (attributes.count("POSITION")) {
                auto& accessor = model.accessors[attributes.at("POSITION")];
                auto& buffer_view = model.bufferViews[accessor.bufferView];
                auto& buffer = model.buffers[buffer_view.buffer];

                vertices.resize(accessor.count);

                auto offset = accessor.byteOffset + buffer_view.byteOffset;
                auto stride = accessor.ByteStride(buffer_view) / sizeof(float);
                auto data = reinterpret_cast<const float*>(&buffer.data[offset]);

                for (auto k = 0; k != vertices.size(); ++k) {
                    vertices[k].position = simd_make_float3(*(data + 0),
                                                            *(data + 1),
                                                            *(data + 2));
                    data += stride;
                }
            }

            // Set joints of vertices.
            if (attributes.count("JOINTS_0")) {
                auto& accessor = model.accessors[attributes.at("JOINTS_0")];
                auto& buffer_view = model.bufferViews[accessor.bufferView];
                auto& buffer = model.buffers[buffer_view.buffer];

                auto offset = accessor.byteOffset + buffer_view.byteOffset;
                auto stride = accessor.ByteStride(buffer_view) / sizeof(uint16_t);
                auto data = reinterpret_cast<const uint16_t*>(&buffer.data[offset]);

                for (auto k = 0; k != vertices.size(); ++k) {
                    vertices[k].joints = simd_make_ushort4(*(data + 0),
                                                           *(data + 1),
                                                           *(data + 2),
                                                           *(data + 3));
                    data += stride;
                }
            }

            // Set weights of vertices.
            if (attributes.count("WEIGHTS_0")) {
                auto& accessor = model.accessors[attributes.at("WEIGHTS_0")];
                auto& buffer_view = model.bufferViews[accessor.bufferView];
                auto& buffer = model.buffers[buffer_view.buffer];

                auto offset = accessor.byteOffset + buffer_view.byteOffset;
                auto stride = accessor.ByteStride(buffer_view) / sizeof(float);
                auto data = reinterpret_cast<const float*>(&buffer.data[offset]);

                for (auto k = 0; k != vertices.size(); ++k) {
                    vertices[k].weights = simd_make_float4(*(data + 0),
                                                           *(data + 1),
                                                           *(data + 2),
                                                           *(data + 3));
                    data += stride;
                }
            }

            auto vertex_buffer = RecordUploadData(device,
                                                  command_buffer, blit_encoder,
                                                  vertices.data(),
                                                  sizeof(Vertex) * vertices.size());
            meshes_[i].primitives[j].vertex_buffer = vertex_buffer;

            // Set indices.
            std::vector<uint16_t> indices;

            auto& accessor = model.accessors[model.meshes[i].primitives[j].indices];
            auto& buffer_view = model.bufferViews[accessor.bufferView];
            auto& buffer = model.buffers[buffer_view.buffer];

            indices.resize(accessor.count);

            auto offset = accessor.byteOffset + buffer_view.byteOffset;
            auto data = reinterpret_cast<const uint16_t*>(&buffer.data[offset]);

            for (auto k = 0; k != indices.size(); ++k) {
                indices[k] = *data;
                data += 1;
            }

            auto index_buffer = RecordUploadData(device,
                                                 command_buffer, blit_encoder,
                                                 indices.data(),
                                                 sizeof(uint16_t) * indices.size());
            meshes_[i].primitives[j].index_buffer = index_buffer;
            meshes_[i].primitives[j].draw_count = indices.size();
        }
    }

    [blit_encoder endEncoding];
    [command_buffer commit];

    //
    // Build nodes.
    //
    for (auto i = 0; i != nodes_.size(); ++i) {
        // Set the name of node.
        nodes_[i].name = model.nodes[i].name;

        // Set the skin of node.
        if (model.nodes[i].skin >= 0) {
            nodes_[i].skin = &skins_[model.nodes[i].skin];
        }

        // Set the mesh of node.
        if (model.nodes[i].mesh >= 0) {
            nodes_[i].mesh = &meshes_[model.nodes[i].mesh];
        }

        // Set the children of node.
        std::transform(model.nodes[i].children.begin(), model.nodes[i].children.end(),
                       std::back_inserter(nodes_[i].children), [this](size_t j) { return &nodes_[j]; });

        // Set the parent of child node.
        for (auto& child : nodes_[i].children) {
            child->parent = &nodes_[i];
        }

        // Set the rotation of node.
        if (!model.nodes[i].rotation.empty()) {
            nodes_[i].rotation = simd_quaternion(static_cast<float>(model.nodes[i].rotation[0]),
                                                 static_cast<float>(model.nodes[i].rotation[1]),
                                                 static_cast<float>(model.nodes[i].rotation[2]),
                                                 static_cast<float>(model.nodes[i].rotation[3]));
        } else {
            nodes_[i].rotation = simd_quaternion(0.0f, simd_make_float3(0.0f, 1.0f, 0.0f));
        }

        // Set the scale of node.
        if (!model.nodes[i].scale.empty()) {
            nodes_[i].scale = simd_make_float3(static_cast<float>(model.nodes[i].scale[0]),
                                               static_cast<float>(model.nodes[i].scale[1]),
                                               static_cast<float>(model.nodes[i].scale[2]));

        } else {
            nodes_[i].scale = simd_make_float3(1.0f, 1.0f, 1.0f);
        }

        // Set the translation of node.
        if (!model.nodes[i].translation.empty()) {
            nodes_[i].translation = simd_make_float3(static_cast<float>(model.nodes[i].translation[0]),
                                                     static_cast<float>(model.nodes[i].translation[1]),
                                                     static_cast<float>(model.nodes[i].translation[2]));

        } else {
            nodes_[i].translation = simd_make_float3(0.0f, 0.0f, 0.0f);
        }

        // Set the matrix of node.
        if (!model.nodes[i].matrix.empty()) {
            nodes_[i].matrix = simd_make_matrix(model.nodes[i].matrix);
        } else {
            UpdateMatrix(&nodes_[i]);
        }
    }

    //
    // Build skins.
    //
    for (auto i = 0; i != skins_.size(); ++i) {
        // Set the name of skin.
        skins_[i].name = model.skins[i].name;

        // Set the inverse bind matrices of skin.
        auto& accessor = model.accessors[model.skins[i].inverseBindMatrices];
        auto& buffer_view = model.bufferViews[accessor.bufferView];
        auto& buffer = model.buffers[buffer_view.buffer];

        skins_[i].inverse_bind_matrices.resize(accessor.count);

        auto offset = accessor.byteOffset + buffer_view.byteOffset;
        auto stride = accessor.ByteStride(buffer_view) / sizeof(float);
        auto data = reinterpret_cast<const float*>(&buffer.data[offset]);

        for (auto j = 0; j != skins_[i].inverse_bind_matrices.size(); ++j) {
            skins_[i].inverse_bind_matrices[j] = simd_make_matrix(data);
            data += stride;
        }

        // Set the skeleton of skin.
        skins_[i].skeleton = &nodes_[model.skins[i].skeleton];

        // Set joints of skin.
        std::transform(model.skins[i].joints.begin(), model.skins[i].joints.end(),
                       std::back_inserter(skins_[i].joints), [this](size_t j) { return &nodes_[j]; });
    }

    //
    // Build scenes.
    //
    for (auto i = 0; i != scenes_.size(); ++i) {
        // Set the name of scene.
        scenes_[i].name = model.scenes[i].name;

        // Set root nodes of scene.
        std::transform(model.scenes[i].nodes.begin(), model.scenes[i].nodes.end(),
                       std::back_inserter(scenes_[i].nodes), [this](size_t j) { return &nodes_[j]; });
    }

    //
    // Set the default scene.
    //
    default_scene_ = &scenes_[model.defaultScene];

    //
    // Build animations.
    //
    for (auto i = 0; i != model.animations.size(); ++i) {
        animations_[i].name = model.animations[i].name;

        animations_[i].samplers.resize(model.animations[i].samplers.size());
        animations_[i].channels.resize(model.animations[i].channels.size());

        for (auto j = 0; j != animations_[i].samplers.size(); ++j) {
            auto& sampler = animations_[i].samplers[j];
            auto& gltf_sampler = model.animations[i].samplers[j];

            auto& input_accessor = model.accessors[gltf_sampler.input];
            auto& input_buffer_view = model.bufferViews[input_accessor.bufferView];
            auto& input_buffer = model.buffers[input_buffer_view.buffer];

            sampler.times.resize(input_accessor.count);

            auto input_offset = input_accessor.byteOffset + input_buffer_view.byteOffset;
            auto input_stride = input_accessor.ByteStride(input_buffer_view) / sizeof(float);
            auto input_data = reinterpret_cast<const float*>(&input_buffer.data[input_offset]);

            for (auto k = 0; k != sampler.times.size(); ++k) {
                sampler.times[k] = *input_data;
                input_data += input_stride;
            }

            auto& output_accessor = model.accessors[gltf_sampler.output];
            auto& output_buffer_view = model.bufferViews[output_accessor.bufferView];
            auto& output_buffer = model.buffers[output_buffer_view.buffer];

            sampler.values.resize(output_buffer_view.byteLength - output_accessor.byteOffset);

            auto output_offset = output_accessor.byteOffset + output_buffer_view.byteOffset;
            auto output_data = &output_buffer.data[output_offset];

            memcpy(sampler.values.data(), output_data,
                   output_buffer_view.byteLength - output_accessor.byteOffset);
        }

        for (auto j = 0; j != animations_[i].channels.size(); ++j) {
            auto& channel = animations_[i].channels[j];
            auto& gltf_channel = model.animations[i].channels[j];

            channel.sampler = &animations_[i].samplers[gltf_channel.sampler];
            channel.target = &nodes_[gltf_channel.target_node];
            channel.path = gltf_channel.target_path;
        }
    }

    // Open a shader file.
    std::fstream fin("../../src/skinning.metal", std::ios::in | std::ios::binary);

    if (!fin.is_open()) {
        throw std::runtime_error("Fail to open a shader file");
    }

    auto source = std::string(std::istreambuf_iterator<char>(fin),
                              std::istreambuf_iterator<char>());

    if (source.empty()) {
        throw std::runtime_error("A shader file is empty");
    }

    NSError* error;

    // Create a library.
    auto library = [device newLibraryWithSource:@(source.data()) options:nullptr error:&error];

    if (error) {
        throw std::runtime_error([[error localizedDescription] UTF8String]);
    }

    //
    // Create a render pipeline state.
    //
    auto pipeline_descriptor = [MTLRenderPipelineDescriptor new];
    assert(pipeline_descriptor);

    pipeline_descriptor.vertexFunction = [library newFunctionWithName:@"vs_main"];
    pipeline_descriptor.fragmentFunction = [library newFunctionWithName:@"fs_main"];
    pipeline_descriptor.vertexDescriptor.layouts[kVertexBufferOffset].stride = sizeof(Vertex);
    pipeline_descriptor.vertexDescriptor.layouts[kVertexBufferOffset].stepRate = 1;
    pipeline_descriptor.vertexDescriptor.layouts[kVertexBufferOffset].stepFunction = MTLVertexStepFunctionPerVertex;
    pipeline_descriptor.vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3;
    pipeline_descriptor.vertexDescriptor.attributes[0].offset = offsetof(Vertex, position);
    pipeline_descriptor.vertexDescriptor.attributes[0].bufferIndex = kVertexBufferOffset;
    pipeline_descriptor.vertexDescriptor.attributes[1].format = MTLVertexFormatUShort4;
    pipeline_descriptor.vertexDescriptor.attributes[1].offset = offsetof(Vertex, joints);
    pipeline_descriptor.vertexDescriptor.attributes[1].bufferIndex = kVertexBufferOffset;
    pipeline_descriptor.vertexDescriptor.attributes[2].format = MTLVertexFormatFloat4;
    pipeline_descriptor.vertexDescriptor.attributes[2].offset = offsetof(Vertex, weights);
    pipeline_descriptor.vertexDescriptor.attributes[2].bufferIndex = kVertexBufferOffset;
    pipeline_descriptor.rasterSampleCount = 1;
    pipeline_descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    pipeline_descriptor.colorAttachments[0].writeMask = MTLColorWriteMaskAll;
    pipeline_descriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;

    pso_ = [device newRenderPipelineStateWithDescriptor:pipeline_descriptor
                                                   error:&error];

    if (error) {
        throw std::runtime_error([[error localizedDescription] UTF8String]);
    }

    // Set the projection matrix.
    projection_matrix_ = simd_make_orthographic_matrix(-1.5f, 1.5, 1.5f, -1.5f, 0.0f, 10.0f);

    // Set the view matrix.
    view_matrix_ = simd_translation_matrix(simd_make_float3(0.0f, -0.85f, 0.0));

    //
    // Wait to done resource copies.
    //
    [command_buffer waitUntilCompleted];
}

//----------------------------------------------------------------------------------------------------------------------

void Document::Update(double delta_time) {
    for (auto& animation : animations_) {
        Update(delta_time, &animation);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Document::Render(id<MTLRenderCommandEncoder> render_encoder) {
    [render_encoder setTriangleFillMode:MTLTriangleFillModeLines];

    // Set the pipeline state.
    [render_encoder setRenderPipelineState:pso_];

    // Set the projection matrix.
    [render_encoder setVertexBytes:&projection_matrix_ length:sizeof(simd_float4x4) atIndex:0];

    // Set the view matrix.
    [render_encoder setVertexBytes:&view_matrix_ length:sizeof(simd_float4x4) atIndex:1];

    for (auto& node : nodes_) {
        Render(render_encoder, &node);
    }
}

//----------------------------------------------------------------------------------------------------------------------

id<MTLBuffer> Document::RecordUploadData(id<MTLDevice> device,
                                         id<MTLCommandBuffer> command_buffer, id<MTLBlitCommandEncoder> blit_encoder,
                                         void* data, size_t size) {
    auto src_buffer = [device newBufferWithBytes:data
                                          length:size
                                         options:MTLResourceCPUCacheModeWriteCombined];

    auto dst_buffer = [device newBufferWithLength:size
                                          options:MTLResourceStorageModePrivate];

    [blit_encoder copyFromBuffer:src_buffer
                    sourceOffset:0
                        toBuffer:dst_buffer
               destinationOffset:0
                            size:size];

    __block auto staging_buffer = src_buffer;
    [command_buffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
        staging_buffer = nil;
    }];

    return src_buffer;
}

//----------------------------------------------------------------------------------------------------------------------

void Document::Update(double delta_time, Animation* animation) {
    // Append the delta time.
    animation->time += delta_time;

    for (auto& channel : animation->channels) {
        if (channel.path == "rotation") {
            auto& times = channel.sampler->times;
            auto& values = channel.sampler->values;

            // Adjust the current time.
            auto time = animation->time;

            if (time < times.front()) {
                time = times.front();
            }

            time = fmod(time, times.back());

            simd_quatf rotation;
            for (auto i = 1; i != times.size(); ++i) {
                if (times[i - 1] <= time && time < times[i]) {
                    auto data = reinterpret_cast<float*>(values.data()) + (i - 1) * 4;

                    // It's the previous rotation.
                    simd_quatf q0 = simd_quaternion(*(data + 0),
                                                    *(data + 1),
                                                    *(data + 2),
                                                    *(data + 3));

                    // It's the next rotation.
                    simd_quatf q1 = simd_quaternion(*(data + 4),
                                                    *(data + 5),
                                                    *(data + 6),
                                                    *(data + 7));

                    // Do the linear interpolation.
                    rotation = simd_slerp(q0, q1,
                                          (time - times[i - 1]) / (times[i] - times[i - 1]));
                    break;
                }
            }

            // Update the rotation.
            channel.target->rotation = rotation;
        } else {
            assert(false && "Only the rotation path is supported.");
        }

        UpdateMatrix(channel.target);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Document::Render(id<MTLRenderCommandEncoder> render_encoder, Node* node) {
    if (node->mesh) {
        for (auto& primitive : node->mesh->primitives) {
            const auto kModelMatrix = CalcModelMatrix(node);
            const auto kInverseModelMatrix = simd_inverse(kModelMatrix);

            // Set the vertex buffer.
            [render_encoder setVertexBuffer:primitive.vertex_buffer offset:0 atIndex:kVertexBufferOffset];

            // Set the model matrix.
            [render_encoder setVertexBytes:&kModelMatrix length:sizeof(simd_float4x4) atIndex:2];

            //
            // Calculate the joint palette.
            //
            if (node->skin) {
                auto& skin = node->skin;
                auto& joints = skin->joints;
                std::vector<simd_float4x4> joint_matrices(joints.size());

                for (auto i = 0; i != joint_matrices.size(); ++i) {
                    // Calculate inverse_model_matrix * joint_matrix * inverse_bind_matrix.
                    joint_matrices[i] = kInverseModelMatrix;
                    joint_matrices[i] = matrix_multiply(joint_matrices[i], CalcModelMatrix(joints[i]));
                    joint_matrices[i] = matrix_multiply(joint_matrices[i], skin->inverse_bind_matrices[i]);
                }

                // Set joint palette matrices.
                [render_encoder setVertexBytes:&joint_matrices[0]
                                        length:sizeof(simd_float4x4) * joint_matrices.size()
                                       atIndex:3];
            }

            // Draw a primitive.
            [render_encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                       indexCount:primitive.draw_count
                                        indexType:MTLIndexTypeUInt16
                                      indexBuffer:primitive.index_buffer
                                indexBufferOffset:0];
        }
    }

    for (auto& child : node->children) {
        Render(render_encoder, child);
    }
}

//----------------------------------------------------------------------------------------------------------------------
