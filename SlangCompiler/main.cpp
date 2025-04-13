/**
 * @file
 * @author Max Godefroy
 * @date 30/11/2024.
 */

#include <cstring>
#include <iostream>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Core/Graphics/Common/ShaderPipeline.hpp>
#include <slang.h>

using namespace KryneEngine;

static void ErrorCallback(const char* _message, void*)
{
    std::cerr << _message << std::endl;
}

enum class TargetApi
{
    Vulkan,
    DirectX12,
    Metal,
};

struct ArgumentsInformation
{
    TargetApi m_targetApi = TargetApi::Vulkan;
};

ArgumentsInformation ParseCommandLineArguments(int _argc, const char** _argv, eastl::vector<const char*>& _args)
{
    ArgumentsInformation argumentsInformation {};

    for (int i = 1; i < _argc; i++)
    {
        if (strcmp(_argv[i], "-target") == 0)
        {
            _args.push_back(_argv[i]);
            i++;
            const char* target = _argv[i];
            if (strcmp(target, "spirv") == 0 || strcmp(target, "glsl") == 0)
            {
                argumentsInformation.m_targetApi = TargetApi::Vulkan;
            }
            else if (strcmp(target, "hlsl") == 0 || strcmp(target, "dxil") == 0)
            {
                argumentsInformation.m_targetApi = TargetApi::DirectX12;
            }
            else if (strcmp(target, "metal") == 0 || strcmp(target, "metallib") == 0)
            {
                argumentsInformation.m_targetApi = TargetApi::Metal;
            }
            else
            {
                ErrorCallback("Unsupported target", nullptr);
            }
        }
        _args.push_back(_argv[i]);
    }

    return argumentsInformation;
}

eastl::pair<DescriptorBindingDesc::Type, TextureTypes> ParseDescriptorBindingType(slang::VariableLayoutReflection* _reflection)
{
    slang::TypeLayoutReflection* typeLayout = _reflection->getTypeLayout();

    if (typeLayout->getKind() == slang::TypeReflection::Kind::ConstantBuffer)
    {
        return { DescriptorBindingDesc::Type::ConstantBuffer, TextureTypes::Single2D };
    }
    else if (typeLayout->getKind() == slang::TypeReflection::Kind::SamplerState)
    {
        return { DescriptorBindingDesc::Type::Sampler, TextureTypes::Single2D };
    }

    const u32 resourceShape = typeLayout->getResourceShape();
    const u32 baseShape = resourceShape & SLANG_RESOURCE_BASE_SHAPE_MASK;
    const u32 shapeFlags = resourceShape & SLANG_RESOURCE_EXT_SHAPE_MASK;
    const SlangResourceAccess access = typeLayout->getResourceAccess();

    DescriptorBindingDesc::Type bindingType = DescriptorBindingDesc::Type::SampledTexture;

    switch (baseShape)
    {
    case SLANG_TEXTURE_1D:
    case SLANG_TEXTURE_2D:
    case SLANG_TEXTURE_3D:
    case SLANG_TEXTURE_CUBE:
        {
            switch (access)
            {
            case SLANG_RESOURCE_ACCESS_READ:
                bindingType = DescriptorBindingDesc::Type::SampledTexture;
                break;
            case SLANG_RESOURCE_ACCESS_READ_WRITE:
            case SLANG_RESOURCE_ACCESS_WRITE:
                bindingType = DescriptorBindingDesc::Type::StorageReadWriteTexture;
                break;
            default:
                KE_ERROR("Unsupported access");
                break;
            }
            break;
        }
    case SLANG_STRUCTURED_BUFFER:
    case SLANG_BYTE_ADDRESS_BUFFER:
        {
            switch (access)
            {
            case SLANG_RESOURCE_ACCESS_READ:
                bindingType = DescriptorBindingDesc::Type::StorageReadOnlyBuffer;
                break;
            case SLANG_RESOURCE_ACCESS_READ_WRITE:
            case SLANG_RESOURCE_ACCESS_WRITE:
                bindingType = DescriptorBindingDesc::Type::StorageReadWriteBuffer;
                break;
            default:
                KE_ERROR("Unsupported access");
                break;
            }
            break;
        }
    default:
        break;
    }

    TextureTypes textureType = TextureTypes::Single2D;
    if (bindingType < DescriptorBindingDesc::Type::StorageReadOnlyBuffer)
    {
        switch (baseShape)
        {
            case SLANG_TEXTURE_1D:
                textureType = (shapeFlags & SLANG_TEXTURE_ARRAY_FLAG) ? TextureTypes::Array1D : TextureTypes::Single1D;
                break;
            case SLANG_TEXTURE_2D:
                textureType = (shapeFlags & SLANG_TEXTURE_ARRAY_FLAG) ? TextureTypes::Array2D : TextureTypes::Single2D;
                break;
            case SLANG_TEXTURE_3D:
                textureType = TextureTypes::Single3D;
                break;
            case SLANG_TEXTURE_CUBE:
                textureType = (shapeFlags & SLANG_TEXTURE_ARRAY_FLAG) ? TextureTypes::ArrayCube : TextureTypes::SingleCube;
                break;
            default:
                KE_ERROR("Unreachable");
                break;
        }
    }

    return { bindingType, textureType };
}

int main(int _argc, const char** _argv)
{
    SlangSession* session = nullptr;
    const SlangResult sessionInitResult = slang_createGlobalSession(SLANG_API_VERSION, &session);

    if (SLANG_FAILED(sessionInitResult))
    {
        return 1;
    }

    SlangCompileRequest* request = spCreateCompileRequest(session);
    spSetDiagnosticCallback(request, ErrorCallback, nullptr);
    request->setCommandLineCompilerMode();

    eastl::vector<const char*> args;
    args.reserve(_argc - 1);
    ArgumentsInformation argInfo = ParseCommandLineArguments(_argc, _argv, args);

    const SlangResult argParseResult = spProcessCommandLineArguments(request, args.data(), (s32)args.size());
    if (SLANG_FAILED(argParseResult))
    {
        spDestroyCompileRequest(request);
        session->release();
        return 1;
    }

    request->compile();

    slang::ShaderReflection* reflection = slang::ShaderReflection::get(request);

    eastl::vector<slang::VariableLayoutReflection*> globalParameterBlocks;
    eastl::vector<slang::VariableLayoutReflection*> globalPushConstants;

    globalParameterBlocks.reserve(reflection->getParameterCount());

    for (u32 i = 0; i < reflection->getParameterCount(); i++)
    {
        slang::VariableLayoutReflection* parameter = reflection->getParameterByIndex(i);
        slang::TypeLayoutReflection* type = parameter->getTypeLayout();
        if (type->getKind() == slang::TypeReflection::Kind::ParameterBlock)
        {
            globalParameterBlocks.push_back(parameter);
        }
        else if (parameter->getCategory() == slang::PushConstantBuffer || type->getKind() == slang::TypeReflection::Kind::ConstantBuffer)
        {
            globalPushConstants.push_back(parameter);
        }
    }

    struct EntryPointReflection
    {
        const char* m_name = nullptr;
        eastl::vector<slang::VariableLayoutReflection*> m_descriptorSets;
        eastl::vector<slang::VariableLayoutReflection*> m_pushConstants;
    };
    eastl::vector<EntryPointReflection> entryPoints;
    entryPoints.reserve(reflection->getEntryPointCount());

    for (u32 i = 0; i < reflection->getEntryPointCount(); i++)
    {
        slang::EntryPointReflection* entryPoint = reflection->getEntryPointByIndex(i);
        EntryPointReflection& entryPointReflection = entryPoints.emplace_back();
        entryPointReflection.m_name = entryPoint->getName();

        entryPointReflection.m_descriptorSets = globalParameterBlocks;
        entryPointReflection.m_pushConstants = globalPushConstants;

        for (u32 j = 0; j < entryPoint->getParameterCount(); j++)
        {
            slang::VariableLayoutReflection* parameter = entryPoint->getParameterByIndex(j);
            slang::TypeLayoutReflection* type = parameter->getTypeLayout();
            if (type->getKind() == slang::TypeReflection::Kind::ParameterBlock)
            {
                entryPointReflection.m_descriptorSets.push_back(parameter);
            }
            else if (parameter->getCategory() == slang::Uniform || parameter->getCategory() == slang::PushConstantBuffer)
            {
                entryPointReflection.m_pushConstants.push_back(parameter);
            }
        }

        if (entryPointReflection.m_pushConstants.size() > 1)
        {
            ErrorCallback("Multiple push constants in entry point, only one push constant is supported.", nullptr);
            return 1;
        }
    }

    printf("Entry points:\n");
    for (const auto& entryPoint : entryPoints)
    {
        printf("- %s:\n", entryPoint.m_name);
        if (entryPoint.m_descriptorSets.empty())
        {
            printf("\tNo descriptor sets\n");
        }
        else
        {
            printf("\tDescriptor sets:\n");
            for (const auto& descriptorSet : entryPoint.m_descriptorSets)
            {
                printf("\t - `%s`: set %d\n", descriptorSet->getName(), descriptorSet->getBindingIndex());

                slang::TypeLayoutReflection* elementType = descriptorSet->getTypeLayout()->getElementTypeLayout();

                if (elementType->getKind() == slang::TypeReflection::Kind::Struct)
                {
                    for (u32 i = 0; i < elementType->getFieldCount(); i++)
                    {
                        slang::VariableLayoutReflection* field = elementType->getFieldByIndex(i);

                        const auto [bindingType, textureType] = ParseDescriptorBindingType(field);
                        const char* bindingTypeString = "";
                        const char* textureTypeString = "";

                        switch (bindingType)
                        {
                        case DescriptorBindingDesc::Type::Sampler:
                            bindingTypeString = "Sampler";
                            break;
                        case DescriptorBindingDesc::Type::SampledTexture:
                            bindingTypeString = "Sampled texture";
                            break;
                        case DescriptorBindingDesc::Type::StorageReadOnlyTexture:
                            bindingTypeString = "Read-only texture";
                            break;
                        case DescriptorBindingDesc::Type::StorageReadWriteTexture:
                            bindingTypeString = "Read/write buffer";
                            break;
                        case DescriptorBindingDesc::Type::ConstantBuffer:
                            bindingTypeString = "Constant buffer";
                            break;
                        case DescriptorBindingDesc::Type::StorageReadOnlyBuffer:
                            bindingTypeString = "Read-only buffer";
                            break;
                        case DescriptorBindingDesc::Type::StorageReadWriteBuffer:
                            bindingTypeString = "Read/write buffer";
                            break;
                        }

                        if (bindingType >= DescriptorBindingDesc::Type::SampledTexture && bindingType <= DescriptorBindingDesc::Type::StorageReadWriteTexture)
                        {
                            switch (textureType)
                            {
                            case TextureTypes::Single1D:
                                textureTypeString = " (1D)";
                                break;
                            case TextureTypes::Single2D:
                                textureTypeString = " (2D)";
                                break;
                            case TextureTypes::Single3D:
                                textureTypeString = " (3D)";
                                break;
                            case TextureTypes::Array1D:
                                textureTypeString = " (1D array)";
                                break;
                            case TextureTypes::Array2D:
                                textureTypeString = " (2D array)";
                                break;
                            case TextureTypes::SingleCube:
                                textureTypeString = " (cube)";
                                break;
                            case TextureTypes::ArrayCube:
                                textureTypeString = " (cube array)";
                                break;
                            }
                        }

                        printf("\t\t- `%s`: %s%s, binding %u\n", field->getName(), bindingTypeString, textureTypeString, field->getBindingIndex());
                    }
                }
            }
        }

        if (entryPoint.m_pushConstants.empty())
        {
            printf("\tNo push constants\n");
        }
        else
        {
            printf("\tPush constants: ");
            for (const auto& pushConstant : entryPoint.m_pushConstants)
            {
                const size_t sizeInBytes = pushConstant->getTypeLayout()->getSize(pushConstant->getCategory());
                printf("`%s` (size %zu)\n", pushConstant->getName(), sizeInBytes);
            }
        }
    }

    spDestroyCompileRequest(request);

    session->release();

    return 0;
}