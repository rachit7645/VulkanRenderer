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

#include "Model.h"

#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include "Vertex.h"
#include "Util/Log.h"
#include "Util/Files.h"
#include "Util/Enum.h"
#include "Util/Util.h"
#include "Util/Maths.h"

namespace Models
{
    // Model folder path
    constexpr auto MODEL_ASSETS_DIR = "GFX/";

    // Default texture paths
    constexpr auto DEFAULT_ALBEDO     = "Albedo.ktx2";
    constexpr auto DEFAULT_NORMAL     = "Normal.ktx2";
    constexpr auto DEFAULT_AO_RGH_MTL = "Metal-Roughness.ktx2";

    Model::Model
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        const std::string_view path
    )
    {
        Logger::Info("Loading model: {}\n", path);

        const std::string assetPath      = Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, path);
        const std::string assetDirectory = Engine::Files::GetDirectory(assetPath);

        fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);

        auto data = fastgltf::GltfDataBuffer::FromPath(assetPath);
        if (auto error = data.error(); error != fastgltf::Error::None)
        {
            Logger::Error
            (
                "Failed to load glTF file! [Error={}] [Path={}]\n",
                fastgltf::getErrorName(error),
                path
            );
        }

        auto asset = parser.loadGltf
        (
            data.get(),
            assetDirectory,
            fastgltf::Options::GenerateMeshIndices | fastgltf::Options::LoadExternalBuffers,
            fastgltf::Category::All
        );

        if (auto error = asset.error(); error != fastgltf::Error::None)
        {
            Logger::Error
            (
                "Failed to load asset! [Error={}] [Path={}]\n",
                fastgltf::getErrorName(error),
                path
            );
        }

        #ifdef ENGINE_DEBUG
        if (auto error = fastgltf::validate(asset.get()); error != fastgltf::Error::None)
        {
            Logger::Error
            (
                "Failed to validate asset! [Error={}] [Path={}]\n",
                fastgltf::getErrorName(error),
                path
            );
        }
        #endif

        ProcessScenes
        (
            context,
            megaSet,
            geometryBuffer,
            textureManager,
            assetDirectory,
            asset.get()
        );
    }

    void Model::ProcessScenes
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        const std::string& directory,
        const fastgltf::Asset& asset
    )
    {
        for (const auto& scene : asset.scenes)
        {
            for (const auto nodeIndex : scene.nodeIndices)
            {
                ProcessNode
                (
                    context,
                    megaSet,
                    geometryBuffer,
                    textureManager,
                    directory,
                    asset,
                    nodeIndex,
                    glm::identity<glm::mat4>()
                );
            }
        }
    }

    void Model::ProcessNode
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        const std::string& directory,
        const fastgltf::Asset& asset,
        usize nodeIndex,
        glm::mat4 nodeMatrix
    )
    {
        auto& node = asset.nodes[nodeIndex];
        nodeMatrix = GetTransformMatrix(node, nodeMatrix);

        if (node.meshIndex.has_value())
        {
            LoadMesh
            (
                context,
                megaSet,
                geometryBuffer,
                textureManager,
                directory,
                asset,
                asset.meshes[node.meshIndex.value()],
                nodeMatrix
            );
        }

        for (const auto child : node.children)
        {
            ProcessNode
            (
                context,
                megaSet,
                geometryBuffer,
                textureManager,
                directory,
                asset,
                child,
                nodeMatrix
            );
        }
    }

    void Model::LoadMesh
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        const std::string& directory,
        const fastgltf::Asset& asset,
        const fastgltf::Mesh& mesh,
        const glm::mat4& nodeMatrix
    )
    {
        for (const auto& primitive : mesh.primitives)
        {
            std::vector<Index>     indices   = {};
            std::vector<glm::vec3> positions = {};
            std::vector<Vertex>    vertices  = {};
            Material               material  = {};

            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                Logger::Warning
                (
                    "Unsupported primitive type! [Type={}]\n",
                    static_cast<std::underlying_type_t<fastgltf::PrimitiveType>>(primitive.type)
                );
            }

            // Indices
            {
                if (!primitive.indicesAccessor.has_value())
                {
                    Logger::Error("{}\n", "Primitive does not contain indices accessor!");
                }

                const auto& indicesAccessor = asset.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indicesAccessor.count);

                if (indicesAccessor.type != fastgltf::AccessorType::Scalar)
                {
                    Logger::Error
                    (
                        "Invalid indices accessor type! [AccessorType={}]\n",
                        static_cast<std::underlying_type_t<fastgltf::AccessorType>>(indicesAccessor.type)
                    );
                }

                // Assume indices are u32s
                switch (indicesAccessor.componentType)
                {
                case fastgltf::ComponentType::Byte:
                {
                    fastgltf::iterateAccessor<s8>(asset, indicesAccessor, [&] (s8 index)
                    {
                        indices.push_back(static_cast<Index>(index));
                    });
                    break;
                }

                case fastgltf::ComponentType::UnsignedByte:
                {
                    fastgltf::iterateAccessor<u8>(asset, indicesAccessor, [&] (u8 index)
                    {
                        indices.push_back(static_cast<Index>(index));
                    });
                    break;
                }

                case fastgltf::ComponentType::Short:
                {
                    fastgltf::iterateAccessor<s16>(asset, indicesAccessor, [&] (s16 index)
                    {
                        indices.push_back(static_cast<Index>(index));
                    });
                    break;
                }

                case fastgltf::ComponentType::UnsignedShort:
                {
                    fastgltf::iterateAccessor<u16>(asset, indicesAccessor, [&] (u16 index)
                    {
                        indices.push_back(static_cast<Index>(index));
                    });
                    break;
                }

                case fastgltf::ComponentType::UnsignedInt:
                {
                    indices.resize(indicesAccessor.count);
                    fastgltf::copyFromAccessor<u32>(asset, indicesAccessor, indices.data());
                    break;
                }

                default:
                    Logger::Error
                    (
                        "Invalid index component type! [ComponentType={}]\n",
                        static_cast<std::underlying_type_t<fastgltf::ComponentType>>(indicesAccessor.componentType)
                    );
                }
            }

            // Position
            {
                const auto& positionAccessor = GetAccessor
                (
                    asset,
                    primitive,
                    "POSITION",
                    fastgltf::AccessorType::Vec3
                );

                positions.resize(positionAccessor.count);

                fastgltf::copyFromAccessor<glm::vec3>(asset, positionAccessor, positions.data());
            }

            // Normals
            {
                const auto& normalAccessor = GetAccessor
                (
                    asset,
                    primitive,
                    "NORMAL",
                    fastgltf::AccessorType::Vec3
                );

                vertices.reserve(normalAccessor.count);

                fastgltf::iterateAccessor<glm::vec3>(asset, normalAccessor, [&] (const glm::vec3& normal)
                {
                    Vertex vertex = {};
                    vertex.normal = normal;
                    vertices.emplace_back(vertex);
                });
            }

            // UVs
            {
                const auto& uvAccessor = GetAccessor
                (
                    asset,
                    primitive,
                    "TEXCOORD_0",
                    fastgltf::AccessorType::Vec2
                );

                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, uvAccessor, [&] (const glm::vec2& uv, usize index)
                {
                    vertices[index].uv0 = uv;
                });
            }

            // Tangent
            {
               const auto& tangentAccessor = GetAccessor
               (
                    asset,
                    primitive,
                    "TANGENT",
                    fastgltf::AccessorType::Vec4
                );

               fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, tangentAccessor, [&] (const glm::vec4& tangent, usize index)
               {
                   vertices[index].tangent = tangent;
               });
            }

            if (!primitive.materialIndex.has_value())
            {
                Logger::Error("{}\n", "No material in primitive!");
            }

            const auto& mat = asset.materials[primitive.materialIndex.value()];

            // Material Factors
            {
                material.albedoFactor    = glm::fastgltf_cast(mat.pbrData.baseColorFactor);
                material.roughnessFactor = mat.pbrData.roughnessFactor;
                material.metallicFactor  = mat.pbrData.metallicFactor;
            }

            // Albedo
            {
                const auto& baseColorTexture = mat.pbrData.baseColorTexture;

                if (baseColorTexture.has_value())
                {
                    // TODO: Support multiple UV channels
                    if (baseColorTexture->texCoordIndex != 0)
                    {
                        Logger::Warning
                        (
                            "Albedo uses more than one UV channel! [texCoordIndex={}]\n",
                            baseColorTexture->texCoordIndex
                        );
                    }

                    material.albedo = LoadTexture
                    (
                        context,
                        megaSet,
                        textureManager,
                        directory,
                        asset,
                        baseColorTexture->textureIndex
                    );
                }
                else
                {
                    material.albedo = textureManager.AddTexture
                    (
                        megaSet,
                        context.device,
                        context.allocator,
                        Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, DEFAULT_ALBEDO)
                    );
                }
            }

            // Normal
            {
                if (mat.normalTexture.has_value())
                {
                    // TODO: Support multiple UV channels
                    if (mat.normalTexture->texCoordIndex != 0)
                    {
                        Logger::Warning
                        (
                            "Normal uses more than one UV channel! [texCoordIndex={}]\n",
                            mat.normalTexture->texCoordIndex
                        );
                    }

                    material.normal = LoadTexture
                    (
                        context,
                        megaSet,
                        textureManager,
                        directory,
                        asset,
                        mat.normalTexture->textureIndex
                    );
                }
                else
                {
                    material.normal = textureManager.AddTexture
                    (
                        megaSet,
                        context.device,
                        context.allocator,
                        Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, DEFAULT_NORMAL)
                    );
                }
            }

            // AO + Roughness + Metallic
            {
                const auto& metallicRoughnessTexture = mat.pbrData.metallicRoughnessTexture;

                if (metallicRoughnessTexture.has_value())
                {
                    // TODO: Support multiple UV channels
                    if (metallicRoughnessTexture->texCoordIndex != 0)
                    {
                        Logger::Warning
                        (
                            "AoRghMtl uses more than one UV channel! [texCoordIndex={}]\n",
                            metallicRoughnessTexture->texCoordIndex
                        );
                    }

                    material.aoRghMtl = LoadTexture
                    (
                        context,
                        megaSet,
                        textureManager,
                        directory,
                        asset,
                        metallicRoughnessTexture->textureIndex
                    );
                }
                else
                {
                    material.aoRghMtl = textureManager.AddTexture
                    (
                        megaSet,
                        context.device,
                        context.allocator,
                        Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, DEFAULT_AO_RGH_MTL)
                    );
                }
            }

            const auto [indexInfo, positionInfo, vertexInfo] = geometryBuffer.SetupUploads
            (
                context.allocator,
                indices,
                positions,
                vertices
            );

            const auto aabb = Maths::AABB(positions);

            meshes.emplace_back
            (
                indexInfo,
                positionInfo,
                vertexInfo,
                material,
                nodeMatrix,
                aabb
            );
        }
    }

    glm::mat4 Model::GetTransformMatrix(const fastgltf::Node& node, const glm::mat4& base)
    {
        return std::visit(fastgltf::visitor {
            [&] (const fastgltf::math::fmat4x4& matrix)
            {
                return base * glm::fastgltf_cast(matrix);
            },
            [&] (const fastgltf::TRS& trs)
            {
                return base * Maths::CreateTransformMatrix
                (
                    glm::fastgltf_cast(trs.translation),
                    glm::eulerAngles(glm::fastgltf_cast(trs.rotation)),
                    glm::fastgltf_cast(trs.scale)
                );
            }
        }, node.transform);
    }

    const fastgltf::Accessor& Model::GetAccessor
    (
        const fastgltf::Asset& asset,
        const fastgltf::Primitive& primitive,
        const std::string_view attribute,
        fastgltf::AccessorType type
    )
    {
        const auto attributeIt = primitive.findAttribute(attribute);
        if (attributeIt == primitive.attributes.cend())
        {
            Logger::Error("Failed to find attribute! [Attribute={}]\n", attribute);
        }

        const auto& accessor = asset.accessors[attributeIt->accessorIndex];

        if (accessor.type != type)
        {
            Logger::Error
            (
                "Invalid accessor type! [AccessorType={}] [Required={}]\n",
                static_cast<std::underlying_type_t<fastgltf::AccessorType>>(accessor.type),
                static_cast<std::underlying_type_t<fastgltf::AccessorType>>(type)
            );
        }

        return accessor;
    }

    u32 Model::LoadTexture
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager,
        const std::string& directory,
        const fastgltf::Asset& asset,
        usize textureIndex
    )
    {
        const auto& texture = asset.textures[textureIndex];

        if (!texture.basisuImageIndex.has_value())
        {
            Logger::Error("Image index not found! [textureIndex={}]\n", textureIndex);
        }

        const auto& image    = asset.images[texture.basisuImageIndex.value()];
        const auto& filePath = std::get<fastgltf::sources::URI>(image.data); // TODO: Replace with visitor

        if (filePath.fileByteOffset != 0)
        {
            Logger::Error("Unsupported file byte offset! [fileByteOffset={}]\n", filePath.fileByteOffset);
        }

        if (!filePath.uri.isLocalPath())
        {
            Logger::Error("Only local paths are supported! [UriPath={}]\n", filePath.uri.c_str());
        }

        return textureManager.AddTexture
        (
            megaSet,
            context.device,
            context.allocator,
            (directory + "/") + filePath.uri.c_str()
        );
    }
}
