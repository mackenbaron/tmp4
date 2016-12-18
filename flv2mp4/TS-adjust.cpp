#include "TS-adjust.h"

#define DIFF_THRESHOLD 1000
#define DIFF_THRESHOLD_F -1000

CTSAdjust::CTSAdjust()
{
	_vts_base = -30;
	_ats_base = -30;

	_vts_last = -30;
	_ats_last = -30;

}

CTSAdjust::~CTSAdjust()
{

}

EInt64 CTSAdjust::AdjustV(EInt64 vts)
{
	EInt64 diff = vts - _vts_last;
	if (diff > DIFF_THRESHOLD_F && diff < DIFF_THRESHOLD)
	{
		_vts_last = vts;
		_vts_base += diff;

		if (_vts_base < _ats_base - 500)
			_vts_base = _ats_base;
		return _vts_base;
	}

	_vts_base += 30;
	_vts_last = vts;
	return _vts_base;
}

EInt64 CTSAdjust::AdjustA(EInt64 ats)
{
	EInt64 diff = ats - _ats_last;
	if (diff > DIFF_THRESHOLD_F && diff < DIFF_THRESHOLD)
	{
		_ats_last = ats;
		_ats_base += diff;

		if (_ats_base < _vts_base - 500)
			_ats_base = _vts_base;
		return _ats_base;
	}

	_ats_base += 30;
	_ats_last = ats;
	return _ats_base;
}
