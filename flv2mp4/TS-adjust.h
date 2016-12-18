#ifndef TS_ADJUST_H
#define TS_ADJUST_H

#ifdef WIN32
typedef __int64 EInt64;
#else
typedef long EInt64;
#endif

class CTSAdjust
{
public:
	CTSAdjust();
	virtual ~CTSAdjust();

	EInt64 AdjustV(EInt64 vts);
	EInt64 AdjustA(EInt64 ats);


private:
	EInt64 _vts_base, _ats_base;
	EInt64 _vts_last, _ats_last;
	bool _bFirstV, _bFirstA;
};

#endif // TS_ADJUST_H
