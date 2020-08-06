//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#include <memory>
#include <spdlog/spdlog.h>

#include "renderer.h"
#include "glfw_lib.h"

//----------------------------------------------------------------------------------------------------------------------

static void OnError(int error, const char* message) {
	spdlog::error("[{}] {}", error, message);
}

//----------------------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
	glfwSetErrorCallback(OnError);

	if (!glfwInit()) {
		spdlog::error("Fail to initialize GLFW!!!");
		exit(EXIT_FAILURE);
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	auto window = glfwCreateWindow(512, 512, "PBR", nullptr, nullptr);
	assert(window);

	auto renderer = std::make_unique<Renderer>(glfwGetWin32Window(window));
	assert(renderer);

	auto prev_time_stamp = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		auto curr_time_stamp = glfwGetTime();
		auto delta_time = curr_time_stamp - prev_time_stamp;

		renderer->Update(delta_time);
		renderer->Render();

		prev_time_stamp = curr_time_stamp;

		glfwPollEvents();
	}

	renderer.reset();

	glfwDestroyWindow(window);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}

//----------------------------------------------------------------------------------------------------------------------
