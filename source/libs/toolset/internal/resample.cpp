//	VirtualDub - Video processing and capture application
//	Graphics support library
//	Copyright (C) 1998-2004 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "toolset.h"

#include "pixmap.h"
#include "resample.h"

#include <xmmintrin.h>

#pragma warning (disable:4456) // declaration of 'xxx' hides previous local declaration
#pragma warning (disable:4458) // declaration of 'xxx' hides class member

void VDCPUCleanupExtensions() {
#ifndef MODE64
    if (ts::CCAPS(ts::CPU_SSE))
        __asm sfence
    if (ts::CCAPS(ts::CPU_MMX))
        __asm emms
#else
    _mm_sfence();
#endif
}

///////////////////////////////////////////////////////////////////////////
//
// utility functions
//
///////////////////////////////////////////////////////////////////////////
void VDMemset32(void *dst, ts::uint32 value, size_t count) {
	if (count) {
        ts::uint32 *dst2 = (ts::uint32 *)dst;

		do {
			*dst2++ = value;
		} while(--count);
	}
}

sint32 scale32x32_fp16(sint32 x, sint32 y) 
{
	return (sint32)(((int64)x * y + 0x8000) >> 16);
}


#ifndef VDPTRSTEP_DECLARED
template<class T>
void vdptrstep(T *&p, ptrdiff_t offset) {
	p = (T *)((char *)p + offset);
}
#endif

#ifndef VDPTROFFSET_DECLARED
template<class T>
T *vdptroffset(T *p, ptrdiff_t offset) {
	return (T *)((char *)p + offset);
}
#endif

namespace ts
{

///////////////////////////////////////////////////////////////////////////
//
// sampling calculations
//
///////////////////////////////////////////////////////////////////////////

namespace {
	struct VDResamplerAxis {
		sint32		dx;
		sint32		u;
		sint32		dudx;
		uint32		dx_precopy;
		uint32		dx_preclip;
		uint32		dx_active;
		uint32		dx_postclip;
		uint32		dx_postcopy;
		uint32		dx_dualclip;

		void Init(sint32 dudx);
		void Compute(sint32 x1, sint32 x2, sint32 fx1_unclipped, sint32 u0, sint32 w, sint32 kernel_width);
	};

	void VDResamplerAxis::Init(sint32 dudx) {
		this->dudx = dudx;
	}

	void VDResamplerAxis::Compute(sint32 x1, sint32 x2, sint32 fx1_unclipped, sint32 u0, sint32 w, sint32 kernel_width) {
		u = u0 + scale32x32_fp16(dudx, ((x1<<16)+0x8000) - fx1_unclipped);

		dx = x2-x1;

		sint32 du_kern	= (kernel_width-1) << 16;
		//sint32 u2		= u + dudx*(dx-1);
		sint32 u_limit	= w << 16;

		dx_precopy	= 0;
		dx_preclip	= 0;
		dx_active	= 0;
		dx_postclip	= 0;
		dx_postcopy = 0;
		dx_dualclip	= 0;

		//sint32 dx_temp = dx;
#ifndef _FINAL
		sint32 u_start = u;
#endif

		// (desired - u0 + (dudx-1)) / dudx : first pixel >= desired

		sint32 dudx_m1_mu0	= dudx - 1 - u;
		sint32 first_preclip	= (dudx_m1_mu0 + 0x10000 - du_kern) / dudx;
		sint32 first_active		= (dudx_m1_mu0                    ) / dudx;
		sint32 first_postclip	= (dudx_m1_mu0 + u_limit - du_kern) / dudx;
		sint32 first_postcopy	= (dudx_m1_mu0 + u_limit - 0x10000) / dudx;

		// clamp
		if (first_preclip < 0)
			first_preclip = 0;
		if (first_active < first_preclip)
			first_active = first_preclip;
		if (first_postclip < first_active)
			first_postclip = first_active;
		if (first_postcopy < first_postclip)
			first_postcopy = first_postclip;
		if (first_preclip > dx)
			first_preclip = dx;
		if (first_active > dx)
			first_active = dx;
		if (first_postclip > dx)
			first_postclip = dx;
		if (first_postcopy > dx)
			first_postcopy = dx;

		// determine widths

		dx_precopy	= first_preclip;
		dx_preclip	= first_active - first_preclip;
		dx_active	= first_postclip - first_active;
		dx_postclip	= first_postcopy - first_postclip;
		dx_postcopy	= dx - first_postcopy;

		// sanity checks
#ifndef _FINAL
		sint32 pos0 = dx_precopy;
		sint32 pos1 = pos0 + dx_preclip;
		sint32 pos2 = pos1 + dx_active;
		sint32 pos3 = pos2 + dx_postclip;
#endif

		ASSERT(!((dx_precopy|dx_preclip|dx_active|dx_postcopy|dx_postclip) & 0x80000000));
		ASSERT(dx_precopy + dx_preclip + dx_active + dx_postcopy + dx_postclip == (uint)dx);

		ASSERT(!pos0			|| u_start + dudx*(pos0 - 1) <  0x10000 - du_kern);	// precopy -> preclip
		ASSERT( pos0 >= pos1	|| u_start + dudx*(pos0    ) >= 0x10000 - du_kern);
		ASSERT( pos1 <= pos0	|| u_start + dudx*(pos1 - 1) <  0);					// preclip -> active
		ASSERT( pos1 >= pos2	|| u_start + dudx*(pos1    ) >= 0 || !dx_active);
		ASSERT( pos2 <= pos1	|| u_start + dudx*(pos2 - 1) <  u_limit - du_kern || !dx_active);	// active -> postclip
		ASSERT( pos2 >= pos3	|| u_start + dudx*(pos2    ) >= u_limit - du_kern);
		ASSERT( pos3 <= pos2	|| u_start + dudx*(pos3 - 1) <  u_limit - 0x10000);	// postclip -> postcopy
		ASSERT( pos3 >= dx	|| u_start + dudx*(pos3    ) >= u_limit - 0x10000);

		u += dx_precopy * dudx;

		// test for overlapping clipping regions
		if (!dx_active) {
			dx_dualclip = dx_preclip + dx_postclip;
			dx_preclip = dx_postclip = 0;
		}
	}

	struct VDResamplerInfo {
		void		*mpDst;
		ptrdiff_t	mDstPitch;
		const void	*mpSrc;
		ptrdiff_t	mSrcPitch;
		vdpixsize	mSrcW;
		vdpixsize	mSrcH;

		VDResamplerAxis mXAxis;
		VDResamplerAxis mYAxis;
	};
}

///////////////////////////////////////////////////////////////////////////
//
// allocation
//
///////////////////////////////////////////////////////////////////////////

namespace {
    class IVDResamplerSeparableRowStage;
    class IVDResamplerSeparableColStage;
    class IVDResamplerFilter;
	class VDSteppedAllocator {
	public:
		typedef	size_t		size_type;
		typedef	ptrdiff_t	difference_type;

		VDSteppedAllocator(size_t initialSize = 1024);
		~VDSteppedAllocator();

		void clear();
		void *allocate(size_type n);

        //template<class T, class... _Valty> T* build(_Valty&&... _Val)
        //{
        //    T * t = (T *)allocate(sizeof(T));
        //    TSPLACENEW(t, std::forward<_Valty>(_Val)...);
        //    return t;
        //}

        template<class T> T* build()
        {
            T * t = (T *)allocate(sizeof(T));
            TSPLACENEW(t);
            return t;
        }
        template<class T> T* build(double p)
        {
            T * t = (T *)allocate(sizeof(T));
            TSPLACENEW(t, p);
            return t;
        }

        template<class T> T* build(IVDResamplerSeparableRowStage *p0, IVDResamplerSeparableColStage *p1)
        {
            T * t = (T *)allocate(sizeof(T));
            TSPLACENEW(t, p0, p1);
            return t;
        }

        template<class T> T* build(const IVDResamplerFilter& p)
        {
            T * t = (T *)allocate(sizeof(T));
            TSPLACENEW(t, p);
            return t;
        }
        
        

	protected:
		struct Block {
			Block *next;
		};

		Block *mpHead;
		char *mpAllocNext;
		size_t	mAllocLeft;
		size_t	mAllocNext;
		size_t	mAllocInit;
	};
	
	VDSteppedAllocator::VDSteppedAllocator(size_t initialSize)
		: mpHead(nullptr)
		, mpAllocNext(nullptr)
		, mAllocLeft(0)
		, mAllocNext(initialSize)
		, mAllocInit(initialSize)
	{
	}

	VDSteppedAllocator::~VDSteppedAllocator() {
		clear();
	}

	void VDSteppedAllocator::clear() {
		while(Block *p = mpHead) {
			mpHead = mpHead->next;
			MM_FREE(p);
		}
		mAllocLeft = 0;
		mAllocNext = mAllocInit;
	}

	void *VDSteppedAllocator::allocate(size_type n) {
		n = (n+15) & ~15;
		if (mAllocLeft < n) {
			mAllocLeft = mAllocNext;
			mAllocNext += (mAllocNext >> 1);
			if (mAllocLeft < n)
				mAllocLeft = n;

			Block *t = (Block *)MM_ALLOC(sizeof(Block) + mAllocLeft);

			if (mpHead)
				mpHead->next = t;

			mpHead = t;
			mpHead->next = nullptr;

			mpAllocNext = (char *)(mpHead + 1);
		}

		void *p = mpAllocNext;
		mpAllocNext += n;
		mAllocLeft -= n;
		return p;
	}

    ///////////////////////////////////////////////////////////////////////////
    //
    // filter kernels
    //
    ///////////////////////////////////////////////////////////////////////////
	class IVDResamplerFilter {
	public:
		virtual int GetFilterWidth() const = 0;
		virtual void GenerateFilterBank(float *dst) const = 0;
	};

	class VDResamplerLinearFilter : public IVDResamplerFilter {
	public:
		VDResamplerLinearFilter(float twofc)
			: mScale(twofc)
			, mTaps((int)ceil(1.0f / twofc) * 2)
		{
		}

		int GetFilterWidth() const { return mTaps; }

		void GenerateFilterBank(float *dst) const {
			const double basepos = -(double)((mTaps>>1)-1) * mScale;
			for(int offset=0; offset<256; ++offset) {
				double pos = basepos - offset*((1.0f/256.0f) * mScale);

				for(unsigned i=0; i<mTaps; ++i) {
					double t = 1.0f - fabs(pos);

					*dst++ = (float)(t+fabs(t));
					pos += mScale;
				}
			}
		}

	protected:
		float		mScale;
		unsigned	mTaps;
	};

	class VDResamplerCubicFilter : public IVDResamplerFilter {
	public:
		VDResamplerCubicFilter(float twofc, float A)
			: mScale(twofc)
			, mA(A)
			, mTaps((int)ceil(2.0f / twofc)*2)
		{
		}

		int GetFilterWidth() const { return mTaps; }

		void GenerateFilterBank(float *dst) const {
			const double a0 = ( 1.0  );
			const double a2 = (-3.0-mA);
			const double a3 = ( 2.0+mA);
			const double b0 = (-4.0*mA);
			const double b1 = ( 8.0*mA);
			const double b2 = (-5.0*mA);
			const double b3 = (     mA);
			const double basepos = -(double)((mTaps>>1)-1) * mScale;

			for(int offset=0; offset<256; ++offset) {
				double pos = basepos - offset*((1.0f/256.0f) * mScale);

				for(unsigned i=0; i<mTaps; ++i) {
					double t = fabs(pos);
					double v = 0;

					if (t < 1.0)
						v = a0 + (t*t)*(a2 + t*a3);
					else if (t < 2.0)
						v = b0 + t*(b1 + t*(b2 + t*b3));

					*dst++ = (float)v;
					pos += mScale;
				}
			}
		}

	protected:
		float		mScale;
		float		mA;
		unsigned	mTaps;
	};

	static inline double sinc(double x) {
		return fabs(x) < 1e-9 ? 1.0 : sin(x) / x;
	}

	class VDResamplerLanczos3Filter : public IVDResamplerFilter {
	public:
		VDResamplerLanczos3Filter(float twofc)
			: mScale(twofc)
			, mTaps((int)ceil(3.0f / twofc)*2)
		{
		}

		int GetFilterWidth() const { return mTaps; }

		void GenerateFilterBank(float *dst) const {
			static const double pi  = 3.1415926535897932384626433832795;	// pi
			static const double pi3 = 1.0471975511965977461542144610932;	// pi/3
			const float basepos = -(float)((mTaps>>1)-1) * mScale;

			for(int offset=0; offset<256; ++offset) {
				float t = basepos - offset*((1.0f/256.0f) * mScale);

				for(unsigned i=0; i<mTaps; ++i) {
					double v = 0;

					if (t < 3.0)
						v = sinc(pi*t) * sinc(pi3*t);

					*dst++ = (float)v;
					t += mScale;
				}
			}
		}

	protected:
		float		mScale;
		unsigned	mTaps;
	};

    ///////////////////////////////////////////////////////////////////////////
    //
    // resampler stages (common)
    //
    ///////////////////////////////////////////////////////////////////////////

    class IVDResamplerStage {
    public:
        virtual ~IVDResamplerStage() {}

        virtual void Process(const VDResamplerInfo& info) {}

        virtual sint32 GetHorizWindowSize() const { return 1; }
        virtual sint32 GetVertWindowSize() const { return 1; }
    };

    class IVDResamplerSeparableRowStage : public IVDResamplerStage {
        using IVDResamplerStage::Process; // no warning #1125 (ICC)
    public:
        virtual void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) = 0;
        virtual int GetWindowSize() const = 0;
    };

    class IVDResamplerSeparableColStage : public IVDResamplerStage {
        using IVDResamplerStage::Process; // no warning #1125 (ICC)
    public:
        virtual int GetWindowSize() const = 0;
        virtual void Process(void *dst, const void *const *src, uint32 w, sint32 phase) = 0;
    };

    ///////////////////////////////////////////////////////////////////////////
    //
    // resampler stages (portable)
    //
    ///////////////////////////////////////////////////////////////////////////

	void GenerateTable(sint32 *dst, const IVDResamplerFilter& filter)
    {
		const unsigned width = filter.GetFilterWidth();
		tmp_tbuf_t<float> filters; filters.set_count( width * 256, false );
		float *src = filters.begin();

		filter.GenerateFilterBank(src);

		for(unsigned phase=0; phase < 256; ++phase) {
			float sum = 0;

			for(unsigned i=0; i<width; ++i)
				sum += src[i];

			float scalefac = 16384.0f / sum;

			for(unsigned j=0; j<width; j += 2) {
				int v0 = VDRoundToIntFast(src[j+0] * scalefac);
				int v1 = VDRoundToIntFast(src[j+1] * scalefac);

				dst[j+0] = v0;
				dst[j+1] = v1;
			}

			src += width;
			dst += width;
		}
	}

	void SwizzleTable(sint32 *dst, unsigned pairs) {
		do {
			sint32 v0 = dst[0];
			sint32 v1 = dst[1];

			dst[0] = dst[1] = (v0 & 0xffff) + (v1<<16);
			dst += 2;
		} while(--pairs);
	}
}

class VDResamplerSeparablePointRowStage : public IVDResamplerSeparableRowStage {
public:
	int GetWindowSize() const {return 1;}
	void Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *src = (const uint32 *)src0;

		do {
			*dst++ = src[u>>16];
			u += dudx;
		} while(--w);
	}
};

class VDResamplerSeparableLinearRowStage : public IVDResamplerSeparableRowStage {
public:
	int GetWindowSize() const {return 2;}
	void Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *src = (const uint32 *)src0;

		do {
			const sint32 iu = u>>16;
			const uint32 p0 = src[iu];
			const uint32 p1 = src[iu+1];
			const uint32 f = (u >> 8) & 0xff;

			const uint32 p0_rb = p0 & 0xff00ff;
			const uint32 p1_rb = p1 & 0xff00ff;
			const uint32 p0_g = p0 & 0xff00;
			const uint32 p1_g = p1 & 0xff00;

			*dst++	= ((p0_rb + (((p1_rb - p0_rb)*f + 0x800080)>>8)) & 0xff00ff)
					+ ((p0_g  + (((p1_g  - p0_g )*f + 0x008000)>>8)) & 0x00ff00);
			u += dudx;
		} while(--w);
	}
};

class VDResamplerSeparableLinearColStage : public IVDResamplerSeparableColStage {
public:
	int GetWindowSize() const {return 2;}
	void Process(void *dst0, const void *const *srcarray, uint32 w, sint32 phase) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *src0 = (const uint32 *)srcarray[0];
		const uint32 *src1 = (const uint32 *)srcarray[1];
		const uint32 f = (phase >> 8) & 0xff;

		do {
			const uint32 p0 = *src0++;
			const uint32 p1 = *src1++;

			const uint32 p0_rb = p0 & 0xff00ff;
			const uint32 p1_rb = p1 & 0xff00ff;
			const uint32 p0_g = p0 & 0xff00;
			const uint32 p1_g = p1 & 0xff00;

			*dst++	= ((p0_rb + (((p1_rb - p0_rb)*f + 0x800080)>>8)) & 0xff00ff)
					+ ((p0_g  + (((p1_g  - p0_g )*f + 0x008000)>>8)) & 0x00ff00);
		} while(--w);
	}
};

class VDResamplerSeparableTableRowStage : public IVDResamplerSeparableRowStage {
public:
	VDResamplerSeparableTableRowStage(const IVDResamplerFilter& filter) 
    {
		mFilterBank.set_count(filter.GetFilterWidth() * 256);
		GenerateTable(mFilterBank.begin(), filter);
	}

	int GetWindowSize() const {return (int)(mFilterBank.count() >> 8);}

	void Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *src = (const uint32 *)src0;
		const unsigned ksize = (unsigned)(mFilterBank.count() >> 8);
		const sint32 *filterBase = mFilterBank.begin();

		do {
			const uint32 *src2 = src + (u>>16);
			const sint32 *filter = filterBase + ksize*((u>>8)&0xff);
			u += dudx;

			int r = 0x8000, g = 0x8000, b = 0x8000;
			for(unsigned i = ksize; i; --i) {
				uint32 p = *src2++;
				sint32 coeff = *filter++;

				r += ((p>>16)&0xff)*coeff;
				g += ((p>> 8)&0xff)*coeff;
				b += ((p    )&0xff)*coeff;
			}

			r <<= 2;
			g >>= 6;
			b >>= 14;

			if ((uint32)r >= 0x01000000)
				r = ~r >> 31;
			if ((uint32)g >= 0x00010000)
				g = ~g >> 31;
			if ((uint32)b >= 0x00000100)
				b = ~b >> 31;

			*dst++ = (r & 0xff0000) + (g & 0xff00) + (b & 0xff);
		} while(--w);
	}

protected:
	tmp_tbuf_t<sint32>	mFilterBank;
};

class VDResamplerSeparableTableColStage : public IVDResamplerSeparableColStage {
public:
	VDResamplerSeparableTableColStage(const IVDResamplerFilter& filter) {
		mFilterBank.set_count(filter.GetFilterWidth() * 256);
		GenerateTable(mFilterBank.begin(), filter);
	}

	int GetWindowSize() const {return (int)(mFilterBank.count() >> 8);}

	void Process(void *dst0, const void *const *src0, uint32 w, sint32 phase) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *const *src = (const uint32 *const *)src0;
		const unsigned ksize = (unsigned)(mFilterBank.count() >> 8);
		const sint32 *filter = &mFilterBank.get(((phase>>8)&0xff) * ksize);

		for(uint32 i=0; i<w; ++i) {
			int r = 0x8000, g = 0x8000, b = 0x8000;
			const sint32 *filter2 = filter;
			const uint32 *const *src2 = src;

			for(unsigned j = ksize; j; --j) {
				uint32 p = (*src2++)[i];
				sint32 coeff = *filter2++;

				r += ((p>>16)&0xff)*coeff;
				g += ((p>> 8)&0xff)*coeff;
				b += ((p    )&0xff)*coeff;
			}

			r <<= 2;
			g >>= 6;
			b >>= 14;

			if ((uint32)r >= 0x01000000)
				r = ~r >> 31;
			if ((uint32)g >= 0x00010000)
				g = ~g >> 31;
			if ((uint32)b >= 0x00000100)
				b = ~b >> 31;

			*dst++ = (r & 0xff0000) + (g & 0xff00) + (b & 0xff);
		} 
	}

protected:
	tbuf_t<sint32>	mFilterBank;
};

class VDResamplerSeparableStage : public IVDResamplerStage
{
    VDResamplerSeparableStage(const VDResamplerSeparableStage&) UNUSED;
    VDResamplerSeparableStage & operator=(const VDResamplerSeparableStage &) UNUSED;
public:
	VDResamplerSeparableStage(IVDResamplerSeparableRowStage *pRow, IVDResamplerSeparableColStage *pCol);
	~VDResamplerSeparableStage() {
		if (mpRowStage)
			mpRowStage->~IVDResamplerSeparableRowStage();
		if (mpColStage)
			mpColStage->~IVDResamplerSeparableColStage();
	}

	void Process(const VDResamplerInfo&);

	sint32 GetHorizWindowSize() const { return mpRowStage ? mpRowStage->GetWindowSize() : 1; }
	sint32 GetVertWindowSize() const { return mpColStage ? mpColStage->GetWindowSize() : 1; }

protected:
	void ProcessPoint();
	void ProcessComplex();
	void ProcessRow(void *dst, const void *src);

	IVDResamplerSeparableRowStage *const mpRowStage;
	IVDResamplerSeparableColStage *const mpColStage;

	uint32				mWinSize;
	uint32				mRowFiltW;

	VDResamplerInfo		mInfo;

	tbuf_t<void *>	mWindow;
	void				**mpAllocWindow;
	tbuf_t<uint32>	mTempSpace;
};

VDResamplerSeparableStage::VDResamplerSeparableStage(IVDResamplerSeparableRowStage *pRow, IVDResamplerSeparableColStage *pCol)
	: mpRowStage(pRow)
	, mpColStage(pCol)
{
	mWinSize = mpColStage ? mpColStage->GetWindowSize() : 1;
	mRowFiltW = mpRowStage->GetWindowSize();
	mWindow.set_count(3*mWinSize);
}

void VDResamplerSeparableStage::Process(const VDResamplerInfo& info) {
	mInfo = info;

	if (mpColStage || (mpRowStage->GetWindowSize()>1 && mInfo.mYAxis.dudx < 0x10000))
		ProcessComplex();
	else
		ProcessPoint();
}

void VDResamplerSeparableStage::ProcessPoint() {
	mTempSpace.set_count(mRowFiltW*3);

	const sint32 winsize = mWinSize;
	//const uint32 dx = mInfo.mXAxis.dx;

	const uint32 *src = (const uint32 *)mInfo.mpSrc;
	const ptrdiff_t srcpitch = mInfo.mSrcPitch;
	const sint32 srch = mInfo.mSrcH;
	void *dst = mInfo.mpDst;
	const ptrdiff_t dstpitch = mInfo.mDstPitch;


	/*uint32 csr_before =*/ _mm_getcsr();

	if (uint32 count = mInfo.mYAxis.dx_precopy) {
		do {
			ProcessRow(dst, src);
			vdptrstep(dst, dstpitch);
		} while(--count);
	}

	if (uint32 count = mInfo.mYAxis.dx_preclip + mInfo.mYAxis.dx_active + mInfo.mYAxis.dx_postclip + mInfo.mYAxis.dx_dualclip) {
		sint32 v = mInfo.mYAxis.u + ((winsize-1) >> 16);
		const sint32 dvdy = mInfo.mYAxis.dudx;

		do {
			sint32 y = (v >> 16);

			if (y<0)
				y=0;
			else if (y >= srch)
				y = srch-1;

			ProcessRow(dst, vdptroffset(src, y*srcpitch));

			vdptrstep(dst, dstpitch);
			v += dvdy;
		} while(--count);
	}

	if (uint32 count = mInfo.mYAxis.dx_postcopy) {
		const uint32 *srcrow = vdptroffset(src, srcpitch*(srch-1));
		do {
			ProcessRow(dst, srcrow);
			vdptrstep(dst, dstpitch);
		} while(--count);
	}

	/*uint32 csr_after=*/ _mm_getcsr();

}

void VDResamplerSeparableStage::ProcessComplex() {
	uint32 clipSpace = (mRowFiltW*3 + 3) & ~3;
	uint32 paddedWidth = (mInfo.mXAxis.dx + 3) & ~3;
	mTempSpace.set_count(clipSpace + paddedWidth * mWinSize);

	uint32 *p = mTempSpace.begin();
	p += clipSpace;

	mpAllocWindow = &mWindow.get(2*mWinSize);

	for(uint32 i=0; i<mWinSize; ++i) {
		mpAllocWindow[i] = p;
		p += paddedWidth;
	}

	const sint32 winsize = mWinSize;
	const uint32 dx = mInfo.mXAxis.dx;

	const uint32 *src = (const uint32 *)mInfo.mpSrc;
	const ptrdiff_t srcpitch = mInfo.mSrcPitch;
	const sint32 srch = mInfo.mSrcH;
	void *dst = mInfo.mpDst;
	const ptrdiff_t dstpitch = mInfo.mDstPitch;
	sint32 winpos = -1;
	sint32 winidx = mWinSize>1 ? 1 : 0;
	sint32 winallocnext = mWinSize>1 ? 1 : 0;

	std::fill(mWindow.begin(), mWindow.begin() + 2*mWinSize, mpAllocWindow[0]);

	if (mInfo.mYAxis.dx_precopy || mInfo.mYAxis.u < 0x10000) {
		winpos = 0;

		ProcessRow(mWindow.get(0), src);
	}

	if (uint32 count = mInfo.mYAxis.dx_precopy) {
		do {
			memcpy(dst, mWindow.get(0), dx * sizeof(uint32));
			vdptrstep(dst, dstpitch);
		} while(--count);
	}

	if (uint32 count = mInfo.mYAxis.dx_preclip + mInfo.mYAxis.dx_active + mInfo.mYAxis.dx_postclip + mInfo.mYAxis.dx_dualclip) {
		sint32 v = mInfo.mYAxis.u + ((winsize-1) >> 16);
		const sint32 dvdy = mInfo.mYAxis.dudx;

		do {
			sint32 desiredpos = (v >> 16) + winsize - 1;

			if (winpos < desiredpos - winsize)
				winpos = desiredpos - winsize;

			while(winpos+1 <= desiredpos) {
				++winpos;
				if (winpos >= srch) {
					mWindow.get(winidx) = mWindow.get(winidx + winsize) = mWindow.get(winidx ? winidx-1 : winsize-1);
				} else {
					mWindow.get(winidx) = mWindow.get(winidx + winsize) = mpAllocWindow[winallocnext];
					if (++winallocnext >= winsize)
						winallocnext = 0;

					ProcessRow(mWindow.get(winidx), vdptroffset(src, winpos*srcpitch));
				}
				if (++winidx >= winsize)
					winidx = 0;
			}

			if (mpColStage)
				mpColStage->Process(dst, &mWindow.get(winidx), dx, v);
			else
				memcpy(dst, mWindow.get(winidx), dx*sizeof(uint32));

			vdptrstep(dst, dstpitch);
			v += dvdy;
		} while(--count);
	}

	if (uint32 count = mInfo.mYAxis.dx_postcopy) {
		if (winpos < srch - 1) {
			mWindow.get(winidx) = mWindow.get(winidx + winsize) = mpAllocWindow[winallocnext];
			ProcessRow(mWindow.get(winidx), vdptroffset(src, srcpitch*(srch-1)));
			if (++winidx >= winsize)
				winidx = 0;
		}

		const void *p = mWindow.get(winidx + winsize - 1);
		do {
			memcpy(dst, p, dx * sizeof(uint32));
			vdptrstep(dst, dstpitch);
		} while(--count);
	}
}

void VDResamplerSeparableStage::ProcessRow(void *dst0, const void *src0) {
	const uint32 *src = (const uint32 *)src0;
	uint32 *dst = (uint32 *)dst0;

	// process pre-copy region
	if (uint32 count = mInfo.mXAxis.dx_precopy) {
		VDMemset32(dst, src[0], count);
		dst += count;
	}

	uint32 *p = mTempSpace.begin();
	sint32 u = mInfo.mXAxis.u;
	const sint32 dudx = mInfo.mXAxis.dudx;

	// process dual-clip region
	if (uint32 count = mInfo.mXAxis.dx_dualclip) {
		VDMemset32(p, src[0], mRowFiltW);
		memcpy(p + mRowFiltW, src+1, (mInfo.mSrcW-2)*sizeof(uint32));
		VDMemset32(p + mRowFiltW + (mInfo.mSrcW-2), src[mInfo.mSrcW-1], mRowFiltW);

		mpRowStage->Process(dst, p, count, u + ((mRowFiltW-1)<<16), dudx);
		u += dudx*count;
		dst += count;
	} else {
		// process pre-clip region
		if (uint32 count = mInfo.mXAxis.dx_preclip) {
			VDMemset32(p, src[0], mRowFiltW);
			memcpy(p + mRowFiltW, src+1, (mRowFiltW-1)*sizeof(uint32));

			mpRowStage->Process(dst, p, count, u + ((mRowFiltW-1)<<16), dudx);
			u += dudx*count;
			dst += count;
		}

		// process active region
		if (uint32 count = mInfo.mXAxis.dx_active) {
			mpRowStage->Process(dst, src, count, u, dudx);
			u += dudx*count;
			dst += count;
		}

		// process post-clip region
		if (uint32 count = mInfo.mXAxis.dx_postclip) {
			uint32 offset = mInfo.mSrcW + 1 - mRowFiltW;

			memcpy(p, src+offset, (mRowFiltW-1)*sizeof(uint32));
			VDMemset32(p + (mRowFiltW-1), src[mInfo.mSrcW-1], mRowFiltW);

			mpRowStage->Process(dst, p, count, u - (offset<<16), dudx);
			dst += count;
		}
	}

	// process post-copy region
	if (uint32 count = mInfo.mXAxis.dx_postcopy) {
		VDMemset32(dst, src[mInfo.mSrcW-1], count);
	}
}

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (scalar, x86)
//
///////////////////////////////////////////////////////////////////////////

namespace {
	struct ScaleInfo {
		void *dst;
		uintptr	src;
		uint32	accum;
		uint32	fracinc;
		sint32	intinc;
		uint32	count;
	};
}

#ifndef MODE64
	extern "C" void __cdecl vdasm_resize_point32(const ScaleInfo *);

	class VDResamplerSeparablePointRowStageX86 : public IVDResamplerSeparableRowStage {
	public:
		int GetWindowSize() const {return 1;}
		void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
			ScaleInfo info;

			info.dst = (uint32 *)dst + w;
			info.src = (ts::p_cast<uintptr>(src) >> 2) + (u>>16);
			info.accum = u<<16;
			info.fracinc = dudx << 16;
			info.intinc = (sint32)dudx >> 16;
			info.count = -(sint32)w*4;

			vdasm_resize_point32(&info);
		}
	};
#endif

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (MMX, x86)
//
///////////////////////////////////////////////////////////////////////////

#ifndef MODE64
	extern "C" void __cdecl vdasm_resize_point32_MMX(const ScaleInfo *);

	class VDResamplerSeparablePointRowStageMMX : public IVDResamplerSeparableRowStage {
	public:
		int GetWindowSize() const {return 1;}
		void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
			ScaleInfo info;

			info.dst = (uint32 *)dst + w;
			info.src = (ts::p_cast<uintptr>(src) >> 2) + (u>>16);
			info.accum = u<<16;
			info.fracinc = dudx << 16;
			info.intinc = (sint32)dudx >> 16;
			info.count = -(sint32)w*4;

			vdasm_resize_point32_MMX(&info);
		}
	};

	extern "C" void _cdecl vdasm_resize_interp_row_run_MMX(void *dst, const void *src, uint32 width, int64 xaccum, int64 x_inc);
	extern "C" void _cdecl vdasm_resize_interp_col_run_MMX(void *dst, const void *src1, const void *src2, uint32 width, uint32 yaccum);
	extern "C" void _cdecl vdasm_resize_ccint_row_MMX(void *dst, const void *src, uint32 count, uint32 xaccum, sint32 xinc, const void *tbl);
	extern "C" void _cdecl vdasm_resize_ccint_col_MMX(void *dst, const void *src1, const void *src2, const void *src3, const void *src4, uint32 count, const void *tbl);
	extern "C" long _cdecl vdasm_resize_table_col_MMX(uint32 *out, const uint32 *const*in_table, const int *filter, int filter_width, uint32 w, long frac);
	extern "C" long _cdecl vdasm_resize_table_row_MMX(uint32 *out, const uint32 *in, const int *filter, int filter_width, uint32 w, long accum, long frac);

	class VDResamplerSeparableLinearRowStageMMX : public IVDResamplerSeparableRowStage {
	public:
		int GetWindowSize() const {return 2;}
		void Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
			vdasm_resize_interp_row_run_MMX(dst0, src0, w, (int64)u << 16, (int64)dudx << 16);
		}
	};

	class VDResamplerSeparableLinearColStageMMX : public IVDResamplerSeparableColStage {
	public:
		int GetWindowSize() const {return 2;}
		void Process(void *dst0, const void *const *srcarray, uint32 w, sint32 phase) {
			vdasm_resize_interp_col_run_MMX(dst0, srcarray[0], srcarray[1], w, phase);
		}
	};

	class VDResamplerSeparableCubicRowStageMMX : public IVDResamplerSeparableRowStage {
	public:
		VDResamplerSeparableCubicRowStageMMX(double A)
			: mFilterBank(1024)
		{
			sint32 *p = mFilterBank.begin();
			GenerateTable(p, VDResamplerCubicFilter(1.0f, (float)A));
			SwizzleTable(p, 512);
		}

		int GetWindowSize() const {return 4;}
		void Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
			vdasm_resize_ccint_row_MMX(dst0, src0, w, u, dudx, mFilterBank.begin());
		}

	protected:
		tbuf_t<sint32> mFilterBank;
	};

	class VDResamplerSeparableCubicColStageMMX : public IVDResamplerSeparableColStage {
	public:
		VDResamplerSeparableCubicColStageMMX(double A)
			: mFilterBank(1024)
		{
			sint32 *p = mFilterBank.begin();
			GenerateTable(p, VDResamplerCubicFilter(1.0f, (float)A));
			SwizzleTable(p, 512);
		}

		int GetWindowSize() const {return 4;}
		void Process(void *dst0, const void *const *srcarray, uint32 w, sint32 phase) {
			vdasm_resize_ccint_col_MMX(dst0, srcarray[0], srcarray[1], srcarray[2], srcarray[3], w, mFilterBank.begin() + ((phase>>6)&0x3fc));
		}

	protected:
		tbuf_t<sint32> mFilterBank;
	};

	class VDResamplerSeparableTableRowStageMMX : public VDResamplerSeparableTableRowStage {
	public:
		VDResamplerSeparableTableRowStageMMX(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableRowStage(filter)
		{
			SwizzleTable(mFilterBank.begin(), mFilterBank.count() >> 1);
		}

		void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
			vdasm_resize_table_row_MMX((uint32 *)dst, (const uint32 *)src, (const int *)mFilterBank.begin(), mFilterBank.count() >> 8, w, u, dudx);
		}
	};

	class VDResamplerSeparableTableColStageMMX : public VDResamplerSeparableTableColStage {
	public:
		VDResamplerSeparableTableColStageMMX(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableColStage(filter)
		{
			SwizzleTable(mFilterBank.begin(), mFilterBank.count() >> 1);
		}

		void Process(void *dst, const void *const *src, uint32 w, sint32 phase) {
			vdasm_resize_table_col_MMX((uint32*)dst, (const uint32 *const *)src, (const int *)mFilterBank.begin(), mFilterBank.count() >> 8, w, (phase >> 8) & 0xff);
		}
	};
#endif

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (SSE2, x86)
//
///////////////////////////////////////////////////////////////////////////

#ifndef MODE64
	extern "C" long _cdecl vdasm_resize_table_col_SSE2(uint32 *out, const uint32 *const*in_table, const int *filter, int filter_width, uint32 w, long frac);
	extern "C" long _cdecl asm_resize_table_row_SSE2(uint32 *out, const uint32 *in, const int *filter, int filter_width, uint32 w, long accum, long frac);
	extern "C" void _cdecl asm_resize_ccint_col_SSE2(void *dst, const void *src1, const void *src2, const void *src3, const void *src4, uint32 count, const void *tbl);

	class VDResamplerSeparableCubicColStageSSE2 : public VDResamplerSeparableCubicColStageMMX {
	public:
		VDResamplerSeparableCubicColStageSSE2(double A)
			: VDResamplerSeparableCubicColStageMMX(A)
		{
		}

		void Process(void *dst0, const void *const *srcarray, uint32 w, sint32 phase) {
			asm_resize_ccint_col_SSE2(dst0, srcarray[0], srcarray[1], srcarray[2], srcarray[3], w, mFilterBank.begin() + ((phase>>6)&0x3fc));
		}
	};

	class VDResamplerSeparableTableRowStageSSE2 : public VDResamplerSeparableTableRowStageMMX {
	public:
		VDResamplerSeparableTableRowStageSSE2(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableRowStageMMX(filter)
		{
		}

		void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
			vdasm_resize_table_row_MMX((uint32 *)dst, (const uint32 *)src, (const int *)mFilterBank.begin(), mFilterBank.count() >> 8, w, u, dudx);
		}
	};

	class VDResamplerSeparableTableColStageSSE2 : public VDResamplerSeparableTableColStageMMX {
	public:
		VDResamplerSeparableTableColStageSSE2(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableColStageMMX(filter)
		{
		}

		void Process(void *dst, const void *const *src, uint32 w, sint32 phase) {
			vdasm_resize_table_col_SSE2((uint32*)dst, (const uint32 *const *)src, (const int *)mFilterBank.begin(), mFilterBank.count() >> 8, w, (phase >> 8) & 0xff);
		}
	};
#endif

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (SSE2, AMD64)
//
///////////////////////////////////////////////////////////////////////////

#ifdef MODE64
	extern "C" long vdasm_resize_table_col_SSE2(uint32 *out, const uint32 *const*in_table, const int *filter, int filter_width, uint32 w);
	extern "C" long vdasm_resize_table_row_SSE2(uint32 *out, const uint32 *in, const int *filter, int filter_width, uint32 w, long accum, long frac);

	class VDResamplerSeparableTableRowStageSSE2 : public VDResamplerSeparableTableRowStage {
	public:
		VDResamplerSeparableTableRowStageSSE2(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableRowStage(filter)
		{
			SwizzleTable(mFilterBank.begin(), (unsigned)(mFilterBank.count() >> 1));
		}

		void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
			vdasm_resize_table_row_SSE2((uint32 *)dst, (const uint32 *)src, (const int *)mFilterBank.data(), (unsigned)(mFilterBank.count() >> 8), w, u, dudx);
		}
	};

	class VDResamplerSeparableTableColStageSSE2 : public VDResamplerSeparableTableColStage {
	public:
		VDResamplerSeparableTableColStageSSE2(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableColStage(filter)
		{
			SwizzleTable(mFilterBank.begin(), (unsigned)(mFilterBank.count() >> 1));
		}

		void Process(void *dst, const void *const *src, uint32 w, sint32 phase) {
			const unsigned filtSize = (unsigned)(mFilterBank.count() >> 8);

			vdasm_resize_table_col_SSE2((uint32*)dst, (const uint32 *const *)src, (const int *)mFilterBank.data() + filtSize*((phase >> 8) & 0xff), filtSize, w);
		}
	};
#endif


///////////////////////////////////////////////////////////////////////////
//
// the resampler (finally)
//
///////////////////////////////////////////////////////////////////////////

class VDPixmapResampler : public IVDPixmapResampler {
public:
	VDPixmapResampler();
	~VDPixmapResampler();

	void SetSplineFactor(double A) { mSplineFactor = A; }
	bool Init(double dw, double dh, int dstformat, double sw, double sh, int srcformat, FilterMode hfilter, FilterMode vfilter, bool bInterpolationOnly);
	void Shutdown();

	void Process(const VDPixmap& dst, double dx1, double dy1, double dx2, double dy2, const VDPixmap& src, double sx, double sy);

protected:
	IVDResamplerSeparableRowStage *CreateRowStage(const IVDResamplerFilter&);
	IVDResamplerSeparableColStage *CreateColStage(const IVDResamplerFilter&);

	IVDResamplerStage	*mpRoot;
	double				mSplineFactor;
	VDResamplerInfo		mInfo;
	VDSteppedAllocator	mStageAllocator;
};

VDPixmapResampler::VDPixmapResampler()
	: mpRoot(nullptr)
	, mSplineFactor(-0.6)
{
}

VDPixmapResampler::~VDPixmapResampler() {
	Shutdown();
}

bool VDPixmapResampler::Init(double dw, double dh, int dstformat, double sw, double sh, int srcformat, FilterMode hfilter, FilterMode vfilter, bool bInterpolationOnly) {
	Shutdown();

	if (dstformat != srcformat || srcformat != nsVDPixmap::kPixFormat_XRGB8888)
		return false;

	// kill flips
	dw = fabs(dw);
	dh = fabs(dh);
	sw = fabs(sw);
	sh = fabs(sh);


	// compute gradients, using truncation toward zero
	//	_controlfp(_RC_CHOP, _MCW_RC);
	//	unsigned fpucw = _controlfp(_RC_CHOP, _MCW_RC);

	//	sint32 dudx = (sint32)((sw * 65536.0) / dw);
	//	sint32 dvdy = (sint32)((sh * 65536.0) / dh);

	//	_mm_setcsr(csr_before);
	//	_controlfp(fpucw, _MCW_RC);

	sint32 dudx = (sint32)floor(floor(sw * 65536.0) / dw);
	sint32 dvdy = (sint32)floor(floor(sh * 65536.0) / dh);

	mInfo.mXAxis.Init(dudx);
	mInfo.mYAxis.Init(dvdy);

	// construct stages
	double x_2fc = 1.0;
	double y_2fc = 1.0;

	if (!bInterpolationOnly && dw < sw)
		x_2fc = dw/sw;
	if (!bInterpolationOnly && dh < sh)
		y_2fc = dh/sh;

	IVDResamplerSeparableRowStage *pRowStage = nullptr;
	IVDResamplerSeparableColStage *pColStage = nullptr;

	if (hfilter == kFilterPoint) {
#ifndef MODE64

		if (CCAPS(CPU_MMX))
			pRowStage = mStageAllocator.build<VDResamplerSeparablePointRowStageMMX>();
		else
			pRowStage = mStageAllocator.build<VDResamplerSeparablePointRowStageX86>();
#else		
		pRowStage = mStageAllocator.build<VDResamplerSeparablePointRowStage>();
#endif
	} else if (hfilter == kFilterLinear) {
		if (x_2fc >= 1.0) {
#ifndef MODE64

			if (CCAPS(CPU_MMX))
				pRowStage = mStageAllocator.build<VDResamplerSeparableLinearRowStageMMX>();
			else
#endif
				pRowStage = mStageAllocator.build<VDResamplerSeparableLinearRowStage>();
		} else
			pRowStage = CreateRowStage(VDResamplerLinearFilter((float)x_2fc));
	} else if (hfilter == kFilterCubic) {
#ifndef MODE64
		if (x_2fc >= 1.0 && CCAPS(CPU_MMX))
			pRowStage = mStageAllocator.build<VDResamplerSeparableCubicRowStageMMX>(mSplineFactor);
		else
#endif
			pRowStage = CreateRowStage(VDResamplerCubicFilter( (float)x_2fc, (float)mSplineFactor));
	} else if (hfilter == kFilterLanczos3)
		pRowStage = CreateRowStage(VDResamplerLanczos3Filter( (float)x_2fc));

	if (hfilter == kFilterLinear) {
		if (y_2fc >= 1.0) {
#ifndef MODE64
			if (CCAPS(CPU_MMX))
				pColStage = mStageAllocator.build<VDResamplerSeparableLinearColStageMMX>();
			else
#endif
				pColStage = mStageAllocator.build<VDResamplerSeparableLinearColStage>();
		} else
			pColStage = CreateColStage(VDResamplerLinearFilter( (float)y_2fc ));
	} else if (hfilter == kFilterCubic) {
#ifndef MODE64
		if (y_2fc >= 1.0 && CCAPS(CPU_MMX)) {
			if (CCAPS(CPU_SSE2))
				pColStage = mStageAllocator.build<VDResamplerSeparableCubicColStageSSE2>(mSplineFactor);
			else
				pColStage = mStageAllocator.build<VDResamplerSeparableCubicColStageMMX>(mSplineFactor);
		} else
#endif
			pColStage = CreateColStage(VDResamplerCubicFilter( float(y_2fc), (float)mSplineFactor));
	} else if (hfilter == kFilterLanczos3)
		pColStage = CreateColStage(VDResamplerLanczos3Filter( (float)y_2fc));

	mpRoot = mStageAllocator.build<VDResamplerSeparableStage>(
					pRowStage,
					pColStage
					);

	return true;
}

IVDResamplerSeparableRowStage *VDPixmapResampler::CreateRowStage(const IVDResamplerFilter& filter) {
#ifndef MODE64
	if (CCAPS(CPU_SSE2))
		return mStageAllocator.build<VDResamplerSeparableTableRowStageSSE2>(filter);
	else if (CCAPS(CPU_MMX))
		return mStageAllocator.build<VDResamplerSeparableTableRowStageMMX>(filter);
	else
		return mStageAllocator.build<VDResamplerSeparableTableRowStage>(filter);
#else
	return mStageAllocator.build<VDResamplerSeparableTableRowStageSSE2>(filter);
#endif
}

IVDResamplerSeparableColStage *VDPixmapResampler::CreateColStage(const IVDResamplerFilter& filter) {
#ifndef MODE64
	if (CCAPS(CPU_SSE2))
		return mStageAllocator.build<VDResamplerSeparableTableColStageSSE2>(filter);
	else if (CCAPS(CPU_MMX))
		return mStageAllocator.build<VDResamplerSeparableTableColStageMMX>(filter);
	else
		return mStageAllocator.build<VDResamplerSeparableTableColStage>(filter);
#else
	return mStageAllocator.build<VDResamplerSeparableTableColStageSSE2>(filter);
#endif
}

void VDPixmapResampler::Shutdown() {
	if (mpRoot) {
		mpRoot->~IVDResamplerStage();
		mpRoot = nullptr;
	}

	mStageAllocator.clear();
}

void VDPixmapResampler::Process(const VDPixmap& dst, double dx1, double dy1, double dx2, double dy2, const VDPixmap& src, double sx1, double sy1) {
	if (!mpRoot)
		return;

	/*uint32 csr_before =*/ _mm_getcsr();

	// convert coordinates to fixed point
	sint32 fdx1 = (sint32)(dx1 * 65536.0);
	sint32 fdy1 = (sint32)(dy1 * 65536.0);
	sint32 fdx2 = (sint32)(dx2 * 65536.0);
	sint32 fdy2 = (sint32)(dy2 * 65536.0);
	sint32 fsx1 = (sint32)(sx1 * 65536.0);
	sint32 fsy1 = (sint32)(sy1 * 65536.0);

	// determine integer destination rectangle (OpenGL coordinates)
	sint32 idx1 = (fdx1 + 0x8000) >> 16;
	sint32 idy1 = (fdy1 + 0x8000) >> 16;
	sint32 idx2 = (fdx2 + 0x8000) >> 16;
	sint32 idy2 = (fdy2 + 0x8000) >> 16;

	// convert destination flips to source flips
	if (idx1 > idx2) {
		std::swap(idx1, idx2);
		sx1 += scale32x32_fp16(mInfo.mXAxis.dudx, idx2 - idx1);
	}

	if (idy1 > idy2) {
		std::swap(idy1, idy2);
		sy1 += scale32x32_fp16(mInfo.mYAxis.dudx, idy2 - idy1);
	}

	// clip destination rect
	if (idx1 < 0)
		idx1 = 0;
	if (idy1 < 0)
		idy1 = 0;
	if (idx2 > dst.w)
		idx2 = dst.w;
	if (idy2 > dst.h)
		idy2 = dst.h;

	// check for degenerate conditions
	if (idx1 >= idx2 || idy1 >= idy2)
		return;

	// render
	const sint32 winw = mpRoot->GetHorizWindowSize();
	const sint32 winh = mpRoot->GetVertWindowSize();

	fsx1 -= (winw-1)<<15;
	fsy1 -= (winh-1)<<15;

	mInfo.mXAxis.Compute(idx1, idx2, fdx1, fsx1, src.w, winw);
	mInfo.mYAxis.Compute(idy1, idy2, fdy1, fsy1, src.h, winh);

	mInfo.mpSrc = src.data;
	mInfo.mSrcPitch = src.pitch;
	mInfo.mSrcW = src.w;
	mInfo.mSrcH = src.h;
	mInfo.mpDst = vdptroffset(dst.data, dst.pitch * idy1 + idx1*4);
	mInfo.mDstPitch = dst.pitch;

//	if (mpRoot) 
	{
		mpRoot->Process(mInfo);
		VDCPUCleanupExtensions();
	}
}

bool VDPixmapResample(const VDPixmap& dst, const VDPixmap& src, IVDPixmapResampler::FilterMode filter) {
	VDPixmapResampler r;

	if (!r.Init(dst.w, dst.h, dst.format, src.w, src.h, src.format, filter, filter, false))
		return false;

	r.Process(dst, 0, 0, dst.w, dst.h, src, 0, 0);
	return true;
}

double _Lanczos3(double x)
{
	const double R = 3.0;
	if (x  < 0.0 ) x = - x;

	if ( x == 0.0) return 1; //-V550

	if ( x < R)
	{
#define M_PI       3.14159265358979323846
		x *= M_PI;
		return R * sin(x) * sin(x / R) / (x*x);
	}
	return 0.0;
}

bool resample(tmp_buf_c &bb, const uint8 * ibuf, long iw, long ih, uint8 * obuf, long ow, long oh, double dRadius)
{	
	const int COLOR_COMPONENTS = 4;

	bool fSuccess = false;
	
	long i, j, n, c;
	double xScale, yScale;

	/* Alias (pointer to uint32) for ibuf */
	uint32 * ib; 
  /* Alias (pointer to uint32 ) for obuf */
	uint32 * ob;

	// Temporary values
	uint32 val; 
	int col; /* This should remain int (a bit tricky stuff) */ 

	double * h_weight; // Weight contribution    [ow][MAX_CONTRIBS]
	long    * h_pixel; // Pixel that contributes [ow][MAX_CONTRIBS]
	long    * h_count; // How many contribution for the pixel [ow]
	double * h_wsum;   // Sum of weights [ow]
										 
	double * v_weight; // Weight contribution    [oh][MAX_CONTRIBS]
	long   * v_pixel;  // Pixel that contributes [oh][MAX_CONTRIBS]
	long  * v_count;	 // How many contribution for the pixel [oh]
	double * v_wsum;   // Sum of weights [oh]
	
	uint32 * tb;        // Temporary (intermediate buffer)

	
	double intensity[COLOR_COMPONENTS];	// RGBA component intensities
	
	double center;				// Center of current sampling 
	double weight;				// Current wight
	long left;						// Left of current sampling
	long right;						// Right of current sampling

	double * p_weight;		// Temporary pointer
	long   * p_pixel;     // Temporary pointer

	long MAX_CONTRIBS;    // Almost-const: max number of contribution for current sampling
	double SCALED_RADIUS;	// Almost-const: scaled radius for downsampling operations
	double FILTER_FACTOR; // Almost-const: filter factor for downsampling operations

	/* Preliminary (redundant ? ) check */
	if ( iw < 1 || ih < 1 || ibuf == nullptr || ow <1 || oh<1 || obuf == nullptr)
		return false;
	
	/* Aliasing buffers */
	ib = (uint32 *)ibuf;
	ob = (uint32 *)obuf;
	
	if ( ow == iw && oh == ih)
	{ /* Aame size, no resampling */
        memcpy(ob, ib, iw * ih * sizeof( TSCOLOR ) );
		return true;
	}

	xScale = ((double)ow / iw);
	yScale = ((double)oh / ih);

	h_weight = nullptr; 
	h_pixel  = nullptr; 
	h_count  = nullptr; 
	h_wsum   = nullptr; 

	v_weight = nullptr; 
	v_pixel  = nullptr; 
	v_count  = nullptr; 
	v_wsum   = nullptr; 


	tb = nullptr;
	
	tb = (uint32 * ) bb.use( ow * ih * sizeof( uint32 ) );
	
	if ( ! tb ) goto Cleanup; 

	if ( xScale > 1.0)
	{
		/* Horizontal upsampling */
		FILTER_FACTOR = 1.0;
		SCALED_RADIUS = dRadius;
	}
	else
	{ /* Horizontal downsampling */ 
		FILTER_FACTOR = xScale;
		SCALED_RADIUS = dRadius / xScale;
	}
	/* The maximum number of contributions for a target pixel */
	MAX_CONTRIBS  = (int) (2 * SCALED_RADIUS  + 1);

	/* Pre-allocating all of the needed memory */
	h_weight = (double *) bb.use(ow * MAX_CONTRIBS * sizeof(double)); /* weights */
	h_pixel  = (long*) bb.use(ow * MAX_CONTRIBS * sizeof(int)); /* the contributing pixels */
	h_count  = (long*) bb.use(ow * sizeof(int)); /* how may contributions for the target pixel */
	h_wsum   = (double *)bb.use(ow * sizeof(double)); /* sum of the weights for the target pixel */
	
	if ( !( h_weight && h_pixel || h_count || h_wsum) ) goto Cleanup;

	/* Pre-calculate weights contribution for a row */
	for (i = 0; i < ow; i++)
	{
		p_weight    = h_weight + i * MAX_CONTRIBS;
		p_pixel     = h_pixel  + i * MAX_CONTRIBS;

		h_count[i] = 0;
		h_wsum[i] =  0.0;
		
		center = ((double)i)/xScale;
		left = (int)((center + .5) - SCALED_RADIUS);
		right = (int)(left + 2 * SCALED_RADIUS);

		for (j = left; j<= right; j++)
		{
			if ( j < 0 || j >= iw) continue;
		
			weight = _Lanczos3( (center - j) * FILTER_FACTOR);
			
			if (weight == 0.0) continue; //-V550

			n = h_count[i]; /* Since h_count[i] is our current index */
			p_pixel[n] = j;
			p_weight[n] = weight;
			h_wsum[i] += weight;
			h_count[i]++; /* Increment contribution count */
		}/* j */
	}/* i */

	/* Filter horizontally from input to temporary buffer */
	for (n = 0; n < ih; n++)
	{
		/* Here 'n' runs on the vertical coordinate */
		for ( i = 0; i < ow; i++)
		{/* i runs on the horizontal coordinate */
			p_weight = h_weight + i * MAX_CONTRIBS;
			p_pixel  = h_pixel  + i * MAX_CONTRIBS;

			for (c=0; c<COLOR_COMPONENTS; c++)
			{
				intensity[c] = 0.0;
			}
			for (j=0; j < h_count[i]; j++)
			{
				weight = p_weight[j];	
				val = ib[p_pixel[j] + n * iw]; /* Using val as temporary storage */
				/* Acting on color components */
				for (c=0; c<COLOR_COMPONENTS; c++)
				{
					intensity[c] += (val & 0xFF) * weight;
					val = val >> 8;
				}				
			}
			/* val is already 0 */val = 0;
			for (c= 0 ; c < COLOR_COMPONENTS; c++)
			{
				val = val << 8;
				col = (int)(intensity[COLOR_COMPONENTS - c - 1] / h_wsum[i]);
			  if (col < 0) col = 0;
				if (col > 255) col = 255;
				val |= col; 
			}
			tb[i+n*ow] = val; /* Temporary buffer ow x ih */
		}/* i */
	}/* n */

	/* Going to vertical stuff */
	if ( yScale > 1.0)
	{
		FILTER_FACTOR = 1.0;
		SCALED_RADIUS = dRadius;
	}
	else
	{
		FILTER_FACTOR = yScale;
		SCALED_RADIUS = dRadius / yScale;
	}
	MAX_CONTRIBS  = (int) (2 * SCALED_RADIUS  + 1);

	/* Pre-calculate filter contributions for a column */
	v_weight = (double *) bb.use(oh * MAX_CONTRIBS * sizeof(double)); /* Weights */
	v_pixel  = (long*) bb.use(oh * MAX_CONTRIBS * sizeof(int)); /* The contributing pixels */
	v_count  = (long*) bb.use(oh * sizeof(int)); /* How may contributions for the target pixel */
	v_wsum   = (double *)bb.use(oh * sizeof(double)); /* Sum of the weights for the target pixel */

	if ( !( v_weight && v_pixel && v_count && v_wsum) ) goto Cleanup;

	for (i = 0; i < oh; i++)
	{
		p_weight = v_weight + i * MAX_CONTRIBS;
		p_pixel  = v_pixel  + i * MAX_CONTRIBS;
		
		v_count[i] = 0;
		v_wsum[i] = 0.0;

		center = ((double) i) / yScale;
		left = (int) (center+.5 - SCALED_RADIUS);
		right = (int)( left + 2 * SCALED_RADIUS);

		for (j = left; j <= right; j++)
		{
			if (j < 0 || j >= ih) continue;

			weight = _Lanczos3((center -j)* FILTER_FACTOR);
			
			if ( weight == 0.0 ) continue; //-V550
			n = v_count[i]; /* Our current index */
			p_pixel[n] = j;
			p_weight[n] = weight;
			v_wsum[i]+= weight;
			v_count[i]++; /* Increment the contribution count */
		}/* j */
	}/* i */

	/* Filter vertically from work to output */
	for (n = 0; n < ow; n++)
	{
		 for (i = 0; i < oh; i++)
		 {
			 p_weight = v_weight + i * MAX_CONTRIBS;
			 p_pixel     = v_pixel  + i * MAX_CONTRIBS;

			 for (c=0; c<COLOR_COMPONENTS; c++)
			 {
				 intensity[c] = 0.0;
			 }
				
			 for (j = 0; j < v_count[i]; j++)
			 {
				 weight = p_weight[j];
 				 val = tb[ n + ow * p_pixel[j]]; /* Using val as temporary storage */
				 /* Acting on color components */
				 for (c=0; c<COLOR_COMPONENTS; c++)
				 {
					 intensity[c] += (val & 0xFF) * weight;
					 val = val >> 8;
				 }				
			 }
			/* val is already 0 */val = 0;
			for (c=0; c<COLOR_COMPONENTS; c++)
			{
				val = val << 8;
				col = (int)(intensity[COLOR_COMPONENTS - c - 1] / v_wsum[i]);
			  if (col < 0) col = 0;
				if (col > 255) col = 255;
				val |= col;
			}
			ob[n+i*ow] = val;
		}/* i */
	}/* n */

	fSuccess = true;

Cleanup: /* CLEANUP */

    /*
	if ( tb ) MM_FREE(tb);
	
	if (h_weight) MM_FREE(h_weight);
	if (h_pixel) MM_FREE(h_pixel);
	if (h_count) MM_FREE(h_count);
	if (h_wsum) MM_FREE(h_wsum);

	if (v_weight) MM_FREE(v_weight);
	if (v_pixel) MM_FREE(v_pixel);
	if (v_count) MM_FREE(v_count);
	if (v_wsum) MM_FREE(v_wsum);
    */

	return fSuccess;
} /* _resample */


#define USE_ASM

    struct VDResizeFilterData
    {
        double new_x, new_y;
        long new_xf, new_yf;
        int filter_mode;
        //COLORREF	rgbColor;

        VDPixmapResampler resampler;

        const uint8* src;
        imgdesc_s srcinfo;

        const bmpcore_exbody_s& bdst;
        uint8* dst;
        uint dst_h;
        uint dst_w;
        int  dst_pitch;	

        bool	fLetterbox;
        bool	fInterlaced;

        VDResizeFilterData(const uint8 *source, const imgdesc_s &souinfo, const bmpcore_exbody_s& dst) : src(source), srcinfo(souinfo), bdst(dst)
        {
        }

    private:
        VDResizeFilterData(const VDResizeFilterData &) UNUSED;
        VDResizeFilterData & operator=(const VDResizeFilterData &) UNUSED;
    };

    ////////////////////

    VDPixmap VDAsPixmap(const uint8 *sou, const imgdesc_s &souinfo)
    {
        VDPixmap pxm = 
        {
            (void *)sou,
            nullptr,
            souinfo.sz.x,
            souinfo.sz.y,
            souinfo.pitch,
            nsVDPixmap::kPixFormat_XRGB8888
        };

        /*
        switch(bm.info().bitpp) 
        {
        case 8:		pxm.format = nsVDPixmap::kPixFormat_Y8; break;
        case 15:	pxm.format = nsVDPixmap::kPixFormat_XRGB1555; break;
        case 16:	pxm.format = nsVDPixmap::kPixFormat_RGB565; break;
        case 24:	pxm.format = nsVDPixmap::kPixFormat_RGB888; break;
        case 32:	pxm.format = nsVDPixmap::kPixFormat_XRGB8888; break;
        }
        pxm.format = nsVDPixmap::kPixFormat_XRGB8888;
        */


        return pxm;
    }

    static int resize_run(VDResizeFilterData *mfd) 
    {
        VDPixmap pxdst(VDAsPixmap(mfd->bdst(), mfd->bdst.info()));
        VDPixmap pxsrc(VDAsPixmap(mfd->src, mfd->srcinfo));

        double dstw = mfd->new_x;
        double dsth = mfd->new_y;

        double dx = (mfd->dst_w - dstw) * 0.5;
        double dy = (mfd->dst_h - dsth) * 0.5;

        mfd->resampler.Process(pxdst, dx, dy, dx + mfd->new_x, dy + mfd->new_y, pxsrc, 0, 0);

        return 0;
    }

    static int resize_start(VDResizeFilterData *mfd)
    {
        double dstw = mfd->new_x;
        double dsth = mfd->new_y;

        if (dstw<1 || dsth<1)
            return 1;

        IVDPixmapResampler::FilterMode fmode = IVDPixmapResampler::kFilterPoint;
        bool bInterpolationOnly = true;

        //mfd->resampler = VDCreatePixmapResampler();

        switch(mfd->filter_mode) {
    case FILTER_NONE:
        fmode = IVDPixmapResampler::kFilterPoint;
        break;
    case FILTER_TABLEBILINEAR:
        bInterpolationOnly = false;
    case FILTER_BILINEAR:
        fmode = IVDPixmapResampler::kFilterLinear;
        break;
    case FILTER_TABLEBICUBIC060:
        mfd->resampler.SetSplineFactor(-0.60);
        fmode = IVDPixmapResampler::kFilterCubic;
        bInterpolationOnly = false;
        break;
    case FILTER_TABLEBICUBIC075:
        bInterpolationOnly = false;
    case FILTER_BICUBIC:
        mfd->resampler.SetSplineFactor(-0.75);
        fmode = IVDPixmapResampler::kFilterCubic;
        break;
    case FILTER_TABLEBICUBIC100:
        bInterpolationOnly = false;
        mfd->resampler.SetSplineFactor(-1.0);
        fmode = IVDPixmapResampler::kFilterCubic;
        break;
    case FILTER_LANCZOS3:
        bInterpolationOnly = false;
        fmode = IVDPixmapResampler::kFilterLanczos3;
        break;
        }

        mfd->resampler.Init(dstw, dsth, nsVDPixmap::kPixFormat_XRGB8888, mfd->srcinfo.sz.x, mfd->srcinfo.sz.y, nsVDPixmap::kPixFormat_XRGB8888, fmode, fmode, bInterpolationOnly);

        return 0;
    }

    static int resize_stop(VDResizeFilterData *mfd) 
    {
        //delete mfd->resampler;	mfd->resampler = nullptr;

        return 0;
    }

#include <xmmintrin.h>
    bool resize3( const bmpcore_exbody_s &extbody, const uint8 *sou, const imgdesc_s &souinfo, resize_filter_e filt_mode=FILTER_NONE )
    {
        VDResizeFilterData mfd(sou, souinfo, extbody);

        mfd.new_x		=extbody.info().sz.x;
        mfd.new_y		=extbody.info().sz.y;

        mfd.dst 	    =extbody();
        mfd.dst_h       =extbody.info().sz.y;
        mfd.dst_w	    =extbody.info().sz.x;
        mfd.dst_pitch   =extbody.info().pitch;

        mfd.filter_mode =filt_mode;

		if (extbody.info().sz.x < 16 || extbody.info().sz.y < 16)
		{
            int iw = souinfo.sz.x;
            int ih = souinfo.sz.y;

            double dRadius = 3.0;

            double xScale = ((double)mfd.dst_w / iw);
            double SCALED_RADIUS = dRadius;
            if (xScale <= 1.0)
                SCALED_RADIUS = dRadius / xScale;
            aint MAX_CONTRIBS = (int)(2 * SCALED_RADIUS + 1);
            
            double yScale = ((double)mfd.dst_h / ih);
            double SCALED_RADIUSh = dRadius;
            if (yScale < 1.0)
                SCALED_RADIUSh = dRadius / yScale;

            aint MAX_CONTRIBSh  = (int) (2 * SCALED_RADIUSh  + 1);

            aint sz = mfd.dst_w * ih * sizeof( uint32 ) + 
                +(mfd.dst_w * MAX_CONTRIBS * sizeof(double))
                +(mfd.dst_w * MAX_CONTRIBS * sizeof(int))
                +(mfd.dst_w * sizeof(int))
                +(mfd.dst_w * sizeof(double))
                +(mfd.dst_h * MAX_CONTRIBSh * sizeof(double))
                +(mfd.dst_h * MAX_CONTRIBSh * sizeof(int))
                +(mfd.dst_h * sizeof(int))
                +(mfd.dst_h * sizeof(double));

            tmp_buf_c bb( sz );

			return resample(bb, sou, iw, ih, mfd.dst, mfd.dst_w, mfd.dst_h, dRadius) != 0;
		}

        bool r(false);
        if( resize_start(&mfd) !=1)
        {
            resize_run(&mfd);
            resize_stop(&mfd);
            r=true;
        }

        //delete mfd;
        return r;
    }

} // namespace ts
