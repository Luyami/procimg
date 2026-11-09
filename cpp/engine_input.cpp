#include <engine.h>
#include <iostream>

bool Engine::__inputStep(){
    if (glfwWindowShouldClose(Engine::window)){
        Engine::stop();
        return false;
    }

    static auto lastUndo = std::chrono::steady_clock::now();

    if (glfwGetKey(Engine::window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS &&
        glfwGetKey(Engine::window, GLFW_KEY_Z) == GLFW_PRESS)
    {
        auto now = std::chrono::steady_clock::now();

        if (now - lastUndo >= std::chrono::milliseconds(500)){ //undo cooldown
            Engine::revertOperation();
            lastUndo = now;
        }
    }

    glfwPollEvents();

    return true;
}
