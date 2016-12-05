/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __SAMPLE_UTILS_H__
#define __SAMPLE_UTILS_H__

#include <stdio.h>
#include <string>
#include <sstream>
#include <vector>
#include "mfxstructures.h"
#include "mfxvideo.h"
#include "strings_defs.h"
#include "file_defs.h"
#include "time_defs.h"

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);               \
    void operator=(const TypeName&)

//! Base class for types that should not be assigned.
class no_assign {
    // Deny assignment
    void operator=( const no_assign& );
public:
};

//! Base class for types that should not be copied or assigned.
class no_copy: no_assign {
    //! Deny copy construction
    no_copy( const no_copy& );
public:
    //! Allow default construction
    no_copy() {}
};

typedef std::basic_string<msdk_char> msdk_string;
typedef std::basic_stringstream<msdk_char> msdk_stringstream;
typedef std::basic_ostream<msdk_char, std::char_traits<msdk_char> > msdk_ostream;
typedef std::basic_istream<msdk_char, std::char_traits<msdk_char> > msdk_istream;

#ifdef UNICODE 
    #define msdk_cout std::wcout
    #define msdk_err std::wcerr
#else
    #define msdk_cout std::cout
    #define msdk_err std::cerr
#endif

struct DeletePtr {
    template <class T> T* operator () (T* p) const {
        delete p;
        return 0;
    }
};

class CSmplBitstreamReader
{
public :

    CSmplBitstreamReader();
    virtual ~CSmplBitstreamReader();

    //resets position to file begin
    virtual void      Reset();
    virtual void      Close();
    virtual mfxStatus Init(const msdk_char *strFileName);
	virtual mfxStatus ReadNextFrame(mfxBitstream *pBS, VOID *);

    FILE*     m_fSource;
    bool      m_bInited;
};

mfxU16 GetFreeSurfaceIndex(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize);
mfxU16 GetFreeSurfaceIndex(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize, mfxU16 step);
mfxU16 GetFreeSurface(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize);
mfxStatus InitMfxBitstream(mfxBitstream* pBitstream, mfxU32 nSize);

//performs copy to end if possible, also move data to buffer begin if necessary
//shifts offset pointer in source bitstream in success case
mfxStatus MoveMfxBitstream(mfxBitstream *pTarget, mfxBitstream *pSrc, mfxU32 nBytesToCopy);
mfxStatus ExtendMfxBitstream(mfxBitstream* pBitstream, mfxU32 nSize);
void WipeMfxBitstream(mfxBitstream* pBitstream);

//piecewise linear function for bitrate approximation
class PartiallyLinearFNC
{
    mfxF64 *m_pX;
    mfxF64 *m_pY;
    mfxU32  m_nPoints;
    mfxU32  m_nAllocated;

public:
    PartiallyLinearFNC();
    ~PartiallyLinearFNC();

    void AddPair(mfxF64 x, mfxF64 y);
    mfxF64 at(mfxF64);
private:
    DISALLOW_COPY_AND_ASSIGN(PartiallyLinearFNC);
};

// function for getting a pointer to a specific external buffer from the array
mfxExtBuffer* GetExtBuffer(mfxExtBuffer** ebuffers, mfxU32 nbuffers, mfxU32 BufferId);

//declare used extended buffers
template<class T>
struct mfx_ext_buffer_id{
    enum {id = 0};
};
template<>struct mfx_ext_buffer_id<mfxExtCodingOption>{
    enum {id = MFX_EXTBUFF_CODING_OPTION};
};
template<>struct mfx_ext_buffer_id<mfxExtCodingOption2>{
    enum {id = MFX_EXTBUFF_CODING_OPTION2};
};
template<>struct mfx_ext_buffer_id<mfxExtAvcTemporalLayers>{
    enum {id = MFX_EXTBUFF_AVC_TEMPORAL_LAYERS};
};
template<>struct mfx_ext_buffer_id<mfxExtAVCRefListCtrl>{
    enum {id = MFX_EXTBUFF_AVC_REFLIST_CTRL};
};

//helper function to initialize mfx ext buffer structure
template <class T>
void init_ext_buffer(T & ext_buffer)
{
    memset(&ext_buffer, 0, sizeof(ext_buffer));
    reinterpret_cast<mfxExtBuffer*>(&ext_buffer)->BufferId = mfx_ext_buffer_id<T>::id;
    reinterpret_cast<mfxExtBuffer*>(&ext_buffer)->BufferSz = sizeof(ext_buffer);
}

// returns false if buf length is insufficient, otherwise
// skips step bytes in buf with specified length and returns true
template <typename Buf_t, typename Length_t>
bool skip(const Buf_t *&buf, Length_t &length, Length_t step)
{
    if (length < step)
        return false;

    buf    += step;
    length -= step;

    return true;
}

//do not link MediaSDK dispatched if class not used
struct MSDKAdapter {
    // returns the number of adapter associated with MSDK session, 0 for SW session
    static mfxU32 GetNumber(mfxSession session = 0) {
        mfxU32 adapterNum = 0; // default
        mfxIMPL impl = MFX_IMPL_SOFTWARE; // default in case no HW IMPL is found

        // we don't care for error codes in further code; if something goes wrong we fall back to the default adapter
        if (session)
        {
            MFXQueryIMPL(session, &impl);
        }
        else
        {
            // an auxiliary session, internal for this function
            mfxSession auxSession;
            memset(&auxSession, 0, sizeof(auxSession));

            mfxVersion ver = {1, 1}; // minimum API version which supports multiple devices
            MFXInit(MFX_IMPL_HARDWARE_ANY, &ver, &auxSession);
            MFXQueryIMPL(auxSession, &impl);
            MFXClose(auxSession);
        }

        // extract the base implementation type
        mfxIMPL baseImpl = MFX_IMPL_BASETYPE(impl);

        const struct
        {
            // actual implementation
            mfxIMPL impl;
            // adapter's number
            mfxU32 adapterID;

        } implTypes[] = {
            {MFX_IMPL_HARDWARE, 0},
            {MFX_IMPL_SOFTWARE, 0},
            {MFX_IMPL_HARDWARE2, 1},
            {MFX_IMPL_HARDWARE3, 2},
            {MFX_IMPL_HARDWARE4, 3}
        };


        // get corresponding adapter number
        for (mfxU8 i = 0; i < sizeof(implTypes)/sizeof(*implTypes); i++)
        {
            if (implTypes[i].impl == baseImpl)
            {
                adapterNum = implTypes[i].adapterID;
                break;
            }
        }

        return adapterNum;
    }
};

struct APIChangeFeatures {
    bool JpegDecode;
    bool JpegEncode;
    bool MVCDecode;
    bool MVCEncode;
    bool IntraRefresh;
    bool LowLatency;
    bool ViewOutput;
    bool LookAheadBRC;
    bool AudioDecode;
};

mfxVersion getMinimalRequiredVersion(const APIChangeFeatures &features);

enum MsdkTraceLevel {
    MSDK_TRACE_LEVEL_SILENT = -1,
    MSDK_TRACE_LEVEL_CRITICAL = 0,
    MSDK_TRACE_LEVEL_ERROR = 1,
    MSDK_TRACE_LEVEL_WARNING = 2,
    MSDK_TRACE_LEVEL_INFO = 3,
    MSDK_TRACE_LEVEL_DEBUG = 4,
};

msdk_string NoFullPath(const msdk_string &);
int  msdk_trace_get_level();
void msdk_trace_set_level(int);
bool msdk_trace_is_printable(int);

msdk_ostream & operator <<(msdk_ostream & os, MsdkTraceLevel tt);


#endif //__SAMPLE_UTILS_H__
