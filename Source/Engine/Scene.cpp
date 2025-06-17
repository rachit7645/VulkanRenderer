/*
 * Copyright (c) 2023 - 2025 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Scene.h"

#include "Util/Files.h"
#include "Util/JSON.h"
#include "Util/Log.h"
#include "Externals/ImGui.h"
#include "Externals/SIMDJSON.h"

namespace Engine
{
    Scene::Scene
    (
        const Engine::Config& config,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Models::ModelManager& modelManager,
        Vk::MegaSet& megaSet,
        Renderer::IBL::Generator& iblGenerator,
        Util::DeletionQueue& deletionQueue
    )
    {
        try
        {
            simdjson::ondemand::parser parser;

            Logger::Info("Loading scene! [Scene={}]\n", config.scene);

            const auto path = Util::Files::GetAssetPath("Scenes/", config.scene + ".json");
            const auto json = simdjson::padded_string::load(path);

            JSON::CheckError(json, "Failed to load json file!");

            auto document = parser.iterate(json);

            JSON::CheckError(document, "Failed to parse json file!");

            // Render Objects
            {
                auto objects = document["RenderObjects"].get_array();

                JSON::CheckError(objects, "Failed to load render objects!");

                for (auto object : objects)
                {
                    Renderer::RenderObject renderObject;

                    auto model = object["Model"].get_string();

                    JSON::CheckError(model, "Failed to load model path!");

                    renderObject.modelID = modelManager.AddModel(context.allocator, deletionQueue, model.value());

                    JSON::CheckError(object["Position"].get<glm::vec3>(renderObject.position), "Failed to load position!");
                    JSON::CheckError(object["Rotation"].get<glm::vec3>(renderObject.rotation), "Failed to load rotation!");
                    JSON::CheckError(object["Scale"   ].get<glm::vec3>(renderObject.scale   ), "Failed to load scale!"   );

                    renderObject.rotation = glm::radians(renderObject.rotation);

                    renderObjects.emplace_back(renderObject);

                    haveRenderObjectsChanged = true;
                }
            }

            // Lights
            {
                // Sun
                JSON::CheckError(document["Sun"].get<GPU::DirLight>(sun), "Failed to load the sun light!");

                // Point Lights
                {
                    auto lights = document["PointLights"].get_array();

                    JSON::CheckError(lights, "Failed to load point lights!");

                    for (auto light : lights)
                    {
                        GPU::PointLight pointLight = {};

                        JSON::CheckError(light.get<GPU::PointLight>(pointLight), "Failed to load point light!");

                        pointLights.emplace_back(pointLight);
                    }
                }

                // Spot Lights
                {
                    auto lights = document["SpotLights"].get_array();

                    JSON::CheckError(lights, "Failed to load spot lights!");

                    for (auto light : lights)
                    {
                        GPU::SpotLight spotLight = {};

                        JSON::CheckError(light.get<GPU::SpotLight>(spotLight), "Failed to load spot light!");

                        spotLights.emplace_back(spotLight);
                    }
                }
            }

            // Camera
            JSON::CheckError(document["Camera"]["FreeCamera"].get<Renderer::Objects::FreeCamera>(camera), "Failed to load free camera!");

            // HDR Map
            JSON::CheckError(document["IBL"].get_string(m_hdrMap), "Failed to load IBL!");

            const auto hdrMapAssetPath = Util::Files::GetAssetPath("GFX/IBL/", m_hdrMap);

            if (Util::Files::Exists(hdrMapAssetPath))
            {
                iblMaps = iblGenerator.Generate
                (
                    cmdBuffer,
                    context,
                    formatHelper,
                    modelManager,
                    megaSet,
                    deletionQueue,
                    hdrMapAssetPath
                );
            }

            m_hdrMap.clear();
        }
        catch (const std::exception& e)
        {
            Logger::Error("Failed to load scene! [Error={}]\n", e.what());
        }
    }

    void Scene::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Util::FrameCounter& frameCounter,
        Engine::Inputs& inputs,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Models::ModelManager& modelManager,
        Vk::MegaSet& megaSet,
        Renderer::IBL::Generator& iblGenerator,
        Util::DeletionQueue& deletionQueue
    )
    {
        camera.Update(frameCounter.frameDelta, inputs);

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Scene"))
            {
                if (ImGui::BeginMenu("Render Objects"))
                {
                    if (ImGui::TreeNode("Load"))
                    {
                        ImGui::InputText("Model Path", &m_modelPath);

                        ImGui::DragFloat3("Position", &m_loadedRenderObject.position[0], 1.0f,                      0.0f, 0.0f, "%.2f");
                        ImGui::DragFloat3("Rotation", &m_loadedRenderObject.rotation[0], glm::radians(1.0f), 0.0f, 0.0f, "%.2f");
                        ImGui::DragFloat3("Scale",    &m_loadedRenderObject.scale[0],    1.0f,                      0.0f, 0.0f, "%.2f");

                        if (ImGui::Button("Load") && !m_modelPath.empty())
                        {
                            const auto modelAssetPath = Util::Files::GetAssetPath("GFX/", m_modelPath);

                            if (Util::Files::Exists(modelAssetPath))
                            {
                                m_loadedRenderObject.modelID = modelManager.AddModel(context.allocator, deletionQueue, m_modelPath);

                                renderObjects.emplace_back(m_loadedRenderObject);

                                haveRenderObjectsChanged = true;
                            }

                            m_loadedRenderObject = {};
                            m_modelPath.clear();
                        }

                        ImGui::TreePop();
                    }

                    ImGui::Separator();

                    usize i = 0;

                    for (auto iter = renderObjects.begin(); iter != renderObjects.end(); ++i)
                    {
                        bool toDelete = false;

                        if (ImGui::TreeNode(fmt::format("[{}]", i).c_str()))
                        {
                            ImGui::Text("Model | %s", modelManager.GetModel(iter->modelID).name.c_str());

                            ImGui::Separator();

                            ImGui::DragFloat3("Position", &iter->position[0], 1.0f,                      0.0f, 0.0f, "%.2f");
                            ImGui::DragFloat3("Rotation", &iter->rotation[0], glm::radians(1.0f), 0.0f, 0.0f, "%.2f");
                            ImGui::DragFloat3("Scale",    &iter->scale[0],    1.0f,                      0.0f, 0.0f, "%.2f");

                            if (ImGui::Button("Delete"))
                            {
                                toDelete = true;
                            }

                            ImGui::TreePop();
                        }

                        if (toDelete)
                        {
                            iter->Destroy
                            (
                                context.device,
                                context.allocator,
                                megaSet,
                                modelManager,
                                deletionQueue
                            );

                            iter = renderObjects.erase(iter);

                            haveRenderObjectsChanged = true;
                        }
                        else
                        {
                            ++iter;
                        }

                        ImGui::Separator();
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Lights"))
                {
                    if (ImGui::BeginMenu("Directional"))
                    {
                        if (ImGui::TreeNode("Sun"))
                        {
                            ImGui::DragFloat3("Position",  &sun.position[0],  1.0f, 0.0f, 0.0f, "%.2f");
                            ImGui::ColorEdit3("Color",     &sun.color[0]);
                            ImGui::DragFloat3("Intensity", &sun.intensity[0], 0.5f, 0.0f, 0.0f, "%.2f");

                            ImGui::TreePop();
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::Separator();

                    if (ImGui::BeginMenu("Point"))
                    {
                        for (usize i = 0; i < pointLights.size(); ++i)
                        {
                            if (ImGui::TreeNode(fmt::format("[{}]", i).c_str()))
                            {
                                auto& light = pointLights[i];

                                ImGui::DragFloat3("Position",  &light.position[0],  1.0f, 0.0f, 0.0f, "%.2f");
                                ImGui::ColorEdit3("Color",     &light.color[0]);
                                ImGui::DragFloat3("Intensity", &light.intensity[0], 0.5f, 0.0f, 0.0f, "%.2f");
                                ImGui::DragFloat( "Range",     &light.range,        1.0f, 0.0f, 0.0f, "%.3f");

                                ImGui::TreePop();
                            }

                            ImGui::Separator();
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::Separator();

                    if (ImGui::BeginMenu("Spot"))
                    {
                        for (usize i = 0; i < spotLights.size(); ++i)
                        {
                            if (ImGui::TreeNode(fmt::format("[{}]", i).c_str()))
                            {
                                auto& light = spotLights[i];

                                constexpr auto ONE_DEGREE    = glm::radians(1.0f);
                                constexpr auto HALF_ROTATION = std::numbers::pi;

                                ImGui::DragFloat3("Position",  &light.position[0],  1.0f,       0.0f, 0.0f,          "%.2f");
                                ImGui::ColorEdit3("Color",     &light.color[0]                                             );
                                ImGui::DragFloat3("Intensity", &light.intensity[0], 0.5f,       0.0f, 0.0f,          "%.2f");
                                ImGui::DragFloat3("Direction", &light.direction[0], 0.05f,     -1.0f, 1.0f,          "%.2f");
                                ImGui::DragFloat2("Cut Off",   &light.cutOff[0],    ONE_DEGREE, 0.0f, HALF_ROTATION, "%.2f");
                                ImGui::DragFloat( "Range",     &light.range,        1.0f,       0.0f, 0.0f,          "%.3f");

                                ImGui::TreePop();
                            }

                            ImGui::Separator();
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();

                camera.ImGuiDisplay();

                ImGui::Separator();

                if (ImGui::BeginMenu("IBL"))
                {
                    ImGui::InputText("HDR Map Path", &m_hdrMap);

                    if (ImGui::Button("Load") && !m_hdrMap.empty())
                    {
                        const auto hdrMapAssetPath = Util::Files::GetAssetPath("GFX/IBL/", m_hdrMap);

                        if (Util::Files::Exists(hdrMapAssetPath))
                        {
                            iblMaps.Destroy
                            (
                                context,
                                modelManager.textureManager,
                                megaSet,
                                deletionQueue
                            );

                            iblMaps = iblGenerator.Generate
                            (
                                cmdBuffer,
                                context,
                                formatHelper,
                                modelManager,
                                megaSet,
                                deletionQueue,
                                hdrMapAssetPath
                            );
                        }

                        m_hdrMap.clear();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }
            
            ImGui::EndMainMenuBar();
        }
    }

    void Scene::Destroy
    (
        const Vk::Context& context,
        Models::ModelManager& modelManager,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        iblMaps.Destroy
        (
            context,
            modelManager.textureManager,
            megaSet,
            deletionQueue
        );

        for (auto& renderObject : renderObjects)
        {
            renderObject.Destroy
            (
                context.device,
                context.allocator,
                megaSet,
                modelManager,
                deletionQueue
            );
        }
    }
}