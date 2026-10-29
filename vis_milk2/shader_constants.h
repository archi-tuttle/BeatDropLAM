#ifndef BEATDROP_SHADER_CONSTANTS_H
#define BEATDROP_SHADER_CONSTANTS_H

#include <d3d9.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <vector>

struct ShaderConstant;

typedef ShaderConstant* ShaderConstantHandle;

enum ShaderConstantRegisterSet
{
    SHADER_CONSTANT_FLOAT4 = 2,
    SHADER_CONSTANT_SAMPLER = 3
};

enum ShaderConstantClass
{
    SHADER_CONSTANT_VECTOR = 1,
    SHADER_CONSTANT_MATRIX_COLUMNS = 3
};

struct ShaderConstantDesc
{
    const char* Name;
    UINT RegisterSet;
    UINT RegisterIndex;
    UINT RegisterCount;
    UINT Class;
};

struct ShaderConstantTableDesc
{
    UINT Constants;
};

struct ShaderConstant
{
    ShaderConstantDesc desc;
    std::string name;
};

class ShaderConstantTable
{
public:
    explicit ShaderConstantTable(bool vertexShader);

    HRESULT Initialize(const void* bytecode, SIZE_T bytecodeSize);
    UINT AddRef();
    UINT Release();
    HRESULT GetDesc(ShaderConstantTableDesc* desc) const;
    ShaderConstantHandle GetConstant(ShaderConstantHandle parent, UINT index);
    HRESULT GetConstantDesc(ShaderConstantHandle handle, ShaderConstantDesc* desc, UINT* count) const;
    HRESULT SetVector(IDirect3DDevice9* device, ShaderConstantHandle handle, const DirectX::XMFLOAT4* value) const;
    HRESULT SetMatrix(IDirect3DDevice9* device, ShaderConstantHandle handle, const D3DMATRIX* value) const;

private:
    LONG m_refCount;
    bool m_vertexShader;
    std::vector<ShaderConstant> m_constants;
};

HRESULT CreateShaderConstantTable(bool vertexShader, const void* bytecode, SIZE_T bytecodeSize,
    ShaderConstantTable** table);

#endif
