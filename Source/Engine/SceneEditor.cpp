/*
 * Copyright (c) 2023 - 2026 Rachit
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

#include "SceneEditor.h"

#include <numbers>

#include "Externals/ImGui.h"
#include "Externals/SIMDJSON.h"
#include "Util/Files.h"
#include "Util/JSON.h"
#include "Util/Log.h"

namespace Engine
{
    SceneEditor::SceneEditor()
    {
        m_sceneToLoad = config.scene;
    }

    bool SceneEditor::Update
    (
        const Util::FrameCounter& frameCounter,
        const Engine::Inputs& inputs,
        Models::ModelManager& modelManager,
        Renderer::IBL::Generator& iblGenerator
    )
    {
        if (scene.has_value())
        {
            // Reset this flag every frame
            scene->haveRenderObjectsChanged = false;
        }

        // Certain temporal effects need to be informed when a full scene change occurs
        bool sceneChanged = false;

        bool toReload = !scene.has_value();

        // UI Part #1: Scene Load/Reload/Save
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Scene"))
            {
                ImGui::InputText("Scene", &m_sceneToLoad);

                if (ImGui::Button("Load From File"))
                {
                    toReload = true;
                }

                ImGui::SameLine();

                if (ImGui::Button("Reload From Config"))
                {
                    config        = Engine::Config{};
                    m_sceneToLoad = config.scene;
                    toReload      = true;
                }

                ImGui::SameLine();

                if (ImGui::Button("Save To File"))
                {
                    Save(modelManager);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        if (toReload)
        {
            if (scene.has_value())
            {
                Destroy(modelManager, iblGenerator);
            }

            Load(modelManager, iblGenerator);

            sceneChanged = true;
        }

        // UI Part #2: Scene Editing
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Scene"))
            {
                if (ImGui::CollapsingHeader("Render Objects"))
                {
                    ImGui::PushID("Render Objects UI");

                    if (ImGui::TreeNode("Load Object"))
                    {
                        ImGui::InputText("Model Path", &m_loadedModelPath);

                        ImGui::DragFloat3("Position", &m_loadedRenderObject.position[0], 1.0f,                      0.0f, 0.0f, "%.2f");
                        ImGui::DragFloat3("Rotation", &m_loadedRenderObject.rotation[0], glm::radians(1.0f), 0.0f, 0.0f, "%.2f");
                        ImGui::DragFloat3("Scale",    &m_loadedRenderObject.scale[0],    1.0f,                      0.0f, 0.0f, "%.2f");

                        if (ImGui::Button("Load") && !m_loadedModelPath.empty())
                        {
                            const auto modelAssetPath = Files::GetAssetPath("GFX/", m_loadedModelPath);

                            if (Files::Exists(modelAssetPath))
                            {
                                m_loadedRenderObject.modelID = modelManager.Load(m_loadedModelPath);

                                scene->renderObjects.emplace_back(m_loadedRenderObject);

                                scene->haveRenderObjectsChanged = true;
                            }

                            m_loadedRenderObject = {};
                            m_loadedModelPath.clear();
                        }

                        ImGui::SameLine();

                        if (ImGui::Button("Cancel"))
                        {
                            m_loadedRenderObject = {};
                            m_loadedModelPath.clear();
                        }

                        ImGui::TreePop();
                    }

                    ImGui::Separator();

                    usize i = 0;

                    for (auto iter = scene->renderObjects.begin(); iter != scene->renderObjects.end(); ++i)
                    {
                        bool toDelete = false;

                        if (ImGui::TreeNode(fmt::format("[{}]", i).c_str()))
                        {
                            std::string name = "Unknown";

                            if (const auto* model = modelManager.TryGetModel(iter->modelID); model != nullptr)
                            {
                                name = model->name;
                            }

                            ImGui::Text("Model | %s", name.c_str());

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
                            modelManager.Free(iter->modelID);

                            iter = scene->renderObjects.erase(iter);

                            scene->haveRenderObjectsChanged = true;
                        }
                        else
                        {
                            ++iter;
                        }

                        ImGui::Separator();
                    }

                    ImGui::PopID();
                }

                if (ImGui::CollapsingHeader("Lights"))
                {
                    if (ImGui::TreeNode("Sun"))
                    {
                        ImGui::DragFloat3("Direction",      &scene->sun.direction[0], 0.01f, -1.0f, 1.0f, "%.2f");
                        ImGui::ColorEdit3("Color",          &scene->sun.color[0]);
                        ImGui::DragFloat ("Intensity (lx)", &scene->sun.intensity,    0.5f,   0.0f, 0.0f, "%.2f");

                        scene->sun.direction = glm::normalize(scene->sun.direction);
                        scene->sun.intensity = std::max(scene->sun.intensity, 0.0f);

                        ImGui::TreePop();
                    }

                    ImGui::Separator();

                    if (ImGui::TreeNode("Point"))
                    {
                        if (ImGui::TreeNode("Add Point Light"))
                        {
                            ImGui::DragFloat3("Position",       &m_loadedPointLight.position[0], 1.0f,  0.0f, 0.0f, "%.2f");
                            ImGui::ColorEdit3("Color",          &m_loadedPointLight.color[0]);
                            ImGui::DragFloat ("Intensity (cd)", &m_loadedPointLight.intensity,   0.5f,  0.0f, 0.0f, "%.2f");
                            ImGui::DragFloat ("Range (m)",      &m_loadedPointLight.range,       0.01f, 0.0f, 0.0f, "%.3f");

                            if (ImGui::Button("Add"))
                            {
                                m_loadedPointLight.intensity = std::max(m_loadedPointLight.intensity, 0.0f);

                                scene->pointLights.emplace_back(m_loadedPointLight);

                                m_loadedPointLight = {};
                            }

                            ImGui::SameLine();

                            if (ImGui::Button("Cancel"))
                            {
                                m_loadedPointLight = {};
                            }

                            ImGui::TreePop();
                        }

                        ImGui::Separator();

                        usize i = 0;

                        for (auto iter = scene->pointLights.begin(); iter != scene->pointLights.end(); ++i)
                        {
                            bool toDelete = false;

                            if (ImGui::TreeNode(fmt::format("[{}]", i).c_str()))
                            {
                                ImGui::DragFloat3("Position",       &iter->position[0], 1.0f,  0.0f, 0.0f, "%.2f");
                                ImGui::ColorEdit3("Color",          &iter->color[0]);
                                ImGui::DragFloat ("Intensity (cd)", &iter->intensity,   0.5f,  0.0f, 0.0f, "%.2f");
                                ImGui::DragFloat ("Range (m)",      &iter->range,       0.01f, 0.0f, 0.0f, "%.3f");

                                iter->intensity = std::max(iter->intensity, 0.0f);

                                if (ImGui::Button("Delete"))
                                {
                                    toDelete = true;
                                }

                                ImGui::TreePop();
                            }

                            if (toDelete)
                            {
                                iter = scene->pointLights.erase(iter);
                            }
                            else
                            {
                                ++iter;
                            }

                            ImGui::Separator();
                        }

                        ImGui::TreePop();
                    }

                    ImGui::Separator();

                    if (ImGui::TreeNode("Spot"))
                    {
                        ImGui::Checkbox("Normalize", &m_normalizeSpotLightDirection);

                        ImGui::Separator();

                        constexpr f32 ONE_DEGREE    = glm::radians(1.0f);
                        constexpr f32 HALF_ROTATION = std::numbers::pi;

                        if (ImGui::TreeNode("Add Spot Light"))
                        {
                            ImGui::DragFloat3("Position",       &m_loadedSpotLight.position[0],  1.0f,       0.0f, 0.0f,          "%.2f");
                            ImGui::ColorEdit3("Color",          &m_loadedSpotLight.color[0]);
                            ImGui::DragFloat ("Intensity (cd)", &m_loadedSpotLight.intensity,    0.5f,       0.0f, 0.0f,          "%.2f");
                            ImGui::DragFloat3("Direction",      &m_loadedSpotLight.direction[0], 0.05f,     -1.0f, 1.0f,          "%.2f");
                            ImGui::DragFloat2("Cut Off",        &m_loadedSpotLight.cutOff[0],    ONE_DEGREE, 0.0f, HALF_ROTATION, "%.2f");
                            ImGui::DragFloat ("Range (m)",      &m_loadedSpotLight.range,        0.01f,      0.0f, 0.0f,          "%.3f");

                            if (ImGui::Button("Add"))
                            {
                                m_loadedSpotLight.intensity = std::max(m_loadedSpotLight.intensity, 0.0f);
                                m_loadedSpotLight.direction = glm::normalize(m_loadedSpotLight.direction);

                                scene->spotLights.emplace_back(m_loadedSpotLight);

                                m_loadedSpotLight = {};
                            }

                            ImGui::SameLine();

                            if (ImGui::Button("Cancel"))
                            {
                                m_loadedSpotLight = {};
                            }

                            ImGui::TreePop();
                        }

                        ImGui::Separator();

                        usize i = 0;

                        for (auto iter = scene->spotLights.begin(); iter != scene->spotLights.end(); ++i)
                        {
                            bool toDelete = false;

                            if (ImGui::TreeNode(fmt::format("[{}]", i).c_str()))
                            {
                                ImGui::DragFloat3("Position",       &iter->position[0],  1.0f,       0.0f, 0.0f,          "%.2f");
                                ImGui::ColorEdit3("Color",          &iter->color[0]);
                                ImGui::DragFloat ("Intensity (cd)", &iter->intensity,    0.5f,       0.0f, 0.0f,          "%.2f");
                                ImGui::DragFloat3("Direction",      &iter->direction[0], 0.05f,     -1.0f, 1.0f,          "%.2f");
                                ImGui::DragFloat2("Cut Off",        &iter->cutOff[0],    ONE_DEGREE, 0.0f, HALF_ROTATION, "%.2f");
                                ImGui::DragFloat ("Range (m)",      &iter->range,        0.01f,      0.0f, 0.0f,          "%.3f");

                                iter->intensity = std::max(iter->intensity, 0.0f);

                                if (ImGui::Button("Delete"))
                                {
                                    toDelete = true;
                                }

                                if (m_normalizeSpotLightDirection)
                                {
                                    iter->direction = glm::normalize(iter->direction);
                                }

                                ImGui::TreePop();
                            }

                            if (toDelete)
                            {
                                iter = scene->spotLights.erase(iter);
                            }
                            else
                            {
                                ++iter;
                            }

                            ImGui::Separator();
                        }

                        ImGui::TreePop();
                    }
                }

                scene->camera.ImGuiDisplay();

                if (ImGui::CollapsingHeader("IBL"))
                {
                    ImGui::InputText("HDR Map Path", &m_loadedHDRMapPath);

                    if (ImGui::Button("Load") && !m_loadedHDRMapPath.empty())
                    {
                        const auto hdrMapAssetPath = Files::GetAssetPath("GFX/IBL/", m_loadedHDRMapPath);

                        if (Files::Exists(hdrMapAssetPath))
                        {
                            scene->iblMapsID = iblGenerator.GenerateIBL(hdrMapAssetPath);

                            m_savedHDRMapPath = m_loadedHDRMapPath;
                        }

                        m_loadedHDRMapPath.clear();
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        scene->camera.Update(frameCounter, inputs);

        return sceneChanged;
    }

    void SceneEditor::Load
    (
        Models::ModelManager& modelManager,
        Renderer::IBL::Generator& iblGenerator
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        if (scene.has_value())
        {
            Logger::Warning("{}\n", "Scene already loaded! Did you forget to destroy the existing scene?");

            return;
        }

        simdjson::ondemand::parser parser;

        Logger::Info("Loading scene! [Scene={}]\n", m_sceneToLoad);

        const auto path = Files::GetAssetPath("Scenes/", m_sceneToLoad + ".json");

        if (!Files::Exists(path))
        {
            Logger::Error("Failed to load scene! [Scene={}]", m_sceneToLoad);
        }

        const auto json = simdjson::padded_string::load(path);

        JSON::CheckError(json, "Failed to load json file!");

        auto document = parser.iterate(json);

        JSON::CheckError(document, "Failed to parse json file!");

        scene = Engine::Scene{};

        // Render Objects
        {
            auto objects = document["RenderObjects"].get_array();

            JSON::CheckError(objects, "Failed to load render objects!");

            for (auto object : objects)
            {
                Renderer::RenderObject renderObject;

                auto model = object["Model"].get_string();

                JSON::CheckError(model, "Failed to load model path!");

                renderObject.modelID = modelManager.Load(model.value_unsafe());

                JSON::CheckError(object["Position"].get<glm::vec3>(renderObject.position), "Failed to load position!");
                JSON::CheckError(object["Rotation"].get<glm::vec3>(renderObject.rotation), "Failed to load rotation!");
                JSON::CheckError(object["Scale"   ].get<glm::vec3>(renderObject.scale   ), "Failed to load scale!"   );

                renderObject.rotation = glm::radians(renderObject.rotation);

                scene->renderObjects.emplace_back(renderObject);

                scene->haveRenderObjectsChanged = true;
            }
        }

        // Lights
        {
            // Sun
            JSON::CheckError(document["Sun"].get<GPU::DirLight>(scene->sun), "Failed to load the sun light!");

            // Point Lights
            {
                auto lights = document["PointLights"].get_array();

                JSON::CheckError(lights, "Failed to load point lights!");

                for (auto light : lights)
                {
                    GPU::PointLight pointLight = {};

                    JSON::CheckError(light.get<GPU::PointLight>(pointLight), "Failed to load point light!");

                    scene->pointLights.emplace_back(pointLight);
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

                    scene->spotLights.emplace_back(spotLight);
                }
            }
        }

        // Camera
        JSON::CheckError(document["Camera"]["FreeCamera"].get<Engine::FreeCamera>(scene->camera), "Failed to load free camera!");

        // HDR Map
        JSON::CheckError(document["IBL"].get_string(m_loadedHDRMapPath), "Failed to load IBL!");

        const auto hdrMapAssetPath = Files::GetAssetPath("GFX/IBL/", m_loadedHDRMapPath);

        if (Files::Exists(hdrMapAssetPath))
        {
            scene->iblMapsID = iblGenerator.GenerateIBL(hdrMapAssetPath);

            m_savedHDRMapPath = m_loadedHDRMapPath;
        }

        m_loadedHDRMapPath.clear();
    }

    void SceneEditor::Destroy
    (
        Models::ModelManager& modelManager,
        Renderer::IBL::Generator& iblGenerator
    )
    {
        if (!scene.has_value())
        {
            return;
        }

        iblGenerator.DestroyIBL(scene->iblMapsID);

        for (const auto& renderObject : scene->renderObjects)
        {
            modelManager.Free(renderObject.modelID);
        }

        scene = std::nullopt;
    }

    void SceneEditor::Save(const Models::ModelManager& modelManager)
    {
        if (!scene.has_value())
        {
            return;
        }

        simdjson::builder::string_builder builder;

        builder.start_object();

        // Render Objects
        // Can't make a tag_invoke for these, since they need access to ModelManager
        {
            builder.escape_and_append_with_quotes<"RenderObjects">();
            builder.append_colon();

            builder.start_array();

            for (usize i = 0; i < scene->renderObjects.size(); ++i)
            {
                const auto& renderObject = scene->renderObjects[i];

                builder.start_object();

                builder.append_key_value<"Model">(modelManager.GetModel(renderObject.modelID).path);
                builder.append_comma();

                builder.append_key_value<"Position">(renderObject.position);
                builder.append_comma();

                builder.append_key_value<"Rotation">(glm::degrees(renderObject.rotation));
                builder.append_comma();

                builder.append_key_value<"Scale">(renderObject.scale);

                builder.end_object();

                if (i < scene->renderObjects.size() - 1)
                {
                    builder.append_comma();
                }
            }

            builder.end_array();
            builder.append_comma();
        }

        builder.append_key_value<"Sun">(scene->sun);
        builder.append_comma();

        builder.append_key_value<"PointLights">(scene->pointLights);
        builder.append_comma();

        builder.append_key_value<"SpotLights">(scene->spotLights);
        builder.append_comma();

        builder.escape_and_append_with_quotes<"Camera">();
        builder.append_colon();
        builder.start_object();
        builder.append_key_value<"FreeCamera">(scene->camera);
        builder.end_object();
        builder.append_comma();

        builder.append_key_value<"IBL">(m_savedHDRMapPath);

        builder.end_object();

        const auto view = builder.view();

        JSON::CheckError(view.error(), "Failed to write to JSON!");

        const auto path = Files::GetAssetPath("Scenes/", m_sceneToLoad + ".json");

        auto output = std::ofstream(path, std::ios::out);

        if (!output.is_open())
        {
            Logger::Error("Failed to open scene! [Scene={}]\n", m_sceneToLoad);
        }

        output << view.value_unsafe();
    }
}
