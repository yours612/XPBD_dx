#include "d3dUtil.h"

using Microsoft::WRL::ComPtr;

// ����Ĭ�϶�
ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer) {
    ComPtr<ID3D12Resource> defaultBuffer;

    // ����ʵ�ʵ�Ĭ�ϻ�������Դ
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),// ָ��������ΪĬ�϶�
        D3D12_HEAP_FLAG_NONE,// ��ָ��flag
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),// �ø�����������byte���ٹ���ṹ��
        D3D12_RESOURCE_STATE_COMMON,// Ĭ�϶�Ϊ���մ洢���ݵĵط���������ʱ��ʼ��Ϊcommon״̬
        nullptr,// ��ָ���Ż�ֵ
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())));// ָ����ԴCOM ID

    // Ϊ�˽�CPU���ڴ��е����ݸ��Ƶ�Ĭ�ϻ����������ǻ���Ҫ����һ�������н�λ�õ��ϴ���
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,// �ϴ��������Դ��Ҫ���Ƹ�Ĭ�϶ѣ������ǿɶ�״̬
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    // �������Ƶ�Ĭ�ϻ�����������
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    // �����ݸ��Ƶ�Ĭ�ϻ�������Դ������
        // �Ƚ�Ĭ�϶�״̬��common�ĳ�copy_dest״̬
    cmdList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST));
        // �����ݴ�CPU���Ƶ��ϴ��ѣ����Ű��ϴ����ڵ����ݸ��Ƶ�Ĭ�϶���
    UpdateSubresources<1>(// 1������Դ
        cmdList,
        defaultBuffer.Get(),
        uploadBuffer.Get(),
        0,// �м���Դ��ƫ����(byte)
        0,// ��Դ�е�һ������Դ������
        1,// ��Դ������Դ��
        &subResourceData);// �ٽ�Ĭ�϶�״̬��copy_dest��Ϊread
    cmdList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_GENERIC_READ));

    return defaultBuffer;
}

// ����Shader
ComPtr<ID3DBlob> d3dUtil::CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target) {
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(filename.c_str(),// hlslԴ�ļ���
        defines,// �߼�ѡ�� ָ��Ϊ��ָ��
        D3D_COMPILE_STANDARD_FILE_INCLUDE,// �߼�ѡ�� ����ָ��Ϊ��ָ��
        entrypoint.c_str(),// ��ɫ������ڵ㺯��
        target.c_str(),// ָ��������ɫ�����ͺͰ汾���ַ���
        compileFlags,// ָ������ɫ���ϴ���Ӧ����α���ı�ָ
        0,// �߼�ѡ��
        &byteCode,// ����õ��ֽ���
        &errors);// ������Ϣ

    if (errors != nullptr)
        OutputDebugStringA((char*)errors->GetBufferPointer());

    ThrowIfFailed(hr);

    return byteCode;
}