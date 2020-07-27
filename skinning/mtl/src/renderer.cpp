//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#include "renderer.h"

#include <cassert>
#include <stdexcept>

#include "document.h"

//----------------------------------------------------------------------------------------------------------------------

Renderer::Renderer(NSWindow* window)
: window_(window) {
    //
    // Create a system default device.
    //
    device_ = MTLCreateSystemDefaultDevice();

    if (!device_) {
        throw std::runtime_error("Fail to create MTLDevice");
    }

    //
    // Create a metal layer.
    //
    layer_ = [CAMetalLayer new];

    if (!layer_) {
        throw std::runtime_error("Fail to create CAMetalLayer");
    }

    //
    // Set a metal layer to a view.
    //
    auto view = [window_ contentView];
    assert(view);

    view.wantsLayer = YES;
    view.layer = layer_;

    //
    // Configure a metal layer.
    //
    layer_.device = device_;
    layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer_.maximumDrawableCount = kNumBackBuffer;
    layer_.drawableSize = view.frame.size;

    //
    // Create a command queue.
    //
    command_queue_ = [device_ newCommandQueue];

    if (!command_queue_) {
        throw std::runtime_error("Fail to create MTLCommandQueue");
    }
}

//----------------------------------------------------------------------------------------------------------------------

Renderer::~Renderer() {
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::LoadGLTF(const tinygltf::Model& model) {
    document_ = std::make_unique<Document>(device_, command_queue_, model);
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::Update(double delta_time) {
    if (document_) {
        document_->Update(delta_time);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::Render(double delta_time) {
    auto drawable = [layer_ nextDrawable];

    if (!drawable) {
        return;
    }

    auto command_buffer = [command_queue_ commandBuffer];
    assert(command_buffer);

    auto present_pass_descriptor = [MTLRenderPassDescriptor new];
    assert(present_pass_descriptor);

    present_pass_descriptor.colorAttachments[0].texture = drawable.texture;
    present_pass_descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    present_pass_descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    present_pass_descriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);

    auto render_encoder = [command_buffer renderCommandEncoderWithDescriptor:present_pass_descriptor];

    if (document_) {
        document_->Render(render_encoder);
    }

    [render_encoder endEncoding];
    [command_buffer presentDrawable:drawable];
    [command_buffer commit];
}

//----------------------------------------------------------------------------------------------------------------------
