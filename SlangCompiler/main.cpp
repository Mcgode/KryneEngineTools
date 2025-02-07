/**
 * @file
 * @author Max Godefroy
 * @date 30/11/2024.
 */

#include <iostream>
#include <KryneEngine/Core/Common/Types.hpp>
#include <slang.h>

using namespace KryneEngine;

static void ErrorCallback(const char* _message, void*)
{
    std::cerr << _message << std::endl;
}

static const char* GetStageName(SlangStage _stage) {
    switch (_stage)
    {

    case SLANG_STAGE_NONE:
        return "none";
    case SLANG_STAGE_VERTEX:
        return "vertex";
    case SLANG_STAGE_HULL:
        return "hull";
    case SLANG_STAGE_DOMAIN:
        return "domain";
    case SLANG_STAGE_GEOMETRY:
        return "geometry";
    case SLANG_STAGE_FRAGMENT:
        return "fragment";
    case SLANG_STAGE_COMPUTE:
        return "compute";
    case SLANG_STAGE_RAY_GENERATION:
        return "ray_gen";
    case SLANG_STAGE_INTERSECTION:
        return "intersection";
    case SLANG_STAGE_ANY_HIT:
        return "any_hit";
    case SLANG_STAGE_CLOSEST_HIT:
        return "closest_hit";
    case SLANG_STAGE_MISS:
        return "miss";
    case SLANG_STAGE_CALLABLE:
        return "callable";
    case SLANG_STAGE_MESH:
        return "mesh";
    case SLANG_STAGE_AMPLIFICATION:
        return "amplification";
    }
}

static const char* GetKindName(slang::TypeReflection::Kind _kind) {
    switch (_kind) {
    case slang::TypeReflection::Kind::None:
        return "none";
    case slang::TypeReflection::Kind::Struct:
        return "struct";
    case slang::TypeReflection::Kind::Array:
        return "array";
    case slang::TypeReflection::Kind::Matrix:
        return "matrix";
    case slang::TypeReflection::Kind::Vector:
        return "vector";
    case slang::TypeReflection::Kind::Scalar:
        return "scalar";
    case slang::TypeReflection::Kind::ConstantBuffer:
        return "constant buffer";
    case slang::TypeReflection::Kind::Resource:
        return "resource";
    case slang::TypeReflection::Kind::SamplerState:
        return "samplerState";
    case slang::TypeReflection::Kind::TextureBuffer:
        return "textureBuffer";
    case slang::TypeReflection::Kind::ShaderStorageBuffer:
        return "ssbo";
    case slang::TypeReflection::Kind::ParameterBlock:
        return "paramBlock";
    case slang::TypeReflection::Kind::GenericTypeParameter:
        return "genericTypeParameter";
    case slang::TypeReflection::Kind::Interface:
        return "interface";
    case slang::TypeReflection::Kind::OutputStream:
    case slang::TypeReflection::Kind::Specialized:
    case slang::TypeReflection::Kind::Feedback:
    case slang::TypeReflection::Kind::Pointer:
    case slang::TypeReflection::Kind::DynamicResource:
        return nullptr;
    }
}

std::vector<slang::TypeLayoutReflection*> structTypes;

static eastl::string GetTypeString(slang::TypeLayoutReflection* _type) {
    switch (_type->getKind()) {
    case slang::TypeReflection::Kind::None:
        return "none";
    case slang::TypeReflection::Kind::Struct:
        structTypes.push_back(_type);
        return _type->getName();
    case slang::TypeReflection::Kind::Array:
        return "array";
    case slang::TypeReflection::Kind::Matrix:
        return "matrix";
    case slang::TypeReflection::Kind::Vector:
    {
        eastl::string name;
        name.sprintf("vector<%s, %d>", _type->getType()->getElementType()->getName(), _type->getElementCount());
        return name.c_str();
    }
    case slang::TypeReflection::Kind::Scalar:
        return _type->getName();
    case slang::TypeReflection::Kind::ConstantBuffer:
        return "constant buffer";
    case slang::TypeReflection::Kind::Resource:
        return "resource";
    case slang::TypeReflection::Kind::SamplerState:
        return "samplerState";
    case slang::TypeReflection::Kind::TextureBuffer:
        return "textureBuffer";
    case slang::TypeReflection::Kind::ShaderStorageBuffer:
        return "ssbo";
    case slang::TypeReflection::Kind::ParameterBlock:
    {
        eastl::string name;
        slang::TypeReflection* elementType = _type->getType()->getElementType();
        name.sprintf(
            "ParameterBlock<%s>: set(%d)",
            _type->getType()->getElementType()->getName(),
            _type->getDescriptorSetCount());
        if (elementType->getKind() == slang::TypeReflection::Kind::Struct)
            structTypes.push_back(_type->getElementTypeLayout());
        return name.c_str();
    }
    case slang::TypeReflection::Kind::GenericTypeParameter:
        return "genericTypeParameter";
    case slang::TypeReflection::Kind::Interface:
        return "interface";
    case slang::TypeReflection::Kind::OutputStream:
    case slang::TypeReflection::Kind::Specialized:
    case slang::TypeReflection::Kind::Feedback:
    case slang::TypeReflection::Kind::Pointer:
    case slang::TypeReflection::Kind::DynamicResource:
        return nullptr;
    }
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

    const SlangResult argParseResult = spProcessCommandLineArguments(request, _argv + 1, _argc - 1);

    if (SLANG_FAILED(argParseResult))
    {
        spDestroyCompileRequest(request);
        session->release();
        return 1;
    }

    request->compile();

    slang::ShaderReflection* reflection = slang::ShaderReflection::get(request);

    const u32 entryPointCount = reflection->getEntryPointCount();
    for (u32 i = 0; i < entryPointCount; i++)
    {
        slang::EntryPointReflection* entryPoint = reflection->getEntryPointByIndex(i);
        printf("Entry point %d: '%s'\n", i, entryPoint->getName());

        printf("Shader type: %s\n", GetStageName(entryPoint->getStage()));

        const u32 parameterCount = entryPoint->getParameterCount();
        printf("Parameter count: %d\n", parameterCount);
        for (u32 j = 0; j < parameterCount; j++) {
            slang::VariableLayoutReflection* parameter = entryPoint->getParameterByIndex(j);

            printf(" - %s: %s\n", parameter->getName(), GetTypeString(parameter->getTypeLayout()).c_str());
        }
        printf("\n");
    }

#define FOO "Types:\n");

    printf( FOO
    for (u32 j = 0; j < structTypes.size(); j++) {
        slang::TypeLayoutReflection* type = structTypes[j];
        printf("- %s: %s", type->getName(), GetKindName(type->getKind()));

        if (type->getKind() == slang::TypeReflection::Kind::Struct) {
            printf("\n\t{\n");

            for (u32 i = 0; i < type->getFieldCount(); i++) {
                slang::VariableLayoutReflection* field = type->getFieldByIndex(i);
                printf("\t\t%s: %s", field->getName(), GetTypeString(field->getTypeLayout()).c_str());
                if (field->getSemanticName() != nullptr)
                {
                    printf(": %s%zu", field->getSemanticName(), field->getSemanticIndex());
                }

                printf("; binding: (%u, %u)", field->getBindingIndex(), field->getBindingSpace(SLANG_PARAMETER_CATEGORY_GENERIC));

                printf(";\n");
            }

            printf("\t}");
        }

        printf("\n");
    }

    spDestroyCompileRequest(request);

    session->release();

    return 0;
}