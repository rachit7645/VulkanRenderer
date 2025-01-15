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

namespace Models
{
    // Model folder path
    constexpr auto MODEL_ASSETS_DIR   = "GFX/";

    constexpr auto DEFAULT_ALBEDO     = "Albedo.png";
    constexpr auto DEFAULT_NORMAL     = "Normal.png";
    constexpr auto DEFAULT_AO_RGH_MTL = "Metal-Roughness.png";

    constexpr auto ALBEDO_FLAGS     = Vk::Texture::Flags::IsSRGB | Vk::Texture::Flags::GenMipmaps;
    constexpr auto NORMAL_FLAGS     = Vk::Texture::Flags::GenMipmaps;
    constexpr auto AO_RGH_MTL_FLAGS = Vk::Texture::Flags::GenMipmaps;

    Model::Model
    (
        const Vk::Context& context,
        Vk::TextureManager& textureManager,
        const std::string_view path
    )
    {
        Logger::Info("Loading model: {}\n", path);

        const std::string assetPath      = Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, path);
        const std::string assetDirectory = Engine::Files::GetDirectory(assetPath);

        fastgltf::Parser parser;

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

        for (const auto& mesh : asset->meshes)
        {
            for (const auto& primitive : mesh.primitives)
            {
                std::vector<Index>  indices  = {};
                std::vector<Vertex> vertices = {};
                Material            material = {};

                if (primitive.type != fastgltf::PrimitiveType::Triangles)
                {
                    Logger::Warning
                    (
                        "Unsupported primitive type! [Type={}] [Path={}]\n",
                        static_cast<std::underlying_type_t<fastgltf::PrimitiveType>>(primitive.type),
                        path
                    );
                }

                // Indices
                {
                    if (!primitive.indicesAccessor.has_value())
                    {
                        Logger::Error("Primitive does not contain indices accessor! [Path={}]\n", path);
                    }

                    const auto& indicesAccessor = asset->accessors[primitive.indicesAccessor.value()];
                    indices.reserve(indicesAccessor.count);

                    if (indicesAccessor.type != fastgltf::AccessorType::Scalar)
                    {
                        Logger::Error
                        (
                            "Invalid indices accessor type! [AccessorType={}] [Path={}]\n",
                            static_cast<std::underlying_type_t<fastgltf::AccessorType>>(indicesAccessor.type),
                            path
                        );
                    }

                    // Assume indices are u32s
                    switch (indicesAccessor.componentType)
                    {
                    case fastgltf::ComponentType::Byte:
                    {
                        fastgltf::iterateAccessor<s8>(asset.get(), indicesAccessor, [&] (s8 index)
                        {
                            indices.push_back(static_cast<Index>(index));
                        });
                        break;
                    }

                    case fastgltf::ComponentType::UnsignedByte:
                    {
                        fastgltf::iterateAccessor<u8>(asset.get(), indicesAccessor, [&] (u8 index)
                        {
                            indices.push_back(static_cast<Index>(index));
                        });
                        break;
                    }

                    case fastgltf::ComponentType::Short:
                    {
                        fastgltf::iterateAccessor<s16>(asset.get(), indicesAccessor, [&] (s16 index)
                        {
                            indices.push_back(static_cast<Index>(index));
                        });
                        break;
                    }

                    case fastgltf::ComponentType::UnsignedShort:
                    {
                        fastgltf::iterateAccessor<u16>(asset.get(), indicesAccessor, [&] (u16 index)
                        {
                            indices.push_back(static_cast<Index>(index));
                        });
                        break;
                    }

                    case fastgltf::ComponentType::UnsignedInt:
                    {
                        fastgltf::copyFromAccessor<Index>(asset.get(), indicesAccessor, indices.data());
                        break;
                    }

                    default:
                        Logger::Error
                        (
                            "Invalid index component type! [ComponentType={}] [Path={}]\n",
                            static_cast<std::underlying_type_t<fastgltf::ComponentType>>(indicesAccessor.componentType),
                            path
                        );
                    }
                }

                // Position
                {
                    const auto positionIt = primitive.findAttribute("POSITION");
                    if (positionIt == primitive.attributes.cend())
                    {
                        Logger::Error("Failed to find attribute! [Attrbute=POSITION] [Path={}]\n", path);
                    }

                    const auto& positionAccessor = asset->accessors[positionIt->accessorIndex];
                    vertices.reserve(positionAccessor.count);

                    if (positionAccessor.type != fastgltf::AccessorType::Vec3)
                    {
                        Logger::Error
                        (
                            "Invalid position accessor type! [AccessorType={}] [Path={}]\n",
                            static_cast<std::underlying_type_t<fastgltf::AccessorType>>(positionAccessor.type),
                            path
                        );
                    }

                    fastgltf::iterateAccessor<glm::vec3>(asset.get(), positionAccessor, [&] (const glm::vec3& position)
                    {
                        Vertex vertex = {};
                        vertex.position_uvX = {position, 0.0f};
                        vertices.emplace_back(vertex);
                    });
                }

                // Normals
                {

                    const auto normalIt = primitive.findAttribute("NORMAL");
                    if (normalIt == primitive.attributes.cend())
                    {
                        Logger::Error("Failed to find attribute! [Attrbute=NORMAL] [Path={}]\n", path);
                    }

                    const auto& normalAccessor = asset->accessors[normalIt->accessorIndex];

                    if (normalAccessor.type != fastgltf::AccessorType::Vec3)
                    {
                        Logger::Error
                        (
                            "Invalid normal accessor type! [AccessorType={}] [Path={}]\n",
                            static_cast<std::underlying_type_t<fastgltf::AccessorType>>(normalAccessor.type),
                            path
                        );
                    }

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), normalAccessor, [&] (const glm::vec3& normal, usize index)
                    {
                        vertices[index].normal_uvY = {normal, 0.0f};
                    });
                }

                // UVs
                {
                    const auto uvIt = primitive.findAttribute("TEXCOORD_0");
                    if (uvIt == primitive.attributes.cend())
                    {
                        Logger::Error("Failed to find attribute! [Attrbute=TEXCOORD_0] [Path={}]\n", path);
                    }

                    const auto& uvAccessor = asset->accessors[uvIt->accessorIndex];

                    if (uvAccessor.type != fastgltf::AccessorType::Vec2)
                    {
                        Logger::Error
                        (
                            "Invalid UV0 accessor type! [AccessorType={}] [Path={}]\n",
                            static_cast<std::underlying_type_t<fastgltf::AccessorType>>(uvAccessor.type),
                            path
                        );
                    }

                    fastgltf::iterateAccessorWithIndex<glm::vec2>(asset.get(), uvAccessor, [&] (const glm::vec2& uv, usize index)
                    {
                        vertices[index].position_uvX.w = uv.x;
                        vertices[index].normal_uvY.w   = uv.y;
                    });
                }

                // Tangent
                {
                    const auto tangentIt = primitive.findAttribute("TANGENT");
                    if (tangentIt == primitive.attributes.cend())
                    {
                        Logger::Error("Failed to find attribute! [Attrbute=TANGENT] [Path={}]\n", path);
                    }

                    const auto& tangentAccessor = asset->accessors[tangentIt->accessorIndex];

                    if (tangentAccessor.type != fastgltf::AccessorType::Vec4)
                    {
                        Logger::Error
                        (
                            "Invalid normal accessor type! [AccessorType={}] [Path={}]\n",
                            static_cast<std::underlying_type_t<fastgltf::AccessorType>>(tangentAccessor.type),
                            path
                        );
                    }

                    fastgltf::iterateAccessorWithIndex<glm::vec4>(asset.get(), tangentAccessor, [&] (const glm::vec4& tangent, usize index)
                    {
                        vertices[index].tangent_padf32 = tangent;
                    });
                }

                if (!primitive.materialIndex.has_value())
                {
                    Logger::Error("No material in primitive! [Path={}]\n", path);
                }

                const auto& mat = asset->materials[primitive.materialIndex.value()];

                material.albedoFactor    = glm::fastgltf_cast(mat.pbrData.baseColorFactor);
                material.roughnessFactor = mat.pbrData.roughnessFactor;
                material.metallicFactor  = mat.pbrData.metallicFactor;

                // Albedo
                {
                    const auto& baseColorTexture = mat.pbrData.baseColorTexture;

                    if (baseColorTexture.has_value())
                    {
                        // FIXME: Support multiple UV channels
                        if (baseColorTexture->texCoordIndex != 0)
                        {
                            Logger::Warning
                            (
                                "Albedo uses more than one UV channel! [texCoordIndex={}] [Path={}] \n",
                                baseColorTexture->texCoordIndex,
                                path
                            );
                        }

                        material.albedo = LoadTexture
                        (
                            context,
                            textureManager,
                            path,
                            assetDirectory,
                            asset.get(),
                            baseColorTexture->textureIndex,
                            ALBEDO_FLAGS
                        );
                    }
                    else
                    {
                        material.albedo = textureManager.AddTexture
                        (
                            context,
                            Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, DEFAULT_ALBEDO),
                            ALBEDO_FLAGS
                        );
                    }
                }

                // Normal
                {
                    if (mat.normalTexture.has_value())
                    {
                        // FIXME: Support multiple UV channels
                        if (mat.normalTexture->texCoordIndex != 0)
                        {
                            Logger::Warning
                            (
                                "Normal uses more than one UV channel! [texCoordIndex={}] [Path={}] \n",
                                mat.normalTexture->texCoordIndex,
                                path
                            );
                        }

                        material.normal = LoadTexture
                        (
                            context,
                            textureManager,
                            path,
                            assetDirectory,
                            asset.get(),
                            mat.normalTexture->textureIndex,
                            NORMAL_FLAGS
                        );
                    }
                    else
                    {
                        material.normal = textureManager.AddTexture
                        (
                            context,
                            Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, DEFAULT_NORMAL),
                            NORMAL_FLAGS
                        );
                    }
                }

                // AO + Roughness + Metallic
                {
                    const auto& metallicRoughnessTexture = mat.pbrData.metallicRoughnessTexture;

                    if (metallicRoughnessTexture.has_value())
                    {
                        // FIXME: Support multiple UV channels
                        if (metallicRoughnessTexture->texCoordIndex != 0)
                        {
                            Logger::Warning
                            (
                                "AoRghMtl uses more than one UV channel! [texCoordIndex={}] [Path={}] \n",
                                metallicRoughnessTexture->texCoordIndex,
                                path
                            );
                        }

                        material.aoRghMtl = LoadTexture
                        (
                            context,
                            textureManager,
                            path,
                            assetDirectory,
                            asset.get(),
                            metallicRoughnessTexture->textureIndex,
                            AO_RGH_MTL_FLAGS
                        );
                    }
                    else
                    {
                        material.aoRghMtl = textureManager.AddTexture
                        (
                            context,
                            Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, DEFAULT_AO_RGH_MTL),
                            AO_RGH_MTL_FLAGS
                        );
                    }
                }

                meshes.emplace_back
                (
                    Vk::VertexBuffer(context, vertices, indices),
                    material
                );
            }
        }

        textureManager.Update(context.device);
    }

    usize Model::LoadTexture
    (
        const Vk::Context& context,
        Vk::TextureManager& textureManager,
        const std::string_view path,
        const std::string& directory,
        const fastgltf::Asset& asset,
        usize textureIndex,
        Vk::Texture::Flags flags
    )
    {
        const auto& texture = asset.textures[textureIndex];

        if (!texture.imageIndex.has_value())
        {
            Logger::Error
            (
                "Image index not found! [textureIndex={}] [Path={}] \n",
                textureIndex,
                path
            );
        }

        const auto& image    = asset.images[texture.imageIndex.value()];
        const auto& filePath = std::get<fastgltf::sources::URI>(image.data); // TODO: Replace with visitor

        if (filePath.fileByteOffset != 0)
        {
            Logger::Error
            (
                "Unsupported file byte offset! [fileByteOffset={}] [Path={}]\n",
                filePath.fileByteOffset,
                path
            );
        }

        if (!filePath.uri.isLocalPath())
        {
            Logger::Error
            (
                "Only local paths are supported! [UriPath={}] [Path={}]\n",
                filePath.uri.c_str(),
                path
            );
        }

        return textureManager.AddTexture
        (
            context,
            (directory + "/") + filePath.uri.c_str(),
            flags
        );
    }
}
