// ============================================================================
// Copyright 2023-2026 Emerson Paradigm Holding LLC. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ============================================================================

#ifndef OES_EML_OPENKV_NUMERICTYPE_H
#define OES_EML_OPENKV_NUMERICTYPE_H

#include <oes/common/HdfAry.h>

#include <cstddef>
#include <cstdint>

namespace oes
{
    namespace eml
    {
        namespace openkv
        {

            //============================================================================
            // NumericType — the single source of truth for the runtime HdfAryType -> the
            // compile-time numeric C++ element type mapping shared by the streaming compute
            // kernels (ArrayStats and future compute extensions).
            //
            // Each of those kernels used to hand-roll the same `switch (type)` to turn a
            // runtime HdfAryType tag into a typed `T*` reinterpretation and call a templated
            // per-element worker. That dispatch was copy-pasted four ways (and drifted in
            // small details), so it lives here once as a tag-dispatch visitor.
            //
            //   * visitNumericType(type, f) calls `f(T{})` EXACTLY ONCE with a
            //     value-initialised element of the mapped C++ type and returns true when
            //     `type` is one of the numeric array types:
            //         eAryOfInt     -> int32_t
            //         eAryOfLong    -> int64_t
            //         eAryOfFloat   -> float
            //         eAryOfDouble  -> double
            //         eAryOfBoolean -> int8_t
            //   * For the non-numeric types (eAryOfBytes / eAryOfString / eAryOfVoid) it
            //     does NOT call `f` and returns false, so callers no-op them uniformly.
            //
            // `f` is a generic callable; recover the type with decltype on its argument:
            //
            //     visitNumericType(type, [&](auto tag) {
            //         using T = decltype(tag);
            //         foldTyped<T>(acc, data, n);
            //     });
            //
            // The visitor only replaces the outer type-switch: the per-element inner loops
            // (e.g. the ArrayStats 8-way unroll / loop-carried Welford update) stay in their
            // own kernels, so the generated hot-path code is identical.
            //============================================================================

            //! Invoke `f(T{})` with the numeric C++ type mapped from `type` and return true,
            //! or return false without calling `f` for the non-numeric types. Header-only so
            //! the generic lambda inlines to the same code the hand-written switch produced.
            template <typename F>
            inline bool visitNumericType(oes::common::hdf::HdfAryType type, F &&f)
            {
                switch (type)
                {
                case oes::common::hdf::eAryOfInt:
                    f(int32_t{});
                    return true;
                case oes::common::hdf::eAryOfLong:
                    f(int64_t{});
                    return true;
                case oes::common::hdf::eAryOfFloat:
                    f(float{});
                    return true;
                case oes::common::hdf::eAryOfDouble:
                    f(double{});
                    return true;
                case oes::common::hdf::eAryOfBoolean:
                    f(int8_t{});
                    return true;
                case oes::common::hdf::eAryOfBytes:
                case oes::common::hdf::eAryOfString:
                case oes::common::hdf::eAryOfVoid:
                    return false; // non-numeric: nothing to visit.
                    // no default on purpose: cover every HdfAryType enumerator.
                }
                return false;
            }

        } // namespace openkv
    } // namespace eml
} // namespace oes

#endif // OES_EML_OPENKV_NUMERICTYPE_H
