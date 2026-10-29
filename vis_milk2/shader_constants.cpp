#include "shader_constants.h"
#include <cstring>
#include <new>

namespace
{
const DWORD ctabFourCc = MAKEFOURCC('C', 'T', 'A', 'B');
const DWORD d3dCommentOpcode = 0xfffe;
const DWORD d3dEndOpcode = 0xffff;

#pragma pack(push, 4)
struct CtabHeader
{
    DWORD size;
    DWORD creator;
    DWORD version;
    DWORD constants;
    DWORD constantInfo;
    DWORD flags;
    DWORD target;
};

struct CtabConstantInfo
{
    DWORD name;
    WORD registerSet;
    WORD registerIndex;
    WORD registerCount;
    WORD reserved;
    DWORD typeInfo;
    DWORD defaultValue;
};

struct CtabTypeInfo
{
    WORD parameterClass;
    WORD parameterType;
    WORD rows;
    WORD columns;
    WORD elements;
    WORD structMembers;
    DWORD structMemberInfo;
};
#pragma pack(pop)

bool ReadConstantTable(const BYTE* shader, SIZE_T shaderSize, const BYTE** tableData, SIZE_T* tableSize)
{
    if (!shader || shaderSize < sizeof(DWORD) || shaderSize % sizeof(DWORD) != 0)
        return false;

    const DWORD* tokens = reinterpret_cast<const DWORD*>(shader);
    const SIZE_T tokenCount = shaderSize / sizeof(DWORD);
    for (SIZE_T i = 1; i < tokenCount;)
    {
        const DWORD token = tokens[i];
        const DWORD opcode = token & 0xffff;
        if (opcode == d3dEndOpcode)
            break;
        if (opcode != d3dCommentOpcode)
        {
            ++i;
            continue;
        }

        const SIZE_T commentWords = (token >> 16) & 0x7fff;
        if (commentWords > tokenCount - i - 1)
            return false;
        const BYTE* comment = reinterpret_cast<const BYTE*>(&tokens[i + 1]);
        const SIZE_T commentBytes = commentWords * sizeof(DWORD);
        if (commentBytes >= sizeof(DWORD) && *reinterpret_cast<const DWORD*>(comment) == ctabFourCc)
        {
            *tableData = comment + sizeof(DWORD);
            *tableSize = commentBytes - sizeof(DWORD);
            return true;
        }
        i += commentWords + 1;
    }
    return false;
}

bool IsValidRange(SIZE_T size, DWORD offset, SIZE_T bytes)
{
    return offset <= size && bytes <= size - offset;
}
}

ShaderConstantTable::ShaderConstantTable(bool vertexShader)
    : m_refCount(1), m_vertexShader(vertexShader)
{
}

HRESULT ShaderConstantTable::Initialize(const void* bytecode, SIZE_T bytecodeSize)
{
    const BYTE* tableData = NULL;
    SIZE_T tableSize = 0;
    if (!ReadConstantTable(static_cast<const BYTE*>(bytecode), bytecodeSize, &tableData, &tableSize) ||
        tableSize < sizeof(CtabHeader))
        return E_FAIL;

    const CtabHeader* header = reinterpret_cast<const CtabHeader*>(tableData);
    if (header->size < sizeof(CtabHeader) || header->size > tableSize ||
        !IsValidRange(tableSize, header->constantInfo,
            static_cast<SIZE_T>(header->constants) * sizeof(CtabConstantInfo)))
        return E_FAIL;

    try
    {
        m_constants.clear();
        m_constants.reserve(header->constants);
        for (DWORD i = 0; i < header->constants; ++i)
        {
            const CtabConstantInfo* source = reinterpret_cast<const CtabConstantInfo*>(
                tableData + header->constantInfo + static_cast<SIZE_T>(i) * sizeof(CtabConstantInfo));
            if (!IsValidRange(tableSize, source->typeInfo, sizeof(CtabTypeInfo)) ||
                source->name >= tableSize)
                return E_FAIL;

            const CtabTypeInfo* type = reinterpret_cast<const CtabTypeInfo*>(tableData + source->typeInfo);
            const char* name = reinterpret_cast<const char*>(tableData + source->name);
            const SIZE_T remaining = tableSize - source->name;
            if (!memchr(name, 0, remaining))
                return E_FAIL;

            ShaderConstant constant = {};
            constant.name.assign(name);
            constant.desc.RegisterSet = source->registerSet;
            constant.desc.RegisterIndex = source->registerIndex;
            constant.desc.RegisterCount = source->registerCount;
            constant.desc.Class = type->parameterClass;
            m_constants.push_back(constant);
        }

        for (size_t i = 0; i < m_constants.size(); ++i)
            m_constants[i].desc.Name = m_constants[i].name.c_str();
    }
    catch (const std::bad_alloc&)
    {
        m_constants.clear();
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

UINT ShaderConstantTable::AddRef()
{
    return static_cast<UINT>(InterlockedIncrement(&m_refCount));
}

UINT ShaderConstantTable::Release()
{
    const LONG count = InterlockedDecrement(&m_refCount);
    if (!count)
        delete this;
    return static_cast<UINT>(count);
}

HRESULT ShaderConstantTable::GetDesc(ShaderConstantTableDesc* desc) const
{
    if (!desc)
        return E_POINTER;
    desc->Constants = static_cast<UINT>(m_constants.size());
    return S_OK;
}

ShaderConstantHandle ShaderConstantTable::GetConstant(ShaderConstantHandle parent, UINT index)
{
    if (parent || index >= m_constants.size())
        return NULL;
    return &m_constants[index];
}

HRESULT ShaderConstantTable::GetConstantDesc(ShaderConstantHandle handle, ShaderConstantDesc* desc, UINT* count) const
{
    if (!handle || !desc || !count || *count == 0)
        return E_INVALIDARG;
    *desc = handle->desc;
    *count = 1;
    return S_OK;
}

HRESULT ShaderConstantTable::SetVector(IDirect3DDevice9* device, ShaderConstantHandle handle, const DirectX::XMFLOAT4* value) const
{
    if (!device || !handle || !value || handle->desc.RegisterSet != SHADER_CONSTANT_FLOAT4)
        return E_INVALIDARG;
    const float* data = &value->x;
    return m_vertexShader
        ? device->SetVertexShaderConstantF(handle->desc.RegisterIndex, data, 1)
        : device->SetPixelShaderConstantF(handle->desc.RegisterIndex, data, 1);
}

HRESULT ShaderConstantTable::SetMatrix(IDirect3DDevice9* device, ShaderConstantHandle handle, const D3DMATRIX* value) const
{
    if (!device || !handle || !value || handle->desc.RegisterSet != SHADER_CONSTANT_FLOAT4)
        return E_INVALIDARG;

    DirectX::XMFLOAT4X4 transposed;
    DirectX::XMStoreFloat4x4(&transposed, DirectX::XMMatrixTranspose(
        DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(value))));
    const UINT registers = min(handle->desc.RegisterCount, 4U);
    return m_vertexShader
        ? device->SetVertexShaderConstantF(handle->desc.RegisterIndex, &transposed._11, registers)
        : device->SetPixelShaderConstantF(handle->desc.RegisterIndex, &transposed._11, registers);
}

HRESULT CreateShaderConstantTable(bool vertexShader, const void* bytecode, SIZE_T bytecodeSize,
    ShaderConstantTable** table)
{
    if (!table)
        return E_POINTER;
    *table = new(std::nothrow) ShaderConstantTable(vertexShader);
    if (!*table)
        return E_OUTOFMEMORY;
    const HRESULT result = (*table)->Initialize(bytecode, bytecodeSize);
    if (FAILED(result))
    {
        (*table)->Release();
        *table = NULL;
    }
    return result;
}
