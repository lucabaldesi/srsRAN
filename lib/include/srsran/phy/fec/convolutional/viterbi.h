/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

/******************************************************************************
 *  File:         viterbi.h
 *
 *  Description:  Viterbi decoder for convolutionally encoded data.
 *                Used for decoding of PBCH and PDCCH (type 37 decoder).
 *
 *  Reference:
 *****************************************************************************/

#ifndef SRSRAN_VITERBI_H
#define SRSRAN_VITERBI_H

#include "srsran/config.h"
#include <stdbool.h>

typedef enum { SRSRAN_VITERBI_27 = 0, SRSRAN_VITERBI_29, SRSRAN_VITERBI_37, SRSRAN_VITERBI_39 } srsran_viterbi_type_t;

typedef struct SRSRAN_API {
  void*    ptr;
  uint32_t R;
  uint32_t K;
  uint32_t framebits;
  bool     tail_biting;
  float    gain_quant;
  int16_t  gain_quant_s;
  int (*decode)(void*, uint8_t*, uint8_t*, uint32_t);
  int (*decode_s)(void*, uint16_t*, uint8_t*, uint32_t);
  int (*decode_f)(void*, float*, uint8_t*, uint32_t);
  void (*free)(void*);
  uint8_t*  tmp;
  uint16_t* tmp_s;
  uint8_t*  symbols_uc;
  uint16_t* symbols_us;
} srsran_viterbi_t;

SRSRAN_API int srsran_viterbi_init(srsran_viterbi_t*     q,
                                   srsran_viterbi_type_t type,
                                   int                   poly[3],
                                   uint32_t              max_frame_length,
                                   bool                  tail_bitting);

SRSRAN_API void srsran_viterbi_set_gain_quant(srsran_viterbi_t* q, float gain_quant);

SRSRAN_API void srsran_viterbi_set_gain_quant_s(srsran_viterbi_t* q, int16_t gain_quant);

SRSRAN_API void srsran_viterbi_free(srsran_viterbi_t* q);

SRSRAN_API int srsran_viterbi_decode_f(srsran_viterbi_t* q, float* symbols, uint8_t* data, uint32_t frame_length);

SRSRAN_API int srsran_viterbi_decode_s(srsran_viterbi_t* q, int16_t* symbols, uint8_t* data, uint32_t frame_length);

SRSRAN_API int srsran_viterbi_decode_us(srsran_viterbi_t* q, uint16_t* symbols, uint8_t* data, uint32_t frame_length);

SRSRAN_API int srsran_viterbi_decode_uc(srsran_viterbi_t* q, uint8_t* symbols, uint8_t* data, uint32_t frame_length);

SRSRAN_API int srsran_viterbi_init_sse(srsran_viterbi_t*     q,
                                       srsran_viterbi_type_t type,
                                       int                   poly[3],
                                       uint32_t              max_frame_length,
                                       bool                  tail_bitting);

SRSRAN_API int srsran_viterbi_init_neon(srsran_viterbi_t*     q,
                                        srsran_viterbi_type_t type,
                                        int                   poly[3],
                                        uint32_t              max_frame_length,
                                        bool                  tail_bitting);

SRSRAN_API int srsran_viterbi_init_avx2(srsran_viterbi_t*     q,
                                        srsran_viterbi_type_t type,
                                        int                   poly[3],
                                        uint32_t              max_frame_length,
                                        bool                  tail_bitting);

#endif // SRSRAN_VITERBI_H