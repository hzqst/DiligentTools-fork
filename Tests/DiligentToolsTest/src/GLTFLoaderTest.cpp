/*
 *  Copyright 2026 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include "GLTFLoader.hpp"
#include "../../../ThirdParty/tinygltf/tiny_gltf.h"

#include "gtest/gtest.h"

#include "Image.h"

#include <string>
#include <vector>

namespace Diligent
{

namespace GLTF
{

namespace MSFTTextureDDS
{

int GetSource(const tinygltf::Texture& gltf_tex,
              const tinygltf::Model&   gltf_model);

} // namespace MSFTTextureDDS

} // namespace GLTF

} // namespace Diligent

using namespace Diligent;

namespace
{

tinygltf::Texture CreateDDSTexture(int Source)
{
    tinygltf::Value::Object Extension;
    Extension.emplace("source", tinygltf::Value{Source});

    tinygltf::Texture Texture;
    Texture.source = 0;
    Texture.extensions.emplace("MSFT_texture_dds", tinygltf::Value{std::move(Extension)});
    return Texture;
}

std::string MakeGltfWithMaterials(const char* Materials)
{
    return std::string{R"({"asset":{"version":"2.0"},"materials":)"} + Materials + "}";
}

GLTF::Model LoadModelFromString(const std::string& Gltf)
{
    GLTF::ModelCreateInfo CI;
    CI.FileName = "memory.gltf";
    CI.FileExistsCallback =
        [](const char*) //
    {
        return true;
    };
    CI.ReadWholeFileCallback =
        [Gltf](const char*, std::vector<unsigned char>& Data, std::string&) //
    {
        Data.assign(Gltf.begin(), Gltf.end());
        return true;
    };

    return GLTF::Model{nullptr, nullptr, CI};
}

TEST(Tools_GLTFLoader, MSFTTextureDDSUsesRawDDSImageData)
{
    tinygltf::Image DDSImage;
    DDSImage.uri        = "texture.dds";
    DDSImage.pixel_type = IMAGE_FILE_FORMAT_DDS;
    DDSImage.image      = {'D', 'D', 'S', ' '};

    tinygltf::Model Model;
    Model.images.emplace_back(tinygltf::Image{});
    Model.images.emplace_back(std::move(DDSImage));

    const tinygltf::Texture Texture = CreateDDSTexture(1);

    EXPECT_EQ(GLTF::MSFTTextureDDS::GetSource(Texture, Model), 1);
}

TEST(Tools_GLTFLoader, MSFTTextureDDSRejectsLoadedImageMetadata)
{
    tinygltf::Image DDSImage;
    DDSImage.uri        = "cached.dds";
    DDSImage.width      = 4;
    DDSImage.height     = 4;
    DDSImage.component  = 4;
    DDSImage.bits       = 8;
    DDSImage.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;

    tinygltf::Model Model;
    Model.images.emplace_back(tinygltf::Image{});
    Model.images.emplace_back(std::move(DDSImage));

    const tinygltf::Texture Texture = CreateDDSTexture(1);

    EXPECT_EQ(GLTF::MSFTTextureDDS::GetSource(Texture, Model), -1);
}

TEST(Tools_GLTFLoader, MSFTTextureDDSRejectsUriOnlyImage)
{
    tinygltf::Image DDSImage;
    DDSImage.uri = "encoded%20texture.dds";

    tinygltf::Model Model;
    Model.images.emplace_back(tinygltf::Image{});
    Model.images.emplace_back(std::move(DDSImage));

    const tinygltf::Texture Texture = CreateDDSTexture(1);

    EXPECT_EQ(GLTF::MSFTTextureDDS::GetSource(Texture, Model), -1);
}

TEST(Tools_GLTFLoader, TransmissionKeepsDefaultOpaqueAlphaMode)
{
    GLTF::Model Model = LoadModelFromString(MakeGltfWithMaterials(R"([
        {
            "extensions": {
                "KHR_materials_transmission": {
                    "transmissionFactor": 0.75
                }
            }
        }
    ])"));

    ASSERT_EQ(Model.Materials.size(), size_t{1});
    const GLTF::Material& Mat = Model.Materials[0];
    ASSERT_NE(Mat.Transmission, nullptr);
    EXPECT_EQ(Mat.Attribs.AlphaMode, GLTF::Material::ALPHA_MODE_OPAQUE);
    EXPECT_FLOAT_EQ(Mat.Transmission->Factor, 0.75f);
}

TEST(Tools_GLTFLoader, TransmissionPreservesAuthoredAlphaMode)
{
    GLTF::Model Model = LoadModelFromString(MakeGltfWithMaterials(R"([
        {
            "alphaMode": "OPAQUE",
            "extensions": {
                "KHR_materials_transmission": {
                    "transmissionFactor": 0.25
                }
            }
        },
        {
            "alphaMode": "MASK",
            "alphaCutoff": 0.37,
            "extensions": {
                "KHR_materials_transmission": {
                    "transmissionFactor": 0.5
                }
            }
        },
        {
            "alphaMode": "BLEND",
            "extensions": {
                "KHR_materials_transmission": {
                    "transmissionFactor": 0.75
                }
            }
        }
    ])"));

    ASSERT_EQ(Model.Materials.size(), size_t{3});
    EXPECT_EQ(Model.Materials[0].Attribs.AlphaMode, GLTF::Material::ALPHA_MODE_OPAQUE);
    EXPECT_EQ(Model.Materials[1].Attribs.AlphaMode, GLTF::Material::ALPHA_MODE_MASK);
    EXPECT_FLOAT_EQ(Model.Materials[1].Attribs.AlphaCutoff, 0.37f);
    EXPECT_EQ(Model.Materials[2].Attribs.AlphaMode, GLTF::Material::ALPHA_MODE_BLEND);

    for (const GLTF::Material& Mat : Model.Materials)
        ASSERT_NE(Mat.Transmission, nullptr);
}

TEST(Tools_GLTFLoader, KHRMaterialsIORAppliesToTransmission)
{
    GLTF::Model Model = LoadModelFromString(MakeGltfWithMaterials(R"([
        {
            "extensions": {
                "KHR_materials_transmission": {
                    "transmissionFactor": 0.5
                },
                "KHR_materials_ior": {
                    "ior": 1.33
                }
            }
        },
        {
            "extensions": {
                "KHR_materials_ior": {
                    "ior": 2.0
                }
            }
        }
    ])"));

    ASSERT_EQ(Model.Materials.size(), size_t{2});
    ASSERT_NE(Model.Materials[0].Transmission, nullptr);
    EXPECT_FLOAT_EQ(Model.Materials[0].Transmission->IOR, 1.33f);
    EXPECT_EQ(Model.Materials[1].Transmission, nullptr);
}

} // namespace
