// game.cpp
#include "game.h"
//#include "cube.h"
#include "model.h"
#include "model_entity.h"
#include "light.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <std_image/stb_image.h>
#include <codecvt>

glm::vec3 directionalLightDirection(0.f, 0.f, 0.f);
glm::vec3 directionaLightColor(1.f, 1.f, 1.f);
float directionaLightIntensity = 3;

glm::vec3 pointLightPosition1(0.f, 6.f, 0.f);
glm::vec3 pointLightColor1(1.f, 1.f, 1.f);
float pointLightintensity1(2);
float pointLightRange1(25.f);

glm::vec3 pointLightPosition2(0.f, 0.f, 0.f);
glm::vec3 pointLightColor2(0.f, 0.f, 0.f);
float pointLightintensity2(0.f);

glm::vec3 spotLightDirection(0.f, 0.f, 0.f);
glm::vec3 spotLightPosition(0.f, 0.f, 0.f);

float spotLightintensity(0.f);
float innerCutoff;
float outerCutoff;
glm::vec3 spotLightColor(0.f, 0.f, 0.f);

glm::vec3 rotationDelta(90.f, 0.f, 0.f);

Game::Game() : 
	m_MainCamera(glm::vec3(0.0f, 0.0f, 3.0f)), 
	m_WindowSize{ WINDOW_WIDTH, WINDOW_HEIGHT },
	m_InputHandler(m_MainCamera, &m_WindowSize),
	m_Renderer(m_WindowSize)
{
	stbi_set_flip_vertically_on_load(true);

	if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
	m_Window = glfwCreateWindow(m_WindowSize.x, m_WindowSize.y, "Game Window", NULL, NULL);
	glfwMakeContextCurrent(m_Window);
	glewInit();
	glfwSwapInterval(1); // vsync
	m_Renderer.Initialize();

	InitializeEntities();

	m_InputHandler.SetCallbacks(m_Window, &m_InputHandler); // Set the input callbacks

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	glCullFace(GL_BACK); 

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
	ImGui_ImplOpenGL3_Init();
}

void Game::InitializeEntities() {
	// Initialize main entity (house model)
	ModelEntity* houseEntity = new ModelEntity("D:/Desktop/LearningOpenGL/res/models/house/source/free_fire_maxx_new_clock_tower_map_download_free.glb");
	houseEntity->SetRotation(rotationDelta);
	houseEntity->SetScale(glm::vec3(0.4f));
	m_Entities.push_back(houseEntity);

	// Directional light (Sun)
	DirectionalLight* sun = new DirectionalLight(directionalLightDirection, directionaLightColor, 0.5f); // Sun-like color
	m_Lights.push_back(sun);

	// Point light (Lamp)
	PointLight* lamp = new PointLight(pointLightPosition1, pointLightColor1, pointLightintensity1, pointLightRange1);
	m_Lights.push_back(lamp);

	// Point light (Lamp)
	PointLight* lamp2 = new PointLight(pointLightPosition2, pointLightColor2, pointLightintensity2, 20.0f);
	m_Lights.push_back(lamp2);

	// Spotlight with adjustable cutoff
	SpotLight* flashlight = new SpotLight(spotLightPosition, spotLightDirection, spotLightColor, 1.0f, 15.0f, 30.0f);
	m_Lights.push_back(flashlight);
}

void Game::Run() {
	while (!glfwWindowShouldClose(m_Window)) {
		float currentFrameTime = static_cast<float>(glfwGetTime());
		deltaTime = currentFrameTime - lastFrameTime;
		lastFrameTime = currentFrameTime;

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.2f, 0.2f, 0.2f, 1.f);
		//std::cout << "started frame" << "\n";
		
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		DrawGUI();
		ImGui::Render();

		ProcessInput();
		Update();
		Render();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(m_Window);
		glfwPollEvents();
	}
}

void Game::ProcessInput() {
	m_InputHandler.Update(m_Window);

	if (glfwGetKey(m_Window, GLFW_KEY_B) == GLFW_PRESS) {
		directionalLightDirection = normalize(m_MainCamera.Front);
		m_Lights[0]->SetDirection(directionalLightDirection);
	}

	m_Entities[0]->SetRotation(rotationDelta);
}

void Game::DrawGUI() {
	ImGui::Begin("Lighting Controls");

	// Control camera properties
	ImGui::Text("Camera Settings");
	ImGui::SliderFloat("Movement Speed", &m_MainCamera.MovementSpeed, 0.1f, 300.0f);
	ImGui::SliderFloat("Zoom", &m_MainCamera.Zoom, 0.0f, 180.0f);
	ImGui::SliderFloat3("Position", &m_MainCamera.Position[0], -5.0f, 5.0f);

	// Light settings
	ImGui::Text("Light Settings");

	ImGui::Text("Sun settings");
	//ImGui::SliderFloat3("Directional Light Direction", &directionalLightDirection[0], -1.0f, 1.0f);
	ImGui::SliderFloat("Directional Light Intensity", &directionaLightIntensity, 0.0f, 2.0f);
	ImGui::SliderFloat3("Directional Light Color", &directionaLightColor[0], 0.0f, 1.0f);
	m_Lights[0]->SetIntensity(directionaLightIntensity);
	//m_Lights[0]->SetDirection(directionalLightDirection);
	m_Lights[0]->SetColor(directionaLightColor);

	ImGui::Text("Point Light 1 settings");
	ImGui::SliderFloat3("Point Light 1 Position", &pointLightPosition1[0], -10.0f, 10.0f);
	ImGui::SliderFloat("Point Light 1 Intensity", &pointLightintensity1, 0.0f, 2.0f);
	ImGui::SliderFloat("Point Light 1 Range", &pointLightRange1, 0.0f, 50);
	ImGui::SliderFloat3("Point Light 1 Color", &pointLightColor1[0], 0.0f, 1.0f);
	
	m_Lights[1]->SetIntensity(pointLightintensity1);
	m_Lights[1]->SetPosition(pointLightPosition1);
	m_Lights[1]->SetColor(pointLightColor1);
	m_Lights[1]->SetRange(pointLightRange1);

	ImGui::Text("Point Light 2 settings");
	ImGui::SliderFloat3("Point Light 2 Position", &pointLightPosition2[0], -10.0f, 10.0f);
	ImGui::SliderFloat("Point Light 2 Intensity", &pointLightintensity2, 0.0f, 2.0f);
	ImGui::SliderFloat3("Point Light 2 Color", &pointLightColor2[0], 0.0f, 1.0f);
	m_Lights[2]->SetIntensity(pointLightintensity2);
	m_Lights[2]->SetPosition(pointLightPosition2);
	m_Lights[2]->SetColor(pointLightColor2);

	ImGui::Text("Spot light settings");
	ImGui::SliderFloat("Inner Cutoff", &innerCutoff, 0.0f, 100.0f);
	ImGui::SliderFloat("Outer Cutoff", &outerCutoff, 0.0f, 100.0f);
	ImGui::SliderFloat("spotLightintensity", &spotLightintensity, 0.0f, 2.0f);
	ImGui::SliderFloat3("spotLightColor", &spotLightColor[0], 0.0f, 1.0f);

	m_Lights[3]->SetInnerCutoff(innerCutoff);
	m_Lights[3]->SetOuterCutoff(outerCutoff);
	m_Lights[3]->SetIntensity(spotLightintensity);
	m_Lights[3]->SetColor(spotLightColor);
	m_Lights[3]->SetPosition(m_MainCamera.Position);
	m_Lights[3]->SetDirection(m_MainCamera.Front);

	
	ImGui::Image(
		(ImTextureID)m_Renderer.m_DirDepthMap,
		ImVec2(600, 600),
		ImVec2(0.0f, 1.0f), // Bottom-left corner
		ImVec2(1.0f, 0.0f)  // Top-right corner
	);

	// Reset button
	if (ImGui::Button("Reset")) {
		m_MainCamera.Position = glm::vec3(0.0f, 0.0f, 2.0f);
		rotationDelta = glm::vec3(0.0f, 0.0f, 0.0f);
	}
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

	ImGui::End();
}

void Game::Update() {
	for (ModelEntity* entity : m_Entities) {
		if (deltaTime > 0.0f) entity->Update(deltaTime);
	}

	//// Collision detection and response
	//for (size_t i = 0; i < m_Entities.size(); ++i) {
	//	for (size_t j = i + 1; j < m_Entities.size(); ++j) {
	//		Rigidbody* rb1 = m_Entities[i]->GetRigidbody();
	//		Rigidbody* rb2 = m_Entities[j]->GetRigidbody();

	//		if (rb1->CheckCollision(*rb2)) {
	//			std::cout << "somthing coliding" << std::endl;
	//			rb1->ResolveCollision(*rb2);
	//		}
	//	}
	//}
}

void Game::Render() {
	m_Renderer.RenderScene(m_WindowSize, m_MainCamera, m_Lights, m_Entities);
	int a = glGetError();
	if (a) {
		std::cout << a << std::endl;
	}
	Shader lightShader = m_Renderer.GetLightShader();
	lightShader.Bind();
	lightShader.SetUniform4f("objectColor", { 1.0f, 1.0f, 1.0f, 1.0f });

	// Render a small cube at each light's position to visualize them
	for (const auto& light : m_Lights) {
		if (light->GetType() != LightType::POINT) continue;
		glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), light->GetPosition());
		modelMatrix = glm::scale(modelMatrix, glm::vec3(0.1f)); // Make the visual marker small
		lightShader.SetUniformMat4f("model", modelMatrix);

		glm::mat4 view = m_MainCamera.GetViewMatrix();
		lightShader.SetUniformMat4f("view", view);

		glm::mat4 projection = m_MainCamera.GetProjectionMatrix(m_WindowSize);
		lightShader.SetUniformMat4f("projection", projection);

		m_Renderer.RenderCube();
	}
}

Game::~Game() {
	m_Renderer.Cleanup();
	for (Entity* entity : m_Entities) delete entity;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(m_Window);
	glfwTerminate();
}
