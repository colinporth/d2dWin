#ifndef WELS_GET_INTRA_PREDICTOR_H__
#define WELS_GET_INTRA_PREDICTOR_H__

#include "typedefs.h"

namespace WelsDec {

void WelsI4x4LumaPredV_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredH_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredDc_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredDcLeft_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredDcTop_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredDcNA_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredDDL_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredDDLTop_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredDDR_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredVL_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredVLTop_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredVR_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredHU_c (uint8_t* pPred, const int32_t kiStride);
void WelsI4x4LumaPredHD_c (uint8_t* pPred, const int32_t kiStride);

void WelsI8x8LumaPredV_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredH_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredDc_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredDcLeft_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredDcTop_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredDcNA_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredDDL_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredDDLTop_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredDDR_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredVL_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredVLTop_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredVR_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredHU_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);
void WelsI8x8LumaPredHD_c (uint8_t* pPred, const int32_t kiStride, bool bTLAvail, bool bTRAvail);

void WelsIChromaPredV_c (uint8_t* pPred, const int32_t kiStride);
void WelsIChromaPredH_c (uint8_t* pPred, const int32_t kiStride);
void WelsIChromaPredPlane_c (uint8_t* pPred, const int32_t kiStride);
void WelsIChromaPredDc_c (uint8_t* pPred, const int32_t kiStride);
void WelsIChromaPredDcLeft_c (uint8_t* pPred, const int32_t kiStride);
void WelsIChromaPredDcTop_c (uint8_t* pPred, const int32_t kiStride);
void WelsIChromaPredDcNA_c (uint8_t* pPred, const int32_t kiStride);

void WelsI16x16LumaPredV_c (uint8_t* pPred, const int32_t kiStride);
void WelsI16x16LumaPredH_c (uint8_t* pPred, const int32_t kiStride);
void WelsI16x16LumaPredPlane_c (uint8_t* pPred, const int32_t kiStride);
void WelsI16x16LumaPredDc_c (uint8_t* pPred, const int32_t kiStride);
void WelsI16x16LumaPredDcTop_c (uint8_t* pPred, const int32_t kiStride);
void WelsI16x16LumaPredDcLeft_c (uint8_t* pPred, const int32_t kiStride);
void WelsI16x16LumaPredDcNA_c (uint8_t* pPred, const int32_t kiStride);

#if defined(__cplusplus)
extern "C" {
#endif//__cplusplus

//#if defined(X86_ASM)
void WelsDecoderI16x16LumaPredPlane_sse2 (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredH_sse2 (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredV_sse2 (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredDc_sse2 (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredDcTop_sse2 (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredDcNA_sse2 (uint8_t* pPred, const int32_t kiStride);

void WelsDecoderIChromaPredDcTop_sse2 (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredPlane_sse2 (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredDc_sse2 (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredH_mmx (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredV_mmx (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredDcLeft_mmx (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredDcNA_mmx (uint8_t* pPred, const int32_t kiStride);

void WelsDecoderI4x4LumaPredH_sse2 (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredDDR_mmx (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredHD_mmx (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredHU_mmx (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredVR_mmx (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredDDL_mmx (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredVL_mmx (uint8_t* pPred, const int32_t kiStride);
//#endif//X86_ASM

#if defined(HAVE_NEON)
void WelsDecoderI16x16LumaPredV_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredH_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredDc_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredPlane_neon (uint8_t* pPred, const int32_t kiStride);

void WelsDecoderI4x4LumaPredV_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredH_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredDDL_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredDDR_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredVL_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredVR_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredHU_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredHD_neon (uint8_t* pPred, const int32_t kiStride);

void WelsDecoderIChromaPredV_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredH_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredDc_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredPlane_neon (uint8_t* pPred, const int32_t kiStride);
#endif//HAVE_NEON

#if defined(HAVE_NEON_AARCH64)
void WelsDecoderI16x16LumaPredV_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredH_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredDc_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredPlane_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredDcTop_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI16x16LumaPredDcLeft_AArch64_neon (uint8_t* pPred, const int32_t kiStride);

void WelsDecoderI4x4LumaPredH_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredDDL_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredDDLTop_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredVL_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredVLTop_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredVR_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredHU_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredHD_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredDc_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderI4x4LumaPredDcTop_AArch64_neon (uint8_t* pPred, const int32_t kiStride);

void WelsDecoderIChromaPredV_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredH_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredDc_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredPlane_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
void WelsDecoderIChromaPredDcTop_AArch64_neon (uint8_t* pPred, const int32_t kiStride);
#endif//HAVE_NEON_AARCH64
#if defined(__cplusplus)
}
#endif//__cplusplus

} // namespace WelsDec

#endif //WELS_GET_INTRA_PREDICTOR_H__


