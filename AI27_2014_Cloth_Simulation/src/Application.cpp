#include <glad\glad.h>
#include <GLFW\glfw3.h>
#include <imgui\imgui.h>
#include <imgui\imgui_impl_opengl3.h>
#include <imgui\imgui_impl_glfw.h>
#include <glm\glm.hpp>
#include <glm\ext.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdio.h>
#include <vector>
#include <chrono>
#include <thread>
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Renderer.h"

// Partikli za simulaciju - predstavljaju vertexe
struct Particles
{
    glm::vec3 position;
    glm::vec3 prevPosition;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    bool isPinned;
};

void CalculateSpringForces(Particles& particle, const std::vector<Particles>& particles, int clothWidth, int clothHeight, float stiffness, float damping, float restLength);
void ApplyDamping(Particles& particle, float damping);
void UpdateParticlePosition(Particles& particle, float deltaTime);

int main()
{
    // Konstante
    const int clothWidth = 20;
    const int clothHeight = 20;

    // Cloth generisanje geometrije
    std::vector<float> clothVertices;
    std::vector<unsigned int> clothIndices;

    // Generate cloth vertices
    for (int row = 0; row < clothHeight; ++row) {
        for (int col = 0; col < clothWidth; ++col) {
            float x = static_cast<float>(col) / (clothHeight - 1);
            float y = static_cast<float>(row) / (clothWidth - 1);
            float z = 0.0f;

            clothVertices.push_back(x);
            clothVertices.push_back(y);
            clothVertices.push_back(z);
        }
    }

    // Generate cloth indices
    for (int row = 0; row < clothHeight - 1; ++row) {
        for (int col = 0; col < clothWidth - 1; ++col) {
            int currVertex = row * clothWidth + col;
            int nextRowVertex = (row + 1) * clothWidth + col;

            clothIndices.push_back(currVertex);
            clothIndices.push_back(nextRowVertex);
            clothIndices.push_back(currVertex + 1);

            clothIndices.push_back(currVertex + 1);
            clothIndices.push_back(nextRowVertex);
            clothIndices.push_back(nextRowVertex + 1);
        }
    }

    std::vector<Particles> clothParticles;

    for (int row = 0; row < clothHeight; ++row)
    {
        for (int col = 0; col < clothWidth; ++col)
        {
            clothParticles.push_back({
                glm::vec3(static_cast<float>(col) / (clothHeight - 1), static_cast<float>(row) / (clothWidth - 1), 0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                (row == 19)
                });
        }
    }

    //Inicijalizacija GLFW za window u OpenGL kontekstu
    if (!glfwInit())
    {
        std::cout << "GLFW nije inicijalizovan proveri link" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    //GLFW prozor za prikaz
    GLFWwindow* window = glfwCreateWindow(800, 800, "AI27-2014 Cloth Simulation", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    //Inicijalizacija GLAD	zajedno sa exepion handling

    if (!gladLoadGL())
    {
        std::cout << "GLAD se nije inicijalizovao" << std::endl;
        return -1;
    }

    // Inicijalizacija ImGUI
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = static_cast<float>(800);
    io.DisplaySize.y = static_cast<float>(800);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    //Cloth Vertex i Index Buffer-i
    VertexBuffer clothVertexBuffer(clothVertices.data(), clothVertices.size() * sizeof(float));
    IndexBuffer clothIndexBuffer(clothIndices.data(), clothIndices.size());

    //Cloth Vertex array za definisanje layouta
    VertexArray clothVAO;
    VertexBufferLayout clothLayout;
    clothLayout.push<float>(3);
    clothVAO.AddBuffer(clothVertexBuffer, clothLayout);

    // Shaders
    Shader shaderProgram("src/Basic.shader");

    // SETUP ALL MATRICES FOR DRAWING IN 3D
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), 800.0f / 800.0f, 0.1f, 150.0f);

    glm::vec3 cameraPosition = glm::vec3(4.5f, 2.0f, 4.5f);
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.5f, 0.0f);
    glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 viewMatrix = glm::lookAt(cameraPosition, cameraTarget, upVector);

    //Cloth transforms
    glm::vec3 clothPosition = glm::vec3(0.0, 0.0f, 0.0f);
    glm::vec3 clothRotation = glm::vec3(0.0f, glm::radians(90.0f), 0.0f);
    glm::vec3 clothScale = glm::vec3(1.0f, 1.0f, 1.0f);

    glm::mat4 clothModel = glm::mat4(1.0f); // Identity matrix
    clothModel = glm::translate(clothModel, clothPosition);
    clothModel = glm::rotate(clothModel, clothRotation.y, glm::vec3(0, 1, 0));
    clothModel = glm::scale(clothModel, clothScale);

    glm::mat4 clothMVP = projectionMatrix * viewMatrix * clothModel;

    float stiffness = 500.0f;
    float restLength = 0.1f;
    float damping = 0.9f;
    float wind = 0.0f;
    float deltaTime = 0.0016f;

    double previousTime = glfwGetTime();
    const double targetFrameTime = 1.0 / 30.0; // 30fps

    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        
        // Framerate
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - previousTime;
        previousTime = currentTime;

        // Limit frame rate to 30fps
        if (deltaTime < targetFrameTime) {
            double sleepTime = targetFrameTime - deltaTime;
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(sleepTime * 1000)));
            currentTime = glfwGetTime();
            deltaTime = currentTime - previousTime;
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        // Create ImGui interface for cloth simulation settings
        ImGui::SetNextWindowPos(ImVec2(10, 610), ImGuiCond_Always);
        ImGui::Begin("Cloth Simulation Settings");

        //Slider-i za cloth elasticnost, otpor i vetar
        ImGui::SliderFloat("Stiffness", &stiffness, 0.0f, 500.0f);
        ImGui::SliderFloat("Rest Length", &restLength, 0.001f, 1.0f);
        ImGui::SliderFloat("Damping", &damping, 0.0f, 1.0f);
        ImGui::SliderFloat("Wind force", &wind, 0.0f, 1.0f);
        ImGui::End();

        // Clear the screen
        glClearColor(0.2f, 0.2f, 0.2f, 0.8f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
          
        // Render the cloth
        shaderProgram.bind();
        shaderProgram.SetUniform4f("u_Color", 1.0f, 0.0f, 1.0f, 1.0f); 
        shaderProgram.SetUniformMat4("u_MVP", clothMVP);
        clothVAO.bind();
        clothIndexBuffer.bind();
        glDrawElements(GL_TRIANGLES, clothIndices.size(), GL_UNSIGNED_INT, nullptr);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        shaderProgram.SetUniform4f("u_Color", 0.5f, 0.0f, 0.5f, 1.0f);
        glDrawElements(GL_TRIANGLES, clothIndices.size(), GL_UNSIGNED_INT, nullptr);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Apply spring forces
        for (int i = 0; i < clothHeight; ++i)
        {
            for (int j = 0; j < clothWidth; ++j)
            {
                int index = i * clothWidth + j;
                Particles& particle = clothParticles.at(index);

                if (!particle.isPinned)
                {
                    // Gravity
                    particle.acceleration = glm::vec3(0.0f, -9.81f, 0.0f);
                    //Wind
                    glm::vec3 windForce = glm::vec3(-1.11f, 0.0f, 0.0f);
                    particle.acceleration += windForce;

                    //Springs, damping and update
                    CalculateSpringForces(particle, clothParticles, clothWidth, clothHeight, stiffness, damping, restLength);
                    ApplyDamping(particle, damping);
                    UpdateParticlePosition(particle, deltaTime);
                }
            }
        }

        // update Cloth vertex position based on particles and strings
        for (int i = 0; i < clothHeight; ++i) {
            for (int j = 0; j < clothWidth; ++j) {
                int index = i * clothWidth + j;
                clothVertices.at(index*3) = clothParticles.at(index).position.x;
                clothVertices.at(index*3 + 1) = clothParticles.at(index).position.y;
                clothVertices.at(index*3 + 2) = clothParticles.at(index).position.z;
            }
        }

        // Update the clothVertexBuffer with the new vertex positions
        clothVertexBuffer.bind();
        glBufferData(GL_ARRAY_BUFFER, clothVertices.size() * sizeof(float), clothVertices.data(), GL_STREAM_DRAW);

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        

        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Shutdown ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();

    shaderProgram.~Shader();

    // Cleanup and exit
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

void CalculateSpringForces(Particles& particle, const std::vector<Particles>& particles, int clothWidth, int clothHeight, float stiffness, float damping, float restLength) 
{
    // Get the row and column of the current particle
    int row = static_cast<int>(particle.position.y * (clothHeight - 1));
    int col = static_cast<int>(particle.position.x * (clothWidth - 1));

    // Iterate through neighboring particles (above, below, left, right)
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) {
                continue; // Skip the current particle
            }

            int neighborRow = row + dr;
            int neighborCol = col + dc;

            // Check if the neighbor is within bounds
            if (neighborRow >= 0 && neighborRow < clothHeight && neighborCol >= 0 && neighborCol < clothWidth) {
                int neighborIndex = neighborRow * clothWidth + neighborCol;
                const Particles& neighborParticle = particles.at(neighborIndex);

                // Calculate the vector from current to neighbor
                glm::vec3 displacement = neighborParticle.position - particle.position;
                float distance = glm::length(displacement);

                // Calculate spring force using Hooke's Law
                float springForceMagnitude = stiffness * (distance - restLength);
                glm::vec3 springForce = springForceMagnitude * glm::normalize(displacement);

                // Apply damping force
                glm::vec3 dampingForce = -damping * (particle.velocity - neighborParticle.velocity);

                // Calculate the total force
                glm::vec3 totalForce = springForce + dampingForce;

                // Apply the total force to the current particle
                particle.acceleration += totalForce;
            }
        }
    }
}

void ApplyDamping(Particles& particle, float damping) {
    // Apply damping to particle velocities and accelerations
    particle.velocity *= (1.0f - damping);
    particle.acceleration *= (1.0f - damping);
}

void UpdateParticlePosition(Particles& particle, float deltaTime) {
    // Update particle position using Verlet integration
    glm::vec3 tempPosition = particle.position;
    particle.position = 2.0f * particle.position - particle.prevPosition + (particle.acceleration * deltaTime * deltaTime);
    particle.prevPosition = tempPosition;

    // Reset acceleration after updating position
    particle.acceleration = glm::vec3(0.0f);
}