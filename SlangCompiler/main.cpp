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
                        printf("\t\t- `%s`: binding %u\n", field->getName(), field->getBindingIndex());
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