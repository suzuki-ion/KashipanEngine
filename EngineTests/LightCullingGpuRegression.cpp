// D3D12 実機から結果を読み戻し、旧版と新版で Forward+ のライト順序とタイルごとの上限が一致することを確認する。
// 自動生成されるネイティブ回帰テストのランナーからのみ取り込む。
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <filesystem>
#include <fstream>
#include "Graphics/Renderer/RendererInternal.h"
namespace {
using Microsoft::WRL::ComPtr;
void GpuCheck(HRESULT hr) { if(FAILED(hr)) throw std::runtime_error("D3D12 light culling regression HRESULT="+std::to_string(hr)); }
std::vector<char> ShaderBytes(const std::filesystem::path &p) {
    std::ifstream f(p,std::ios::binary); if(!f) throw std::runtime_error("Missing test shader: "+p.string());
    return {std::istreambuf_iterator<char>(f),{}};
}
}
void RunLightCullingGpuRegression(const std::filesystem::path &root) {
    using namespace KashipanEngine::RendererInternal;
    ComPtr<IDXGIFactory6> factory;GpuCheck(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    ComPtr<IDXGIAdapter1> adapter;GpuCheck(factory->EnumAdapterByGpuPreference(0,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter)));
    ComPtr<ID3D12Device> device;GpuCheck(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
    D3D12_ROOT_PARAMETER params[10]{};
    for(unsigned i=0;i<10;++i){params[i].ParameterType=i<2?D3D12_ROOT_PARAMETER_TYPE_CBV:i<9?D3D12_ROOT_PARAMETER_TYPE_SRV:D3D12_ROOT_PARAMETER_TYPE_UAV;params[i].Descriptor.ShaderRegister=i<2?i:i<9?i-2:0;}
    D3D12_ROOT_SIGNATURE_DESC rd{10,params,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> blob,error;
    GpuCheck(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error));ComPtr<ID3D12RootSignature> signature;
    GpuCheck(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&signature)));
    ComPtr<ID3D12PipelineState> pipelines[2];
    for(int i=0;i<2;++i){auto bytes=ShaderBytes(root/"Generated"/(i?"LightCullingOptimized.dxil":"LightCullingReference.dxil"));D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=signature.Get();pd.CS={bytes.data(),bytes.size()};GpuCheck(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipelines[i])));}
    D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue> queue;GpuCheck(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)));
    ComPtr<ID3D12CommandAllocator> allocator;GpuCheck(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    ComPtr<ID3D12GraphicsCommandList> list;GpuCheck(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));GpuCheck(list->Close());
    ComPtr<ID3D12Fence> fence;GpuCheck(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));UINT64 serial=0;
    const auto buffer=[&](size_t size,D3D12_HEAP_TYPE heap,D3D12_RESOURCE_STATES state,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){
        D3D12_HEAP_PROPERTIES hp{};hp.Type=heap;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=std::max<size_t>(256,size);d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;ComPtr<ID3D12Resource> r;GpuCheck(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)));return r;};
    const auto upload=[&](const void *data,size_t size){auto r=buffer(size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void *p=nullptr;GpuCheck(r->Map(0,nullptr,&p));if(size)std::memcpy(p,data,size);r->Unmap(0,nullptr);return r;};
    for(unsigned population:{0u,1u,63u,64u,65u,2046u})for(unsigned cap:{7u,1024u})for(bool ortho:{false,true}){
        FrameLights f;
        const auto fill=[&]<typename T>(std::vector<T> &v,unsigned seed){v.resize(population?std::max(1u,population/(seed+1)):0);for(unsigned i=0;i<v.size();++i){auto &l=v[i];l.enabled=i%9!=2;Vector3 p(float((i*31+seed)%70)-35,float((i*17+seed)%40)-20,float((i*7+seed)%60)-5);if constexpr(requires{l.position;})l.position=p;else{l.p0=p;l.p1=Vector3(p.x+3,p.y+1,p.z);}if constexpr(requires{l.radius;})l.radius=1+float(i%13);if constexpr(requires{l.distance;})l.distance=1+float(i%13);}};
        fill(f.pointLights,0);fill(f.spotLights,1);fill(f.sphereLights,2);fill(f.discLights,3);fill(f.rectLights,4);fill(f.tubeLights,5);fill(f.boxLights,6);
        ComPtr<ID3D12Resource> inputs[]={upload(f.pointLights.data(),f.pointLights.size()*sizeof(PointLightElement)),upload(f.spotLights.data(),f.spotLights.size()*sizeof(SpotLightElement)),upload(f.sphereLights.data(),f.sphereLights.size()*sizeof(SphereLightElement)),upload(f.discLights.data(),f.discLights.size()*sizeof(DiscLightElement)),upload(f.rectLights.data(),f.rectLights.size()*sizeof(RectLightElement)),upload(f.tubeLights.data(),f.tubeLights.size()*sizeof(TubeLightElement)),upload(f.boxLights.data(),f.boxLights.size()*sizeof(BoxLightElement))};
        float camera[56]{};for(unsigned j=0;j<3;++j)for(unsigned i=0;i<4;++i)camera[j*16+i*5]=1;
        if(!ortho){camera[16]=1.2f;camera[21]=1.8f;camera[26]=1.01f;camera[27]=1;camera[30]=-1.01f;camera[31]=0;}
        auto cameraBuffer=upload(camera,sizeof(camera));
        TileCullingConstants c{};c.screenSize=Vector2(319,181);c.tileCountX=20;c.tileCountY=12;c.pointLightCount=static_cast<UINT>(f.pointLights.size());c.spotLightCount=static_cast<UINT>(f.spotLights.size());c.sphereLightCount=static_cast<UINT>(f.sphereLights.size());c.discLightCount=static_cast<UINT>(f.discLights.size());c.rectLightCount=static_cast<UINT>(f.rectLights.size());c.tubeLightCount=static_cast<UINT>(f.tubeLights.size());c.boxLightCount=static_cast<UINT>(f.boxLights.size());c.maxLightsPerTile=cap;c.tileSize=16;
        auto constants=upload(&c,sizeof(c));size_t bytes=size_t(c.tileCountX)*c.tileCountY*(cap+1)*sizeof(UINT);
        std::vector<UINT> output[2];
        for(unsigned which=0;which<2;++which){auto result=buffer(bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);auto readback=buffer(bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            GpuCheck(allocator->Reset());GpuCheck(list->Reset(allocator.Get(),pipelines[which].Get()));list->SetComputeRootSignature(signature.Get());list->SetComputeRootConstantBufferView(0,cameraBuffer->GetGPUVirtualAddress());list->SetComputeRootConstantBufferView(1,constants->GetGPUVirtualAddress());for(unsigned i=0;i<7;++i)list->SetComputeRootShaderResourceView(i+2,inputs[i]->GetGPUVirtualAddress());list->SetComputeRootUnorderedAccessView(9,result->GetGPUVirtualAddress());list->Dispatch(3,2,1);
            D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={result.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};list->ResourceBarrier(1,&barrier);list->CopyBufferRegion(readback.Get(),0,result.Get(),0,bytes);GpuCheck(list->Close());ID3D12CommandList *commands[]={list.Get()};queue->ExecuteCommandLists(1,commands);GpuCheck(queue->Signal(fence.Get(),++serial));HANDLE event=CreateEvent(nullptr,FALSE,FALSE,nullptr);if(!event)throw std::runtime_error("Fence event");GpuCheck(fence->SetEventOnCompletion(serial,event));auto wait=WaitForSingleObject(event,30000);CloseHandle(event);if(wait!=WAIT_OBJECT_0)throw std::runtime_error("GPU test timeout");
            void *p=nullptr;GpuCheck(readback->Map(0,nullptr,&p));output[which].assign(static_cast<UINT*>(p),static_cast<UINT*>(p)+bytes/sizeof(UINT));readback->Unmap(0,nullptr);
        }
        for(unsigned tile=0;tile<c.tileCountX*c.tileCountY;++tile){size_t offset=size_t(tile)*(cap+1);UINT count=output[0][offset];if(count!=output[1][offset]||count>cap||!std::equal(output[0].begin()+offset+1,output[0].begin()+offset+1+count,output[1].begin()+offset+1))throw std::runtime_error("GPU tile mismatch population="+std::to_string(population)+" cap="+std::to_string(cap)+" tile="+std::to_string(tile));}
    }
    std::puts("PASS: GPU readback matches reference for 7 light types, empty/63/64/65/2046 lights, tile caps, perspective/orthographic and partial groups");
}
