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

#include "Model.h"

#include "GPU/Vertex.h"
#include "Util/Log.h"
#include "Util/Files.h"
#include "Util/Enum.h"
#include "Util/Types.h"
#include "Util/Visitor.h"

namespace Models
{
    // Model folder path
    constexpr auto MODEL_ASSETS_DIR = "GFX/";

    void Model::LoadFromFile
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Util::DeletionQueue& deletionQueue,
        const std::string_view filePath
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", path.data());
        #endif

        if (isLoaded)
        {
            Logger::Error("Model already loaded! [Path={}]", path);
        }

        path = std::string(filePath);

        const std::string assetPath      = Files::GetAssetPath(MODEL_ASSETS_DIR, path);
        const std::string assetDirectory = Files::GetDirectory(assetPath);

        fastgltf::Parser parser
        (
            fastgltf::Extensions::KHR_texture_basisu |
            fastgltf::Extensions::KHR_materials_ior |
            fastgltf::Extensions::KHR_materials_emissive_strength |
            fastgltf::Extensions::KHR_lights_punctual |
            fastgltf::Extensions::KHR_mesh_quantization
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
            device,
            allocator,
            geometryBuffer,
            textureManager,
            stagingPool,
            executor,
            deletionQueue,
            assetDirectory,
            asset.get()
        );

        isLoaded = true;
    }

    void Model::ProcessScenes
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Util::DeletionQueue& deletionQueue,
        const std::string_view directory,
        const fastgltf::Asset& asset
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        for (const auto& scene : asset.scenes)
        {
            for (const auto nodeIndex : scene.nodeIndices)
            {
                ProcessNode
                (
                    device,
                    allocator,
                    geometryBuffer,
                    textureManager,
                    stagingPool,
                    executor,
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
        VkDevice device,
        VmaAllocator allocator,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Util::DeletionQueue& deletionQueue,
        const std::string_view directory,
        const fastgltf::Asset& asset,
        usize nodeIndex,
        glm::mat4 nodeMatrix
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const auto& node = asset.nodes[nodeIndex];

        nodeMatrix = fastgltf::GetTransformMatrix(node, nodeMatrix);

        if (node.meshIndex.has_value())
        {
            LoadMesh
            (
                device,
                allocator,
                geometryBuffer,
                textureManager,
                stagingPool,
                executor,
                deletionQueue,
                directory,
                asset,
                asset.meshes[node.meshIndex.value()],
                nodeMatrix
            );
        }

        if (node.lightIndex.has_value())
        {
            LoadLight(asset.lights[node.lightIndex.value()], nodeMatrix);
        }

        for (const auto child : node.children)
        {
            ProcessNode
            (
                device,
                allocator,
                geometryBuffer,
                textureManager,
                stagingPool,
                executor,
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
        VkDevice device,
        VmaAllocator allocator,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Util::DeletionQueue& deletionQueue,
        const std::string_view directory,
        const fastgltf::Asset& asset,
        const fastgltf::Mesh& mesh,
        const glm::mat4& nodeMatrix
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        for (const auto& primitive : mesh.primitives)
        {
            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                Logger::Warning
                (
                    "Unsupported primitive type! [Type={}]\n",
                    static_cast<std::underlying_type_t<fastgltf::PrimitiveType>>(primitive.type)
                );

                continue;
            }

            // Geometry data
            GPU::SurfaceInfo surfaceInfo = {};
            GPU::AABB        aabb        = {};

            // Indices
            {
                #ifdef ENGINE_PROFILE
                ZoneScopedN("Load Indices");
                #endif

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

                const auto [data, info] = geometryBuffer.indexBuffer.Allocate
                (
                    device,
                    allocator,
                    indicesAccessor.count,
                    stagingPool,
                    deletionQueue
                );

                surfaceInfo.indexInfo = info;

                switch (indicesAccessor.componentType)
                {
                case fastgltf::ComponentType::Byte:
                {
                    fastgltf::iterateAccessorWithIndex<s8>(asset, indicesAccessor, [&] (s8 index, usize i)
                    {
                        const auto copy = static_cast<GPU::Index>(static_cast<u8>(index));

                        std::memcpy(&data[i], &copy, sizeof(GPU::Index));
                    });

                    break;
                }

                case fastgltf::ComponentType::UnsignedByte:
                {
                    fastgltf::iterateAccessorWithIndex<u8>(asset, indicesAccessor, [&] (u8 index, usize i)
                    {
                        const auto copy = static_cast<GPU::Index>(index);

                        std::memcpy(&data[i], &copy, sizeof(GPU::Index));
                    });

                    break;
                }

                case fastgltf::ComponentType::Short:
                {
                    fastgltf::iterateAccessorWithIndex<s16>(asset, indicesAccessor, [&] (s16 index, usize i)
                    {
                        const auto copy = static_cast<GPU::Index>(index);

                        std::memcpy(&data[i], &copy, sizeof(GPU::Index));
                    });

                    break;
                }

                case fastgltf::ComponentType::UnsignedShort:
                {
                    fastgltf::iterateAccessorWithIndex<u16>(asset, indicesAccessor, [&] (u16 index, usize i)
                    {
                        const auto copy = static_cast<GPU::Index>(index);

                        std::memcpy(&data[i], &copy, sizeof(GPU::Index));
                    });

                    break;
                }

                case fastgltf::ComponentType::UnsignedInt:
                {
                    static_assert(std::is_same_v<GPU::Index, u32>, "Assume indices are u32s for faster copying.");

                    fastgltf::copyFromAccessor<GPU::Index>(asset, indicesAccessor, data);

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

            Vk::VertexBuffer::Allocation vertexAllocation = {};

            // Positions
            {
                #ifdef ENGINE_PROFILE
                ZoneScopedN("Load Positions");
                #endif

                const auto& positionAccessor = fastgltf::GetAccessor
                (
                    asset,
                    primitive,
                    "POSITION",
                    fastgltf::AccessorType::Vec3
                );

                vertexAllocation = geometryBuffer.vertexBuffer.Allocate
                (
                    positionAccessor.count,
                    device,
                    allocator,
                    stagingPool,
                    deletionQueue
                );

                surfaceInfo.vertexInfo = vertexAllocation.info;

                aabb.min = glm::vec3(std::numeric_limits<f32>::max());
                aabb.max = glm::vec3(std::numeric_limits<f32>::lowest());

                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor, [&] (const GPU::Position& position, usize index)
                {
                    aabb.min = glm::min(aabb.min, position);
                    aabb.max = glm::max(aabb.max, position);

                    std::memcpy(&vertexAllocation.position[index], &position, sizeof(GPU::Position));
                });
            }

            const auto& normalAccessor = fastgltf::GetAccessor
            (
                asset,
                primitive,
                "NORMAL",
                fastgltf::AccessorType::Vec3
            );

            // UV Maps
            {
                #ifdef ENGINE_PROFILE
                ZoneScopedN("Load UVs");
                #endif

                const auto uv0AccessorIndex = fastgltf::GetUVAccessorIndex
                (
                    asset,
                    primitive,
                    "TEXCOORD_0"
                );

                const auto uv1AccessorIndex = fastgltf::GetUVAccessorIndex
                (
                    asset,
                    primitive,
                    "TEXCOORD_1"
                );

                for (usize i = 0; i < normalAccessor.count; ++i)
                {
                    GPU::UV uvs = {};

                    std::optional<glm::vec2> uv0 = std::nullopt;
                    std::optional<glm::vec2> uv1 = std::nullopt;

                    if (uv0AccessorIndex.has_value())
                    {
                        uv0 = glm::glm_cast(fastgltf::getAccessorElement<glm::vec2>(asset, asset.accessors[*uv0AccessorIndex], i));
                    }

                    if (uv1AccessorIndex.has_value())
                    {
                        uv1 = glm::glm_cast(fastgltf::getAccessorElement<glm::vec2>(asset, asset.accessors[*uv1AccessorIndex], i));
                    }

                    uvs.uv[0] = uv0.has_value() ? uv0.value() : uv1.value_or(glm::f16vec2(0, 0));
                    uvs.uv[1] = uv1.has_value() ? uv1.value() : uv0.value_or(glm::f16vec2(0, 0));

                    std::memcpy(&vertexAllocation.uv[i], &uvs, sizeof(GPU::UV));
                }
            }

            // Vertices
            {
                #ifdef ENGINE_PROFILE
                ZoneScopedN("Load Tangents and Normals");
                #endif

                const auto& tangentAccessor = fastgltf::GetAccessor
                (
                    asset,
                    primitive,
                    "TANGENT",
                    fastgltf::AccessorType::Vec4
                );

                for (usize i = 0; i < normalAccessor.count; ++i)
                {
                    const auto normal          = fastgltf::getAccessorElement<glm::vec3>(asset, normalAccessor,  i);
                    const auto tangentWithSign = fastgltf::getAccessorElement<glm::vec4>(asset, tangentAccessor, i);

                    const glm::vec3 unitNormal            = glm::normalize(normal);
                    const glm::vec2 packedNormal          = Maths::PackOctahedron(unitNormal);
                    const u32       packedNormalQuantized = glm::packSnorm2x16(packedNormal);

                    const glm::vec3 unitTangent       = glm::normalize(glm::vec3(tangentWithSign));
                    const glm::vec2 octEncodedTangent = Maths::PackOctahedron(unitTangent);

                    const s32 packedTangentX = Maths::QuantizeSNorm(octEncodedTangent.x, 15);
                    const s32 packedTangentY = Maths::QuantizeSNorm(octEncodedTangent.y, 15);
                    const s32 bitangentSign  = tangentWithSign.w >= 1.0f ? 1 : 0;

                    u32 packedTangentQuantized = 0;

                    packedTangentQuantized = glm::bitfieldInsert(packedTangentQuantized, static_cast<u32>(packedTangentX), 0,  15);
                    packedTangentQuantized = glm::bitfieldInsert(packedTangentQuantized, static_cast<u32>(packedTangentY), 15, 15);
                    packedTangentQuantized = glm::bitfieldInsert(packedTangentQuantized, static_cast<u32>(bitangentSign),  30, 1);

                    const auto vertex = GPU::Vertex
                    {
                        .normal  = packedNormalQuantized,
                        .tangent = packedTangentQuantized
                    };

                    std::memcpy(&vertexAllocation.normalAndTangent[i], &vertex, sizeof(GPU::Vertex));
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
                #ifdef ENGINE_PROFILE
                ZoneScopedN("Load Material Factors");
                #endif

                material.albedoFactor     = glm::fastgltf_cast(mat.pbrData.baseColorFactor);
                material.roughnessFactor  = mat.pbrData.roughnessFactor;
                material.metallicFactor   = mat.pbrData.metallicFactor;
                material.emissiveFactor   = glm::fastgltf_cast(mat.emissiveFactor);
                material.emissiveStrength = mat.emissiveStrength;
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
                #ifdef ENGINE_PROFILE
                ZoneScopedN("Load Albedo");
                #endif

                constexpr auto        DEFAULT_ALBEDO_NAME  = "Default/Albedo";
                constexpr glm::u8vec4 DEFAULT_ALBEDO_VALUE = {255, 255, 255, 255};

                const auto& baseColorTexture = mat.pbrData.baseColorTexture;

                const auto textureInfo = LoadTexture
                (
                    device,
                    allocator,
                    textureManager,
                    stagingPool,
                    executor,
                    deletionQueue,
                    directory,
                    asset,
                    baseColorTexture,
                    DEFAULT_ALBEDO_NAME,
                    DEFAULT_ALBEDO_VALUE
                );

                material.albedoID      = textureInfo.id;
                material.albedoUVMapID = textureInfo.uvMapIndex;
            }

            // Normal
            {
                #ifdef ENGINE_PROFILE
                ZoneScopedN("Load Normal Map");
                #endif

                constexpr auto        DEFAULT_NORMAL_NAME  = "Default/NormalMap";
                constexpr glm::u8vec4 DEFAULT_NORMAL_VALUE = {128, 128, 255, 255};

                const auto& normalTexture = mat.normalTexture;

                const auto textureInfo = LoadTexture
                (
                    device,
                    allocator,
                    textureManager,
                    stagingPool,
                    executor,
                    deletionQueue,
                    directory,
                    asset,
                    normalTexture,
                    DEFAULT_NORMAL_NAME,
                    DEFAULT_NORMAL_VALUE
                );

                material.normalID      = textureInfo.id;
                material.normalUVMapID = textureInfo.uvMapIndex;
            }

            // AO + Roughness + Metallic
            {
                #ifdef ENGINE_PROFILE
                ZoneScopedN("Load Roughness and Metallic");
                #endif

                constexpr auto        DEFAULT_AO_RGH_MTL_NAME  = "Default/RoughnessMetallic";
                constexpr glm::u8vec4 DEFAULT_AO_RGH_MTL_VALUE = {255, 255, 0, 255};

                const auto& metallicRoughnessTexture = mat.pbrData.metallicRoughnessTexture;

                const auto textureInfo = LoadTexture
                (
                    device,
                    allocator,
                    textureManager,
                    stagingPool,
                    executor,
                    deletionQueue,
                    directory,
                    asset,
                    metallicRoughnessTexture,
                    DEFAULT_AO_RGH_MTL_NAME,
                    DEFAULT_AO_RGH_MTL_VALUE
                );

                material.aoRghMtlID      = textureInfo.id;
                material.aoRghMtlUVMapID = textureInfo.uvMapIndex;
            }

            // Emissive
            {
                #ifdef ENGINE_PROFILE
                ZoneScopedN("Load Emissive");
                #endif

                constexpr auto        DEFAULT_EMISSIVE_NAME  = "Default/Emissive";
                constexpr glm::u8vec4 DEFAULT_EMISSIVE_VALUE = {255, 255, 255, 255};

                const auto& emissiveTexture = mat.emissiveTexture;

                const auto textureInfo = LoadTexture
                (
                    device,
                    allocator,
                    textureManager,
                    stagingPool,
                    executor,
                    deletionQueue,
                    directory,
                    asset,
                    emissiveTexture,
                    DEFAULT_EMISSIVE_NAME,
                    DEFAULT_EMISSIVE_VALUE
                );

                material.emissiveID      = textureInfo.id;
                material.emissiveUVMapID = textureInfo.uvMapIndex;
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

    void Model::LoadLight(const fastgltf::Light& light, const glm::mat4& nodeMatrix)
    {
        switch (light.type)
        {
        // Only one directional light is supported
        // That light is reserved by Engine::Scene
        // So we can't add any others
        case fastgltf::LightType::Directional:
            break;

        case fastgltf::LightType::Spot:
        {
            spotLights.emplace_back(Models::LocalSpotLight{
                .transform = nodeMatrix,
                .light     = GPU::SpotLight{
                    .position  = glm::vec3(0.0f),
                    .color     = glm::fastgltf_cast(light.color),
                    .intensity = light.intensity,
                    .direction = glm::vec3(0.0f, 0.0f, -1.0f),
                    .cutOff    = glm::vec2(light.innerConeAngle.value_or(0.0f), light.outerConeAngle.value_or(0.0f)),
                    .range     = light.range.value_or(0.0f)
                }
            });

            break;
        }

        case fastgltf::LightType::Point:
        {
            pointLights.emplace_back(Models::LocalPointLight{
                .transform = nodeMatrix,
                .light     = GPU::PointLight{
                    .position  = glm::vec3(0.0f),
                    .color     = glm::fastgltf_cast(light.color),
                    .intensity = light.intensity,
                    .range     = light.range.value_or(0.0f)
                }
            });

            break;
        }

        default:
            Logger::Error("{}\n", "Invalid light type!");
        }
    }

    template<typename T>
    requires fastgltf::IsTextureInfo<T>
    Model::TextureInfo Model::LoadTexture
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::TextureManager& textureManager,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Util::DeletionQueue& deletionQueue,
        const std::string_view directory,
        const fastgltf::Asset& asset,
        const std::optional<T>& textureInfo,
        const std::string_view defaultName,
        const glm::u8vec4& defaultData
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        if (!textureInfo.has_value())
        {
            const auto id = textureManager.AddTexture
            (
                device,
                allocator,
                stagingPool,
                executor,
                deletionQueue,
                Vk::ImageUpload{
                    .type   = Vk::ImageUploadType::RAW,
                    .flags  = Vk::ImageUploadFlags::None,
                    .source = Vk::ImageUploadRawMemory{
                        .name   = std::string(defaultName),
                        .width  = 1,
                        .height = 1,
                        .format = VK_FORMAT_R8G8B8A8_UNORM,
                        .data   = std::vector(&defaultData[0], &defaultData[3])
                    }
                }
            );

            return Model::TextureInfo
            {
                .id         = id,
                .uvMapIndex = 0
            };
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

        const auto& texture = asset.textures[textureInfo->textureIndex];

        usize imageIndex = 0;
        auto  type       = Vk::ImageUploadType::KTX2;
        auto  flags      = Vk::ImageUploadFlags::None;

        if (texture.basisuImageIndex.has_value())
        {
            imageIndex = texture.basisuImageIndex.value();
            type       = Vk::ImageUploadType::KTX2;
            flags      = Vk::ImageUploadFlags::None;
        }
        else if (texture.imageIndex.has_value())
        {
            imageIndex = texture.imageIndex.value();
            type       = Vk::ImageUploadType::SDR;
            flags      = Vk::ImageUploadFlags::Mipmaps;
        }
        else
        {
            Logger::Error("Image index not found! [TextureIndex={}]\n", textureInfo->textureIndex);
        }

        const auto& image = asset.images[imageIndex];

        const auto id = std::visit(Util::Visitor{
            [&] (ENGINE_UNUSED const auto& argument) -> Vk::TextureID
            {
                Logger::Error
                (
                    "Unsupported source! [TextureIndex={}] [ImageIndex={}]\n",
                    textureInfo->textureIndex,
                    imageIndex
                );

                return {};
            },
            [&] (const fastgltf::sources::URI& filePath) -> Vk::TextureID
            {
                if (filePath.fileByteOffset != 0)
                {
                    Logger::Error
                    (
                        "Unsupported file byte offset! [TextureIndex={}] [FileByteOffset={}]\n",
                        textureInfo->textureIndex,
                        filePath.fileByteOffset
                    );
                }

                if (!filePath.uri.isLocalPath())
                {
                    Logger::Error
                    (
                        "Only local paths are supported! [TextureIndex={}] [UriPath={}]\n",
                        textureInfo->textureIndex,
                        filePath.uri.c_str()
                    );
                }

                return textureManager.AddTexture
                (
                    device,
                    allocator,
                    stagingPool,
                    executor,
                    deletionQueue,
                    Vk::ImageUpload{
                        .type   = type,
                        .flags  = flags,
                        .source = Vk::ImageUploadFile{
                            .path = fmt::format("{}{}{}", directory.data(), "/", filePath.uri.c_str())
                        }
                    }
                );
            },
            [&] (const fastgltf::sources::Array& array) -> Vk::TextureID
            {
                const auto* arrayBegin = reinterpret_cast<const u8*>(array.bytes.data());
                const auto* arrayEnd   = reinterpret_cast<const u8*>(array.bytes.data() + array.bytes.size());

                return textureManager.AddTexture
                (
                    device,
                    allocator,
                    stagingPool,
                    executor,
                    deletionQueue,
                    Vk::ImageUpload{
                        .type   = type,
                        .flags  = flags,
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

                return std::visit(Util::Visitor{
                    // Because we specify LoadExternalBuffers, all buffers are already loaded into a vector.
                    [&] (ENGINE_UNUSED const auto& argument) -> Vk::TextureID
                    {
                        Logger::Error
                        (
                            "Unsupported buffer source! [TextureIndex={}] [ImageIndex={}]\n",
                            textureInfo->textureIndex,
                            imageIndex
                        );

                        return {};
                    },
                    [&] (const fastgltf::sources::Array& array) -> Vk::TextureID
                    {
                        const auto* arrayBegin = reinterpret_cast<const u8*>(array.bytes.data() + bufferView.byteOffset);
                        const auto* arrayEnd   = reinterpret_cast<const u8*>(array.bytes.data() + bufferView.byteOffset + bufferView.byteLength);

                        return textureManager.AddTexture
                        (
                            device,
                            allocator,
                            stagingPool,
                            executor,
                            deletionQueue,
                            Vk::ImageUpload{
                                .type   = type,
                                .flags  = flags,
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

        const auto index = glm::clamp<u32>(textureInfo->texCoordIndex, 0, 1);

        return Model::TextureInfo
        {
            .id         = id,
            .uvMapIndex = index
        };
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
}