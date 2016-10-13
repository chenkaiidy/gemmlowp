// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// output_sse.h: optimized SSE4.2 specializations of the templates in output.h.

#ifndef GEMMLOWP_INTERNAL_OUTPUT_SSE4_H_
#define GEMMLOWP_INTERNAL_OUTPUT_SSE4_H_

#include <smmintrin.h>
#include "output.h"
#include "fixedpoint.h"

namespace gemmlowp {

// Definitions of Fragment types wrapping SSE4.2 vector types.
typedef Fragment<__m128i, 4, 1, MapOrder::ColMajor> SSE4FragmentInt32x4x1;
typedef Fragment<__m128i[4], 16, 1, MapOrder::ColMajor> SSE4FragmentInt32x16x1;
typedef Fragment<uint32_t, 4, 1, MapOrder::ColMajor> SSE4FragmentUint8x4x1;
typedef Fragment<uint64_t, 16, 1, MapOrder::ColMajor> SSE4FragmentUint8x16x1;

// The code in unpack_neon.h will whenever possible process
// 16 entries at once (4 SIMD vectors of 4 entries each at once),
// to offer the compiler better optimization opportunities, reducing
// register dependencies. From the perspective of interfacing with the output
// pipeline, this takes the form of passing Fragment types wrapping int32x4x4_t
// data. In most cases, such data is handled simply by handling separately its
// 4 __m128i components. This partial specialization handles that for
// arbitrary output stages implementing a __m128i path. Only some output
// stages below will override this to use custom code to handle int32x4x4_t
// data all at once (see OutputStageSaturatingCastToUint8 below).
template <typename OutputStageType>
struct OutputStageEvalImpl<OutputStageType, SSE4FragmentInt32x16x1> {
  typedef SSE4FragmentInt32x16x1 InputType;
  typedef SSE4FragmentInt32x16x1 OutputType;
  typedef OutputStageEvalImpl<OutputStageType, SSE4FragmentInt32x4x1>
      ImplInt32x4;
  OutputStageEvalImpl(const OutputStageType& s) : impl_int32x4(s) {}

  OutputType Eval(InputType input, int row, int col) const {
    OutputType output;

    for (int i = 0; i < 4; i++) {
      output.data.val[i] =
          impl_int32x4.Eval(input.data.val[i], row + 4 * i, col);
    }
    return output;
  }

  ImplInt32x4 impl_int32x4;
};

// Implementation of OutputStageQuantizeDownInt32ToUint8Scale for
// SSE4FragmentInt32x4x1
template <>
struct OutputStageEvalImpl<OutputStageQuantizeDownInt32ToUint8Scale,
                           SSE4FragmentInt32x4x1> {
  typedef SSE4FragmentInt32x4x1 InputType;
  typedef SSE4FragmentInt32x4x1 OutputType;
  typedef OutputStageQuantizeDownInt32ToUint8Scale OutputStage;

  OutputStageEvalImpl(const OutputStage& s) : output_stage(s) {}

  OutputType Eval(InputType input, int, int) const {
    const std::int32_t result_shift = output_stage.result_shift;
    const std::int32_t result_mult_int = output_stage.result_mult_int;
    const std::int32_t result_offset = output_stage.result_offset;
    const std::int32_t kRoundingTerm =
        (result_shift < 1) ? 0 : (1 << (result_shift - 1));
    const __m128i a = Add(Mul(Add(input, Dup(result_offset)), result_mult_int), kRoundingTerm);
    return ShiftRight(a, result_shift);
  }

  const OutputStage& output_stage;
};

// Implementation of OutputStageQuantizeDownInt32ToUint8ScalePC for
// SSE4FragmentInt32x4x1
template <>
struct OutputStageEvalImpl<
    OutputStageQuantizeDownInt32ToUint8ScalePC<VectorShape::Col>,
    SSE4FragmentInt32x4x1> {
  typedef SSE4FragmentInt32x4x1 InputType;
  typedef SSE4FragmentInt32x4x1 OutputType;
  typedef OutputStageQuantizeDownInt32ToUint8ScalePC<VectorShape::Col>
      OutputStage;

  OutputStageEvalImpl(const OutputStage& s) : output_stage(s) {}

  OutputType Eval(InputType input, int row, int col) const {
    const std::int32_t result_shift = output_stage.result_shift;
    const std::int32_t result_mult_int =
      _mm_lddqu_si128((__m128i*)(output_stage.result_mult_int(row)));
    const std::int32_t result_offset =
      _mm_lddqu_si128((__m128i*)(output_stage.result_offset(row)));
    const std::int32_t kRoundingTerm =
        (result_shift < 1) ? 0 : (1 << (result_shift - 1));
    const __m128i a = Add(Mul(Add(input, Dup(result_offset)), result_mult_int), kRoundingTerm);
    return ShiftRight(a, result_shift);
  }

  const OutputStage& output_stage;
};

// Implementation of OutputStageQuantizeDownInt32ToUint8ScalePC for
// SSE4FragmentInt32x4x1
template <>
struct OutputStageEvalImpl<
    OutputStageQuantizeDownInt32ToUint8ScalePC<VectorShape::Row>,
    SSE4FragmentInt32x4x1> {
  typedef SSE4FragmentInt32x4x1 InputType;
  typedef SSE4FragmentInt32x4x1 OutputType;
  typedef OutputStageQuantizeDownInt32ToUint8ScalePC<VectorShape::Row>
      OutputStage;

  OutputStageEvalImpl(const OutputStage& s) : output_stage(s) {}

  OutputType Eval(InputType input, int row, int col) const {
    const std::int32_t result_shift = output_stage.result_shift;
    const std::int32_t result_mult_int =
      _mm_lddqu_si128((__m128i*)(output_stage.result_mult_int(col)));
    const std::int32_t result_offset =
      _mm_lddqu_si128((__m128i*)(output_stage.result_offset(col)));
    const std::int32_t kRoundingTerm =
        (result_shift < 1) ? 0 : (1 << (result_shift - 1));
    const __m128i a = Add(Mul(Add(input, Dup(result_offset)), result_mult_int), kRoundingTerm);
    return ShiftRight(a, result_shift);
  }

  const OutputStage& output_stage;
};

// Implementation of OutputStageQuantizeDownInt32ToUint8ScaleByFixedPoint for
// SSE4FragmentInt32x4x1
template <>
struct OutputStageEvalImpl<OutputStageQuantizeDownInt32ToUint8ScaleByFixedPoint,
                           SSE4FragmentInt32x4x1> {
  typedef SSE4FragmentInt32x4x1 InputType;
  typedef SSE4FragmentInt32x4x1 OutputType;
  typedef OutputStageQuantizeDownInt32ToUint8ScaleByFixedPoint OutputStage;

  OutputStageEvalImpl(const OutputStage& s)
      : output_stage(s),
        preshift_offset((s.result_shift < 1) ? 0
                                             : (1 << (s.result_shift - 1))) {}

  OutputType Eval(InputType input, int, int) const {
    const __m128i mulhigh_val = SaturatingRoundingDoublingHighMul(
        input.data, output_stage.result_fixedpoint_multiplier);
    const std::int32_t result_shift = output_stage.result_shift;
    const std::int32_t kRoundingTerm =
      (result_shift < 1) ? 0 : (1 << (result_shift - 1));
    
    const __m128i shifted_val = ShifRight(Add(mulhigh_val, Dup(kRoundingTerm)), result_shift);
    return Add(shifted_val, Dup(output_stage.result_offset_after_shift));
  }

  const OutputStage& output_stage;
};

 
// Implementation of OutputStageSaturatingCastToUint8 for SSE4FragmentInt32x4x1
template <>
struct OutputStageEvalImpl<OutputStageSaturatingCastToUint8,
                           SSE4FragmentInt32x4x1> {
  typedef SSE4FragmentInt32x4x1 InputType;
  typedef SSE4FragmentUint8x4x1 OutputType;
  typedef OutputStageSaturatingCastToUint8 OutputStage;

  OutputStageEvalImpl(const OutputStage&) {}

  OutputType Eval(InputType input, int, int) const {
    const __m128i zero = Dup(0);
    __m128i res_16 = _mm_packus_epi32(input, zero);
    __m128i res_8  = _mm_packus_epi16(res_16, zero);
    return _mm_cvtsi128_si32(res_8);
  }
};

// In the case of OutputStageSaturatingCastToUint8, the handling of
// SSE4FragmentInt32x16x1 data can be made much more efficient by handling
// it all at once, instead of as 4 separate int32x4 values as in the above
// generic partial specialization. This also avoids the poor (50%) register
// utilization of FragmentUint8x4x1: by handling 16 scalar values at once,
// we are able to fill a uint8x16_t.
template <>
struct OutputStageEvalImpl<OutputStageSaturatingCastToUint8,
                           SSE4FragmentInt32x16x1> {
  typedef SSE4FragmentInt32x16x1 InputType;
  typedef SSE4FragmentUint8x16x1 OutputType;
  typedef OutputStageSaturatingCastToUint8 OutputStage;

  OutputStageEvalImpl(const OutputStage&) {}

  OutputType Eval(InputType input, int, int) const {
    __m128i q16[2];
    for (int i = 0; i < 2; i++) {
      q16[i] = _mm_packus_epi32(input.data.val[2 * i],
				input.data.val[2 * i + 1]);
    }
    return _mm_packus_epi16(q16[0], q16[1]);
  }
};

// Implementation of OutputStageBiasAddition for SSE4FragmentInt32x4x1
template <typename VectorType>
struct OutputStageEvalImpl<OutputStageBiasAddition<VectorType>,
                           SSE4FragmentInt32x4x1> {
  typedef SSE4FragmentInt32x4x1 InputType;
  typedef SSE4FragmentInt32x4x1 OutputType;
  typedef OutputStageBiasAddition<VectorType> OutputStage;

  OutputStageEvalImpl(const OutputStage& s) : output_stage(s) {}

  OutputType Eval(InputType input, int row, int col) const {
    __m128i bias;
    if (VectorType::kShape == VectorShape::Row) {
      bias = Dup(output_stage.bias_vector(col));
    } else {
      bias = _mm_lddqu_si128(output_stage.bias_vector.data(row));
    }
    return Add(input, bias);
  }

  const OutputStage& output_stage;
};

// Implementation of OutputStageClamp for SSE4FragmentInt32x4x1
template <>
struct OutputStageEvalImpl<OutputStageClamp, SSE4FragmentInt32x4x1> {
  typedef SSE4FragmentInt32x4x1 InputType;
  typedef SSE4FragmentInt32x4x1 OutputType;
  typedef OutputStageClamp OutputStage;

  OutputStageEvalImpl(const OutputStage& s) : output_stage(s) {}

  OutputType Eval(InputType input, int, int) const {
    const __m128i min = Dup(output_stage.min);
    const __m128i max = Dup(output_stage.max);
    return _mm_min_epi32(_mm_max_epi32(input, min), max);
  }

  const OutputStage& output_stage;
};

// Implementation of OutputStageTanh for SSE4FragmentInt32x4x1
template <>
struct OutputStageEvalImpl<OutputStageTanh, SSE4FragmentInt32x4x1>
    : OutputStageTanhEvalImpl<SSE4FragmentInt32x4x1> {
  OutputStageEvalImpl(const OutputStageTanh& output_stage)
      : OutputStageTanhEvalImpl(output_stage) {}
};

// Specialization of StoreFinalOutput for SSE4FragmentUint8x4x1.
template <typename DstType>
inline void StoreFinalOutput(SSE4FragmentUint8x4x1 value, DstType* dst, int row,
                             int col) {
  __m128i input_value = _mm_cvtsi32_si128(value);
  _mm_store_ss((float *) (dst->data(row,col)), value);
}

// Specialization of StoreFinalOutput for SSE4FragmentUint8x16x1.
template <typename DstType>
inline void StoreFinalOutput(SSE4FragmentUint8x16x1 value, DstType* dst,
                             int row, int col) {
  _mm_storeu_si128((__m128i*) (dst->data(row, col)), value);
}

// Specialization of StoreFinalOutput for SSE4FragmentInt32x4x1.
template <typename DstType>
inline void StoreFinalOutput(SSE4FragmentInt32x4x1 value, DstType* dst, int row,
                             int col) {
  _mm_storeu_si128((__m128i*) (dst->data(row, col)), value);
}

// Specialization of StoreFinalOutput for SSE4FragmentInt32x16x1.
template <typename DstType>
inline void StoreFinalOutput(SSE4FragmentInt32x16x1 value, DstType* dst,
                             int row, int col) {
  for (int i = 0; i < 4; i++) {
    _mm_storeu_si128((__m128i*) (dst->data(row, col)), value);
  }
}

}  // namespace gemmlowp

#endif  // GEMMLOWP_INTERNAL_OUTPUT_SSE4_H_
