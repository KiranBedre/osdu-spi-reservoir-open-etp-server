// ============================================================================
// Copyright 2008-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/Singleton.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <stdexcept>
#include <map>
#include <oes/core/utils/config_leave_3rdparty.h>

/*! \file oes/core/utils/Singleton.h
 * \brief Base classes to implement singleton objects.
 * \ingroup oes_core_utils
 */

namespace oes {
    namespace core {

        /*! \defgroup oes_core_utils_singletons Singleton
         * \ingroup oes_core_utils
         *
         * \par Generalities
         * A singleton class is a class for which a single instance can
         * exist at a given time during the lifetime of the application.
         * A singleton cannot be assigned or copied.
         *
         * \par Singleton classes
         * The library provides 2 singleton classes with different behaviors:
         * - Singleton: a "one-shot" singleton that can be instantiated only once and
         * cannot be instantiated again once deleted.
         * - PhoenixSingleton: a singleton that can be "resurrected", that is
         * it can be instantiated again after it has been deleted.
         *
         * \par Usage
         * Both Singleton and PhoenixSingleton work the same way: they are
         * template classes that transform their template argument into a
         * singleton class. To create a singleton class C, C must inherit from
         * either one of the singleton classes with itself as template
         * argument.
         *
         * \par
         * The following code makes class MySingleton a one-shot singleton by
         * inheritance of Singleton<MySingleton>:
         *
         * \code
         * class MySingleton : public Singleton<MySingleton> {
         * public:
         *      // public MySingleton methods
         * protected:
         *      MySingleton();
         *      ~MySingleton();
         * };
         * \endcode
         *
         * \par Common methods
         * Both singleton classes provide the following 3 static member
         * functions by inheritance:
         * - instance: return a reference to the singleton object
         * - destroy: destroy the singleton object
         * - isAlive: check if the singleton is alive, that is: if the instance has been created.
         *
         * The first 2 functions behave differently, depending on the singleton
         * class that implements them. See specific singleton class for
         * details.
         *
         * In order to enforce the singleton behavior, constructors and
         * destructors are declared protected.
         */

        /*! \class Singleton oes/core/utils/Singleton.h
         * \ingroup oes_core_utils oes_core_utils_singletons
         * \brief Base class used to define one-shot singleton classes
         * \details Singleton is a convenience template base class to implement
         * one-shot (non-resurrectable) singleton classes.
         *
         * One shot singleton can be instantiated only once and deleted only once.
         * \li %Any attempt to recreate a one-shot singleton instance raises
         * an exception
         * \li %Any attempt to delete a one-shot singleton instance twice also
         * raises an exception
         *
         * To define a one-shot singleton class, one must inherit from
         * Singleton has follows:
         *
         * \code
         * class MySingleton : public Singleton<MySingleton> {
         * public:
         *      // public MySingleton methods
         * protected:
         *      MySingleton();
         *      ~MySingleton();
         * };
         * \endcode
         */

        /*!
         * \typedef Singleton Singleton::SingletonType
         * \brief Defines the type of the concrete phoenix singleton deriving
         * from Singleton
         */

        /*!
         * \fn T& Singleton::instance()
         * \brief Returns the singleton instance
         * \details
         * If the the function has been called already, returns the created
         * instance, otherwise instance() calls createInstance() to create a
         * new instance on the fly and returns it.
         * Any attempt to call this function after the singleton has been
         * destroyed (see destroy()) throws a std::logic_error exception with
         * message "Attempt to access already destroyed singleton".
         *
         * \tparam T The concrete singleton class
         * \returns a reference to the singleton instance
         * \see detail::fatalCannotResurrect()
         * \see createInstance()
         * \see destroy()
         */

        /*!
         * \fn bool Singleton::isAlive()
         * \brief Checks if a single instance exists
         * \retval true if the singleton has been already instantiated
         * \retval false otherwise
         */

        /*!
         * \fn void Singleton::destroy()
         * \brief Destroys the singleton instance
         * \details
         * Once the singleton instance is deleted, it is impossible to
         * recreate it (see instance(). Any further attempt to destroy the
         * singleton with throw a std::logic_error exception with message
         * "Attempt to destroy singleton twice"
         * \see detail::fatalDoubleDestroy()
         * \see instance()
         */

        /*!
         * \fn Singleton::Singleton()
         * \brief Singleton constructor
         * \details
         * The base Singleton constructor does nothing. It is protected to
         * prevent direct instantiation and thus enforce the singleton
         * behavior.
         */

        /*!
         * \fn Singleton::~Singleton()
         * \brief Singleton destructor
         * \details
         * The base Singleton destructor does nothing. It is protected to
         * prevent direct destruction and thus enforce the singleton
         * behavior.
         */

        /*!
         * \fn void Singleton::createInstance(T*& instance)
         * \brief Default singleton method for creating an instance
         * \details
         * This function is called by instance() to create a new singleton
         * instance when it does not already exists. The default
         * implementation simply calls \c new(T). Concrete singleton classes
         * can redefine this static method to implement a custom allocation
         * mechanism. This is typically used by the Service singleton which
         * creates services instances using factories.
         *
         * \par Example
         *
         * \code
         * class MySingletonWithCustomAllocation : public Singleton<MySingletonWithCustomAllocation> {
         * public:
         *      // public MySingletonWithCustomAllocation methods
         * protected:
         *      MySingletonWithCustomAllocation();
         *      ~MySingletonWithCustomAllocation();
         *
         *      static void createInstance(MySingletonWithCustomAllocation*& instance) {
         *          instance = Pool<MySingletonWithCustomAllocation>.allocate();
         *      }
         * };
         * \endcode
         *
         * \param[out] instance a reference to the singleton instance pointer 
         * \tparam T type of the concrete singleton class
         * \see instance()
         */

        /*!
         * \fn const char* Singleton::className()
         * \brief Returns the singleton class name
         * \details
         * This function is called by instance(), isAlive() and destroy() to
         * identify the singleton instance in the singleton map (see
         * details). The default implementation calls
         * \c typeid(T).name(). Concrete singleton classes can redefine this
         * static method to return a custom name if needed.
         *
         * \par Example
         *
         * \code
         * class MySingletonWithCustomName : public Singleton<MySingletonWithCustomName> {
         * public:
         *      // public MySingletonWithCustomName methods
         * protected:
         *      MySingletonWithCustomName();
         *      ~MySingletonWithCustomName();
         *
         *      static const char* className() {
         *          return "MyCustomName";
         *      }
         * };
         * \endcode
         *
         * \return the singleton class name
         */


        /*! \class PhoenixSingleton oes/core/utils/Singleton.h
         * \ingroup oes_core_utils oes_core_utils_singletons
         * \brief Base class used to define phoenix singleton classes
         * \details PhoenixSingleton is a convenience template base class to implement
         * phoenix singleton (or resurrectable singleton) classes.
         *
         * Unlike the Singleton a PhoenixSingleton can be instantiated again
         * after it has been deleted.
         *
         * To define a phoenix singleton class, one must inherit from
         * PhoenixSingleton has follows:
         *
         * \code
         * class MySingleton : public PhoenixSingleton<MySingleton> {
         * public:
         *      // public MySingleton methods
         * protected:
         *      MySingleton();
         *      ~MySingleton();
         * };
         * \endcode
         */

        /*!
         * \typedef PhoenixSingleton PhoenixSingleton::SingletonType
         * \brief Defines the type of the concrete phoenix singleton deriving
         * from PhoenixSingleton
         */

        /*!
         * \fn T& PhoenixSingleton::instance()
         * \brief Returns the singleton instance
         * \details
         * If the the function has been called already, returns the created
         * instance, otherwise instance() calls createInstance() to create a
         * new instance on the fly and returns it.
         *
         * \tparam T The concrete singleton class
         * \returns a reference to the singleton instance
         * \see createInstance()
         */

        /*!
         * \fn bool PhoenixSingleton::isAlive()
         * \brief Checks if a single instance exists
         * \retval true if the singleton has been already instantiated
         * \retval false otherwise
         */

        /*!
         * \fn void PhoenixSingleton::destroy()
         * \brief Destroys the singleton instance
         * \details
         * Once the singleton instance is deleted, it is possible to create it
         * again by calling instance(). Unlike Singleton, any further attempt
         * to destroy the singleton with \b not throw any exception.`
         * \see instance()
         */

        /*!
         * \fn PhoenixSingleton::PhoenixSingleton()
         * \brief PhoenixSingleton constructor
         * \details
         * The base PhoenixSingleton constructor does nothing. It is protected to
         * prevent direct instantiation and thus enforce the singleton
         * behavior.
         */

        /*!
         * \fn PhoenixSingleton::~PhoenixSingleton()
         * \brief PhoenixSingleton destructor
         * \details
         * The base PhoenixSingleton destructor does nothing. It is protected to
         * prevent direct destruction and thus enforce the singleton
         * behavior.
         */

        /*!
         * \fn void PhoenixSingleton::createInstance(T*& instance)
         * \brief Default singleton method for creating an instance
         * \details
         * This function is called by instance() to create a new singleton
         * instance when it does not already exists. The default
         * implementation simply calls \c new(T). Concrete singleton classes
         * can redefine this static method to implement a custom allocation
         * mechanism. This is typically used by the Service singleton which
         * creates services instances using factories.
         *
         * \par Example
         *
         * \code
         * class MySingletonWithCustomAllocation : public PhoenixSingleton<MySingletonWithCustomAllocation> {
         * public:
         *      // public MySingletonWithCustomAllocation methods
         * protected:
         *      MySingletonWithCustomAllocation();
         *      ~MySingletonWithCustomAllocation();
         *
         *      static void createInstance(MySingletonWithCustomAllocation*& instance) {
         *          instance = Pool<MySingletonWithCustomAllocation>.allocate();
         *      }
         * };
         * \endcode
         *
         * \param[out] instance a reference to the singleton instance pointer 
         * \tparam T type of the concrete singleton class
         * \see instance()
         */

        /*!
         * \fn const char* PhoenixSingleton::className()
         * \brief Returns the singleton class name
         * \details
         * This function is called by instance(), isAlive() and
         * destroy() to identify the singleton instance in the singleton map
         * (see details). The default implementation calls
         * \c typeid(T).name(). Concrete singleton classes can redefine this
         * static method to return a custom name if needed.
         *
         * \par Example
         *
         * \code
         * class MySingletonWithCustomName : public PhoenixSingleton<MySingletonWithCustomName> {
         * public:
         *      // public MySingletonWithCustomName methods
         * protected:
         *      MySingletonWithCustomName();
         *      ~MySingletonWithCustomName();
         *
         *      static const char* className() {
         *          return "MyCustomName";
         *      }
         * };
         * \endcode
         *
         * \return the singleton class name
         */

        /*!
         * \ingroup oes_core_utils_singletons
         * \brief Encapsulates singleton implementation details
         * \details
         * This namespace is reserved for singleton implementation and although it
         * is kept public for access reasons, developers should never use it
         * directly.
         */
        namespace detail {

            /*!
             * \struct Entry oes/core/utils/Singleton.h
             * \brief Stores information about created singleton instances
             * \details
             * Entry remembers information about created singleton instances
             * in a global non templated map. Windows has a bug that causes
             * singleton instances created in different DLLs to be duplicated,
             * which is a shame for a singleton. This implementation choice
             * fixes that bug.
             */

            /*!
             * \brief Entry constructor
             */
            Entry::Entry() :
                instance(nullptr),
                deleted(false)
            {
            }

            /*!
             * \brief Returns singleton information for a given name
             * \details
             * Search the singleton instance map for a singleton
             * with the given \a name and returns the information found.
             * \param[in] name a singleton name
             * \return a reference to the singleton information.
             */
            Entry& getInstance(const String& name) {
                static std::map<String, Entry> map;
                return map[name];
            }

            /*!
             * \brief Throws a "cannot resurrect" exception
             * \details
             * This exception is thrown when a non resurrectable singleton is
             * created twice. The thrown exception is a std::logic_error
             * exception with message "Attempt to access already destroyed singleton".
             * \param[in] className the name of singleton class being instantiated
             */
            void fatalCannotResurrect(const String& className) {
                throw std::logic_error(String("Attempt to access already destroyed singleton: ") + className);
            }

            /*!
             * \brief Throws a "deleted twice" exception
             * \details
             * This exception is thrown when a non resurrectable singleton is
             * deleted twice. The thrown exception is a std::logic_error exception
             * with message "Attempt to access already destroyed singleton".
             * \param[in] className the name of singleton class being deleted
             */
            void fatalDoubleDestroy(const String& className) {
                throw std::logic_error(String("Attempt to destroy singleton twice: ") + className);
            }
        } // end namespace detail
    }
}

