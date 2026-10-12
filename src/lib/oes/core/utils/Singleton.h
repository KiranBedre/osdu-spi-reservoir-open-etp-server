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

#ifndef OES_CORE_UTILS_SINGLETON_H
#define OES_CORE_UTILS_SINGLETON_H

#include <oes/core/utils/export.h>
#include <oes/core/utils/String.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <typeinfo>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes {
    namespace core {

        namespace detail {
            struct Entry {
                Entry();
                void* instance;
                bool deleted;
            };

            OES_CORE_UTILS_EXPORT Entry& getInstance(const String& name);
            OES_CORE_UTILS_EXPORT void fatalCannotResurrect(const String& className);
            OES_CORE_UTILS_EXPORT void fatalDoubleDestroy(const String& className);
        }

        template <class T>
            class Singleton {
            public:
                typedef Singleton<T> SingletonType;
                
                static T& instance() {
                    detail::Entry& entry = detail::getInstance(T::className());
                    if( entry.deleted ) {
                        detail::fatalCannotResurrect(T::className());
                    }
                    if( entry.instance == nullptr ) {
                        T::createInstance(*reinterpret_cast<T**>(&entry.instance));
                    }
                    return *reinterpret_cast<T*>(entry.instance);
                }

                static bool isAlive() {
                    const detail::Entry& entry = detail::getInstance(T::className());
                    return entry.instance != nullptr;
                }

                static void destroy() {
                    detail::Entry& entry = detail::getInstance(T::className());
                    if( entry.deleted ) {
                        detail::fatalDoubleDestroy(T::className());
                    }
                    T* entry_instance = static_cast<T*>(entry.instance);
                    entry.instance = nullptr;
                    entry.deleted = true;
                    delete entry_instance;
                }

            protected:
                Singleton() {
                }

                virtual ~Singleton() {
                }

                static void createInstance(T*& instance) {
                    instance = new T;
                }

                static const char* className() {
                    return typeid(T).name();
                }

            private:
                Singleton(Singleton const& x);
                Singleton& operator=(Singleton const& x);
            };


        template <typename T>
            class PhoenixSingleton {
            public:
                typedef PhoenixSingleton<T> SingletonType;

                static T& instance() {
                    detail::Entry& entry = detail::getInstance(T::className());
                    if( entry.instance == nullptr ) {
                        T::createInstance(*reinterpret_cast<T**>(&entry.instance));
                    }
                    return *reinterpret_cast<T*>(entry.instance);
                }

                static bool isAlive() {
                    const detail::Entry& entry = detail::getInstance(T::className());
                    return entry.instance != nullptr;
                }

                static void destroy() {
                    detail::Entry& entry = detail::getInstance(T::className());
                    T* entry_instance = static_cast<T*>(entry.instance);
                    entry.instance = nullptr;
                    delete entry_instance;
                }

            protected:
                PhoenixSingleton() {
                }

                virtual ~PhoenixSingleton() {
                }

                static void createInstance(T*& instance) {
                    instance = new T;
                }

                static const char* className() {
                    return typeid(T).name();
                }

            private:
                PhoenixSingleton(PhoenixSingleton const& x);
                PhoenixSingleton& operator=(PhoenixSingleton const& x);
            };
    }
}

#endif

