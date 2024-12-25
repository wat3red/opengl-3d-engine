#if 0
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

#include "renderer.h"
#include "camera.h"
#include "error_handler.h"
#include "input_handler.h" 
#include "model.h" 
#include "cubemap.h" 

const unsigned int WINDOW_WIDTH = 640;
const unsigned int WINDOW_HEIGHT = 480;

int displayW = 0, displayH = 0;

float lastX = WINDOW_WIDTH / 2.0f;
float lastY = WINDOW_HEIGHT / 2.0f;
Camera mainCamera(glm::vec3(0.0f, 0.0f, 3.0f));

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void RenderCube()
{
	// initialize (if necessary)
	if (cubeVAO == 0)
	{
		float vertices[] = {
			// back face
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
			// front face
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			// left face
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			// right face
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
			 // bottom face
			 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			  1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
			  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			 -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			 // top face
			 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			  1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			  1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
			  1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			 -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
		};
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		// link vertex attributes
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

int main(void) {
	//ofstream fileOut("output_fbx.txt");
	ofstream fileOut("output_obj.txt");
	// Redirecting cout to write to "output.txt"
	//cout.rdbuf(fileOut.rdbuf());

	GLFWwindow* window;

	if (!glfwInit())
		return -1;

	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Cube", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	if (glewInit() != GLEW_OK) {
		std::cout << "glewInit() != GLEW_OK, exiting!" << std::endl;
		exit(1);
	}

	std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(GLDebugMessageCallback, NULL);
	stbi_set_flip_vertically_on_load(true);

	// callbacks
	InputHandler inputHandler(mainCamera, WINDOW_WIDTH, WINDOW_HEIGHT);
	InputHandler::SetCallbacks(window, &inputHandler); // Set the input callbacks

	glEnable(GL_DEPTH_TEST);
	//glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glfwSwapInterval(1); // vsync

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	// Create indices for the cube

	float nearPlane = 0.1f;
	float farPlane = 1000.0f;

	glm::vec3 rotationDelta(-90.f, 0.f, 0.f);

	Shader shader("shaders/Main.shader");
	Shader lightShader("shaders/Light.shader");
	Shader skyboxShader("shaders/Skybox.shader");
	Shader simpleDepthShader("shaders/PointShadowsDepth.shader");

	//Model ourModel("res/models/backpack/backpack.obj", true);
	//Model ourModel("res/models/room/source/am_apartment_fixed_up_rem.obj");
	//Model ourModel("res/models/bottle/source/Bottle_Water_perfect.glb");
	//Model ourModel("res/models/city/scene.gltf");
	//Model ourModel("res/models/car/125p.fbx");
	//Model ourModel("res/models/trees/source/SnowTrees.fbx");
	//Model ourModel("res/models/rome/Rom block 1 copy and box.fbm/1.fbx");
	//Model ourModel("res/models/grass/scene.gltf");
	Model ourModel("res/models/mercedes-amg-gt3/source/mercedes_amg_gt3_evo.glb");
	//Model ourModel("res/models/house/source/free_fire_maxx_new_clock_tower_map_download_free.glb");

	glm::mat4 lightModelMatrix(1.f);
	glm::vec3 lightPosition(1.5f, 1.5f, 0.f);
	lightModelMatrix = glm::scale(lightModelMatrix, glm::vec3(0.5f)); // Make the light cube smaller

	/*
	"right.jpg",
	"left.jpg",
	"top.jpg",
	"bottom.jpg",
	"front.jpg",
	"back.jpg"
	*/

	/*Cubemap skybox({
		"res/textures/skybox_Storforsen/posx.jpg",
		"res/textures/skybox_Storforsen/negx.jpg",
		"res/textures/skybox_Storforsen/posy.jpg",
		"res/textures/skybox_Storforsen/negy.jpg",
		"res/textures/skybox_Storforsen/posz.jpg",
		"res/textures/skybox_Storforsen/negz.jpg"
	});*/

	//skyboxShader.Bind();
	//skyboxShader.SetUniform1i("skybox", 0);

	// configure depth map FBO
	// -----------------------
	const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	// create depth cubemap texture
	unsigned int depthCubemap;
	glGenTextures(1, &depthCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
	for (unsigned int i = 0; i < 6; ++i)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	// attach depth texture as FBO's depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	while (!glfwWindowShouldClose(window)) {
		inputHandler.Update(window);

		// ImGui
		// |-------------------------------------|
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		{
			ImGui::Begin("Hello, world!");

			ImGui::Text("Camera settings");
			ImGui::SliderFloat("mainCamera.MovementSpeed", &mainCamera.MovementSpeed, 0.1, 300);
			ImGui::SliderFloat("mainCamera.Zoom", &mainCamera.Zoom, 0, 180);
			ImGui::SliderFloat3("mainCamera.Position", &mainCamera.Position[0], -5.0f, 5.0f);

			ImGui::Text("Other");
			ImGui::SliderFloat3("rotationDelta", &rotationDelta[0], -180.f, 180.f);
			ImGui::SliderFloat3("lightPos", &lightPosition[0], -100, 100);

			if (ImGui::Button("Reset")) {
				mainCamera.Position = glm::vec3(0.f, 0.f, 2.f);
				rotationDelta = glm::vec3(0.f, 0.f, 0.f);
			}

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::End();
		}
		ImGui::Render();
		// |-------------------------------------|
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// 1. render depth of scene to texture (from light's perspective)
		// --------------------------------------------------------------

		glm::mat4 modelMatrix(1.f);
		modelMatrix = glm::rotate(modelMatrix, glm::radians(rotationDelta.x), glm::vec3(1.f, 0.f, 0.f));
		modelMatrix = glm::rotate(modelMatrix, glm::radians(rotationDelta.y), glm::vec3(0.f, 1.f, 0.f));
		modelMatrix = glm::rotate(modelMatrix, glm::radians(rotationDelta.z), glm::vec3(0.f, 0.f, 1.f));
		//modelMatrix = glm::scale(modelMatrix, glm::vec3(110.f)); // Make the light cube smaller


		float near_plane = 1.f;
		float far_plane = 25.f;
		glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), (float)SHADOW_WIDTH / (float)SHADOW_HEIGHT, near_plane, far_plane);
		std::vector<glm::mat4> shadowTransforms;
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));

		// 1. render scene to depth cubemap
		// --------------------------------
		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);

		simpleDepthShader.Bind();
		for (unsigned int i = 0; i < 6; ++i)
			simpleDepthShader.SetUniformMat4f("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
		simpleDepthShader.SetUniformMat4f("model", modelMatrix);
		simpleDepthShader.SetUniform1f("far_plane", far_plane);
		simpleDepthShader.SetUniform3f("lightPos", lightPosition[0], lightPosition[1], lightPosition[2]);
		ourModel.Draw(simpleDepthShader);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// update matrices

		glViewport(0, 0, displayW, displayH);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 viewMatrix = mainCamera.GetViewMatrix();

		glm::mat4 projectionMatrix = glm::perspective(
			glm::radians(mainCamera.Zoom),
			static_cast<float>(displayW) / static_cast<float>(displayH),
			nearPlane,
			farPlane
		);

		shader.Bind();

		shader.SetUniformMat4f("model", modelMatrix);
		shader.SetUniformMat4f("view", viewMatrix);
		shader.SetUniformMat4f("projection", projectionMatrix);

		shader.SetUniform3f("viewPos", mainCamera.Position[0], mainCamera.Position[1], mainCamera.Position[2]);

		shader.SetUniform3f("lightPos", lightPosition[0], lightPosition[1], lightPosition[2]);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
		shader.SetUniform1i("depthMap", 1); // Match texture unit
		shader.SetUniform1i("shadows", 1); // Match texture unit
		shader.SetUniform1f("far_plane", far_plane);

		ourModel.Draw(shader);

		// draw the light source cube
		// ---------------------------------------------

		// move light cube to light position
		lightModelMatrix = glm::mat4(1.f);
		lightModelMatrix = glm::translate(lightModelMatrix, lightPosition);

		lightShader.Bind();

		lightShader.SetUniform3f("objectColor", 1.f, 1.f, 1.f);
		lightShader.SetUniformMat4f("model", lightModelMatrix);
		lightShader.SetUniformMat4f("view", viewMatrix);
		lightShader.SetUniformMat4f("projection", projectionMatrix);

		RenderCube();
		// draw skybox
		//skyboxShader.Bind();
		//
		//glm::mat4 view = glm::mat4(glm::mat3(mainCamera.GetViewMatrix()));  // Remove translation from view matrix
		//
		//skyboxShader.SetUniformMat4f("view", view);
		//skyboxShader.SetUniformMat4f("projection", projectionMatrix);
		//
		//skybox.Render();

		// update size
		glfwGetFramebufferSize(window, &displayW, &displayH);
		glViewport(0, 0, displayW, displayH);

		// end frame
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

#endif
// main.cpp
#include "game.h"
#include <iostream>

int main() {
	try {
		Game game;
		game.Run();
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return -1;
	}
	return 0;
}

// main.cpp
