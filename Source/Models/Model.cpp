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

#include "GPU/Vertex.h"
#include "Util/Log.h"
#include "Util/Files.h"
#include "Util/Enum.h"
#include "Util/Types.h"
#include "Util/Maths.h"
#include "Util/Visitor.h"

namespace Models
{
    // Model folder path
    constexpr auto MODEL_ASSETS_DIR = "GFX/";

    // Default texture paths
    constexpr auto DEFAULT_ALBEDO     = "Albedo.ktx2";
    constexpr auto DEFAULT_NORMAL     = "Normal.ktx2";
    constexpr auto DEFAULT_AO_RGH_MTL = "Albedo.ktx2";
    constexpr auto DEFAULT_EMMISIVE   = "Albedo.ktx2";

    Model::Model
    (
        VmaAllocator allocator,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path
    )
        : name(Util::Files::GetNameWithoutExtension(path))
    {
        Logger::Info("Loading model! [Name={}]\n", name);

        const std::string assetPath      = Util::Files::GetAssetPath(MODEL_ASSETS_DIR, path);
        const std::string assetDirectory = Util::Files::GetDirectory(assetPath);

        fastgltf::Parser parser
        (
            fastgltf::Extensions::KHR_texture_basisu |
            fastgltf::Extensions::KHR_materials_ior |
            fastgltf::Extensions::KHR_materials_emissive_strength
        );

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

    void Model::Destroy
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager,
        Vk::GeometryBuffer& geometryBuffer,
        Util::DeletionQueue& deletionQueue
    )
    {
        for (auto& mesh : meshes)
        {
            mesh.Destroy
            (
                device,
                allocator,
                megaSet,
                textureManager,
                geometryBuffer,
                deletionQueue
            );
        }
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

                const auto [writePointer, info] = geometryBuffer.indexBuffer.Allocate
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

                const auto [writePointer, info] = geometryBuffer.positionBuffer.Allocate
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

            const auto& normalAccessor = GetAccessor
            (
                asset,
                primitive,
                "NORMAL",
                fastgltf::AccessorType::Vec3
            );

            // UV Maps
            {
                const auto uv0AccessorIndex = GetAccessorIndex
                (
                    asset,
                    primitive,
                    "TEXCOORD_0",
                    fastgltf::AccessorType::Vec2
                );

                const auto uv1AccessorIndex = GetAccessorIndex
                (
                    asset,
                    primitive,
                    "TEXCOORD_1",
                    fastgltf::AccessorType::Vec2
                );

                const auto [writePointer, info] = geometryBuffer.uvBuffer.Allocate
                (
                   allocator,
                   normalAccessor.count,
                   deletionQueue
                );

                surfaceInfo.uvInfo = info;

                for (usize i = 0; i < normalAccessor.count; ++i)
                {
                    GPU::UV uvs = {};

                    std::optional<glm::vec2> uv0 = std::nullopt;
                    std::optional<glm::vec2> uv1 = std::nullopt;

                    if (uv0AccessorIndex.has_value())
                    {
                        uv0 = fastgltf::getAccessorElement<glm::vec2>(asset, asset.accessors[*uv0AccessorIndex], i);
                    }

                    if (uv1AccessorIndex.has_value())
                    {
                        uv1 = fastgltf::getAccessorElement<glm::vec2>(asset, asset.accessors[*uv1AccessorIndex], i);
                    }

                    uvs.uv[0] = uv0.has_value() ? uv0.value() : uv1.value_or(glm::vec2(0.0f, 0.0f));
                    uvs.uv[1] = uv1.has_value() ? uv1.value() : uv0.value_or(glm::vec2(0.0f, 0.0f));

                    writePointer[i] = uvs;
                }
            }

            // Vertices
            {
                const auto& tangentAccessor = GetAccessor
                (
                    asset,
                    primitive,
                    "TANGENT",
                    fastgltf::AccessorType::Vec4
                );

                const auto [writePointer, info] = geometryBuffer.vertexBuffer.Allocate
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
                        .tangent = fastgltf::getAccessorElement<glm::vec4>(asset, tangentAccessor, i)
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
                material.albedoFactor     = glm::fastgltf_cast(mat.pbrData.baseColorFactor);
                material.roughnessFactor  = mat.pbrData.roughnessFactor;
                material.metallicFactor   = mat.pbrData.metallicFactor;
                material.emmisiveFactor   = glm::fastgltf_cast(mat.emissiveFactor);
                material.emmisiveStrength = mat.emissiveStrength;
                material.ior              = mat.ior;

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

                std::tie(material.albedoID, material.albedoUVMapID) = LoadTexture
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

                std::tie(material.normalID, material.normalUVMapID) = LoadTexture
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

                std::tie(material.aoRghMtlID, material.aoRghMtlUVMapID) = LoadTexture
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

            // Emmisive
            {
                const auto& emmisiveTexture = mat.emissiveTexture;

                std::tie(material.emmisiveID, material.emmisiveUVMapID) = LoadTexture
                (
                    allocator,
                    textureManager,
                    deletionQueue,
                    directory,
                    asset,
                    emmisiveTexture,
                    DEFAULT_EMMISIVE
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
        return std::visit(Util::Visitor {
            [&] (const fastgltf::math::fmat4x4& matrix)
            {
                return base * glm::fastgltf_cast(matrix);
            },
            [&] (const fastgltf::TRS& trs)
            {
                return base * Maths::TransformMatrix
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

    std::optional<usize> Model::GetAccessorIndex
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
            return std::nullopt;
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

        return attributeIt->accessorIndex;
    }

    std::pair<Vk::TextureID, u32> Model::LoadTexture
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
            const auto id = textureManager.AddTexture
            (
                allocator,
                deletionQueue,
                Vk::ImageUpload{
                    .type   = Vk::ImageUploadType::KTX2,
                    .flags  = Vk::ImageUploadFlags::None,
                    .source = Vk::ImageUploadFile{
                        .path = Util::Files::GetAssetPath(MODEL_ASSETS_DIR, defaultTexture)
                    }
                }
            );

            return std::make_pair(id, 0);
        }

        if (textureInfo->texCoordIndex > 1)
        {
            Logger::Warning
            (
                "Texture uses more than 2 UV channels! [TextureIndex={}] [TexCoordIndex={}]\n",
                textureInfo->textureIndex,
                textureInfo->texCoordIndex
            );
        }

        const auto id = LoadTextureInternal
        (
            allocator,
            textureManager,
            deletionQueue,
            directory,
            asset,
            textureInfo->textureIndex
        );

        const auto index = glm::clamp<u32>(textureInfo->texCoordIndex, 0, 1);

        return std::make_pair(id, index);
    }

    std::pair<Vk::TextureID, u32> Model::LoadTexture
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
            const auto id = textureManager.AddTexture
            (
                allocator,
                deletionQueue,
                Vk::ImageUpload{
                    .type   = Vk::ImageUploadType::KTX2,
                    .flags  = Vk::ImageUploadFlags::None,
                    .source = Vk::ImageUploadFile{
                        .path = Util::Files::GetAssetPath(MODEL_ASSETS_DIR, DEFAULT_NORMAL)
                    }
                }
            );

            return std::make_pair(id, 0);
        }

        if (textureInfo->texCoordIndex > 1)
        {
            Logger::Warning
            (
                "Normal texture uses more than two UV channels! [TextureIndex={}] [TexCoordIndex={}]\n",
                textureInfo->textureIndex,
                textureInfo->texCoordIndex
            );
        }

        const auto id = LoadTextureInternal
        (
            allocator,
            textureManager,
            deletionQueue,
            directory,
            asset,
            textureInfo->textureIndex
        );

        const auto index = glm::clamp<u32>(textureInfo->texCoordIndex, 0, 1);

        return std::make_pair(id, index);
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

        usize imageIndex         = 0;
        Vk::ImageUploadType type = Vk::ImageUploadType::KTX2;

        if (texture.basisuImageIndex.has_value())
        {
            imageIndex = texture.basisuImageIndex.value();
            type       = Vk::ImageUploadType::KTX2;
        }
        else if (texture.imageIndex.has_value())
        {
            imageIndex = texture.imageIndex.value();
            type       = Vk::ImageUploadType::SDR;
        }
        else
        {
            Logger::Error("Image index not found! [TextureIndex={}]\n", textureIndex);
        }

        const auto& image = asset.images[imageIndex];

        return std::visit(Util::Visitor{
            [&] (ENGINE_UNUSED const auto& argument) -> Vk::TextureID
            {
                Logger::Error
                (
                    "Unsupported source! [TextureIndex={}] [ImageIndex={}]\n",
                    textureIndex,
                    imageIndex
                );

                return 0;
            },
            [&] (const fastgltf::sources::URI& filePath) -> Vk::TextureID
            {
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
                    Vk::ImageUpload{
                        .type   = type,
                        .flags  = Vk::ImageUploadFlags::None,
                        .source = Vk::ImageUploadFile{
                            .path = fmt::format("{}{}{}", directory.data(), "/", filePath.uri.c_str())
                        }
                    }
                );
            },
            [&] (const fastgltf::sources::Array& array) -> Vk::TextureID
            {
                const auto arrayBegin = reinterpret_cast<const u8*>(array.bytes.data() + 0);
                const auto arrayEnd   = reinterpret_cast<const u8*>(array.bytes.data() + array.bytes.size());

                return textureManager.AddTexture
                (
                    allocator,
                    deletionQueue,
                    Vk::ImageUpload{
                        .type   = type,
                        .flags  = Vk::ImageUploadFlags::None,
                        .source = Vk::ImageUploadMemory{
                            .name = std::string(image.name),
                            .data = std::vector(arrayBegin, arrayEnd)
                        }
                    }
                );
            },
            [&] (const fastgltf::sources::BufferView& view) -> Vk::TextureID
            {
                const auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                const auto& buffer     = asset.buffers[bufferView.bufferIndex];

                return std::visit(Util::Visitor {
                    // Because we specify LoadExternalBuffers, all buffers are already loaded into a vector.
                    [&] (ENGINE_UNUSED const auto& argument) -> Vk::TextureID
                    {
                        Logger::Error
                        (
                            "Unsupported buffer source! [TextureIndex={}] [ImageIndex={}]\n",
                            textureIndex,
                            imageIndex
                        );

                        return 0;
                    },
                    [&] (const fastgltf::sources::Array& array) -> Vk::TextureID
                    {
                        const auto arrayBegin = reinterpret_cast<const u8*>(array.bytes.data() + bufferView.byteOffset + 0);
                        const auto arrayEnd   = reinterpret_cast<const u8*>(array.bytes.data() + bufferView.byteOffset + bufferView.byteLength);

                        return textureManager.AddTexture
                        (
                            allocator,
                            deletionQueue,
                            Vk::ImageUpload{
                                .type   = type,
                                .flags  = Vk::ImageUploadFlags::None,
                                .source = Vk::ImageUploadMemory{
                                    .name = std::string(image.name),
                                    .data = std::vector(arrayBegin, arrayEnd)
                                }
                            }
                        );
                    }
                }, buffer.data);
            },
        }, image.data);
    }
}
