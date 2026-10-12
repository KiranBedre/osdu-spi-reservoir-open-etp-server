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

#include <oes/core/utils/ScopedUtils.h>

#include <stdexcept>

/*! \file oes/core/utils/ScopedUtils.h
 * \brief Tools to define and execute actions at end of scope.
 * \ingroup oes_core_utils
 */

namespace oes {
namespace core {


/******************************************************************************/

/*!
 * \class ScopedValue oes/core/utils/ScopedUtils.h
 * \brief Sets a temporary value to an object.
 * \ingroup oes_core_utils
 *
 * This class sets a new value to an object and resets it to a desired value at destruction.
 */

/*!
 * \fn template <class T> ScopedValue<T>::ScopedValue(T& scoped_object, T initial_value)
 * Creates a ScopedValue for object \p scoped_object.
 * \p scoped_object is initialized to \p initial_value and gets reset to its
 * original value when this ScopedValue is destroyed.
 */

/*!
 * \fn template <class T> ScopedValue<T>::ScopedValue(T& scoped_object, T initial_value, T reset_value)
 * Creates a ScopedValue for object \p scoped_object.
 * \p scoped_object is initialized to \p initial_value and gets reset to
 * \p reset_value when this ScopedValue is destroyed.
 */

/*!
 * \fn template <class T> ScopedValue<T>::~ScopedValue()
 * Destructor.
 * Resets the scoped_object to the reset value.
 */

/******************************************************************************/

/*!
 * \class ScopedFunctor oes/core/utils/ScopedUtils.h
 * \brief Functor executed at end of scope.
 * \ingroup oes_core_utils
 *
 * The functor must be compliant with std::function<void()>
 */

/*!
 * Constructor with \p functor to be executed at end of scope.
 */
ScopedFunctor::ScopedFunctor( std::function<void()> functor )
: functor_( std::move( functor ) )
{
    if( !functor_ ) {
        throw std::invalid_argument( "Empty functor given to ScopedFunctor constructor" );
    }
}

/*!
 * Destructor executing the functor.
 */
ScopedFunctor::~ScopedFunctor() {
    functor_();
}

/******************************************************************************/

/*!
 * \class ScopedFunctors oes/core/utils/ScopedUtils.h
 * \brief List of functors executed at end of scope.
 * \ingroup oes_core_utils
 *
 * The functors must be compliant with std::function<void()>
 * It is a LIFO list.
 */

/*!
 * Destructor executing the functors in reverse order
 *
 */
ScopedFunctors::~ScopedFunctors() {
    while( !functors_.empty() ) {
        functors_.top()();
        functors_.pop();
    }
}

/*!
 * Adds \p functor to the list of functors to be executed at end of scope.
 */
void ScopedFunctors::defer( std::function<void()> functor ) {
    if( functor ) {
        functors_.emplace( std::move( functor ) );
    } else {
        throw std::invalid_argument( "Empty functor given to ScopedFunctors::defer()" );
    }
}

/******************************************************************************/

} // core
} // oes
