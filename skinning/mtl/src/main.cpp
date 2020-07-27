//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#include <cstdlib>
#include <spdlog/spdlog.h>

#include "renderer.h"
#include "glfw_lib.h"
#include "gltf_lib.h"

//----------------------------------------------------------------------------------------------------------------------

static void OnError(int err, const char* msg) {
    spdlog::error("[{}] {}", err, msg);
}

//----------------------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    @autoreleasepool {
        glfwSetErrorCallback(OnError);

        if (!glfwInit()) {
            spdlog::error("Fail to initialize GLFW!!!");
            exit(EXIT_FAILURE);
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        auto window = glfwCreateWindow(512, 512, "Skinning", nullptr, nullptr);
        assert(window);

        std::unique_ptr<Renderer> renderer;

        try {
            NSWindow* native_window = glfwGetCocoaWindow(window);
            assert(native_window);

            renderer = std::make_unique<Renderer>(native_window);
        } catch(std::exception e) {
            spdlog::error("{}", e.what());
            exit(EXIT_FAILURE);
        }

        tinygltf::Model model;
        std::string error, warning;
        auto is_loaded = tinygltf::TinyGLTF().LoadASCIIFromFile(&model, &error, &warning, "../../asset/SimpleSkin.gltf");

        if (!error.empty()) {
            spdlog::error("{}", error);
        }

        if (!warning.empty()) {
            spdlog::warn("{}", warning);
        }

        if (!is_loaded) {
            spdlog::error("{}", "Fail to load SimpleSkin.gltf");
            exit(EXIT_FAILURE);
        }

        renderer->LoadGLTF(model);

        auto prev_time_stamp = glfwGetTime();
        while (!glfwWindowShouldClose(window)) {
            auto curr_time_stamp = glfwGetTime();
            auto delta_time = curr_time_stamp - prev_time_stamp;

            renderer->Update(delta_time);
            renderer->Render(delta_time);

            prev_time_stamp = curr_time_stamp;

            glfwPollEvents();
        }

        glfwDestroyWindow(window);
        glfwTerminate();

        exit(EXIT_SUCCESS);
    }
}

//----------------------------------------------------------------------------------------------------------------------
