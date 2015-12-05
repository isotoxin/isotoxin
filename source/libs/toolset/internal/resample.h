#pragma once
#include <float.h>
#include "../toolset.h"
#include "vdstl.h"
#include "math.h"

struct VDPixmap;

namespace ts
{

class IVDPixmapResampler
{
public:
	enum FilterMode {
		kFilterPoint,
		kFilterLinear,
		kFilterCubic,
		kFilterLanczos3,
		kFilterCount
	};

	virtual ~IVDPixmapResampler() {}
	virtual void SetSplineFactor(double A) = 0;
	virtual bool Init(double dw, double dh, int dstformat, double sw, double sh, int srcformat, FilterMode hfilter, FilterMode vfilter, bool bInterpolationOnly) = 0;
	virtual void Shutdown() = 0;

	virtual void Process(const VDPixmap& dst, double dx1, double dy1, double dx2, double dy2, const VDPixmap& src, double sx, double sy) = 0;
};

bool VDPixmapResample(const VDPixmap& dst, const VDPixmap& src, IVDPixmapResampler::FilterMode filter);

}
