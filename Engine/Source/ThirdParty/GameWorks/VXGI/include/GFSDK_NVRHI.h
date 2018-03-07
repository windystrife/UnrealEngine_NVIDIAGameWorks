/*
* Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef GFSDK_NVRHI_H_
#define GFSDK_NVRHI_H_

#include <stdint.h>
#include <memory.h>

namespace NVRHI
{
    class IRenderThreadCommand
    {
        IRenderThreadCommand& operator=(const IRenderThreadCommand& other); //undefined
    protected:
        //The user cannot delete this
        virtual ~IRenderThreadCommand() {};
    public:
        //execute the operation
        virtual void execute() = 0;
        //the caller is finished with this object and it can be destroyed
        virtual void dispose() = 0;
        //do both
        virtual void executeAndDispose() = 0;
    };

    //////////////////////////////////////////////////////////////////////////
    // Basic Types
    //////////////////////////////////////////////////////////////////////////

    struct Color
    {
        float r, g, b, a;
        Color() : r(0.f), g(0.f), b(0.f), a(0.f) { }
        Color(float c) : r(c), g(c), b(c), a(c) { }
        Color(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) { }

        bool operator ==(const Color& _b) const { return r == _b.r && g == _b.g && b == _b.b && a == _b.a; }
        bool operator !=(const Color& _b) const { return r != _b.r || g != _b.g || b != _b.b || a != _b.a; }
    };

    struct Viewport
    {
        float minX, maxX;
        float minY, maxY;
        float minZ, maxZ;

        Viewport() : minX(0.f), maxX(0.f), minY(0.f), maxY(0.f), minZ(0.f), maxZ(1.f) { }
        Viewport(float width, float height) : minX(0.f), maxX(width), minY(0.f), maxY(height), minZ(0.f), maxZ(1.f) { }
        Viewport(float _minX, float _maxX, float _minY, float _maxY, float _minZ, float _maxZ) : minX(_minX), maxX(_maxX), minY(_minY), maxY(_maxY), minZ(_minZ), maxZ(_maxZ) { }
    };

    struct Rect
    {
        int minX, maxX;
        int minY, maxY;

        Rect() : minX(0), maxX(0), minY(0), maxY(0) { }
        Rect(int width, int height) : minX(0), maxX(width), minY(0), maxY(height) { }
        Rect(int _minX, int _maxX, int _minY, int _maxY) : minX(_minX), maxX(_maxX), minY(_minY), maxY(_maxY) { }
    };

    //////////////////////////////////////////////////////////////////////////
    // Texture
    //////////////////////////////////////////////////////////////////////////

    class Texture;
    typedef Texture* TextureHandle;

    struct Format
    {
        enum Enum
        {
            UNKNOWN,
            R8_UINT,
            R8_UNORM,
            RG8_UINT,
            RG8_UNORM,
            R16_UINT,
            R16_UNORM,
            R16_FLOAT,
            RGBA8_UNORM,
            BGRA8_UNORM,
            SRGBA8_UNORM,
            R10G10B10A2_UNORM,
			R11G11B10_FLOAT,
            RG16_UINT,
            RG16_FLOAT,
            R32_UINT,
            R32_FLOAT,
            RGBA16_FLOAT,
			RGBA16_UNORM,
			RGBA16_SNORM,
            RG32_UINT,
            RG32_FLOAT,
            RGB32_UINT,
            RGB32_FLOAT,
            RGBA32_UINT,
            RGBA32_FLOAT,
            D16,
            D24S8,
            X24G8_UINT,
            D32,
        };
    };

    struct TextureDesc
    {
        enum Usage
        {
            USAGE_DEFAULT,
            USAGE_IMMUTABLE,
            USAGE_DYNAMIC
        };

        uint32_t width;
        uint32_t height;
        uint32_t depthOrArraySize;
        uint32_t mipLevels;
        uint32_t sampleCount, sampleQuality;
        Format::Enum format;
        Usage usage;
        const char* debugName;

        bool isArray; //3D or array if .z != 0?
        bool isCubeMap;
        bool isRenderTarget;
        bool isUAV;
        bool isCPUWritable;
        bool disableGPUsSync;

        Color clearValue;
        bool useClearValue;

        TextureDesc() :
            format(Format::UNKNOWN),
            width(0),
            height(0),
            depthOrArraySize(0),
            mipLevels(1),
            usage(USAGE_DEFAULT),
            sampleCount(1),
            sampleQuality(0),
            debugName(0),
            isCPUWritable(false),
            isUAV(false),
            isRenderTarget(false),
            isArray(false),
            isCubeMap(false),
            disableGPUsSync(false),
            useClearValue(false)
        {
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Input Layout
    //////////////////////////////////////////////////////////////////////////

    struct VertexAttributeDesc
    {
        enum { MAX_NAME_LENGTH = 256 };

        char name[MAX_NAME_LENGTH];
        Format::Enum format;
        uint32_t bufferIndex;
        uint32_t offset;
        bool isInstanced;
    };

    class InputLayout;
    typedef InputLayout* InputLayoutHandle;

    //////////////////////////////////////////////////////////////////////////
    // Buffer
    //////////////////////////////////////////////////////////////////////////

    class Buffer;
    typedef Buffer* BufferHandle;

    struct BufferDesc
    {
        // Notice that there are no atomic/append/consume buffer-related things here.
        // We should use another UAV of uints instead since you can do that in both DX and GL
        uint32_t byteSize;
        uint32_t structStride; //if non-zero it's structured
        const char* debugName;
        bool canHaveUAVs;
        bool isVertexBuffer;
        bool isIndexBuffer;
        bool isCPUWritable;
        bool isDrawIndirectArgs;
        bool disableGPUsSync;

        BufferDesc()  { memset(this, 0, sizeof(*this)); }
    };

    //////////////////////////////////////////////////////////////////////////
    // Constant Buffer
    //////////////////////////////////////////////////////////////////////////

    class ConstantBuffer;
    typedef ConstantBuffer* ConstantBufferHandle;

    struct ConstantBufferDesc
    {
        uint32_t byteSize;
        const char* debugName;

        ConstantBufferDesc(uint32_t size, const char* name) : byteSize(size), debugName(name) {}
        ConstantBufferDesc()  { memset(this, 0, sizeof(*this)); }
    };

    //////////////////////////////////////////////////////////////////////////
    // Shader
    //////////////////////////////////////////////////////////////////////////

    class Shader;
    typedef Shader* ShaderHandle;

    struct ShaderType
    {
        enum Enum
        {
            SHADER_VERTEX,
            SHADER_HULL,
            SHADER_DOMAIN,
            SHADER_GEOMETRY,
            SHADER_PIXEL,
            GRAPHIC_SHADERS_NUM,
            SHADER_COMPUTE
        };
    };

    struct ShaderMetadata
    {
        uint32_t slotsSRV[4];
        uint32_t slotsSampler[4];
		uint32_t slotsUAV;
		uint32_t constantBufferSizes[16];
    };

    struct ShaderDesc
    {
        ShaderType::Enum shaderType;
        IRenderThreadCommand* preCreationCommand;
        IRenderThreadCommand* postCreationCommand;

        // Pointers to NVAPI D3D12 extension descriptor structures required for this shader.
        // All structures are derived from NVAPI_D3D12_PSO_EXTENSION_DESC base type.
        // A pipeline state containing a shader that uses at least one extension 
        // has to be created using one of these functions instead of regular D3D12 ones:
        //  - NvAPI_D3D12_CreateGraphicsPipelineState
        //  - NvAPI_D3D12_CreateComputePipelineState
        // On graphics APIs other than D3D12, these fields should be ignored.
        // When NVAPI is unavailable, the backend is expected to fail silently
        // and return NULL from createShader.
        const void* pipelineStateExtensions[4];
        uint32_t numPipelineStateExtensions;

        // Information about resource bindings used by the shader.
        // A bit set to 1 in the "slotsX" field means the corresponding resource slot is used.
        // Resource (slotIndex) used <=> slotsX[slotIndex >> 5] & (1 << (slotIndex & 31))
        bool metadataValid;
        ShaderMetadata metadata;
        
        ShaderDesc(ShaderType::Enum type) 
            : shaderType(type)
            , preCreationCommand(0)
            , postCreationCommand(0)
            , metadataValid(false)
            , numPipelineStateExtensions(0)
        { }
    };

    //////////////////////////////////////////////////////////////////////////
    // Blend State
    //////////////////////////////////////////////////////////////////////////

    struct BlendState
    {
        enum { MAX_MRT_BLEND_COUNT = 8 };

        enum BlendValue : unsigned char
        {
            BLEND_ZERO = 1,
            BLEND_ONE = 2,
            BLEND_SRC_COLOR = 3,
            BLEND_INV_SRC_COLOR = 4,
            BLEND_SRC_ALPHA = 5,
            BLEND_INV_SRC_ALPHA = 6,
            BLEND_DEST_ALPHA = 7,
            BLEND_INV_DEST_ALPHA = 8,
            BLEND_DEST_COLOR = 9,
            BLEND_INV_DEST_COLOR = 10,
            BLEND_SRC_ALPHA_SAT = 11,
            BLEND_BLEND_FACTOR = 14,
            BLEND_INV_BLEND_FACTOR = 15,
            BLEND_SRC1_COLOR = 16,
            BLEND_INV_SRC1_COLOR = 17,
            BLEND_SRC1_ALPHA = 18,
            BLEND_INV_SRC1_ALPHA = 19
        };

        enum BlendOp : unsigned char
        {
            BLEND_OP_ADD = 1,
            BLEND_OP_SUBTRACT = 2,
            BLEND_OP_REV_SUBTRACT = 3,
            BLEND_OP_MIN = 4,
            BLEND_OP_MAX = 5
        };

        enum ColorMask : unsigned char
        {
            COLOR_MASK_RED = 1,
            COLOR_MASK_GREEN = 2,
            COLOR_MASK_BLUE = 4,
            COLOR_MASK_ALPHA = 8,
            COLOR_MASK_ALL = 0xF
        };

        bool        blendEnable[MAX_MRT_BLEND_COUNT];
        BlendValue  srcBlend[MAX_MRT_BLEND_COUNT];
        BlendValue  destBlend[MAX_MRT_BLEND_COUNT];
        BlendOp     blendOp[MAX_MRT_BLEND_COUNT];
        BlendValue  srcBlendAlpha[MAX_MRT_BLEND_COUNT];
        BlendValue  destBlendAlpha[MAX_MRT_BLEND_COUNT];
        BlendOp     blendOpAlpha[MAX_MRT_BLEND_COUNT];
        ColorMask   colorWriteEnable[MAX_MRT_BLEND_COUNT];
        Color    blendFactor;
        bool        alphaToCoverage;
        uint8_t     padding[7];

        BlendState() :
            blendFactor(0, 0, 0, 0),
            alphaToCoverage(false)
        {
            for (uint32_t i = 0; i < MAX_MRT_BLEND_COUNT; i++)
            {
                blendEnable[i] = false;
                colorWriteEnable[i] = COLOR_MASK_ALL;
                srcBlend[i] = BLEND_ONE;
                destBlend[i] = BLEND_ZERO;
                blendOp[i] = BLEND_OP_ADD;
                srcBlendAlpha[i] = BLEND_ONE;
                destBlendAlpha[i] = BLEND_ZERO;
                blendOpAlpha[i] = BLEND_OP_ADD;
            }

            memset(padding, 0, sizeof(padding));
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Raster State
    //////////////////////////////////////////////////////////////////////////

    struct RasterState
    {
        enum FillMode : unsigned char
        {
            FILL_SOLID,
            FILL_LINE
        };

        enum CullMode : unsigned char
        {
            CULL_BACK,
            CULL_FRONT,
            CULL_NONE
        };

        FillMode    fillMode;
        CullMode    cullMode;
        bool frontCounterClockwise;
        bool depthClipEnable;
        bool scissorEnable;
        bool multisampleEnable;
        bool antialiasedLineEnable;
        int depthBias;
        float depthBiasClamp;
        float slopeScaledDepthBias;

        // Extended rasterizer state supported by Maxwell
        // In D3D11, use NvAPI_D3D11_CreateRasterizerState to create such rasterizer state.
        char forcedSampleCount;
        bool programmableSamplePositionsEnable;
        bool conservativeRasterEnable;
        char samplePositionsX[16];
        char samplePositionsY[16];

        RasterState()  {
            memset(this, 0, sizeof(RasterState));
            depthClipEnable = true;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Depth Stencil State
    //////////////////////////////////////////////////////////////////////////

    struct DepthStencilState
    {
        enum DepthWriteMask : unsigned char
        {
            DEPTH_WRITE_MASK_ZERO = 0,
            DEPTH_WRITE_MASK_ALL = 1
        };

        enum StencilOp : unsigned char
        {
            STENCIL_OP_KEEP = 1,
            STENCIL_OP_ZERO = 2,
            STENCIL_OP_REPLACE = 3,
            STENCIL_OP_INCR_SAT = 4,
            STENCIL_OP_DECR_SAT = 5,
            STENCIL_OP_INVERT = 6,
            STENCIL_OP_INCR = 7,
            STENCIL_OP_DECR = 8
        };

        enum ComparisonFunc : unsigned char
        {
            COMPARISON_NEVER = 1,
            COMPARISON_LESS = 2,
            COMPARISON_EQUAL = 3,
            COMPARISON_LESS_EQUAL = 4,
            COMPARISON_GREATER = 5,
            COMPARISON_NOT_EQUAL = 6,
            COMPARISON_GREATER_EQUAL = 7,
            COMPARISON_ALWAYS = 8
        };

        struct StencilOpDesc
        {
            StencilOp stencilFailOp;
            StencilOp stencilDepthFailOp;
            StencilOp stencilPassOp;
            ComparisonFunc stencilFunc;
        };

        bool            depthEnable;
        DepthWriteMask  depthWriteMask;
        ComparisonFunc  depthFunc;
        bool            stencilEnable;
        uint8_t         stencilReadMask;
        uint8_t         stencilWriteMask;
        uint8_t         stencilRefValue;
        uint8_t         padding;
        StencilOpDesc   frontFace;
        StencilOpDesc   backFace;

        DepthStencilState() :
            stencilRefValue(0),
            depthEnable(true),
            depthWriteMask(DEPTH_WRITE_MASK_ALL),
            depthFunc(COMPARISON_LESS),
            stencilEnable(false),
            stencilReadMask(0xff),
            stencilWriteMask(0xff),
            padding(0)
        {
            StencilOpDesc stencilOpDesc;
            stencilOpDesc.stencilFailOp = STENCIL_OP_KEEP;
            stencilOpDesc.stencilDepthFailOp = STENCIL_OP_KEEP;
            stencilOpDesc.stencilPassOp = STENCIL_OP_KEEP;
            stencilOpDesc.stencilFunc = COMPARISON_ALWAYS;
            frontFace = stencilOpDesc;
            backFace = stencilOpDesc;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Sampler
    //////////////////////////////////////////////////////////////////////////

    class Sampler;
    typedef Sampler* SamplerHandle;

    struct SamplerDesc
    {
        enum WrapMode : unsigned char
        {
            WRAP_MODE_CLAMP,
            WRAP_MODE_WRAP,
            WRAP_MODE_BORDER
        };

        WrapMode wrapMode[3];
        float mipBias, anisotropy;
        bool minFilter, magFilter, mipFilter;
        bool shadowCompare;
        Color borderColor;

        SamplerDesc() :
            minFilter(true),
            magFilter(true),
            mipFilter(true),
            mipBias(0),
            anisotropy(1),
            shadowCompare(false),
            borderColor(1, 1, 1, 1)
        {
            wrapMode[0] = wrapMode[1] = wrapMode[2] = WRAP_MODE_CLAMP;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Render State (used by DrawCallState)
    //////////////////////////////////////////////////////////////////////////

    struct RenderState
    {
        enum { MAX_RENDER_TARGETS = 8, MAX_VIEWPORTS = 16 };

        uint32_t targetCount;
        TextureHandle targets[MAX_RENDER_TARGETS];
        uint32_t targetIndicies[MAX_RENDER_TARGETS]; // this is the array index, 3d-z, coord or cube face. For cube arrays, slice is /6, face is %6
        uint32_t targetMipSlices[MAX_RENDER_TARGETS];

        //These are in pixels
        uint32_t viewportCount;
        Viewport viewports[MAX_VIEWPORTS];
        Rect     scissorRects[MAX_VIEWPORTS];

        TextureHandle depthTarget;
        uint32_t depthIndex;
        uint32_t depthMipSlice;

        Color clearColor;
        float clearDepth;
        uint8_t clearStencil;

        bool clearColorTarget, clearDepthTarget, clearStencilTarget;

        // This flag is used by VXGI on OpenGL and it indicates that the rendering backend should call 
        // IGlobalIllumination::setupExtraVoxelizationState() when applying this render state.
        bool setupExtraVoxelizationState;

        BlendState blendState;
        DepthStencilState depthStencilState;
        RasterState rasterState;

        RenderState() : targetCount(0), viewportCount(0), clearColor(0, 0, 0, 0), clearDepth(1.0f), clearStencil(0),
            clearColorTarget(false), clearDepthTarget(false), clearStencilTarget(false),
            depthTarget(0), depthIndex(0), depthMipSlice(0), 
            setupExtraVoxelizationState(false)
        {
            memset(targets, 0, sizeof(targets));
            memset(targetIndicies, 0, sizeof(targetIndicies));
            memset(targetMipSlices, 0, sizeof(targetMipSlices));
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Bindings
    //////////////////////////////////////////////////////////////////////////

    struct ConstantBufferBinding
    {
        ConstantBufferHandle buffer;
        uint32_t slot;
    };

    struct SamplerBinding
    {
        SamplerHandle sampler;
        uint32_t slot;
    };

    struct TextureBinding
    {
        TextureHandle texture;
        uint32_t slot : 8;
        Format::Enum format : 8;
        uint32_t mipLevel : 8;
        bool isWritable : 1; // true if this is a UAV (DX) or image (GL)
    };

    struct BufferBinding
    {
        BufferHandle buffer;
        uint32_t slot : 8;
        Format::Enum format : 8;
        bool isWritable : 1; // true if this is a UAV (DX) or SSBO (GL)
    };

    struct VertexBufferBinding
    {
        BufferHandle buffer;
        uint32_t slot;
        uint32_t offset;
        uint32_t stride;
    };

    //////////////////////////////////////////////////////////////////////////
    // Draw State
    //////////////////////////////////////////////////////////////////////////

    struct PrimitiveType
    {
        enum Enum
        {
            POINT_LIST,
            TRIANGLE_LIST,
            TRIANGLE_STRIP,
            PATCH_1_CONTROL_POINT,
            PATCH_3_CONTROL_POINT
        };
    };

    struct PipelineStageBindings
    {
        enum { MAX_TEXTURE_BINDINGS = 128, MAX_SAMPLER_BINDINGS = 16, MAX_BUFFER_BINDINGS = 128, MAX_CB_BINDINGS = 15 };

        // This field helps other code identify which stage the binding set is intended for
        ShaderType::Enum stage;

        ShaderHandle shader;

        //If this state came from an IUserDefinedShaderSet this is the index of the permutation we are using in case the application needs it to find reflection data
        uint32_t     userDefinedShaderPermutationIndex;

        //Each texture has a corresponding sampler to ease GL vs. DX porting
        TextureBinding textures[MAX_TEXTURE_BINDINGS];
        uint32_t textureBindingCount;
        SamplerBinding textureSamplers[MAX_SAMPLER_BINDINGS];
        uint32_t textureSamplerBindingCount;
        BufferBinding buffers[MAX_BUFFER_BINDINGS];
        uint32_t bufferBindingCount;
        ConstantBufferBinding constantBuffers[MAX_CB_BINDINGS];
        uint32_t constantBufferBindingCount;

        PipelineStageBindings()
            : stage(ShaderType::SHADER_PIXEL)
            , textureBindingCount(0)
            , textureSamplerBindingCount(0)
            , bufferBindingCount(0)
            , constantBufferBindingCount(0)
            , shader(nullptr)
            , userDefinedShaderPermutationIndex(0)
        {
            memset(textures, 0, sizeof(textures));
            memset(textureSamplers, 0, sizeof(textureSamplers));
            memset(buffers, 0, sizeof(buffers));
            memset(constantBuffers, 0, sizeof(constantBuffers));
        }

        PipelineStageBindings(ShaderType::Enum _stage)
            : stage(_stage)
            , textureBindingCount(0)
            , textureSamplerBindingCount(0)
            , bufferBindingCount(0)
            , constantBufferBindingCount(0)
            , shader(nullptr)
            , userDefinedShaderPermutationIndex(0)
        { 
            memset(textures, 0, sizeof(textures));
            memset(textureSamplers, 0, sizeof(textureSamplers));
            memset(buffers, 0, sizeof(buffers));
            memset(constantBuffers, 0, sizeof(constantBuffers));
        }
            
    };

    struct DrawCallState
    {
        enum { MAX_VERTEX_ATTRIBUTE_COUNT = 16 };
        
        PrimitiveType::Enum primType;
        InputLayoutHandle inputLayout;
        BufferHandle indexBuffer;
        Format::Enum indexBufferFormat;
        uint32_t indexBufferOffset;

        PipelineStageBindings VS;
        PipelineStageBindings HS;
        PipelineStageBindings DS;
        PipelineStageBindings GS;
        PipelineStageBindings PS;

        uint32_t vertexBufferCount;
        VertexBufferBinding vertexBuffers[MAX_VERTEX_ATTRIBUTE_COUNT];

        RenderState renderState;

        DrawCallState() 
            : VS(ShaderType::SHADER_VERTEX)
            , HS(ShaderType::SHADER_HULL)
            , DS(ShaderType::SHADER_DOMAIN)
            , GS(ShaderType::SHADER_GEOMETRY)
            , PS(ShaderType::SHADER_PIXEL)
            , primType(PrimitiveType::TRIANGLE_LIST)
            , inputLayout(nullptr)
            , indexBuffer(nullptr)
            , indexBufferFormat(Format::R32_UINT)
            , indexBufferOffset(0)
            , vertexBufferCount(0)
        {
            memset(vertexBuffers, 0, sizeof(vertexBuffers));
        }
    };

    struct DispatchState : PipelineStageBindings
    {
        DispatchState()
            : PipelineStageBindings(ShaderType::SHADER_COMPUTE)
        { }
    };

    //////////////////////////////////////////////////////////////////////////
    // Misc
    //////////////////////////////////////////////////////////////////////////

    struct APISpecificInterface
    {
        enum Enum
        {
            D3D11DEVICE,
            D3D11DEVICECONTEXT,
            D3D12DEVICE
        };
    };

    struct GraphicsAPI
    {
        enum Enum
        {
            D3D11,
            D3D12,
            OPENGL4
        };
    };

    class PerformanceQuery;
    typedef PerformanceQuery* PerformanceQueryHandle;

    struct DrawArguments
    {
        uint32_t vertexCount;
        uint32_t instanceCount;
        uint32_t startIndexLocation;
        uint32_t startVertexLocation;
        uint32_t startInstanceLocation;

        DrawArguments()
            : vertexCount(0)
            , instanceCount(1)
            , startIndexLocation(0)
            , startVertexLocation(0)
            , startInstanceLocation(0)
        { }
    };

    // Should be implemented by the application.
    // Clients will call signalError(...) on every error it encounters, in addition to returning one of the 
    // failure status codes. The application can display a message box in case of errors.
    class IErrorCallback
    {
        IErrorCallback& operator=(const IErrorCallback& other); //undefined
    protected:
        virtual ~IErrorCallback() {};
    public:
        virtual void signalError(const char* file, int line, const char* errorDesc) = 0;
    };


    //////////////////////////////////////////////////////////////////////////
    // IRendererInterface
    //////////////////////////////////////////////////////////////////////////

    class IRendererInterface
    {
        IRendererInterface& operator=(const IRendererInterface& other); //undefined
    protected:
        virtual ~IRendererInterface() {};
    public:

        virtual TextureHandle createTexture(const TextureDesc& d, const void* data) = 0;
        virtual TextureDesc describeTexture(TextureHandle t) = 0;
        virtual void clearTextureFloat(TextureHandle t, const Color& clearColor) = 0;
        virtual void clearTextureUInt(TextureHandle t, uint32_t clearColor) = 0;
        virtual void writeTexture(TextureHandle t, uint32_t subresource, const void* data, uint32_t rowPitch, uint32_t depthPitch) = 0;
        virtual void destroyTexture(TextureHandle t) = 0;

        virtual BufferHandle createBuffer(const BufferDesc& d, const void* data) = 0;
        virtual void writeBuffer(BufferHandle b, const void* data, size_t dataSize) = 0;
        virtual void clearBufferUInt(BufferHandle b, uint32_t clearValue) = 0;
        virtual void copyToBuffer(BufferHandle dest, uint32_t destOffsetBytes, BufferHandle src, uint32_t srcOffsetBytes, size_t dataSizeBytes) = 0;
        virtual void readBuffer(BufferHandle b, void* data, size_t* dataSize) = 0; // for debugging purposes only
        virtual void destroyBuffer(BufferHandle b) = 0;

        virtual ConstantBufferHandle createConstantBuffer(const ConstantBufferDesc& d, const void* data) = 0;
        virtual void writeConstantBuffer(ConstantBufferHandle b, const void* data, size_t dataSize) = 0;
        virtual void destroyConstantBuffer(ConstantBufferHandle b) = 0;
        virtual ShaderHandle createShader(const ShaderDesc& d, const void* binary, const size_t binarySize) = 0;
        virtual void destroyShader(ShaderHandle s) = 0;

        virtual SamplerHandle createSampler(const SamplerDesc& d) = 0;
        virtual void destroySampler(SamplerHandle s) = 0;

        virtual InputLayoutHandle createInputLayout(const VertexAttributeDesc* d, uint32_t attributeCount, const void* vertexShaderBinary, const size_t binarySize) = 0;
        virtual void destroyInputLayout(InputLayoutHandle i) = 0;

        // Performance queries
        virtual PerformanceQueryHandle createPerformanceQuery(const char* name) = 0;
        virtual void destroyPerformanceQuery(PerformanceQueryHandle query) = 0;
        virtual void beginPerformanceQuery(PerformanceQueryHandle query, bool onlyAnnotation = false) = 0;
        virtual void endPerformanceQuery(PerformanceQueryHandle query) = 0;
        virtual float getPerformanceQueryTimeMS(PerformanceQueryHandle query) = 0;

        // Returns the API kind that the RHI backend is running on top of.
        virtual GraphicsAPI::Enum getGraphicsAPI() = 0;

        // This returns an api-specific handle which is required for some NVAPI operations.
        // In D3D11 this is ID3D11DeviceContext*
        virtual void* getAPISpecificInterface(APISpecificInterface::Enum interfaceType) = 0;

        // This function wraps the API specific shader handle with a ShaderHandle and transfers shader ownership to the backend.
        // In D3D11, the handle is a ID3D11{Pixel,Compute,etc.}Shader*, according to shaderType.
        // In GL, it's a separate program identifier.
        // Clients calls destroyShader when these handles are no longer needed, and the backend should properly destroy the shader.
        virtual ShaderHandle createShaderFromAPIInterface(ShaderType::Enum shaderType, const void* apiInterface) = 0;

        virtual bool isOpenGLExtensionSupported(const char* name) = 0;

        // Try to get the address of openGL procedure specified by procname; return 0 if unsupported or not GL.
        virtual void* getOpenGLProcAddress(const char* procname) = 0;

        virtual void draw(const DrawCallState& state, const DrawArguments* args, uint32_t numDrawCalls) = 0;
        virtual void drawIndexed(const DrawCallState& state, const DrawArguments* args, uint32_t numDrawCalls) = 0;
        virtual void drawIndirect(const DrawCallState& state, BufferHandle indirectParams, uint32_t offsetBytes) = 0;

        virtual void dispatch(const DispatchState& state, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) = 0;
        virtual void dispatchIndirect(const DispatchState& state, BufferHandle indirectParams, uint32_t offsetBytes) = 0;

        // A simple implementation would just look like
        // {
        //     onCommand->executeAndDispose();
        // }
        virtual void executeRenderThreadCommand(IRenderThreadCommand* onCommand) = 0;

        // For SLI configurations, call NvAPI_D3D_GetCurrentSLIState and return the value of numAFRGroups;
        // For non-SLI or APIs where SLI is not supported, just return "1".
        virtual uint32_t getNumberOfAFRGroups() = 0;

        // Normally you should just be able to return 0, for non-SLI or call NvAPI_D3D_GetCurrentSLIState.
        // However, if the application is buffering frames this may require some more complex logic to implement.
        // The application is passed the number of AFR groups for validation.
        virtual uint32_t getAFRGroupOfCurrentFrame(uint32_t numAFRGroups) = 0;

		// Tells the D3D12 backend whether UAV barriers should be used for the given texture or buffer between draw calls.
		// A barrier should still be placed before the first draw call in the group and after the last one.
		virtual void setEnableUavBarriersForTexture(TextureHandle texture, bool enableBarriers) = 0;
		virtual void setEnableUavBarriersForBuffer(BufferHandle buffer, bool enableBarriers) = 0;
    };

}

#endif // GFSDK_NVRHI_H_
