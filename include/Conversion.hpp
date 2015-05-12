/*
* BM3D denoising filter - VapourSynth plugin
* Copyright (C) 2015  mawen1250
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef _CONVERSION_HPP_
#define _CONVERSION_HPP_


#include "Helper.h"
#include "Specification.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template < typename _Dt1, typename _St1 >
void RangeConvert(_Dt1 *dst, const _St1 *src,
    const PCType height, const PCType width, const PCType dst_stride, const PCType src_stride,
    _Dt1 dFloor, _Dt1 dNeutral, _Dt1 dCeil,
    _St1 sFloor, _St1 sNeutral, _St1 sCeil,
    bool clip = false)
{
    typedef _St1 srcType;
    typedef _Dt1 dstType;

    const bool dstFloat = isFloat(dstType);

    const auto sRange = sCeil - sFloor;
    const auto dRange = dCeil - dFloor;

    bool srcPCChroma = isPCChroma(sFloor, sNeutral, sCeil);
    bool dstPCChroma = isPCChroma(dFloor, dNeutral, dCeil);

    // Always apply clipping if source is PC range chroma
    if (srcPCChroma) clip = true;

    FLType gain = static_cast<FLType>(dRange) / sRange;
    FLType offset = -static_cast<FLType>(sNeutral) * gain + dNeutral;
    if (!dstFloat) offset += FLType(dstPCChroma ? 0.499999 : 0.5);

    if (clip)
    {
        const FLType lowerL = static_cast<FLType>(dFloor);
        const FLType upperL = static_cast<FLType>(dCeil);

        LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
        {
            dst[i0] = static_cast<dstType>(Clip(static_cast<FLType>(src[i1]) * gain + offset, lowerL, upperL));
        });
    }
    else
    {
        LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
        {
            dst[i0] = static_cast<dstType>(static_cast<FLType>(src[i1]) * gain + offset);
        });
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template < typename _Dt1, typename _St1 >
void ConvertToY(_Dt1 *dst, const _St1 *srcR, const _St1 *srcG, const _St1 *srcB,
    const PCType height, const PCType width, const PCType dst_stride, const PCType src_stride,
    _Dt1 dFloor, _Dt1 dCeil, _St1 sFloor, _St1 sCeil,
    ColorMatrix matrix = ColorMatrix::OPP)
{
    typedef _St1 srcType;
    typedef _Dt1 dstType;

    const bool dstFloat = isFloat(dstType);

    const auto sRange = sCeil - sFloor;
    const auto dRange = dCeil - dFloor;

    if (matrix == ColorMatrix::GBR)
    {
        RangeConvert(dst, srcG, height, width, dst_stride, src_stride, dFloor, dFloor, dCeil, sFloor, sFloor, sCeil, false);
    }
    else if (matrix == ColorMatrix::OPP)
    {
        FLType gain = static_cast<FLType>(dRange) / (sRange * FLType(3));
        FLType offset = -static_cast<FLType>(sFloor) * FLType(3) * gain + dFloor;
        if (!dstFloat) offset += FLType(0.5);

        LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
        {
            dst[i0] = static_cast<dstType>(
                (static_cast<FLType>(srcR[i1])
                + static_cast<FLType>(srcG[i1])
                + static_cast<FLType>(srcB[i1]))
                * gain + offset);
        });
    }
    else if (matrix == ColorMatrix::Minimum)
    {
        FLType gain = static_cast<FLType>(dRange) / sRange;
        FLType offset = -static_cast<FLType>(sFloor) * gain + dFloor;
        if (!dstFloat) offset += FLType(0.5);

        LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
        {
            dst[i0] = static_cast<dstType>(static_cast<FLType>(
                ::Min(srcR[i1], ::Min(srcG[i1], srcB[i1]))
                ) * gain + offset);
        });
    }
    else if (matrix == ColorMatrix::Maximum)
    {
        FLType gain = static_cast<FLType>(dRange) / sRange;
        FLType offset = -static_cast<FLType>(sFloor) * gain + dFloor;
        if (!dstFloat) offset += FLType(0.5);

        LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
        {
            dst[i0] = static_cast<dstType>(static_cast<FLType>(
                ::Max(srcR[i1], ::Max(srcG[i1], srcB[i1]))
                ) * gain + offset);
        });
    }
    else
    {
        FLType gain = static_cast<FLType>(dRange) / sRange;
        FLType offset = -static_cast<FLType>(sFloor) * gain + dFloor;
        if (!dstFloat) offset += FLType(0.5);

        FLType Kr, Kg, Kb;
        ColorMatrix_Parameter(matrix, Kr, Kg, Kb);

        Kr *= gain;
        Kg *= gain;
        Kb *= gain;

        LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
        {
            dst[i0] = static_cast<dstType>(
                Kr * static_cast<FLType>(srcR[i1])
                + Kg * static_cast<FLType>(srcG[i1])
                + Kb * static_cast<FLType>(srcB[i1])
                + offset);
        });
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template < typename _Dt1, typename _St1 >
void MatrixConvert_RGB2YUV(_Dt1 *dstY, _Dt1 *dstU, _Dt1 *dstV,
    const _St1 *srcR, const _St1 *srcG, const _St1 *srcB,
    const PCType height, const PCType width, const PCType dst_stride, const PCType src_stride,
    _Dt1 dFloorY, _Dt1 dCeilY, _Dt1 dFloorC, _Dt1 dNeutralC, _Dt1 dCeilC, _St1 sFloor, _St1 sCeil,
    ColorMatrix matrix = ColorMatrix::OPP)
{
    typedef _St1 srcType;
    typedef _Dt1 dstType;

    const bool dstFloat = isFloat(dstType);

    const bool dstPCChroma = isPCChroma(dFloorC, dNeutralC, dCeilC);

    const auto sRange = sCeil - sFloor;
    const auto dRangeY = dCeilY - dFloorY;
    const auto dRangeC = dCeilC - dFloorC;

    if (matrix == ColorMatrix::GBR)
    {
        RangeConvert(dstY, srcG, height, width, dst_stride, src_stride, dFloorY, dFloorY, dCeilY, sFloor, sFloor, sCeil, false);
        RangeConvert(dstU, srcB, height, width, dst_stride, src_stride, dFloorY, dFloorY, dCeilY, sFloor, sFloor, sCeil, false);
        RangeConvert(dstV, srcR, height, width, dst_stride, src_stride, dFloorY, dFloorY, dCeilY, sFloor, sFloor, sCeil, false);
    }
    else if (matrix == ColorMatrix::OPP)
    {
        FLType gainY = static_cast<FLType>(dRangeY) / (sRange * FLType(3));
        FLType offsetY = -static_cast<FLType>(sFloor) * FLType(3) * gainY + dFloorY;
        if (!dstFloat) offsetY += FLType(0.5);
        FLType gainU = static_cast<FLType>(dRangeC) / (sRange * FLType(2));
        FLType gainV = static_cast<FLType>(dRangeC) / (sRange * FLType(4));
        FLType offsetC = static_cast<FLType>(dNeutralC);
        if (!dstFloat) offsetC += FLType(dstPCChroma ? 0.499999 : 0.5);

        LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
        {
            dstY[i0] = static_cast<dstType>(
                (static_cast<FLType>(srcR[i1])
                + static_cast<FLType>(srcG[i1])
                + static_cast<FLType>(srcB[i1]))
                * gainY + offsetY
                );
            dstU[i0] = static_cast<dstType>(
                (static_cast<FLType>(srcR[i1])
                - static_cast<FLType>(srcB[i1]))
                * gainU + offsetC
                );
            dstV[i0] = static_cast<dstType>(
                (static_cast<FLType>(srcR[i1])
                - static_cast<FLType>(srcG[i1]) * FLType(2)
                + static_cast<FLType>(srcB[i1]))
                * gainV + offsetC
                );
        });
    }
    else if (matrix == ColorMatrix::Minimum || matrix == ColorMatrix::Maximum)
    {
        std::cerr << "MatrixConvert_RGB2YUV: ColorMatrix::Minimum or ColorMatrix::Maximum is invalid!\n";
        return;
    }
    else
    {
        FLType gainY = static_cast<FLType>(dRangeY) / sRange;
        FLType offsetY = -static_cast<FLType>(sFloor) * gainY + dFloorY;
        if (!dstFloat) offsetY += FLType(0.5);
        FLType gainC = static_cast<FLType>(dRangeC) / sRange;
        FLType offsetC = static_cast<FLType>(dNeutralC);
        if (!dstFloat) offsetC += FLType(dstPCChroma ? 0.499999 : 0.5);

        FLType Yr, Yg, Yb, Ur, Ug, Ub, Vr, Vg, Vb;
        ColorMatrix_RGB2YUV_Parameter(matrix, Yr, Yg, Yb, Ur, Ug, Ub, Vr, Vg, Vb);

        Yr *= gainY;
        Yg *= gainY;
        Yb *= gainY;
        Ur *= gainC;
        Ug *= gainC;
        Ub *= gainC;
        Vr *= gainC;
        Vg *= gainC;
        Vb *= gainC;

        LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
        {
            dstY[i0] = static_cast<dstType>(
                Yr * static_cast<FLType>(srcR[i1])
                + Yg * static_cast<FLType>(srcG[i1])
                + Yb * static_cast<FLType>(srcB[i1])
                + offsetY);
            dstU[i0] = static_cast<dstType>(
                Ur * static_cast<FLType>(srcR[i1])
                + Ug * static_cast<FLType>(srcG[i1])
                + Ub * static_cast<FLType>(srcB[i1])
                + offsetC);
            dstV[i0] = static_cast<dstType>(
                Vr * static_cast<FLType>(srcR[i1])
                + Vg * static_cast<FLType>(srcG[i1])
                + Vb * static_cast<FLType>(srcB[i1])
                + offsetC);
        });
    }
}


template < typename _Dt1, typename _St1 >
void MatrixConvert_YUV2RGB(_Dt1 *dstR, _Dt1 *dstG, _Dt1 *dstB,
    const _St1 *srcY, const _St1 *srcU, const _St1 *srcV,
    const PCType height, const PCType width, const PCType dst_stride, const PCType src_stride,
    _Dt1 dFloor, _Dt1 dCeil, _St1 sFloorY, _St1 sCeilY, _St1 sFloorC, _St1 sNeutralC, _St1 sCeilC,
    ColorMatrix matrix = ColorMatrix::OPP)
{
    typedef _St1 srcType;
    typedef _Dt1 dstType;

    const bool dstFloat = isFloat(dstType);

    const auto sRangeY = sCeilY - sFloorY;
    const auto sRangeC = sCeilC - sFloorC;
    const auto dRange = dCeil - dFloor;

    if (matrix == ColorMatrix::GBR)
    {
        RangeConvert(dstG, srcY, height, width, dst_stride, src_stride, dFloor, dFloor, dCeil, sFloorY, sFloorY, sCeilY, false);
        RangeConvert(dstB, srcU, height, width, dst_stride, src_stride, dFloor, dFloor, dCeil, sFloorY, sFloorY, sCeilY, false);
        RangeConvert(dstR, srcV, height, width, dst_stride, src_stride, dFloor, dFloor, dCeil, sFloorY, sFloorY, sCeilY, false);
    }
    else if (matrix == ColorMatrix::Minimum || matrix == ColorMatrix::Maximum)
    {
        std::cerr << "MatrixConvert_YUV2RGB: ColorMatrix::Minimum or ColorMatrix::Maximum is invalid!\n";
        return;
    }
    else
    {
        FLType gainY = static_cast<FLType>(dRange) / sRangeY;
        FLType gainC = static_cast<FLType>(dRange) / sRangeC;

        FLType Ry, Ru, Rv, Gy, Gu, Gv, By, Bu, Bv;
        ColorMatrix_YUV2RGB_Parameter(matrix, Ry, Ru, Rv, Gy, Gu, Gv, By, Bu, Bv);

        Ry *= gainY;
        Ru *= gainC;
        Rv *= gainC;
        Gy *= gainY;
        Gu *= gainC;
        Gv *= gainC;
        By *= gainY;
        Bu *= gainC;
        Bv *= gainC;

        FLType offsetR = -static_cast<FLType>(sFloorY) * Ry - sNeutralC * (Ru + Rv) + dFloor;
        if (!dstFloat) offsetR += FLType(0.5);
        FLType offsetG = -static_cast<FLType>(sFloorY) * Gy - sNeutralC * (Gu + Gv) + dFloor;
        if (!dstFloat) offsetG += FLType(0.5);
        FLType offsetB = -static_cast<FLType>(sFloorY) * By - sNeutralC * (Bu + Bv) + dFloor;
        if (!dstFloat) offsetB += FLType(0.5);

        const FLType lowerL = static_cast<FLType>(dFloor);
        const FLType upperL = static_cast<FLType>(dCeil);

        if (matrix == ColorMatrix::YCgCo)
        {
            LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
            {
                dstR[i0] = static_cast<dstType>(Clip(
                    Ry * static_cast<FLType>(srcY[i1])
                    + Ru * static_cast<FLType>(srcU[i1])
                    + Rv * static_cast<FLType>(srcV[i1])
                    + offsetR, lowerL, upperL));
                dstG[i0] = static_cast<dstType>(Clip(
                    Gy * static_cast<FLType>(srcY[i1])
                    + Gu * static_cast<FLType>(srcU[i1])
                    + offsetG, lowerL, upperL));
                dstB[i0] = static_cast<dstType>(Clip(
                    By * static_cast<FLType>(srcY[i1])
                    + Bu * static_cast<FLType>(srcU[i1])
                    + Bv * static_cast<FLType>(srcV[i1])
                    + offsetB, lowerL, upperL));
            });
        }
        else if (matrix == ColorMatrix::OPP)
        {
            LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
            {
                dstR[i0] = static_cast<dstType>(Clip(
                    Ry * static_cast<FLType>(srcY[i1])
                    + Ru * static_cast<FLType>(srcU[i1])
                    + Rv * static_cast<FLType>(srcV[i1])
                    + offsetR, lowerL, upperL));
                dstG[i0] = static_cast<dstType>(Clip(
                    Gy * static_cast<FLType>(srcY[i1])
                    + Gv * static_cast<FLType>(srcV[i1])
                    + offsetG, lowerL, upperL));
                dstB[i0] = static_cast<dstType>(Clip(
                    By * static_cast<FLType>(srcY[i1])
                    + Bu * static_cast<FLType>(srcU[i1])
                    + Bv * static_cast<FLType>(srcV[i1])
                    + offsetB, lowerL, upperL));
            });
        }
        else
        {
            LOOP_VH(height, width, dst_stride, src_stride, [&](PCType i0, PCType i1)
            {
                dstR[i0] = static_cast<dstType>(Clip(
                    Ry * static_cast<FLType>(srcY[i1])
                    + Rv * static_cast<FLType>(srcV[i1])
                    + offsetR, lowerL, upperL));
                dstG[i0] = static_cast<dstType>(Clip(
                    Gy * static_cast<FLType>(srcY[i1])
                    + Gu * static_cast<FLType>(srcU[i1])
                    + Gv * static_cast<FLType>(srcV[i1])
                    + offsetG, lowerL, upperL));
                dstB[i0] = static_cast<dstType>(Clip(
                    By * static_cast<FLType>(srcY[i1])
                    + Bu * static_cast<FLType>(srcU[i1])
                    + offsetB, lowerL, upperL));
            });
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Template functions of class VSProcess


template < typename T >
void VSProcess::Int2Float(FLType *dst, const T *src,
    PCType height, PCType width, PCType dst_stride, PCType src_stride,
    bool chroma, bool clip)
{
    const FLType dFloor = FLType(chroma ? -0.5 : 0);
    const FLType dNeutral = chroma ? FLType(0) : dFloor;
    const FLType dCeil = FLType(chroma ? 0.5 : 1);

    const T sFloor = T(0);
    const T sNeutral = chroma ? T(1 << (bps - 1)) : sFloor;
    const T sCeil = T((1 << bps) - 1);

    RangeConvert(dst, src, height, width, dst_stride, src_stride,
        dFloor, dNeutral, dCeil, sFloor, sNeutral, sCeil, clip);
}

template < typename T >
void VSProcess::Float2Int(T *dst, const FLType *src,
    PCType height, PCType width, PCType dst_stride, PCType src_stride,
    bool chroma, bool clip)
{
    const T dFloor = T(0);
    const T dNeutral = chroma ? T(1 << (bps - 1)) : dFloor;
    const T dCeil = T((1 << bps) - 1);

    const FLType sFloor = FLType(chroma ? -0.5 : 0);
    const FLType sNeutral = chroma ? FLType(0) : sFloor;
    const FLType sCeil = FLType(chroma ? 0.5 : 1);

    RangeConvert(dst, src, height, width, dst_stride, src_stride,
        dFloor, dNeutral, dCeil, sFloor, sNeutral, sCeil, clip);
}

template < typename T >
void VSProcess::IntRGB2FloatY(FLType *dst,
    const T *srcR, const T *srcG, const T *srcB,
    PCType height, PCType width, PCType dst_stride, PCType src_stride,
    ColorMatrix matrix)
{
    const FLType dFloor = FLType(0);
    const FLType dCeil = FLType(1);

    const T sFloor = T(0);
    const T sCeil = T((1 << bps) - 1);

    ConvertToY(dst, srcR, srcG, srcB,
        height, width, dst_stride, src_stride,
        dFloor, dCeil, sFloor, sCeil,
        matrix);
}

template < typename T >
void VSProcess::IntRGB2FloatYUV(FLType *dstY, FLType *dstU, FLType *dstV,
    const T *srcR, const T *srcG, const T *srcB,
    PCType height, PCType width, PCType dst_stride, PCType src_stride,
    ColorMatrix matrix)
{
    const FLType dFloorY = FLType(0);
    const FLType dCeilY = FLType(1);
    const FLType dFloorC = FLType(-0.5);
    const FLType dNeutralC = FLType(0);
    const FLType dCeilC = FLType(0.5);

    const T sFloor = T(0);
    const T sCeil = T((1 << bps) - 1);

    MatrixConvert_RGB2YUV(dstY, dstU, dstV, srcR, srcG, srcB,
        height, width, dst_stride, src_stride,
        dFloorY, dCeilY, dFloorC, dNeutralC, dCeilC, sFloor, sCeil,
        matrix);
}

template < typename T >
void VSProcess::FloatYUV2IntRGB(T *dstR, T *dstG, T *dstB,
    const FLType *srcY, const FLType *srcU, const FLType *srcV,
    PCType height, PCType width, PCType dst_stride, PCType src_stride,
    ColorMatrix matrix)
{
    const T dFloor = T(0);
    const T dCeil = T((1 << bps) - 1);

    const FLType sFloorY = FLType(0);
    const FLType sCeilY = FLType(1);
    const FLType sFloorC = FLType(-0.5);
    const FLType sNeutralC = FLType(0);
    const FLType sCeilC = FLType(0.5);

    MatrixConvert_YUV2RGB(dstR, dstG, dstB, srcY, srcU, srcV,
        height, width, dst_stride, src_stride,
        dFloor, dCeil, sFloorY, sCeilY, sFloorC, sNeutralC, sCeilC,
        matrix);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif
