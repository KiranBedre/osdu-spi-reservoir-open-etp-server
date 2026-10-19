// ============================================================================
// Copyright 2013-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CORE_UTILS_SOCPEDUTILS_H
#define OES_CORE_UTILS_SOCPEDUTILS_H

#include <oes/core/utils/export.h>

#include <functional>
#include <stack>

namespace oes {
namespace core {


/******************************************************************************/

template <class T>
class ScopedValue {

public:

    ScopedValue(T& scoped_object, T initial_value)
        : scoped_object_(scoped_object), reset_value_(scoped_object) {
        scoped_object_ = std::move( initial_value );
    }

    ScopedValue(T& scoped_object, T initial_value, T reset_value)
        : scoped_object_(scoped_object), reset_value_( std::move( reset_value ) ) {
        scoped_object_ = std::move( initial_value );
    }

    ~ScopedValue() noexcept {
        scoped_object_ = std::move( reset_value_ );
    }

private:
    T& scoped_object_;
    T reset_value_;
};

/******************************************************************************/

class OES_CORE_UTILS_EXPORT ScopedFunctor {
public:
    explicit ScopedFunctor( std::function<void()> functor );
    ~ScopedFunctor() noexcept; // abort since transmitting exception is hazardous

    ScopedFunctor( const ScopedFunctor& ) = delete;
    ScopedFunctor& operator=( const ScopedFunctor& ) = delete;
    ScopedFunctor( ScopedFunctor&& ) = delete;
    ScopedFunctor& operator=( ScopedFunctor&& ) = delete;

private:
    std::function<void()> functor_;
};

/******************************************************************************/

class OES_CORE_UTILS_EXPORT ScopedFunctors {
public:
    ScopedFunctors() = default;
    ~ScopedFunctors() noexcept; // abort since transmitting exception is hazardous

    ScopedFunctors( const ScopedFunctors& ) = delete;
    ScopedFunctors& operator=( const ScopedFunctors& ) = delete;
    ScopedFunctors( ScopedFunctors&& ) = delete;
    ScopedFunctors& operator=( ScopedFunctors&& ) = delete;

    void defer( std::function<void()> functor );

private:
    std::stack<std::function<void()>> functors_;
};

/******************************************************************************/

} // core
} // oes

#endif

