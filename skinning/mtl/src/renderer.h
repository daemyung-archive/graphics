//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#ifndef SKINNING_RENDERER_H_
#define SKINNING_RENDERER_H_

#include <memory>
#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>
#include <tiny_gltf.h>

//----------------------------------------------------------------------------------------------------------------------

class Document;

//----------------------------------------------------------------------------------------------------------------------

class Renderer final {
public:
    constexpr static auto kNumBackBuffer = 3;

    Renderer(NSWindow* window);

    ~Renderer();

    void LoadGLTF(const tinygltf::Model& model);

    void Update(double delta_time);

    void Render(double delta_time);

private:
    NSWindow* window_;
    id<MTLDevice> device_;
    CAMetalLayer* layer_;
    id<MTLCommandQueue> command_queue_;
    std::unique_ptr<Document> document_;
};

//----------------------------------------------------------------------------------------------------------------------

#endif // SKINNING_RENDERER_H_
