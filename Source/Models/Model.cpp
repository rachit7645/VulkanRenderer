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

#include "GPU/Vertex.h"
#include "Util/Log.h"
#include "Util/Files.h"
#include "Util/Enum.h"
#include "Util/Types.h"
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
        VmaAllocator allocator,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path
    )
    {
        Logger::Info("Loading model: {}\n", path);

        const std::string assetPath      = Util::Files::GetAssetPath(MODEL_ASSETS_DIR, path);
        const std::string assetDirectory = Util::Files::GetDirectory(assetPath);

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
            allocator,
            geometryBuffer,
            textureManager,
            deletionQueue,
            assetDirectory,
            asset.get()
        );
    }

    void Model::ProcessScenes
    (
        VmaAllocator allocator,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Util::DeletionQueue& deletionQueue,
        const std::string_view directory,
        const fastgltf::Asset& asset
    )
    {
        for (const auto& scene : asset.scenes)
        {
            for (const auto nodeIndex : scene.nodeIndices)
            {
                ProcessNode
                (
                    allocator,
                    geometryBuffer,
                    textureManager,
                    deletionQueue,
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
        VmaAllocator allocator,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Util::DeletionQueue& deletionQueue,
        const std::string_view directory,
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
                allocator,
                geometryBuffer,
                textureManager,
                deletionQueue,
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
                allocator,
                geometryBuffer,
                textureManager,
                deletionQueue,
                directory,
                asset,
                child,
                nodeMatrix
            );
        }
    }

    void Model::LoadMesh
    (
        VmaAllocator allocator,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Util::DeletionQueue& deletionQueue,
        const std::string_view directory,
        const fastgltf::Asset& asset,
        const fastgltf::Mesh& mesh,
        const glm::mat4& nodeMatrix
    )
    {
        for (const auto& primitive : mesh.primitives)
        {
            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                Logger::Warning
                (
                    "Unsupported primitive type! [Type={}]\n",
                    static_cast<std::underlying_type_t<fastgltf::PrimitiveType>>(primitive.type)
                );
            }

            // Geometry data
            GPU::SurfaceInfo surfaceInfo = {};
            GPU::AABB        aabb        = {};

            // Indices
            {
                if (!primitive.indicesAccessor.has_value())
                {
                    Logger::Error("{}\n", "Primitive does not contain indices accessor!");
                }

                const auto& indicesAccessor = asset.accessors[primitive.indicesAccessor.value()];

                if (indicesAccessor.type != fastgltf::AccessorType::Scalar)
                {
                    Logger::Error
                    (
                        "Invalid indices accessor type! [AccessorType={}]\n",
                        static_cast<std::underlying_type_t<fastgltf::AccessorType>>(indicesAccessor.type)
                    );
                }

                const auto [writePointer, info] = geometryBuffer.indexBuffer.GetWriteHandle
                (
                    allocator,
                    indicesAccessor.count,
                    deletionQueue
                );

                surfaceInfo.indexInfo = info;

                // Assume indices are u32s
                switch (indicesAccessor.componentType)
                {
                case fastgltf::ComponentType::Byte:
                {
                    fastgltf::iterateAccessorWithIndex<s8>(asset, indicesAccessor, [&] (s8 index, usize i)
                    {
                        writePointer[i] = static_cast<GPU::Index>(static_cast<u8>(index));
                    });
                    break;
                }

                case fastgltf::ComponentType::UnsignedByte:
                {
                    fastgltf::iterateAccessorWithIndex<u8>(asset, indicesAccessor, [&] (u8 index, usize i)
                    {
                        writePointer[i] = static_cast<GPU::Index>(index);
                    });
                    break;
                }

                case fastgltf::ComponentType::Short:
                {
                    fastgltf::iterateAccessorWithIndex<s16>(asset, indicesAccessor, [&] (s16 index, usize i)
                    {
                        writePointer[i] = static_cast<GPU::Index>(index);
                    });
                    break;
                }

                case fastgltf::ComponentType::UnsignedShort:
                {
                    fastgltf::iterateAccessorWithIndex<u16>(asset, indicesAccessor, [&] (u16 index, usize i)
                    {
                        writePointer[i] = static_cast<GPU::Index>(index);
                    });
                    break;
                }

                case fastgltf::ComponentType::UnsignedInt:
                {
                    fastgltf::copyFromAccessor<GPU::Index>(asset, indicesAccessor, writePointer);
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

            // Positions
            {
                const auto& positionAccessor = GetAccessor
                (
                    asset,
                    primitive,
                    "POSITION",
                    fastgltf::AccessorType::Vec3
                );

                const auto [writePointer, info] = geometryBuffer.positionBuffer.GetWriteHandle
                (
                    allocator,
                    positionAccessor.count,
                    deletionQueue
                );

                surfaceInfo.positionInfo = info;

                aabb.min = glm::vec3(std::numeric_limits<f32>::max());
                aabb.max = glm::vec3(std::numeric_limits<f32>::lowest());

                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor, [&] (const GPU::Position& position, usize index)
                {
                    aabb.min = glm::min(aabb.min, position);
                    aabb.max = glm::max(aabb.max, position);

                    writePointer[index] = position;
                });
            }

            // Vertices
            {
                const auto& normalAccessor = GetAccessor
                (
                    asset,
                    primitive,
                    "NORMAL",
                    fastgltf::AccessorType::Vec3
                );

                const auto& uvAccessor = GetAccessor
                (
                    asset,
                    primitive,
                    "TEXCOORD_0",
                    fastgltf::AccessorType::Vec2
                );

                const auto& tangentAccessor = GetAccessor
                (
                    asset,
                    primitive,
                    "TANGENT",
                    fastgltf::AccessorType::Vec4
                );

                if (normalAccessor.count != uvAccessor.count || normalAccessor.count != tangentAccessor.count)
                {
                    Logger::Error("{}\n", "Invalid primitive!");
                }

                const auto [writePointer, info] = geometryBuffer.vertexBuffer.GetWriteHandle
                (
                    allocator,
                    normalAccessor.count,
                    deletionQueue
                );

                surfaceInfo.vertexInfo = info;

                for (usize i = 0; i < normalAccessor.count; ++i)
                {
                    writePointer[i] = GPU::Vertex
                    {
                        .normal  = fastgltf::getAccessorElement<glm::vec3>(asset, normalAccessor,  i),
                        .uv0     = fastgltf::getAccessorElement<glm::vec2>(asset, uvAccessor,      i),
                        .tangent = fastgltf::getAccessorElement<glm::vec4>(asset, tangentAccessor, i),
                    };
                }
            }

            if (!primitive.materialIndex.has_value())
            {
                Logger::Error("{}\n", "No material in primitive!");
            }

            Models::Material material = {};

            const auto& mat = asset.materials[primitive.materialIndex.value()];

            // Material Factors
            {
                material.albedoFactor    = glm::fastgltf_cast(mat.pbrData.baseColorFactor);
                material.roughnessFactor = mat.pbrData.roughnessFactor;
                material.metallicFactor  = mat.pbrData.metallicFactor;

                if (mat.doubleSided)
                {
                    material.flags |= GPU::MaterialFlags::DoubleSided;
                }

                // TODO: Add proper support for AlphaMode::Blend
                if (mat.alphaMode == fastgltf::AlphaMode::Mask || mat.alphaMode == fastgltf::AlphaMode::Blend)
                {
                    material.flags       |= GPU::MaterialFlags::AlphaMasked;
                    material.alphaCutOff  = mat.alphaCutoff;
                }
            }

            // Albedo
            {
                const auto& baseColorTexture = mat.pbrData.baseColorTexture;

                material.albedo = LoadTexture
                (
                    allocator,
                    textureManager,
                    deletionQueue,
                    directory,
                    asset,
                    baseColorTexture,
                    DEFAULT_ALBEDO
                );
            }

            // Normal
            {
                const auto& normalTexture = mat.normalTexture;

                material.normal = LoadTexture
                (
                    allocator,
                    textureManager,
                    deletionQueue,
                    directory,
                    asset,
                    normalTexture
                );
            }

            // AO + Roughness + Metallic
            {
                const auto& metallicRoughnessTexture = mat.pbrData.metallicRoughnessTexture;

                material.aoRghMtl = LoadTexture
                (
                    allocator,
                    textureManager,
                    deletionQueue,
                    directory,
                    asset,
                    metallicRoughnessTexture,
                    DEFAULT_AO_RGH_MTL
                );
            }

            meshes.emplace_back
            (
                surfaceInfo,
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

    Vk::TextureID Model::LoadTexture
    (
        VmaAllocator allocator,
        Vk::TextureManager& textureManager,
        Util::DeletionQueue& deletionQueue,
        const std::string_view directory,
        const fastgltf::Asset& asset,
        const std::optional<fastgltf::TextureInfo>& textureInfo,
        const std::string_view defaultTexture
    )
    {
        if (!textureInfo.has_value())
        {
            return textureManager.AddTexture
            (
                allocator,
                deletionQueue,
                Util::Files::GetAssetPath(MODEL_ASSETS_DIR, defaultTexture)
            );
        }

        // TODO: Support multiple UV channels
        if (textureInfo->texCoordIndex != 0)
        {
            Logger::Warning
            (
                "Texture uses more than one UV channel! [TextureIndex={}] [TexCoordIndex={}]\n",
                textureInfo->textureIndex,
                textureInfo->texCoordIndex
            );
        }

        return LoadTextureInternal
        (
            allocator,
            textureManager,
            deletionQueue,
            directory,
            asset,
            textureInfo->textureIndex
        );
    }

    Vk::TextureID Model::LoadTexture
    (
        VmaAllocator allocator,
        Vk::TextureManager& textureManager,
        Util::DeletionQueue& deletionQueue,
        const std::string_view directory,
        const fastgltf::Asset& asset,
        const std::optional<fastgltf::NormalTextureInfo>& textureInfo
    )
    {
        if (!textureInfo.has_value())
        {
            return textureManager.AddTexture
            (
                allocator,
                deletionQueue,
                Util::Files::GetAssetPath(MODEL_ASSETS_DIR, DEFAULT_NORMAL)
            );
        }

        // TODO: Support multiple UV channels
        if (textureInfo->texCoordIndex != 0)
        {
            Logger::Warning
            (
                "Normal texture uses more than one UV channel! [TextureIndex={}] [TexCoordIndex={}]\n",
                textureInfo->textureIndex,
                textureInfo->texCoordIndex
            );
        }

        return LoadTextureInternal
        (
            allocator,
            textureManager,
            deletionQueue,
            directory,
            asset,
            textureInfo->textureIndex
        );
    }

    Vk::TextureID Model::LoadTextureInternal
    (
        VmaAllocator allocator,
        Vk::TextureManager& textureManager,
        Util::DeletionQueue& deletionQueue,
        const std::string_view directory,
        const fastgltf::Asset& asset,
        usize textureIndex
    )
    {
        const auto& texture = asset.textures[textureIndex];

        if (!texture.basisuImageIndex.has_value())
        {
            Logger::Error("Image index not found! [TextureIndex={}]\n", textureIndex);
        }

        const auto& image = asset.images[texture.basisuImageIndex.value()];

        if (!std::holds_alternative<fastgltf::sources::URI>(image.data))
        {
            Logger::Error
            (
                "Unsupported format! [TextureIndex={}] [ImageIndex={}]\n",
                textureIndex,
                texture.basisuImageIndex.value()
            );
        }

        const auto& filePath = std::get<fastgltf::sources::URI>(image.data);

        if (filePath.fileByteOffset != 0)
        {
            Logger::Error
            (
                "Unsupported file byte offset! [TextureIndex={}] [FileByteOffset={}]\n",
                textureIndex,
                filePath.fileByteOffset
            );
        }

        if (!filePath.uri.isLocalPath())
        {
            Logger::Error
            (
                "Only local paths are supported! [TextureIndex={}] [UriPath={}]\n",
                textureIndex,
                filePath.uri.c_str()
            );
        }

        return textureManager.AddTexture
        (
            allocator,
            deletionQueue,
            fmt::format("{}{}{}", directory.data(), "/", filePath.uri.c_str())
        );
    }
}