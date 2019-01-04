/*===================== begin_copyright_notice ==================================

Copyright (c) 2017 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


======================= end_copyright_notice ==================================*/


#include "common/LLVMWarningsPush.hpp"
#include <llvm/Support/ScaledNumber.h>
#include "common/LLVMWarningsPop.hpp"

#include "sp_g8.h"
#include "sp_convert_g8.h"
#include "sp_debug.h"

#include "../util/BinaryStream.h"

#include "../Platform/cmd_shared_init_g8.h"
#include "../Platform/cmd_media_init_g8.h"
#include "../Platform/cmd_parser_g8.h"

#include "common/allocator.h"
#include "common/igc_regkeys.hpp"
#include "common/Stats.hpp"
#include "common/SystemThread.h"
#include "common/secure_mem.h"
#include "common/debug/Dump.hpp"
#include "common/debug/Debug.hpp"

#include <iStdLib/MemCopy.h>

#include "patch_list.h"
#include "program_debug_data.h"
#include "patch_shared.h"
#include "patch_g7.h"
#include "patch_g8.h"

#include "../../../Compiler/CodeGenPublic.h"

#include "../../../visa/include/visaBuilder_interface.h"

#include "gtpin_igc_ocl.h"

#include <algorithm>
#include <cstring>


using namespace IGC;
using namespace IGC::IGCMD; 
using namespace IGC::Debug;

namespace iOpenCL
{

__forceinline long FloatToLong( const float value )
{
#if defined(_WIN32) && defined(_MSC_VER)
    return _mm_cvtsi128_si32( _mm_cvttps_epi32( _mm_set_ps1( value ) ) );
#else
    return (long)value;
#endif
}

template<typename Type>
__forceinline Type GetAlignmentOffset( const Type size, const size_t alignSize )
{
    ICBE_ASSERT( alignSize );
    Type typedAlignSize = (Type)alignSize;
    return (typedAlignSize - (size % typedAlignSize)) % typedAlignSize;
}

template <class Type>
__forceinline Type FloatToFixed(
    float value,
    const int whole,
    const int fractional, 
    const int round = 0 )
{
    ICBE_ASSERT( fractional + whole <= 32 );

    // Optional floating point rounding precision
    value += ( round != 0 )
        ? 0.5f * ( 1.0f / (float)( 1 << round ) )
        : 0;

    Type fixed = (Type)FloatToLong( value * (float)( 1 << fractional ) );

#ifdef _DEBUG
    DWORD mask = 0xffffffff << ( whole + fractional );
    ICBE_ASSERT( 
        (( fixed >= 0 ) && (( fixed & mask ) == 0 )) ||
        (( fixed <  0 ) && (( fixed & mask ) == mask )) );
#endif

    return fixed;
}

#define HASH_JENKINS_MIX(a,b,c)    \
{                                  \
a -= b; a -= c; a ^= (c>>13);  \
b -= c; b -= a; b ^= (a<<8);   \
c -= a; c -= b; c ^= (b>>13);  \
a -= b; a -= c; a ^= (c>>12);  \
b -= c; b -= a; b ^= (a<<16);  \
c -= a; c -= b; c ^= (b>>5);   \
a -= b; a -= c; a ^= (c>>3);   \
b -= c; b -= a; b ^= (a<<10);  \
c -= a; c -= b; c ^= (b>>15);  \
}

inline QWORD Hash( const DWORD *data, DWORD count )
{
    DWORD   a = 0x428a2f98, hi = 0x71374491, lo = 0xb5c0fbcf; 
    while( count-- )
    {
        a ^= *(data++);
        //ICBE_DPF( 0xffffffff, "%8x, %8x, %8x, %8x\n", a ,hi, lo, *data );
        HASH_JENKINS_MIX( a, hi, lo );
    }
    return (((QWORD)hi)<<32)|lo;
}
#undef HASH_JENKINS_MIX

template< typename Type >
__forceinline bool IsAligned( Type size, const size_t alignSize )
{
    return ( ( size % alignSize ) == 0 );
}

RETVAL g_cInitRetValue = { 1 };

static const int numAllowedAttributes = 4;
const char* allowedAttributes[numAllowedAttributes] =
{
    "reqd_sub_group_size",
    "reqd_work_group_size",
    "vec_type_hint",
    "work_group_size_hint",
};

/*****************************************************************************\
STRUCT: SStateProcessorContextGen8_0
\*****************************************************************************/
struct SStateProcessorContextGen8_0
{
    struct _Runtime
    {
        // CS_URB_STATE
        DWORD   URBEntryAllocationSize;

    } Runtime;

    struct _Kernel
    {
        // System Kernel
        bool    SystemKernelPresent;
        DWORD   SystemKernelOffset;

        // Client Kernel
        DWORD   KernelOffset;

    } Kernel;

    struct _Dynamic
    {
        // SAMPLER_BORDER_COLOR_STATE
        DWORD   SamplerBorderColorStateOffset;

        // SAMPLER_STATE
        DWORD   SamplerCount;
        DWORD   SamplerArrayOffset;

        DWORD   SamplerOffset[ G6HWC::g_cNumSamplersPerProgram ];

        // INTERFACE_DESCRIPTOR
        DWORD   InterfaceDescriptorDataOffset;

        // VFE_STATE
        DWORD   VFEStateOffset;

        // CONSTANT_BUFFER
        DWORD   ConstantBufferDataOffset;
        DWORD   ConstantBufferDataLength;

    } Dynamic;

    struct _Static
    {
        // Scratch Space
        bool    ScratchSpacePresent;
        DWORD   ScratchSpaceOffset;

    } Static;

    struct _Surface
    {
        // BINDING_TABLE_STATE
        DWORD   BindingTableOffset;

        // SURFACE_STATE
        DWORD   SurfaceCount;
        DWORD   SurfaceArrayOffset;

        DWORD   SurfaceOffset[ G6HWC::g_cNumSurfacesPerProgram ];

    } Surface;
};
CGen8OpenCLStateProcessor::CGen8OpenCLStateProcessor( PLATFORM platform, const IGC::OpenCLProgramContext &context ) :
    m_Context( context ),
    m_Platform( platform )
{
    G6HWC::InitializeCapsGen8( &m_HWCaps );

}

CGen8OpenCLStateProcessor::~CGen8OpenCLStateProcessor( )
{
    // Nothing
}

void CGen8OpenCLStateProcessor::CreateKernelBinary(
    const char*  rawIsaBinary,
    unsigned int rawIsaBinarySize,
    const IGC::SOpenCLKernelInfo& annotations,
    const IGC::SOpenCLProgramInfo& programInfo,
    const IGC::CBTILayout& layout,
    Util::BinaryStream& kernelBinary,
    USC::SSystemThreadKernelOutput* pSystemThreadKernelOutput,
    unsigned int unpaddedBinarySize)
{
    Util::BinaryStream kernelHeap;
    Util::BinaryStream surfaceStateHeap;
    Util::BinaryStream dynamicStateHeap;
    Util::BinaryStream generalStateHeap;
    Util::BinaryStream patchListHeap;

    SStateProcessorContextGen8_0 kernelContext = {};

    RETVAL retValue = g_cInitRetValue;

    const bool gtpinEnabled = GTPIN_IGC_OCL_IsEnabled();

    ICBE_DPF(GFXDBG_HARDWARE, "\n");
    ICBE_DPF(GFXDBG_HARDWARE,
        "** Kernel Patch Lists : Kernel Name = %s **\n", annotations.m_kernelName.c_str());
    ICBE_DPF(GFXDBG_HARDWARE, "\n");

    if( retValue.Success )
    {
        retValue = CreateKernelHeap( 
            annotations,
            rawIsaBinary, 
            rawIsaBinarySize,
            pSystemThreadKernelOutput,
            kernelContext,
            kernelHeap );
    }

    if( retValue.Success )
    {
        retValue = CreateSurfaceStateHeap( 
            annotations,
            layout,
            gtpinEnabled,
            kernelContext,
            surfaceStateHeap );
    }

    if( retValue.Success )
    {
        retValue = CreateDynamicStateHeap( 
            annotations,
            kernelContext,
            dynamicStateHeap );
    }

    if( retValue.Success )
    {
        retValue = CreatePatchList( 
            annotations, 
            programInfo,
            layout,
            kernelContext,
            gtpinEnabled,
            patchListHeap );
    }

    if( retValue.Success )
    {
        retValue = CombineKernelBinary(
            kernelContext,
            annotations,
            kernelHeap,
            generalStateHeap,
            dynamicStateHeap,
            surfaceStateHeap,
            patchListHeap,
            unpaddedBinarySize,
            kernelBinary );
    }

    if (IGC_IS_FLAG_ENABLED(EnableCosDump))
    {
        auto name = DumpName(IGC::Debug::GetShaderOutputName())
            .Hash(m_Context.hash)
            .Type(ShaderType::OPENCL_SHADER)
            .PostFix("kernel_" + annotations.m_kernelName + std::to_string( annotations.m_executionEnivronment.CompiledSIMDSize))
            .Extension("cos");
        Dump dump(name, DumpType::COS_TEXT);

        IGC::Debug::DumpLock();
        dump.stream() << m_oclStateDebugMessagePrintOut;
        IGC::Debug::DumpUnlock();
        m_oclStateDebugMessagePrintOut.clear();
    }
}

void CGen8OpenCLStateProcessor::CreateProgramScopePatchStream(const IGC::SOpenCLProgramInfo& annotations,
                                                              Util::BinaryStream& membuf)
{
    RETVAL retValue = g_cInitRetValue;

    ICBE_DPF(GFXDBG_HARDWARE, "\n");
    ICBE_DPF(GFXDBG_HARDWARE, "** Program Scope patch lists **\n");
    ICBE_DPF(GFXDBG_HARDWARE, "\n");

    for (auto &iter : annotations.m_initConstantAnnotation)
    {
        iOpenCL::SPatchAllocateConstantMemorySurfaceProgramBinaryInfo   patch;
        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
        patch.Size = sizeof( patch );
        patch.ConstantBufferIndex = DEFAULT_CONSTANT_BUFFER_INDEX;
        patch.InlineDataSize = (DWORD)iter->InlineData.size();

        retValue = AddPatchItem(
            patch,
            membuf );

        // And now write the actual data
        membuf.Write((char*)iter->InlineData.data(), 
                     iter->InlineData.size());
    }

    for (auto &iter : annotations.m_initGlobalAnnotation)
    {
        iOpenCL::SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo   patch;
        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
        patch.Size = sizeof( patch );
        patch.Type = iOpenCL::GLOBAL_BUFFER_TYPE_INLINE;
        patch.GlobalBufferIndex = 0;
        patch.InlineDataSize = (DWORD)iter->InlineData.size();

        retValue = AddPatchItem(
            patch,
            membuf );

        // And now write the actual data
        membuf.Write((char*)iter->InlineData.data(), 
                     iter->InlineData.size());
    }


    for (auto &iter : annotations.m_initKernelTypeAnnotation)
    {
        iOpenCL::SPatchKernelTypeProgramBinaryInfo    patch;

        memset(&patch, 0, sizeof(patch));

        patch.Token = iOpenCL::PATCH_TOKEN_CONSTRUCTOR_DESTRUCTOR_KERNEL_PROGRAM_BINARY_INFO;
        patch.Size = sizeof(patch);
        patch.Type = iter->Type;
        patch.InlineDataSize = iter->KernelName.size();

        retValue = AddPatchItem(
            patch,
            membuf);
        // And now write the actual data
        membuf.Write((char*)iter->KernelName.data(),
            iter->KernelName.size());
    }

    for (auto &iter : annotations.m_initGlobalPointerAnnotation)
    {
        iOpenCL::SPatchGlobalPointerProgramBinaryInfo patch;
        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO;
        patch.Size  = sizeof( patch );
        patch.GlobalBufferIndex = iter->PointerBufferIndex;
        patch.GlobalPointerOffset = iter->PointerOffset;
        patch.BufferType = (iter->PointeeAddressSpace == IGC::ADDRESS_SPACE_GLOBAL) ?
            PROGRAM_SCOPE_GLOBAL_BUFFER : PROGRAM_SCOPE_CONSTANT_BUFFER;
        patch.BufferIndex = iter->PointeeBufferIndex;

        retValue = AddPatchItem(
            patch,
            membuf );
    }

    for (auto &iter : annotations.m_initConstantPointerAnnotation)
    {
        iOpenCL::SPatchConstantPointerProgramBinaryInfo patch;
        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO;
        patch.Size  = sizeof( patch );
        patch.ConstantBufferIndex = iter->PointerBufferIndex;
        patch.ConstantPointerOffset = iter->PointerOffset;
        patch.BufferType = (iter->PointeeAddressSpace == IGC::ADDRESS_SPACE_GLOBAL) ?
            PROGRAM_SCOPE_GLOBAL_BUFFER : PROGRAM_SCOPE_CONSTANT_BUFFER;
        patch.BufferIndex = iter->PointeeBufferIndex;

        retValue = AddPatchItem(
            patch,
            membuf );
    }
}

void CGen8OpenCLStateProcessor::CreateKernelDebugData(
    const char*  rawDebugDataVISA,
    unsigned int rawDebugDataVISASize,
    const char*  rawDebugDataGenISA,
    unsigned int rawDebugDataGenISASize,
    const std::string& kernelName,
    Util::BinaryStream& kernelDebugData)
{
    RETVAL retValue = g_cInitRetValue;

    Util::BinaryStream kernelVisaDbg;
    Util::BinaryStream kernelGenIsaDbg;

    // Source -> VISA debug info
    if( kernelVisaDbg.Write( rawDebugDataVISA, rawDebugDataVISASize ) == false )
    {
        ICBE_ASSERT( 0 );
        retValue.Success = false;
    }
    kernelVisaDbg.Align( sizeof( DWORD ) );

    // VISA -> Gen ISA debug info
    if( kernelGenIsaDbg.Write( rawDebugDataGenISA, rawDebugDataGenISASize ) == false )
    {
        ICBE_ASSERT( 0 );
        retValue.Success = false;
    }
    kernelGenIsaDbg.Align( sizeof( DWORD ) );

    if( retValue.Success )
    {
        iOpenCL::SKernelDebugDataHeaderIGC header;


        memset( &header, 0, sizeof( header ) );

        header.KernelNameSize = (uint32_t)kernelName.size() + 1;
        header.KernelNameSize += GetAlignmentOffset( header.KernelNameSize, sizeof(DWORD) );
        header.SizeVisaDbgInBytes = (uint32_t)kernelVisaDbg.Size();
        header.SizeGenIsaDbgInBytes = (uint32_t)kernelGenIsaDbg.Size();

        kernelDebugData.Write( header );
        kernelDebugData.Write( kernelName.c_str(), kernelName.size() + 1 );
        kernelDebugData.Align( sizeof (DWORD ) );
        kernelDebugData.Write( kernelVisaDbg );
        kernelDebugData.Write( kernelGenIsaDbg );

        ICBE_ASSERT( IsAligned( kernelDebugData.Size(),  sizeof( DWORD ) ) );
    }
}

const G6HWC::SMediaHardwareCapabilities& CGen8OpenCLStateProcessor::HWCaps() const
{
    return m_HWCaps;
}

/*****************************************************************************\

Function:
CGen8StateProcessor::AddSystemKernel

Description:

Input:

Output:
RETVAL

\*****************************************************************************/
RETVAL CGen8OpenCLStateProcessor::AddSystemKernel(
    const USC::SSystemThreadKernelOutput* pSystemThreadKernelOutput,
    SStateProcessorContextGen8_0& context,
    Util::BinaryStream& membuf)
{
    RETVAL retValue = g_cInitRetValue;

    assert(context.Kernel.SystemKernelPresent == false);

    context.Kernel.SystemKernelPresent = true;
    context.Kernel.SystemKernelOffset = (DWORD)membuf.Size();

    // Add the system kernel.
    if (retValue.Success)
    {
        const void* pKernel = pSystemThreadKernelOutput->m_pKernelProgram;
        const DWORD size = pSystemThreadKernelOutput->m_KernelProgramSize;

        if (membuf.Write(static_cast<const char*>(pKernel), size) == false)
        {
            ICBE_ASSERT(0);
            retValue.Success = false;
        }

        membuf.AddPadding(HWCaps().InstructionCachePrefetchSize);
        membuf.Align(sizeof(DWORD));
    }

    return retValue;
}

RETVAL CGen8OpenCLStateProcessor::CreateKernelHeap( 
    const IGC::SOpenCLKernelInfo& annotations, 
    const char* kernelBinary,
    unsigned int kernelBinarySize,
    USC::SSystemThreadKernelOutput* pSystemThreadKernelOutput,
    SStateProcessorContextGen8_0& context,
    Util::BinaryStream& membuf )
{
    RETVAL retValue = g_cInitRetValue;

    // Add the system kernel if required.
    {
        const auto options = m_Context.m_InternalOptions;
        if (retValue.Success &&
            (options.IncludeSIPCSR ||
             options.IncludeSIPKernelDebug ||
             options.IncludeSIPKernelDebugWithLocalMemory))
        {
            retValue = AddSystemKernel(
                pSystemThreadKernelOutput,
                context,
                membuf);
        }
    }

    if (retValue.Success)
    {
        if (membuf.Write(kernelBinary, kernelBinarySize) == false)
        {
            ICBE_ASSERT(0);
            retValue.Success = false;
        }

        membuf.AddPadding(HWCaps().InstructionCachePrefetchSize);
        membuf.Align(sizeof(DWORD));
    }

    return retValue;
}

RETVAL CGen8OpenCLStateProcessor::CreateSurfaceStateHeap( 
    const IGC::SOpenCLKernelInfo& annotations, 
    const IGC::CBTILayout& layout,
    const bool gtpinEnabled,
    SStateProcessorContextGen8_0& context,
    Util::BinaryStream& membuf )
{
    struct SurfaceState {
        SURFACE_TYPE type;
        SURFACE_FORMAT surfaceFormat;
        DWORD bufferLength;
        bool isMultiSampleImage;
        SurfaceState(SURFACE_TYPE type, SURFACE_FORMAT surfaceFormat,
            DWORD bufferLength, bool isMultiSampleImage) :
            type(type), surfaceFormat(surfaceFormat), bufferLength(bufferLength),
                isMultiSampleImage(isMultiSampleImage) {}
    };

    RETVAL retValue = g_cInitRetValue;

    // collect all the surface state entries in an ordered map keyed on the BTI.
    // Walking the map at the end will insert all the BTI entries in ascending order.
    std::map<unsigned, SurfaceState> SurfaceStates;

    // First, System Thread Surface
    if (retValue.Success && m_Context.m_InternalOptions.KernelDebugEnable)
    {
        unsigned int bti = layout.GetSystemThreadBindingTableIndex();

        SurfaceStates.insert(
            std::make_pair(
                bti,
                SurfaceState(
                    SURFACE_BUFFER,
                    SURFACE_FORMAT_UNKNOWN,
                    0,
                    false)));
    }

    // Now, add the constant buffer, if present.
    for( auto iter : annotations.m_pointerInput )
    {
        PointerInputAnnotation* annotation = iter;
        unsigned int bti = annotations.m_argIndexMap.at(annotation->ArgumentNumber);
        context.Surface.SurfaceOffset[ bti ] = (DWORD)membuf.Size();

        SurfaceStates.insert(
            std::make_pair(
                bti, 
                SurfaceState(
                    SURFACE_BUFFER,
                    SURFACE_FORMAT_RAW,
                    0,
                    false)));
    }

    for( auto iter : annotations.m_pointerArgument )
    {
        PointerArgumentAnnotation* annotation = iter;
        unsigned int bti = annotations.m_argIndexMap.at(annotation->ArgumentNumber);
        context.Surface.SurfaceOffset[bti] = (DWORD)membuf.Size();

        SurfaceStates.insert(
            std::make_pair(
                bti, 
                SurfaceState(
                    SURFACE_BUFFER,
                    SURFACE_FORMAT_UNKNOWN,
                    0,
                    false)));
    }

    // Images
    for( auto i : annotations.m_imageInputAnnotations )
    {
        ImageArgumentAnnotation* annotation = i;

        if( annotation->IsFixedBindingTableIndex )
        {
            bool isMultiSampleImage = false;

            SURFACE_TYPE surfaceType = SURFACE_UNKNOWN;

            switch( annotation->ImageType ) 
            {
            case iOpenCL::IMAGE_MEMORY_OBJECT_BUFFER:
                surfaceType = SURFACE_BUFFER;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_1D:
                surfaceType = SURFACE_1D;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_1D_ARRAY:
                surfaceType = SURFACE_1D_ARRAY;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_2D:
                surfaceType = SURFACE_2D;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_2D_ARRAY:
                surfaceType = SURFACE_2D_ARRAY;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_3D:
                surfaceType = SURFACE_3D;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_CUBE:
                surfaceType = SURFACE_CUBE;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_CUBE_ARRAY:
                surfaceType = SURFACE_CUBE_ARRAY;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_2D_DEPTH:
                surfaceType = SURFACE_2D;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_2D_ARRAY_DEPTH:
                surfaceType = SURFACE_2D_ARRAY;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA:
                surfaceType = SURFACE_2D_MEDIA;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA_BLOCK:
                surfaceType = SURFACE_2D_MEDIA_BLOCK;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_2D_MSAA:
                surfaceType = SURFACE_2D;
                isMultiSampleImage = true;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_2D_MSAA_DEPTH:
                surfaceType = SURFACE_2D;
                isMultiSampleImage = true;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_2D_ARRAY_MSAA:
                surfaceType = SURFACE_2D_ARRAY;
                isMultiSampleImage = true;
                break;
            case iOpenCL::IMAGE_MEMORY_OBJECT_2D_ARRAY_MSAA_DEPTH:
                surfaceType = SURFACE_2D_ARRAY;
                isMultiSampleImage = true;
                break;
            default:
                // unknown image annotation type
                ICBE_ASSERT( 0 );
                retValue.Success = false;
                isMultiSampleImage =  false;
            }

            unsigned int bti = annotations.m_argIndexMap.at(annotation->ArgumentNumber);

            SurfaceStates.insert(
                  std::make_pair(
                      bti, 
                      SurfaceState(
                          surfaceType,
                          SURFACE_FORMAT_UNKNOWN,
                          0,
                          isMultiSampleImage)));

        }
        else
        {
            retValue.Success = false;
            ICBE_ASSERT( 0 );
        }
    }

    if (annotations.m_printfBufferAnnotation != NULL)
    {
        unsigned int bti = annotations.m_argIndexMap.at(annotations.m_printfBufferAnnotation->ArgumentNumber);
        context.Surface.SurfaceOffset[bti] = (DWORD)membuf.Size();

        SurfaceStates.insert(
            std::make_pair(
                bti,
                SurfaceState(
                    SURFACE_BUFFER,
                    SURFACE_FORMAT_RAW,
                    0,
                    false)));
    }

    // If GT-Pin is enabled, assign btis to GT-Pin's surfaces
    if (gtpinEnabled)
    {
        const unsigned numGTPinSurfaces = 2;
        assert(GTPIN_IGC_OCL_NumberOfSurfaces() == numGTPinSurfaces);
        unsigned btiAssigned[numGTPinSurfaces];

        for ( unsigned i = 0; i < numGTPinSurfaces; i++ )
        {
            bool found = false;
            const int maxUseableBTI = 250;
            for (int bti=0; bti<maxUseableBTI; bti++)
            {
                if (SurfaceStates.find(bti) == SurfaceStates.end())
                {
                    btiAssigned[i] = bti;

                    SurfaceStates.insert(
                        std::make_pair(
                            bti,
                            SurfaceState(
                                SURFACE_BUFFER,
                                SURFACE_FORMAT_UNKNOWN,
                                0,
                                false)));
                    found = true;
                    break;
                }
            }

            if (! found)
            {
                printf("GTPIN_IGC_OCL Error: Fail to find a free BTI for GT-Pin surface %d\n", i);
                assert(0);
            }
        }
        const int res = GTPIN_IGC_OCL_UpdateKernelInfo(0, btiAssigned[0], btiAssigned[1]);
        if (res)
        {
            printf("GTPIN_IGC_OCL Error: Failed to call GTPIN_IGC_OCL_UpdateKernelInfo\n");
        }
        assert(res == 0);
    }

    // Fill up the SSH with BTI offsets increasing.  The runtime currently
    // expects this format.
    for (auto &kv : SurfaceStates)
    {
        const unsigned bti = kv.first;
        const SurfaceState &state = kv.second;

        if (retValue.Success)
        {
            context.Surface.SurfaceOffset[ bti ] = (DWORD)membuf.Size();
            retValue = AddSurfaceState(
                    state.type,
                    state.surfaceFormat,
                    state.bufferLength,
                    state.isMultiSampleImage,
                    membuf );
        }
    }

    const DWORD btiCount = (DWORD)SurfaceStates.size();
    context.Surface.SurfaceCount = btiCount;

    if( btiCount && retValue.Success )
    {
        const DWORD alignment = HWCaps().BindingTableStatePointerAlignSize;
        membuf.Align( alignment );

        context.Surface.BindingTableOffset = (DWORD)membuf.Size();        

        for( DWORD i = 0; i < btiCount; i++ )
        {
            G6HWC::SSharedStateBindingTableState    bts =
                G6HWC::g_cInitSharedStateBindingTableState;

            bts.DW0.All.SurfaceStatePointer =
                context.Surface.SurfaceOffset[ i ] /
                HWCaps().SurfaceStatePointerAlignSize;

            if( membuf.Write( bts ) == false )
            {
                retValue.Success = false;
            }
        }
#if ( defined( _DEBUG ) || defined( _INTERNAL ) || defined( _RELEASE_INTERNAL ) )
        {
            G6HWC::DebugBindingTableStateCommand(
                membuf.GetLinearPointer() + context.Surface.BindingTableOffset,
                btiCount,
                m_Platform );
        }
#endif
    }

    return retValue;
}

RETVAL CGen8OpenCLStateProcessor::CreateDynamicStateHeap( 
    const IGC::SOpenCLKernelInfo& annotations, 
    SStateProcessorContextGen8_0& context,
    Util::BinaryStream& membuf )
{
    RETVAL retValue = g_cInitRetValue;

    const DWORD numArgumentSamplers = (DWORD)annotations.m_samplerArgument.size();
    const DWORD numInlineSamplers = (DWORD)annotations.m_samplerInput.size();
    const DWORD numSamplers = numArgumentSamplers + numInlineSamplers;

    // Driver may assume that sampler states are in consecutive locations, so 
    // allocate border color states first, so the layout will be:
    // |BC0|BC1|BC2|...|SS0|SS1|SS2|...
    // (instead of |BC0|SS0|BC1|SS1|...)

    std::vector<DWORD> borderColorOffsets(numSamplers);
    if (borderColorOffsets.size() != numSamplers)
    {
        retValue.Success = false;
    }

    if( numSamplers && retValue.Success )
    {
        // Handle border color state:
        if (m_Context.m_DriverInfo.ProgrammableBorderColorInCompute())
        {
            // Indirect states for argument samplers:
            for (DWORD i = 0; i < numArgumentSamplers && retValue.Success; i++)
            {
                DWORD borderColorOffset = AllocateSamplerIndirectState(G6HWC::g_cInitGfxSamplerIndirectState, membuf);
                borderColorOffsets[i] = borderColorOffset;
            }

            //  Indirect states for inline samplers:
            for (DWORD i = 0; i < numInlineSamplers && retValue.Success; i++)
            {
                const SamplerInputAnnotation* samplerAnnotation = annotations.m_samplerInput[i];

                G6HWC::SGfxSamplerIndirectState bcState = G6HWC::g_cInitGfxSamplerIndirectState;
                bcState.BorderColorRed      = samplerAnnotation->BorderColorR;
                bcState.BorderColorGreen    = samplerAnnotation->BorderColorG;
                bcState.BorderColorBlue     = samplerAnnotation->BorderColorB;
                bcState.BorderColorAlpha    = samplerAnnotation->BorderColorA;

                DWORD borderColorOffset = AllocateSamplerIndirectState(bcState, membuf);
                borderColorOffsets[i + numArgumentSamplers] = borderColorOffset;
            }
        }
        else
        {
            // If border color is not programmable, we can create only one that will be shared 
            // among all sampler states used in a kernel.
            DWORD borderColorOffset = AllocateSamplerIndirectState(G6HWC::g_cInitGfxSamplerIndirectState, membuf);
            borderColorOffsets[0] = borderColorOffset;
        }

        // Now create sampler states:

        const DWORD alignment = HWCaps().SamplerStatePointerAlignSize;
        membuf.Align( alignment );

        context.Dynamic.SamplerCount = numSamplers;
        context.Dynamic.SamplerArrayOffset = (DWORD)membuf.Size();

        // We need to do this so patch.BorderColorOffset is set to any border color.
        context.Dynamic.SamplerBorderColorStateOffset = borderColorOffsets[0];

        DWORD borderColorIndex = 0;
        DWORD borderColorStep = m_Context.m_DriverInfo.ProgrammableBorderColorInCompute() ? 1 : 0;

        // First handle the sampler arguments
        for (auto i = annotations.m_samplerArgument.begin(); 
            i != annotations.m_samplerArgument.end(); 
            ++i, borderColorIndex += borderColorStep)
        {
            const SamplerArgumentAnnotation* samplerAnnotation = *i;

            context.Dynamic.SamplerOffset[samplerAnnotation->SamplerTableIndex] = (DWORD)membuf.Size();

            // No need for sampler state for VME and VA...
            if( samplerAnnotation->SamplerType == SAMPLER_OBJECT_TEXTURE )
            {
                SAMPLER_TEXTURE_ADDRESS_MODE    addressMode = SAMPLER_TEXTURE_ADDRESS_MODE_CLAMP;
                SAMPLER_MAPFILTER_TYPE          mapFilter = SAMPLER_MAPFILTER_POINT;
                SAMPLER_MIPFILTER_TYPE          mipFilter = SAMPLER_MIPFILTER_NONE;
                SAMPLER_COMPARE_FUNC_TYPE       compareFunc = SAMPLER_COMPARE_FUNC_NEVER;

                bool    normalizedCoords = true;
                bool    enable = true;

                assert(m_Context.m_DriverInfo.ProgrammableBorderColorInCompute() || borderColorIndex == 0);

                retValue = AddSamplerState(
                    enable,
                    addressMode,
                    addressMode,
                    addressMode,
                    mapFilter,
                    mapFilter,
                    mipFilter,
                    normalizedCoords,
                    compareFunc,
                    borderColorOffsets[borderColorIndex],
                    membuf );

                if( !retValue.Success )
                {
                    ICBE_DPF( GFXDBG_CRITICAL,
                        "Error during adding sampler state for sampler argument (index: %d, type: %d).\n",
                        samplerAnnotation->SamplerTableIndex,
                        samplerAnnotation->SamplerType );
                    ICBE_ASSERT( 0 );
                    break;
                }
            }
            // ... but for VA and VME we need to reserve size for sampler state on the heap:
            else
            {
                // Texture SAMPLER_STATE is located in the dynamic heap by
                // multiply the sampler index by 4 DWORDS.  This is not true
                // for VA samplers because they are longer in length.  This 
                // means sampler index 4, for example, can point to multiple
                // locations in the dynamic heap depending on if a SAMPLE
                // or VA_SAMPLE instruction is used.  We currently don't check
                // for overlapping SAMPLER_STATE.
                DWORD samplerStateSize = sizeof(G6HWC::SSharedStateSamplerState) *
                    GetSamplerStateSizeMultiplier( samplerAnnotation->SamplerType );

                membuf.AddPadding( samplerStateSize );
            }
        }

        if( retValue.Success )
        {
            assert(!m_Context.m_DriverInfo.ProgrammableBorderColorInCompute() || borderColorIndex == numArgumentSamplers);

            // And then the inline samplers
            for (auto i = annotations.m_samplerInput.begin(); 
                i != annotations.m_samplerInput.end(); 
                ++i, borderColorIndex += borderColorStep)
            {
                const SamplerInputAnnotation* samplerAnnotation = *i;

                context.Dynamic.SamplerOffset[samplerAnnotation->SamplerTableIndex] = (DWORD)membuf.Size( );

                ICBE_ASSERT( samplerAnnotation->SamplerType == SAMPLER_OBJECT_TEXTURE );

                bool enable = true;

                assert(m_Context.m_DriverInfo.ProgrammableBorderColorInCompute() || borderColorIndex == 0);

                retValue = AddSamplerState(
                    enable,
                    samplerAnnotation->TCXAddressMode,
                    samplerAnnotation->TCYAddressMode,
                    samplerAnnotation->TCZAddressMode,
                    samplerAnnotation->MagFilterType,
                    samplerAnnotation->MinFilterType,
                    samplerAnnotation->MipFilterType,
                    samplerAnnotation->NormalizedCoords,
                    samplerAnnotation->CompareFunc,
                    borderColorOffsets[borderColorIndex],
                    membuf );

                if( !retValue.Success )
                {
                    ICBE_DPF( GFXDBG_CRITICAL,
                        "Error during adding sampler state for inline sampler (index: %d, type: %d).\n",
                        samplerAnnotation->SamplerTableIndex,
                        samplerAnnotation->SamplerType );
                    ICBE_ASSERT( 0 );
                    break;
                }
            }
        }
    }

    // Interface Descriptor
    if( retValue.Success )
    {
        unsigned int alignment = HWCaps().InterfaceDescriptorDataAlignSize;

        if( membuf.Align( alignment ) == false )
        {
            retValue.Success = false;
        }

        context.Dynamic.InterfaceDescriptorDataOffset = (DWORD)membuf.Size();

        if( retValue.Success )
        {
            G6HWC::SMediaStateInterfaceDescriptorData ifdd =
                G6HWC::g_cInitMediaStateInterfaceDescriptorData;

            ifdd.DW1.Gen8 = G6HWC::g_cInitMediaStateInterfaceDescriptorDataDW1Gen8;
            ifdd.DW2.Gen8 = G6HWC::g_cInitMediaStateInterfaceDescriptorDataDW2Gen8;
            ifdd.DW3.Gen8 = G6HWC::g_cInitMediaStateInterfaceDescriptorDataDW3Gen8;
            ifdd.DW4.Gen8 = G6HWC::g_cInitMediaStateInterfaceDescriptorDataDW4Gen8;
            ifdd.DW5.Gen8 = G6HWC::g_cInitMediaStateInterfaceDescriptorDataDW5Gen8;

            ifdd.DW0.All.KernelStartPointer = 
                context.Kernel.KernelOffset /
                HWCaps().KernelPointerAlignSize;

            ifdd.DW2.Gen8.SoftwareExceptionEnable = false;
            ifdd.DW2.Gen8.MaskStackExceptionEnable = false;
            ifdd.DW2.Gen8.IllegalOpcodeExceptionEnable = false;
            ifdd.DW2.Gen8.FloatingPointMode = 
                G6HWC::GFXMEDIASTATE_FLOATING_POINT_IEEE_754;
            ifdd.DW2.Gen8.ThreadPriority = 
                G6HWC::GFXMEDIASTATE_THREAD_PRIORITY_NORMAL;

            ifdd.DW2.Gen8.SingleProgramFlow = 
                annotations.m_executionEnivronment.IsSingleProgramFlow;

            ifdd.DW3.Gen8.SamplerCount = 
                context.Dynamic.SamplerCount;
            ifdd.DW3.Gen8.SamplerStatePointer =
                context.Dynamic.SamplerArrayOffset /
                HWCaps().SamplerStatePointerAlignSize;

            ifdd.DW4.Gen8.BindingTableEntryCount = 
                context.Surface.SurfaceCount;
            ifdd.DW4.Gen8.BindingTablePointer    =
                context.Surface.BindingTableOffset /
                HWCaps().BindingTableStatePointerAlignSize;              

            ifdd.DW5.Gen8.ConstantURBEntryReadOffset = 0;
            ifdd.DW5.Gen8.ConstantURBEntryReadLength = 0;

            ifdd.DW7.Gen8.CrossThreadConstantDataReadLength = 
                annotations.m_kernelProgram.ConstantBufferLength;

            if( membuf.Write( ifdd ) == false )
            {
                retValue.Success = false;
            }

#if ( defined( _DEBUG ) || defined( _INTERNAL ) || defined( _RELEASE_INTERNAL ) )
            {
                G6HWC::DebugInterfaceDescriptorDataCommand(
                    &ifdd,
                    m_Platform );
            }
#endif
        }
    }

    return retValue;
}

DWORD CGen8OpenCLStateProcessor::AllocateSamplerIndirectState(
    const G6HWC::SGfxSamplerIndirectState& borderColor,
    Util::BinaryStream &membuf)
{
    const DWORD alignment = HWCaps().DefaultColorPointerAlignSize;

    membuf.Align(alignment);

    DWORD samplerIndirectStateOffset = (DWORD)membuf.Size();

    if (membuf.Write(borderColor) == false)
    {
        ICBE_ASSERT(0);
    }

#if ( defined( _DEBUG ) || defined( _INTERNAL ) || defined( _RELEASE_INTERNAL ) )
    {
        G6HWC::DebugSamplerIndirectStateCommand(
            &samplerIndirectStateOffset,
            m_Platform);
    }
#endif

    return samplerIndirectStateOffset;
}

RETVAL CGen8OpenCLStateProcessor::CreatePatchList(
    const IGC::SOpenCLKernelInfo& annotations, 
    const IGC::SOpenCLProgramInfo& programInfo,
    const IGC::CBTILayout& layout,
    const SStateProcessorContextGen8_0& context,
    const bool gtpinEnabled,
    Util::BinaryStream& membuf )
{
    RETVAL retValue = g_cInitRetValue;

    DWORD  dataParameterStreamSize = 0;

    struct structhasDeviceEnqueue{
       bool hasParentEvent;
       bool hasDefaultQueue;
       bool hasQueueArg;
       operator bool() const { return hasParentEvent && (hasDefaultQueue || hasQueueArg); };
       structhasDeviceEnqueue() : hasParentEvent(false), hasDefaultQueue(false), hasQueueArg(false){};
    }hasDeviceEnqueue;

    // Add a patch item for STATE_SIP, if required.
    if (retValue.Success &&
        context.Kernel.SystemKernelPresent)
    {
        iOpenCL::SPatchStateSIP patch;

        memset(&patch, 0, sizeof(patch));

        patch.Token = iOpenCL::PATCH_TOKEN_STATE_SIP;
        patch.Size = sizeof(patch);
        patch.SystemKernelOffset = context.Kernel.SystemKernelOffset;

        retValue = AddPatchItem(
            patch,
            membuf);
    }


    // Patch for MEDIA_VFE_STATE
    if( retValue.Success )
    {
      const DWORD perThreadScratchSpaceSizeInBytes = annotations.m_executionEnivronment.PerThreadSpillFillSize
        + annotations.m_executionEnivronment.PerThreadScratchSpace;
      if (perThreadScratchSpaceSizeInBytes > 0)
        {
            iOpenCL::SPatchMediaVFEState    patch;

            memset( &patch, 0, sizeof( patch ) );

            patch.Token = iOpenCL::PATCH_TOKEN_MEDIA_VFE_STATE;
            patch.Size = sizeof( patch );
            patch.ScratchSpaceOffset = 0; 

            patch.PerThreadScratchSpace = 
                iSTD::RoundPower2( 
                    iSTD::Max(perThreadScratchSpaceSizeInBytes,
                               static_cast<DWORD>( sizeof(KILOBYTE) ) ) );

            retValue = AddPatchItem(
                patch,
                membuf );
        }
    }

    // Patch for MEDIA_INTERFACE_DESCRIPTOR_LOAD
    if( retValue.Success )
    {
        iOpenCL::SPatchMediaInterfaceDescriptorLoad patch;

        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD;
        patch.Size = sizeof( patch );
        patch.InterfaceDescriptorDataOffset = context.Dynamic.InterfaceDescriptorDataOffset;

        retValue = AddPatchItem(
            patch,
            membuf );
    }

    if (annotations.m_HasInlineVmeSamplers)
    {
        iOpenCL::SPatchInlineVMESamplerInfo patch;

        iSTD::SafeMemSet(
            &patch,
            0,
            sizeof(patch));

        patch.Token = iOpenCL::PATCH_TOKEN_INLINE_VME_SAMPLER_INFO;
        patch.Size = sizeof(patch);
        retValue = AddPatchItem(
            patch,
            membuf);
    }

    // Patch for Samplers
    // Add patch items for samplers.
    if( retValue.Success &&
        context.Dynamic.SamplerCount )
    {
        // Add the patch item for the entire sampler state array.

        DWORD   count = context.Dynamic.SamplerCount;
        DWORD   offset = context.Dynamic.SamplerArrayOffset;
        DWORD   borderColorOffset = 
            context.Dynamic.SamplerBorderColorStateOffset;

        iOpenCL::SPatchSamplerStateArray    patch;
        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_SAMPLER_STATE_ARRAY;
        patch.Size = sizeof( patch );
        patch.Offset = offset;
        patch.Count = count;
        patch.BorderColorOffset = borderColorOffset;

        retValue = AddPatchItem(
            patch,
            membuf );

        // Add a patch item for each sampler kernel argument.

        if( retValue.Success )
        {
            for( SamplerArgumentIterator i = annotations.m_samplerArgument.begin(); 
                 i != annotations.m_samplerArgument.end(); 
                 ++i )
            {
                SamplerArgumentAnnotation* samplerAnnotation = *i;

                iOpenCL::SPatchSamplerKernelArgument    patch;
                memset( &patch, 0, sizeof( patch ) );

                patch.Token = iOpenCL::PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT;
                patch.Size = sizeof( patch );
                patch.ArgumentNumber = samplerAnnotation->ArgumentNumber;
                patch.Offset = samplerAnnotation->IsBindlessAccess ? samplerAnnotation->PayloadPosition : context.Dynamic.SamplerOffset[samplerAnnotation->SamplerTableIndex];
                patch.btiOffset = context.Dynamic.SamplerOffset[samplerAnnotation->SamplerTableIndex];
                patch.LocationIndex = samplerAnnotation->LocationIndex;
                patch.LocationIndex2 = samplerAnnotation->LocationCount;
                patch.Type = samplerAnnotation->SamplerType;
				patch.needBindlessHandle = samplerAnnotation->IsBindlessAccess;
                patch.IsEmulationArgument = samplerAnnotation->IsEmulationArgument;

                if(samplerAnnotation->IsBindlessAccess)
                {
                    dataParameterStreamSize = std::max(
                        dataParameterStreamSize,
                        samplerAnnotation->PayloadPosition + 8);
                }

                if( retValue.Success )
                {
                    retValue = AddPatchItem(
                        patch,
                        membuf );
                }
            }
        }
    }


    // Patch for INTERFACE_DESCRIPTOR_DATA
    if( retValue.Success )
    {
        iOpenCL::SPatchInterfaceDescriptorData  patch;

        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA;
        patch.Size = sizeof( patch );
        patch.Offset = context.Dynamic.InterfaceDescriptorDataOffset;
        patch.SamplerStateOffset = context.Dynamic.SamplerArrayOffset;
        patch.KernelOffset = context.Kernel.KernelOffset;
        patch.BindingTableOffset = context.Surface.BindingTableOffset;

        retValue = AddPatchItem(
            patch,
            membuf );
    }

    // Add a patch item for BINDING_TABLE_STATE.
    if( retValue.Success )
    {
        iOpenCL::SPatchBindingTableState    patch;

        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_BINDING_TABLE_STATE;
        patch.Size = sizeof( patch );
        patch.Offset = context.Surface.BindingTableOffset;
        patch.Count = context.Surface.SurfaceCount;
        patch.SurfaceStateOffset = context.Surface.SurfaceArrayOffset;

        retValue = AddPatchItem(
            patch,
            membuf );
    }

    // Patch for SRVs & UAVs
    if( retValue.Success )
    {
        bool transformable = InlineSamplersAllow3DImageTransformation(annotations); 
        
        for (ImageArgumentIterator i = annotations.m_imageInputAnnotations.begin();
            i != annotations.m_imageInputAnnotations.end(); 
            ++i )
        {
            ImageArgumentAnnotation* imageInput = *i;

            iOpenCL::SPatchImageMemoryObjectKernelArgument  patch;

            memset( &patch, 0, sizeof( patch ) );

            unsigned int bti = annotations.m_argIndexMap.at(imageInput->ArgumentNumber);
            patch.Token = iOpenCL::PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT;
            patch.Size = sizeof( patch );
            patch.ArgumentNumber = imageInput->ArgumentNumber;
			patch.Offset = imageInput->IsBindlessAccess ? imageInput->PayloadPosition : context.Surface.SurfaceOffset[bti];
            patch.btiOffset = context.Surface.SurfaceOffset[bti];
            patch.Type =  imageInput->ImageType;
            patch.Writeable = imageInput->Writeable;
            patch.LocationIndex = imageInput->LocationIndex;
            patch.LocationIndex2 = imageInput->LocationCount;
			patch.needBindlessHandle = imageInput->IsBindlessAccess;
            patch.IsEmulationArgument = imageInput->IsEmulationArgument;

            if(imageInput->IsBindlessAccess)
            {
                dataParameterStreamSize = std::max(
                    dataParameterStreamSize,
                    imageInput->PayloadPosition + 8);
            }

            patch.Transformable = transformable &&
                (imageInput->AccessedByIntCoords && !imageInput->AccessedByFloatCoords &&
                imageInput->ImageType == IMAGE_MEMORY_OBJECT_3D);

            if( retValue.Success )
            {
                retValue = AddPatchItem(
                    patch,
                    membuf );
            }
        }
    }

    // Patch for TGSM
    if( retValue.Success )
    {
        if( annotations.m_executionEnivronment.SumFixedTGSMSizes > 0 )
        {
            iOpenCL::SPatchAllocateLocalSurface patch;
            memset( &patch, 0, sizeof( patch ) );

            patch.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE;
            patch.Size = sizeof( patch );
            patch.TotalInlineLocalMemorySize = 
                annotations.m_executionEnivronment.SumFixedTGSMSizes;

            retValue = AddPatchItem(
                patch,
                membuf );  
        }
    }     

    // Patch for Private Memory

    // Patch for Surface State Scratch Space

    // Patch for ALLOCATE_SIP_SURFACE
    if (retValue.Success && m_Context.m_InternalOptions.KernelDebugEnable)
    {
        unsigned int bti = layout.GetSystemThreadBindingTableIndex();

        iOpenCL::SPatchAllocateSystemThreadSurface patch;

        memset(&patch, 0, sizeof(patch));

        patch.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_SIP_SURFACE;
        patch.Size = sizeof(patch);
        patch.Offset = context.Surface.SurfaceOffset[bti];
        patch.BTI = bti;
        patch.PerThreadSystemThreadSurfaceSize = (unsigned int)0x1800;// SIP::cGen8SIPThreadScratchSize;

        retValue = AddPatchItem(
            patch,
            membuf);
    }
    // Patch for DataParameterStream

    // Patch information for variable TGSM data parameters:
    if( retValue.Success )
    {
        for( LocalArgumentIterator i = annotations.m_localPointerArgument.begin(); 
            i != annotations.m_localPointerArgument.end(); 
            ++i )
        {
            LocalArgumentAnnotation* localArg = *i;

            iOpenCL::SPatchDataParameterBuffer  patch;

            memset( &patch, 0, sizeof( patch ) );

            patch.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER;
            patch.Size = sizeof( patch );
            patch.ArgumentNumber = localArg->ArgumentNumber;
            patch.DataSize = localArg->PayloadSizeInBytes;
            patch.SourceOffset = localArg->Alignment;
            patch.Offset = localArg->PayloadPosition;
            patch.Type =  iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES;

            dataParameterStreamSize = std::max( 
                dataParameterStreamSize, 
                localArg->PayloadPosition + localArg->PayloadSizeInBytes );

            if( retValue.Success )
            {
                retValue = AddPatchItem(
                    patch,
                    membuf );
            }
        }
    }

    //     Constant Input Parameters
    if( retValue.Success )
    {
        for( ConstantInputIterator i = annotations.m_constantInputAnnotation.begin(); 
            i != annotations.m_constantInputAnnotation.end(); 
            ++i )
        {
            ConstantInputAnnotation* constInput = *i;

            iOpenCL::SPatchDataParameterBuffer  patch;

            memset( &patch, 0, sizeof( patch ) );

            patch.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER;
            patch.Size = sizeof( patch );
            patch.ArgumentNumber = constInput->ArgumentNumber;
            patch.DataSize = constInput->PayloadSizeInBytes;
            patch.SourceOffset = constInput->Offset;
            patch.Offset = constInput->PayloadPosition;
            patch.Type =  constInput->ConstantType;
            if( patch.Type == iOpenCL::DATA_PARAMETER_PARENT_EVENT )
            {
               hasDeviceEnqueue.hasParentEvent = true;
            }
            patch.LocationIndex = constInput->LocationIndex;
            patch.LocationIndex2 = constInput->LocationCount;

            dataParameterStreamSize = std::max( 
                dataParameterStreamSize, 
                constInput->PayloadPosition + constInput->PayloadSizeInBytes );

            if( retValue.Success )
            {
                retValue = AddPatchItem(
                    patch,
                    membuf );
            }
        }
    }
    
    //     Pointer Kernel Arguments
    if( retValue.Success )
    {
        for( PointerArgumentIterator i = annotations.m_pointerArgument.begin(); 
            i != annotations.m_pointerArgument.end(); 
            ++i )
        {
            PointerArgumentAnnotation* ptrArg = *i;

            if( ptrArg->IsStateless == true || ptrArg->IsBindlessAccess)
            {
                if( ptrArg->AddressSpace == KERNEL_ARGUMENT_ADDRESS_SPACE_GLOBAL )
                {                    
                    iOpenCL::SPatchStatelessGlobalMemoryObjectKernelArgument patch;

                    memset( &patch, 0, sizeof( patch ) );

                    unsigned int bti = annotations.m_argIndexMap.at(ptrArg->ArgumentNumber);        
                    patch.Token = iOpenCL::PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT;
                    patch.Size = sizeof( patch );
                    patch.ArgumentNumber = ptrArg->ArgumentNumber;
                    patch.SurfaceStateHeapOffset = context.Surface.SurfaceOffset[ bti ];
                    patch.DataParamOffset = ptrArg->PayloadPosition;
                    patch.DataParamSize = ptrArg->PayloadSizeInBytes;
                    patch.LocationIndex = ptrArg->LocationIndex;
                    patch.LocationIndex2 = ptrArg->LocationCount;
                    patch.IsEmulationArgument = ptrArg->IsEmulationArgument;

                    dataParameterStreamSize = std::max( 
                        dataParameterStreamSize, 
                        ptrArg->PayloadPosition + ptrArg->PayloadSizeInBytes );

                    retValue = AddPatchItem(
                        patch,
                        membuf );
                }
                else if(ptrArg->AddressSpace == KERNEL_ARGUMENT_ADDRESS_SPACE_CONSTANT)
                {
                    iOpenCL::SPatchStatelessConstantMemoryObjectKernelArgument patch;

                    memset( &patch, 0, sizeof( patch ) );

                    unsigned int bti = annotations.m_argIndexMap.at(ptrArg->ArgumentNumber);  
                    patch.Token = iOpenCL::PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT;
                    patch.Size = sizeof( patch );
                    patch.ArgumentNumber = ptrArg->ArgumentNumber;
                    patch.SurfaceStateHeapOffset = context.Surface.SurfaceOffset[ bti ];
                    patch.DataParamOffset = ptrArg->PayloadPosition;
                    patch.DataParamSize = ptrArg->PayloadSizeInBytes;
                    patch.LocationIndex = ptrArg->LocationIndex;
                    patch.LocationIndex2 = ptrArg->LocationCount;
                    patch.IsEmulationArgument = ptrArg->IsEmulationArgument;

                    dataParameterStreamSize = std::max( 
                        dataParameterStreamSize, 
                        ptrArg->PayloadPosition + ptrArg->PayloadSizeInBytes );

                    retValue = AddPatchItem(
                        patch,
                        membuf );
                }
                else if(ptrArg->AddressSpace == KERNEL_ARGUMENT_ADDRESS_SPACE_DEVICE_QUEUE)
                {
                    iOpenCL::SPatchStatelessDeviceQueueKernelArgument    patch;

                    memset(&patch, 0, sizeof(patch));

                    hasDeviceEnqueue.hasQueueArg = true;

                    unsigned int bti = annotations.m_argIndexMap.at(ptrArg->ArgumentNumber);

                    patch.Token = iOpenCL::PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT;
                    patch.Size = sizeof(patch);
                    patch.ArgumentNumber = ptrArg->ArgumentNumber;
                    patch.SurfaceStateHeapOffset = context.Surface.SurfaceOffset[bti];;
                    patch.DataParamOffset = ptrArg->PayloadPosition;
                    patch.DataParamSize = ptrArg->PayloadSizeInBytes;
                    patch.LocationIndex = ptrArg->LocationIndex;
                    patch.LocationIndex2 = ptrArg->LocationCount;
                    patch.IsEmulationArgument = ptrArg->IsEmulationArgument;

                    dataParameterStreamSize = std::max(
                        dataParameterStreamSize,
                        ptrArg->PayloadPosition + ptrArg->PayloadSizeInBytes);

                    retValue = AddPatchItem(
                        patch,
                        membuf);
                }
                else
                {
                    retValue.Success = false;
                    assert(0);
                }
            }
            else
            {
                // Pointer Kernel arguments must be stateless
                retValue.Success = false;
            }
        }
    }

    // Patch for Printf Output Buffer Offset
    if (retValue.Success)
    {
        if (annotations.m_printfBufferAnnotation != nullptr)
        {
            iOpenCL::PrintfBufferAnnotation  *printfBufAnn = annotations.m_printfBufferAnnotation;

            iOpenCL::SPatchAllocateStatelessPrintfSurface  patch;
            memset(&patch, 0, sizeof(patch));

            unsigned int bti = annotations.m_argIndexMap.at(printfBufAnn->ArgumentNumber);

            patch.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE;
            patch.Size = sizeof(patch);
            patch.PrintfSurfaceIndex = printfBufAnn->Index;
            patch.SurfaceStateHeapOffset = context.Surface.SurfaceOffset[bti];
            patch.DataParamOffset = printfBufAnn->PayloadPosition;
            patch.DataParamSize = printfBufAnn->DataSize;

            dataParameterStreamSize = std::max(
                dataParameterStreamSize,
                printfBufAnn->PayloadPosition + printfBufAnn->DataSize );

            retValue = AddPatchItem(patch, membuf);
        }
    }

    // Pointer inputs with initializer
    if( retValue.Success )
    {
        for( PointerInputIterator i = annotations.m_pointerInput.begin(); 
            i != annotations.m_pointerInput.end(); 
            ++i )
        {
            PointerInputAnnotation* ptrArg = *i;

            if( ptrArg->IsStateless == true )
            {
                if(ptrArg->AddressSpace == iOpenCL::KERNEL_ARGUMENT_ADDRESS_SPACE_CONSTANT) 
                {
                    iOpenCL::SPatchAllocateStatelessConstantMemorySurfaceWithInitialization patch;
                    memset( &patch, 0, sizeof( patch ) );

                    unsigned int bti = annotations.m_argIndexMap.at(ptrArg->ArgumentNumber);
                    patch.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_STATELESS_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION;
                    patch.Size = sizeof( patch );
                    patch.SurfaceStateHeapOffset = context.Surface.SurfaceOffset[ bti ];
                    patch.DataParamOffset = ptrArg->PayloadPosition;
                    patch.DataParamSize = ptrArg->PayloadSizeInBytes;
                    patch.ConstantBufferIndex = DEFAULT_CONSTANT_BUFFER_INDEX;

                    dataParameterStreamSize = std::max( 
                        dataParameterStreamSize, 
                        ptrArg->PayloadPosition + ptrArg->PayloadSizeInBytes );

                    retValue = AddPatchItem(
                        patch,
                        membuf );
                }
                else if(ptrArg->AddressSpace == iOpenCL::KERNEL_ARGUMENT_ADDRESS_SPACE_GLOBAL) 
                {
                    iOpenCL::SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization patch;
                    memset( &patch, 0, sizeof( patch ) );

                    unsigned int bti = annotations.m_argIndexMap.at(ptrArg->ArgumentNumber);
                    patch.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_STATELESS_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION;
                    patch.Size = sizeof( patch );
                    patch.SurfaceStateHeapOffset = context.Surface.SurfaceOffset[ bti ];
                    patch.DataParamOffset = ptrArg->PayloadPosition;
                    patch.DataParamSize = ptrArg->PayloadSizeInBytes;
                    patch.GlobalBufferIndex = 0;

                    dataParameterStreamSize = std::max( 
                        dataParameterStreamSize, 
                        ptrArg->PayloadPosition + ptrArg->PayloadSizeInBytes );

                    retValue = AddPatchItem(
                        patch,
                        membuf );
                }
                else if(ptrArg->AddressSpace == iOpenCL::KERNEL_ARGUMENT_ADDRESS_SPACE_PRIVATE) 
                {

                    iOpenCL::SPatchAllocateStatelessPrivateSurface patch;
                    memset( &patch, 0, sizeof( patch ) );

                    PrivateInputAnnotation* ptrArg = static_cast<PrivateInputAnnotation*>(*i);
                    unsigned int bti = annotations.m_argIndexMap.at(ptrArg->ArgumentNumber);        
                    patch.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_STATELESS_PRIVATE_MEMORY;
                    patch.Size = sizeof( patch );
                    patch.SurfaceStateHeapOffset = context.Surface.SurfaceOffset[ bti ];
                    patch.PerThreadPrivateMemorySize = ptrArg->PerThreadPrivateMemorySize;
                    patch.DataParamOffset = ptrArg->PayloadPosition;
                    patch.DataParamSize = ptrArg->PayloadSizeInBytes;

                    dataParameterStreamSize = std::max( 
                        dataParameterStreamSize, 
                        ptrArg->PayloadPosition + ptrArg->PayloadSizeInBytes );

                    retValue = AddPatchItem(
                        patch,
                        membuf );
                }
                else if(ptrArg->AddressSpace == ADDRESS_SPACE_INTERNAL_DEFAULT_DEVICE_QUEUE)
                {

                    iOpenCL::SPatchAllocateStatelessDefaultDeviceQueueSurface   patch;

                    memset(&patch, 0, sizeof(patch));

                    hasDeviceEnqueue.hasDefaultQueue = true;

                    unsigned int bti = annotations.m_argIndexMap.at(ptrArg->ArgumentNumber);

                    patch.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_STATELESS_DEFAULT_DEVICE_QUEUE_SURFACE;
                    patch.Size = sizeof(patch);
                    patch.SurfaceStateHeapOffset = context.Surface.SurfaceOffset[bti];
                    patch.DataParamOffset = ptrArg->PayloadPosition;
                    patch.DataParamSize = ptrArg->PayloadSizeInBytes;

                    dataParameterStreamSize = std::max(
                        dataParameterStreamSize,
                        ptrArg->PayloadPosition + ptrArg->PayloadSizeInBytes);

                    retValue = AddPatchItem(
                        patch,
                        membuf);
                }
                else if(ptrArg->AddressSpace == ADDRESS_SPACE_INTERNAL_EVENT_POOL)
                {

                    iOpenCL::SPatchAllocateStatelessEventPoolSurface   patch;

                    memset(&patch, 0, sizeof(patch));

                    unsigned int bti = annotations.m_argIndexMap.at(ptrArg->ArgumentNumber);

                    patch.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_STATELESS_EVENT_POOL_SURFACE;
                    patch.Size = sizeof(patch);
                    patch.SurfaceStateHeapOffset = context.Surface.SurfaceOffset[bti];
                    patch.DataParamOffset = ptrArg->PayloadPosition;
                    patch.DataParamSize = ptrArg->PayloadSizeInBytes;

                    dataParameterStreamSize = std::max(
                        dataParameterStreamSize,
                        ptrArg->PayloadPosition + ptrArg->PayloadSizeInBytes);

                    retValue = AddPatchItem(
                        patch,
                        membuf);
                }
                else
                {
                    // Only constant and private address space is supported for now.
                    retValue.Success = false;
                }
            }
            else
            {
                // Pointer inputs must be stateless
                retValue.Success = false;
            }
        }
    }

    // Constant Arguments
    if( retValue.Success )
    {
        for( ConstantArgumentIterator i = annotations.m_constantArgumentAnnotation.begin(); 
            i != annotations.m_constantArgumentAnnotation.end(); 
            ++i )
        {
            ConstantArgumentAnnotation* constInput = *i;

            iOpenCL::SPatchDataParameterBuffer  patch;

            memset( &patch, 0, sizeof( patch ) );

            patch.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER;
            patch.Size = sizeof( patch );
            patch.ArgumentNumber = constInput->ArgumentNumber;
            //Datasize = PayloadSize, SourceOffset = 0, is wrong for everything > 32-bit
            patch.DataSize = constInput->PayloadSizeInBytes;
            patch.SourceOffset = constInput->Offset;
            patch.Offset = constInput->PayloadPosition;
            patch.Type =  iOpenCL::DATA_PARAMETER_KERNEL_ARGUMENT;
            patch.LocationIndex = constInput->LocationIndex;
            patch.LocationIndex2 = constInput->LocationCount;
            patch.IsEmulationArgument = constInput->IsEmulationArgument;

            dataParameterStreamSize = std::max( 
                dataParameterStreamSize, 
                constInput->PayloadPosition + constInput->PayloadSizeInBytes );

            if( retValue.Success )
            {
                retValue = AddPatchItem(
                    patch,
                    membuf );
            }
        }
    }
    if (retValue.Success && annotations.m_startGAS != NULL)
    {
        const iOpenCL::StartGASAnnotation* startGAS = annotations.m_startGAS;

        iOpenCL::SPatchDataParameterBuffer  patch;
        memset(&patch, 0, sizeof(patch));

        patch.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER;
        patch.Size = sizeof(patch);
        patch.Type = iOpenCL::DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS;
        patch.Offset = startGAS->Offset;
        patch.DataSize = startGAS->gpuPointerSizeInBytes;
                 
        dataParameterStreamSize = std::max(
            dataParameterStreamSize,
            startGAS->Offset + startGAS->gpuPointerSizeInBytes);

        retValue = AddPatchItem(patch, membuf);
    }

    if (retValue.Success && (annotations.m_WindowSizeGAS != NULL))
    {
        iOpenCL::SPatchDataParameterBuffer  patch;

        memset(&patch, 0, sizeof(patch));

        patch.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER;
        patch.Size = sizeof(patch);
        patch.Type = iOpenCL::DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_SIZE;
        patch.Offset = annotations.m_WindowSizeGAS->Offset;
        patch.DataSize = iOpenCL::DATA_PARAMETER_DATA_SIZE;

        dataParameterStreamSize = std::max(
            dataParameterStreamSize,
            annotations.m_WindowSizeGAS->Offset + iOpenCL::DATA_PARAMETER_DATA_SIZE);

        retValue = AddPatchItem(patch, membuf);
    }

    if (retValue.Success && (annotations.m_PrivateMemSize != NULL))
    {
        iOpenCL::SPatchDataParameterBuffer  patch;

        memset(&patch, 0, sizeof(patch));

        patch.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER;
        patch.Size = sizeof(patch);
        patch.Type = iOpenCL::DATA_PARAMETER_PRIVATE_MEMORY_STATELESS_SIZE;
        patch.Offset = annotations.m_PrivateMemSize->Offset;
        patch.DataSize = iOpenCL::DATA_PARAMETER_DATA_SIZE;

        retValue = AddPatchItem(patch, membuf);

        dataParameterStreamSize = std::max(
            dataParameterStreamSize,
            annotations.m_PrivateMemSize->Offset + iOpenCL::DATA_PARAMETER_DATA_SIZE );
    }

    // Payload must be a multiple of a GRF register
    dataParameterStreamSize += GetAlignmentOffset( dataParameterStreamSize, 32 );

    if( retValue.Success )
    {
        iOpenCL::SPatchDataParameterStream  patch;

        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_DATA_PARAMETER_STREAM;
        patch.Size = sizeof( patch );
        patch.DataParameterStreamSize = dataParameterStreamSize;

        retValue = AddPatchItem(
            patch,
            membuf );
    }

    // Patch for Thread Payload
    if( retValue.Success )
    {   
        iOpenCL::SPatchThreadPayload    patch;

        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_THREAD_PAYLOAD;
        patch.Size  = sizeof( patch );
        patch.HeaderPresent = false;

        patch.LocalIDXPresent = annotations.m_threadPayload.HasLocalIDx;
        patch.LocalIDYPresent = annotations.m_threadPayload.HasLocalIDy;
        patch.LocalIDZPresent = annotations.m_threadPayload.HasLocalIDz;
        patch.GetGlobalOffsetPresent = annotations.m_threadPayload.HasGlobalIDOffset;
        patch.GetGroupIDPresent = annotations.m_threadPayload.HasGroupID;
        patch.GetLocalIDPresent = annotations.m_threadPayload.HasLocalID;
        patch.StageInGridOriginPresent = annotations.m_threadPayload.HasStageInGridOrigin;
        patch.StageInGridSizePresent = annotations.m_threadPayload.HasStageInGridSize;

        patch.IndirectPayloadStorage = annotations.m_threadPayload.CompiledForIndirectPayloadStorage;
        patch.UnusedPerThreadConstantPresent = annotations.m_threadPayload.UnusedPerThreadConstantPresent;
        patch.OffsetToSkipPerThreadDataLoad = annotations.m_threadPayload.OffsetToSkipPerThreadDataLoad;
        patch.PassInlineData = annotations.m_threadPayload.PassInlineData;

        retValue = AddPatchItem(
            patch,
            membuf );

    }

    // Patch for Execution Enivronment
    if( retValue.Success )
    {   
        iOpenCL::SPatchExecutionEnvironment patch;

        memset( &patch, 0, sizeof( patch ) );

        patch.Token = iOpenCL::PATCH_TOKEN_EXECUTION_ENVIRONMENT;
        patch.Size  = sizeof( patch );

        if( annotations.m_executionEnivronment.HasFixedWorkGroupSize )
        {
            patch.RequiredWorkGroupSizeX = annotations.m_executionEnivronment.FixedWorkgroupSize[0];
            patch.RequiredWorkGroupSizeY = annotations.m_executionEnivronment.FixedWorkgroupSize[1];
            patch.RequiredWorkGroupSizeZ = annotations.m_executionEnivronment.FixedWorkgroupSize[2];
        }

		patch.WorkgroupWalkOrderDims = 0;
		patch.WorkgroupWalkOrderDims |= annotations.m_executionEnivronment.WorkgroupWalkOrder[0];
		patch.WorkgroupWalkOrderDims |= annotations.m_executionEnivronment.WorkgroupWalkOrder[1] << 2;
		patch.WorkgroupWalkOrderDims |= annotations.m_executionEnivronment.WorkgroupWalkOrder[2] << 4;

        patch.CompiledSIMD32 = ( annotations.m_executionEnivronment.CompiledSIMDSize == 32 );
        patch.CompiledSIMD16 = ( annotations.m_executionEnivronment.CompiledSIMDSize == 16 );
        patch.CompiledSIMD8  = ( annotations.m_executionEnivronment.CompiledSIMDSize == 8 );

        patch.LargestCompiledSIMDSize = 8;
        patch.LargestCompiledSIMDSize = patch.CompiledSIMD16 ? 16 : patch.LargestCompiledSIMDSize;
        patch.LargestCompiledSIMDSize = patch.CompiledSIMD32 ? 32 : patch.LargestCompiledSIMDSize;


        patch.HasBarriers                       = annotations.m_executionEnivronment.HasBarriers;
        patch.DisableMidThreadPreemption        = annotations.m_executionEnivronment.DisableMidThreadPreemption;


        patch.UsesStatelessSpillFill = (annotations.m_executionEnivronment.PerThreadSpillFillSize > 0)
          || (annotations.m_executionEnivronment.PerThreadScratchSpace > 0);

        patch.HasDeviceEnqueue = (bool)hasDeviceEnqueue;

        patch.UsesFencesForReadWriteImages = annotations.m_executionEnivronment.HasReadWriteImages;

        patch.IsInitializer = annotations.m_executionEnivronment.IsInitializer;
        patch.IsFinalizer = annotations.m_executionEnivronment.IsFinalizer;

        patch.SubgroupIndependentForwardProgressRequired = annotations.m_executionEnivronment.SubgroupIndependentForwardProgressRequired;

        patch.CompiledSubGroupsNumber = annotations.m_executionEnivronment.CompiledSubGroupsNumber;

        patch.CompiledForGreaterThan4GBBuffers = annotations.m_executionEnivronment.CompiledForGreaterThan4GBBuffers;

        patch.NumGRFRequired = annotations.m_executionEnivronment.NumGRFRequired;

        patch.HasGlobalAtomics = annotations.m_executionEnivronment.HasGlobalAtomics;



        retValue = AddPatchItem(
            patch,
            membuf );
    }

    // Patch for Kernel Attributes
    if( retValue.Success )
    {
        retValue = AddKernelAttributePatchItems(annotations, membuf);
    }

    // Patch for Kernel Arguments
    if( retValue.Success )
    {
        retValue = AddKernelArgumentPatchItems(annotations, membuf);
    }

    // Patch for String Annotations
    if( retValue.Success )
    {
        for( PrintfStringIterator i = annotations.m_printfStringAnnotations.begin(); 
                                  i != annotations.m_printfStringAnnotations.end(); 
                                  i++ )
        {
            PrintfStringAnnotation *stringAnn = *i;

            iOpenCL::SPatchString  patch;
            memset( &patch, 0, sizeof( patch ) );

            uint32_t  alignedStringSize = 0;

            patch.Token = iOpenCL::PATCH_TOKEN_STRING;
            patch.Size  = sizeof( patch ); 
            patch.Index = stringAnn->Index;

            std::streamsize tokenStart = membuf.Size();

            if( !membuf.Write( patch ) )
            {
                retValue.Success = false;
                return retValue;
            }

            if( retValue.Success )
            {
                retValue = AddStringPatchItem( 
                    stringAnn->StringData,
                    membuf,
                    alignedStringSize );
            }

            patch.StringSize = alignedStringSize;
            patch.Size += alignedStringSize;
            membuf.WriteAt( patch, tokenStart );

#if ( defined( _DEBUG ) || defined( _INTERNAL ) || defined( _RELEASE_INTERNAL ) )
            DebugPatchList(membuf.GetLinearPointer() + tokenStart, patch.Size, m_oclStateDebugMessagePrintOut);
#endif
        }
    }

    // Patch for GT-Pin surfaces
    if (gtpinEnabled)
    {
        for (int i = 0; i < GTPIN_IGC_OCL_NumberOfSurfaces(); i++)
        {
            if (retValue.Success)
            {
                const unsigned bti = GTPIN_IGC_OCL_GetSurfaceBTI(i);
                const unsigned offset = context.Surface.SurfaceOffset[bti];
                const unsigned param = GTPIN_IGC_OCL_GetSurfaceKernelArgNo(i);

                // patched as global memory object
                iOpenCL::SPatchGlobalMemoryObjectKernelArgument patch;
                memset(&patch, 0, sizeof(patch));

                patch.Token = iOpenCL::PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT;
                patch.Size = sizeof(patch);
                patch.ArgumentNumber = param;
                patch.Offset = offset;

                retValue = AddPatchItem(patch, membuf);
            }
        }
    }
    
    // Patch for GTPin output structure
    if (retValue.Success)
    {
        iOpenCL::SPatchItemHeader patch;
        memset(&patch, 0, sizeof(patch));
        
        patch.Token = PATCH_TOKEN_GTPIN_INFO;
        unsigned int size = 0;
        void* buffer = nullptr;
        const IGC::SKernelProgram* program = &(annotations.m_kernelProgram);
        if (annotations.m_executionEnivronment.CompiledSIMDSize == 8)
        {
            buffer = program->simd8.m_gtpinBuffer;
            size = program->simd8.m_gtpinBufferSize;
        }
        else if (annotations.m_executionEnivronment.CompiledSIMDSize == 16)
        {
            buffer = program->simd16.m_gtpinBuffer;
            size = program->simd16.m_gtpinBufferSize;
        }
        else if (annotations.m_executionEnivronment.CompiledSIMDSize == 32)
        {
            buffer = program->simd32.m_gtpinBuffer;
            size = program->simd32.m_gtpinBufferSize;
        }
        
        if (size > 0)
        {
            patch.Size = sizeof(patch) + size;
            
            retValue = AddPatchItem(patch, membuf);
            
            if (!membuf.Write((const char*)buffer, size))
            {
                retValue.Success = false;
                return retValue;
            }
            freeBlock(buffer);
        }
    }

    return retValue;
}

inline RETVAL CGen8OpenCLStateProcessor::AddStringPatchItem( 
    const std::string& str, 
    Util::BinaryStream& membuf, 
    uint32_t& bytesWritten ) const
{
    RETVAL  retValue = g_cInitRetValue;

    unsigned int length = (unsigned int)str.size();
    bytesWritten = iSTD::Align( length + 1, sizeof( uint32_t ) );

    if (!membuf.Write(str.data(), length))
    {
        retValue.Success =  false;
        return retValue;
    }

    for( ; length < bytesWritten; length++)
    {
        if (!membuf.Write('\0'))
        {
            retValue.Success = false;
            return retValue;
        }
    }

    return retValue;
}

RETVAL CGen8OpenCLStateProcessor::AddKernelAttributePatchItems(
    const IGC::SOpenCLKernelInfo& annotations,
    Util::BinaryStream& membuf )
{
    RETVAL  retValue = g_cInitRetValue;

    iOpenCL::SPatchKernelAttributesInfo patch;
    memset( &patch, 0, sizeof( patch ) );

    patch.Token = iOpenCL::PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO;
    patch.Size = sizeof( patch );
    patch.AttributesSize = 0;

    // We need to start writing after the token, but we don't know the sizes yet.
    // So write it as place-holder, then replace with the real token.
    std::streamsize tokenStart = membuf.Size();
    if( !membuf.Write( patch ) )
    {
        retValue.Success = false;
        return retValue;
    }

    if( retValue.Success )
    {
        std::istringstream buf(annotations.m_kernelAttributeInfo);
        std::istream_iterator< std::string > beg(buf), end;
    
        std::vector<std::string> tokens(beg, end);

        std::string filteredAttributes;

        for( auto &s: tokens )
        {
            for( int index = 0; index < numAllowedAttributes; index++ )
            {
                if( s.find( allowedAttributes[index] ) != std::string::npos )
                {
                    if( filteredAttributes.length() > 0 )
                    {
                        filteredAttributes += " ";
                    }

                    filteredAttributes += s;
                    break;
                }
            }
        }

        retValue = AddStringPatchItem( 
            filteredAttributes,
            membuf,
            patch.AttributesSize );
                
        patch.Size += patch.AttributesSize;
    }

    if( retValue.Success && !membuf.WriteAt( patch, tokenStart ) )
    {
        retValue.Success = false;
        return retValue;
    }

#if ( defined( _DEBUG ) || defined( _INTERNAL ) || defined( _RELEASE_INTERNAL ) )
    DebugPatchList(membuf.GetLinearPointer() + tokenStart, patch.Size, m_oclStateDebugMessagePrintOut);
#endif

    return retValue;
}

RETVAL CGen8OpenCLStateProcessor::AddKernelArgumentPatchItems(
    const IGC::SOpenCLKernelInfo& annotations,
    Util::BinaryStream& membuf )
{

    RETVAL  retValue = g_cInitRetValue;

    const std::vector<iOpenCL::KernelArgumentInfoAnnotation*>& kernelArgInfo = annotations.m_kernelArgInfo;

    int index = 0;
    for(auto argInfo : kernelArgInfo)
    {
         iOpenCL::SPatchKernelArgumentInfo patch;
         memset( &patch, 0, sizeof( patch ) );

         patch.Token = iOpenCL::PATCH_TOKEN_KERNEL_ARGUMENT_INFO;
         patch.Size  = sizeof( patch );
         patch.ArgumentNumber = index;

         // We need to start writing after the token, but we don't know the sizes yet.
         // So write it as place-holder, then replace with the real token.
         std::streamsize tokenStart = membuf.Size();
         if( !membuf.Write( patch ) )
         {
             retValue.Success = false;
             return retValue;
         }

        if( retValue.Success )
        {
            retValue = AddStringPatchItem( 
                argInfo->AddressQualifier,
                membuf,
                patch.AddressQualifierSize );
                
            patch.Size += patch.AddressQualifierSize;

        }

        if( retValue.Success )
        {
            retValue = AddStringPatchItem( 
                argInfo->AccessQualifier,
                membuf,
                patch.AccessQualifierSize );

            patch.Size += patch.AccessQualifierSize;
        }

        if( retValue.Success )
        {
            retValue = AddStringPatchItem( 
                argInfo->ArgumentName,
                membuf,
                patch.ArgumentNameSize );

            patch.Size += patch.ArgumentNameSize;
        }

        if( retValue.Success )
        {
            retValue = AddStringPatchItem( 
                argInfo->TypeName,
                membuf,
                patch.TypeNameSize );

            patch.Size += patch.TypeNameSize;
        }

        if( retValue.Success )
        {
            retValue = AddStringPatchItem(
                argInfo->TypeQualifier, 
                membuf, 
                patch.TypeQualifierSize );

            patch.Size += patch.TypeQualifierSize;
        }

        if( retValue.Success && !membuf.WriteAt( patch, tokenStart ) )
        {
            retValue.Success = false;
            return retValue;
        }

#if ( defined( _DEBUG ) || defined( _INTERNAL ) || defined( _RELEASE_INTERNAL ) )
        DebugPatchList(membuf.GetLinearPointer() + tokenStart, patch.Size, m_oclStateDebugMessagePrintOut);
#endif
         index++;
    }

    return retValue;
}

RETVAL CGen8OpenCLStateProcessor::CombineKernelBinary(
    const SStateProcessorContextGen8_0& context,
    const IGC::SOpenCLKernelInfo& annotations,
    const Util::BinaryStream& kernelHeap,
    const Util::BinaryStream& generalStateHeap,
    const Util::BinaryStream& dynamicStateHeap,
    const Util::BinaryStream& surfaceStateHeap,
    const Util::BinaryStream& patchList,
    unsigned int unpaddedBinarySize,
    Util::BinaryStream& kernelBinary )
{
    RETVAL retValue = g_cInitRetValue;    

    iOpenCL::SKernelBinaryHeaderGen7    header;

    memset( &header, 0, sizeof( header ) );

    header.ShaderHashCode = annotations.m_ShaderHashCode;
    header.KernelNameSize = (DWORD)annotations.m_kernelName.size() + 1;
    header.KernelNameSize += GetAlignmentOffset( header.KernelNameSize, sizeof(DWORD) );
    header.PatchListSize = (DWORD)patchList.Size();
    header.KernelHeapSize = (DWORD)kernelHeap.Size();
    header.GeneralStateHeapSize = (DWORD)generalStateHeap.Size();
    header.DynamicStateHeapSize = (DWORD)dynamicStateHeap.Size();
    header.SurfaceStateHeapSize = (DWORD)surfaceStateHeap.Size();

    header.KernelUnpaddedSize = (DWORD)unpaddedBinarySize;

    ICBE_DPF( GFXDBG_HARDWARE, "Kernel Name: %s\n", annotations.m_kernelName.c_str() );

    kernelBinary.Write( header );
    kernelBinary.Write( annotations.m_kernelName.c_str(), annotations.m_kernelName.size() + 1 );
    kernelBinary.Align( 4 );
    kernelBinary.Write( kernelHeap );
    kernelBinary.Write( generalStateHeap );
    kernelBinary.Write( dynamicStateHeap );
    kernelBinary.Write( surfaceStateHeap );
    kernelBinary.Write( patchList );

    ICBE_ASSERT( IsAligned( kernelBinary.Size(), sizeof(DWORD) ) );

    const char* pBuffer = kernelBinary.GetLinearPointer() + sizeof( iOpenCL::SKernelBinaryHeaderGen7 );

    DWORD checkSumSize = (DWORD)kernelBinary.Size() - sizeof( iOpenCL::SKernelBinaryHeaderGen7 );

    ICBE_ASSERT( IsAligned( checkSumSize, sizeof(DWORD) ) );

    QWORD hash = Hash( (const DWORD*) pBuffer, checkSumSize / sizeof(DWORD) );

    header.CheckSum = hash & 0xFFFFFFFF;

    kernelBinary.WriteAt( header, 0 );

#if defined(_DEBUG)
    DWORD   combinedBinarySize = 
        header.KernelNameSize + 
        header.KernelHeapSize +
        header.GeneralStateHeapSize +
        header.DynamicStateHeapSize +
        header.SurfaceStateHeapSize +
        header.PatchListSize;

    ICBE_ASSERT( combinedBinarySize == checkSumSize );

    const iOpenCL::SKernelBinaryHeaderGen7* pHeaderCheck = 
        (const iOpenCL::SKernelBinaryHeaderGen7*)kernelBinary.GetLinearPointer();

    ICBE_ASSERT( pHeaderCheck->CheckSum == ( hash & 0xFFFFFFFF ) );

#endif

    if (IGC_IS_FLAG_ENABLED(DumpOCLProgramInfo))
    {
        DebugKernelBinaryHeader_Gen7(
            &header, 
            m_oclStateDebugMessagePrintOut);
    }

    return retValue;
}

RETVAL CGen8OpenCLStateProcessor::AddSamplerState(
    const bool enable,
    const SAMPLER_TEXTURE_ADDRESS_MODE& addressModeX,
    const SAMPLER_TEXTURE_ADDRESS_MODE& addressModeY,
    const SAMPLER_TEXTURE_ADDRESS_MODE& addressModeZ,
    const SAMPLER_MAPFILTER_TYPE& filterMag,
    const SAMPLER_MAPFILTER_TYPE& filterMin,
    const SAMPLER_MIPFILTER_TYPE& filterMip,
    const bool normalizedCoords,
    const SAMPLER_COMPARE_FUNC_TYPE& compareFunc,
    const DWORD borderColorOffset,
    Util::BinaryStream& membuf) const
{

    RETVAL  retValue = g_cInitRetValue;

    if( retValue.Success )
    {
        if (filterMag >= NUM_SAMPLER_MAPFILTER_TYPES ||
            filterMin >= NUM_SAMPLER_MAPFILTER_TYPES ||
            addressModeX >= NUM_SAMPLER_TEXTURE_ADDRESS_MODES ||
            addressModeY >= NUM_SAMPLER_TEXTURE_ADDRESS_MODES ||
            addressModeZ >= NUM_SAMPLER_TEXTURE_ADDRESS_MODES)
        {
            ICBE_ASSERT( 0 );
            retValue.Success = false;
        }
    }

    if( retValue.Success )
    {
        G6HWC::SSharedStateSamplerState samplerState = 
            G6HWC::g_cInitSharedStateSamplerState;

        samplerState.DW0.Gen7 = G6HWC::g_cInitSharedStateSamplerStateDW0Gen7;
        samplerState.DW1.Gen7 = G6HWC::g_cInitSharedStateSamplerStateDW1Gen7;
        samplerState.DW3.Gen7 = G6HWC::g_cInitSharedStateSamplerStateDW3Gen7;

        if( enable )
        {
            // Program client sampler state.

            samplerState.DW0.Gen7.MagModeFilter = g_cConvertSamplerMapFilter[filterMag];
            samplerState.DW0.Gen7.MinModeFilter = g_cConvertSamplerMapFilter[filterMin];
            samplerState.DW0.Gen7.MipModeFilter = g_cConvertSamplerMipFilter[filterMip];

            samplerState.DW1.Gen7.MinLOD = 0;
            samplerState.DW1.Gen7.MaxLOD = FloatToFixed<unsigned>( HWCaps().SampleLOD.Max, 4, 8 );

            samplerState.DW2.Gen8.All.IndirectStatePointer = 
                borderColorOffset /
                HWCaps().DefaultColorPointerAlignSize;

            samplerState.DW3.Gen7.TCXAddressControlMode = g_cConvertSamplerTextureAddressMode[addressModeX];
            samplerState.DW3.Gen7.TCYAddressControlMode = g_cConvertSamplerTextureAddressMode[addressModeY];
            samplerState.DW3.Gen7.TCZAddressControlMode = g_cConvertSamplerTextureAddressMode[addressModeZ];
            samplerState.DW3.Gen7.NonNormalizedCoordinateEnable = !normalizedCoords;

            // Program additional sampler state.

            samplerState.DW0.Gen7.SamplerDisable = false;

            if( filterMag != SAMPLER_MAPFILTER_POINT )
            {
                samplerState.DW3.Gen7.UAddressMagFilterAddressRoundingEnable = true;
                samplerState.DW3.Gen7.VAddressMagFilterAddressRoundingEnable = true;
                samplerState.DW3.Gen7.RAddressMagFilterAddressRoundingEnable = true;
            }

            if (filterMin != SAMPLER_MAPFILTER_POINT)
            {
                samplerState.DW3.Gen7.UAddressMinFilterAddressRoundingEnable = true;
                samplerState.DW3.Gen7.VAddressMinFilterAddressRoundingEnable = true;
                samplerState.DW3.Gen7.RAddressMinFilterAddressRoundingEnable = true;
            }

            samplerState.DW1.Gen7.ShadowFunction = g_cConvertCompareFunc[compareFunc];
        }
        else
        {
            samplerState.DW0.Gen7.SamplerDisable = true;
        }

        if( membuf.Write( samplerState ) == false )
        {
            retValue.Success = false;
            ICBE_ASSERT( 0 );
        }

#if ( defined( _DEBUG ) || defined( _INTERNAL ) || defined( _RELEASE_INTERNAL ) )
        {
            G6HWC::DebugSamplerStateCommand(
                &samplerState,
                m_Platform );
        }
#endif
    }

    return retValue;
}

RETVAL CGen8OpenCLStateProcessor::AddSurfaceState(
    const SURFACE_TYPE& type,
    const SURFACE_FORMAT& surfaceFormat,
    const DWORD bufferLength,
    bool isMultiSampleImage,
    Util::BinaryStream& membuf ) const
{
    RETVAL  retValue = g_cInitRetValue;

    if( retValue.Success )
    {
        G6HWC::SSharedStateSurfaceState surf = 
            G6HWC::g_cInitSharedStateSurfaceState;

        surf.DW0.Gen8 = G6HWC::g_cInitSharedStateSurfaceStateDW0Gen8;
        surf.DW1.Gen8 = G6HWC::g_cInitSharedStateSurfaceStateDW1Gen8;
        surf.DW2.Gen7 = G6HWC::g_cInitSharedStateSurfaceStateDW2Gen7;
        surf.DW3.Gen7 = G6HWC::g_cInitSharedStateSurfaceStateDW3Gen7;
        surf.DW4.Gen7 = G6HWC::g_cInitSharedStateSurfaceStateDW4Gen7;
        surf.DW5.Gen8 = G6HWC::g_cInitSharedStateSurfaceStateDW5Gen8;
        surf.DW6.Gen7.SurfaceMCS = G6HWC::g_cInitSharedStateSurfaceStateDW6MCSGen7;
        surf.DW7.Gen7 = G6HWC::g_cInitSharedStateSurfaceStateDW7Gen7;
        surf.DW8.Gen8 = G6HWC::g_cInitSharedStateSurfaceStateDW8Gen8;
        surf.DW9.Gen8 = G6HWC::g_cInitSharedStateSurfaceStateDW9Gen8;
        surf.DW10.Gen8 = G6HWC::g_cInitSharedStateSurfaceStateDW10Gen8;
        surf.DW11.Gen8 = G6HWC::g_cInitSharedStateSurfaceStateDW11Gen8;
        surf.DW12.Gen8 = G6HWC::g_cInitSharedStateSurfaceStateDW12Gen8;

        surf.DW0.Gen8.TileMode = G6HWC::GFXSHAREDSTATE_TILEMODE_LINEAR;

        if( type != SURFACE_UNKNOWN )
        {
            surf.DW0.Gen8.SurfaceType =
                g_cConvertSurfaceType[ type ];

            if( type == SURFACE_BUFFER )
            {
                G6HWC::SSurfaceStateBufferLength ssbl = 
                    G6HWC::g_cInitSurfaceStateBufferLength;
                ssbl.Length = bufferLength - 1;

                surf.DW0.Gen8.SurfaceFormat =
                    g_cConvertSurfaceFormat[ surfaceFormat ];

                surf.DW2.Gen7.Height = ssbl.All.Height;
                surf.DW2.Gen7.Width  = ssbl.All.Width;
                surf.DW3.Gen7.Depth  = ssbl.All.Depth;
            }            
            else if( ( type == SURFACE_1D_ARRAY ) ||
                ( type == SURFACE_2D_ARRAY ) )
            {
                surf.DW0.Gen7.SurfaceArray = true;
            }

            surf.DW4.Gen7.SurfaceAll.MultisampledSurfaceStorageFormat = isMultiSampleImage;
        }     

        G6HWC::DebugSurfaceStateCommand(
            &surf,
            m_Platform );

        if( membuf.Write( surf ) == false )
        {
            ICBE_ASSERT( 0 );
            retValue.Success = false;
        }
    }

    return retValue;
}

bool CGen8OpenCLStateProcessor::InlineSamplersAllow3DImageTransformation(const IGC::SOpenCLKernelInfo& annotations) const
{
    // To be transformable, we need inline samplers to have:
    //   addressMode == CLAMP_TO_EDGE
    //   filter == NEAREST
    //   normalizedCoords = false
    for (auto i = annotations.m_samplerInput.begin(); i != annotations.m_samplerInput.end(); ++i)
    {
        const SamplerInputAnnotation* samplerAnnotation = *i;
        if (samplerAnnotation->SamplerType == SAMPLER_OBJECT_TEXTURE)
        {
            if (samplerAnnotation->NormalizedCoords ||
                samplerAnnotation->TCXAddressMode != SAMPLER_TEXTURE_ADDRESS_MODE_CLAMP ||
                samplerAnnotation->TCYAddressMode != SAMPLER_TEXTURE_ADDRESS_MODE_CLAMP ||
                samplerAnnotation->TCZAddressMode != SAMPLER_TEXTURE_ADDRESS_MODE_CLAMP ||
                samplerAnnotation->MagFilterType != SAMPLER_MAPFILTER_POINT ||
                samplerAnnotation->MinFilterType != SAMPLER_MAPFILTER_POINT)
            {
                return false;
            }
        }
    }

    return true;
}

}
