// Copyright (c) 2022 by Apex.AI Inc. All rights reserved.
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
//
// SPDX-License-Identifier: Apache-2.0
#include "test_cxx_functional_interface_types.hpp"

namespace
{
using namespace test_cxx_functional_interface;
using namespace ::testing;

#define IOX_TEST(TestName, variationPoint)                                                                             \
    using SutType = typename TestFixture::TestFactoryType::Type;                                                       \
    TestName<iox::cxx::internal::HasValueMethod<SutType>::value,                                                       \
             iox::cxx::internal::HasGetErrorMethod<SutType>::value>::                                                  \
        template performTest<typename TestFixture::TestFactoryType>(                                                   \
            [](auto& sut, auto andThenCallback, auto orElseCallback) {                                                 \
                variationPoint.and_then(andThenCallback).or_else(orElseCallback);                                      \
            })

constexpr bool TYPE_HAS_VALUE_METHOD = true;
constexpr bool TYPE_HAS_NO_VALUE_METHOD = false;
constexpr bool TYPE_HAS_GET_ERROR_METHOD = true;
constexpr bool TYPE_HAS_NO_GET_ERROR_METHOD = false;

template <bool HasValue, bool HasError>
struct AndThenOrElseConcatenatedWorksWhenInvalid;

template <>
struct AndThenOrElseConcatenatedWorksWhenInvalid<TYPE_HAS_NO_VALUE_METHOD, TYPE_HAS_NO_GET_ERROR_METHOD>
{
    template <typename TestFactory, typename SutCall>
    static void performTest(const SutCall& sutCall)
    {
        auto sut = TestFactory::createInvalidObject();
        bool wasAndThenCalled = false;
        bool wasOrElseCalled = false;
        sutCall(
            sut, [&] { wasAndThenCalled = true; }, [&] { wasOrElseCalled = true; });

        EXPECT_FALSE(wasAndThenCalled);
        EXPECT_TRUE(wasOrElseCalled);
    }
};

template <>
struct AndThenOrElseConcatenatedWorksWhenInvalid<TYPE_HAS_VALUE_METHOD, TYPE_HAS_NO_GET_ERROR_METHOD>
{
    template <typename TestFactory, typename SutCall>
    static void performTest(const SutCall& sutCall)
    {
        auto sut = TestFactory::createInvalidObject();
        bool wasAndThenCalled = false;
        bool wasOrElseCalled = false;
        sutCall(
            sut, [&](auto&) { wasAndThenCalled = true; }, [&] { wasOrElseCalled = true; });

        EXPECT_FALSE(wasAndThenCalled);
        EXPECT_TRUE(wasOrElseCalled);
    }
};

template <>
struct AndThenOrElseConcatenatedWorksWhenInvalid<TYPE_HAS_NO_VALUE_METHOD, TYPE_HAS_GET_ERROR_METHOD>
{
    template <typename TestFactory, typename SutCall>
    static void performTest(const SutCall& sutCall)
    {
        auto sut = TestFactory::createInvalidObject();
        bool wasAndThenCalled = false;
        bool wasOrElseCalled = false;
        sutCall(
            sut,
            [&] { wasAndThenCalled = true; },
            [&](auto& error) {
                wasOrElseCalled = true;
                EXPECT_THAT(error, Eq(TestFactory::usedErrorValue));
            });

        EXPECT_FALSE(wasAndThenCalled);
        EXPECT_TRUE(wasOrElseCalled);
    }
};

template <>
struct AndThenOrElseConcatenatedWorksWhenInvalid<TYPE_HAS_VALUE_METHOD, TYPE_HAS_GET_ERROR_METHOD>
{
    template <typename TestFactory, typename SutCall>
    static void performTest(const SutCall& sutCall)
    {
        auto sut = TestFactory::createInvalidObject();
        bool wasAndThenCalled = false;
        bool wasOrElseCalled = false;

        sutCall(
            sut,
            [&](auto&) { wasAndThenCalled = true; },
            [&](auto& error) {
                wasOrElseCalled = true;
                EXPECT_THAT(error, Eq(TestFactory::usedErrorValue));
            });
        EXPECT_FALSE(wasAndThenCalled);
        EXPECT_TRUE(wasOrElseCalled);
    }
};

TYPED_TEST(FunctionalInterface_test, AndThenOrElseConcatenatedWorksWhenInvalid_LValueCase)
{
    IOX_TEST(AndThenOrElseConcatenatedWorksWhenInvalid, sut);
}

TYPED_TEST(FunctionalInterface_test, AndThenOrElseConcatenatedWorksWhenInvalid_ConstLValueCase)
{
    IOX_TEST(AndThenOrElseConcatenatedWorksWhenInvalid, const_cast<const SutType&>(sut));
}

TYPED_TEST(FunctionalInterface_test, AndThenOrElseConcatenatedWorksWhenInvalid_RValueCase)
{
    IOX_TEST(AndThenOrElseConcatenatedWorksWhenInvalid, std::move(sut));
}

TYPED_TEST(FunctionalInterface_test, AndThenOrElseConcatenatedWorksWhenInvalid_ConstRValueCase)
{
    IOX_TEST(AndThenOrElseConcatenatedWorksWhenInvalid, std::move(const_cast<const SutType&>(sut)));
}

template <bool HasValue, bool HasError>
struct AndThenOrElseConcatenatedWorkWhenValid;

template <>
struct AndThenOrElseConcatenatedWorkWhenValid<TYPE_HAS_NO_VALUE_METHOD, TYPE_HAS_NO_GET_ERROR_METHOD>
{
    template <typename TestFactory, typename SutCall>
    static void performTest(const SutCall& sutCall)
    {
        auto sut = TestFactory::createValidObject();
        bool wasAndThenCalled = false;
        bool wasOrElseCalled = false;
        sutCall(
            sut, [&] { wasAndThenCalled = true; }, [&] { wasOrElseCalled = true; });

        EXPECT_TRUE(wasAndThenCalled);
        EXPECT_FALSE(wasOrElseCalled);
    }
};

template <>
struct AndThenOrElseConcatenatedWorkWhenValid<TYPE_HAS_VALUE_METHOD, TYPE_HAS_NO_GET_ERROR_METHOD>
{
    template <typename TestFactory, typename SutCall>
    static void performTest(const SutCall& sutCall)
    {
        auto sut = TestFactory::createValidObject();
        bool wasAndThenCalled = false;
        bool wasOrElseCalled = false;
        sutCall(
            sut,
            [&](auto& value) {
                wasAndThenCalled = true;
                EXPECT_THAT(value, Eq(TestFactory::usedTestValue));
            },
            [&] { wasOrElseCalled = true; });

        EXPECT_TRUE(wasAndThenCalled);
        EXPECT_FALSE(wasOrElseCalled);
    }
};

template <>
struct AndThenOrElseConcatenatedWorkWhenValid<TYPE_HAS_NO_VALUE_METHOD, TYPE_HAS_GET_ERROR_METHOD>
{
    template <typename TestFactory, typename SutCall>
    static void performTest(const SutCall& sutCall)
    {
        auto sut = TestFactory::createValidObject();
        bool wasAndThenCalled = false;
        bool wasOrElseCalled = false;
        sutCall(
            sut, [&] { wasAndThenCalled = true; }, [&](auto&) { wasOrElseCalled = true; });

        EXPECT_TRUE(wasAndThenCalled);
        EXPECT_FALSE(wasOrElseCalled);
    }
};

template <>
struct AndThenOrElseConcatenatedWorkWhenValid<TYPE_HAS_VALUE_METHOD, TYPE_HAS_GET_ERROR_METHOD>
{
    template <typename TestFactory, typename SutCall>
    static void performTest(const SutCall& sutCall)
    {
        auto sut = TestFactory::createValidObject();
        bool wasAndThenCalled = false;
        bool wasOrElseCalled = false;

        sutCall(
            sut,
            [&](auto& value) {
                wasAndThenCalled = true;
                EXPECT_THAT(value, Eq(TestFactory::usedTestValue));
            },
            [&](auto&) { wasOrElseCalled = true; });
        EXPECT_TRUE(wasAndThenCalled);
        EXPECT_FALSE(wasOrElseCalled);
    }
};

TYPED_TEST(FunctionalInterface_test, AndThenOrElseConcatenatedWorkWhenValid_LValueCase)
{
    IOX_TEST(AndThenOrElseConcatenatedWorkWhenValid, sut);
}

TYPED_TEST(FunctionalInterface_test, AndThenOrElseConcatenatedWorkWhenValid_ConstLValueCase)
{
    IOX_TEST(AndThenOrElseConcatenatedWorkWhenValid, const_cast<SutType&>(sut));
}

TYPED_TEST(FunctionalInterface_test, AndThenOrElseConcatenatedWorkWhenValid_RValueCase)
{
    IOX_TEST(AndThenOrElseConcatenatedWorkWhenValid, std::move(sut));
}

TYPED_TEST(FunctionalInterface_test, AndThenOrElseConcatenatedWorkWhenValid_ConstRValueCase)
{
    IOX_TEST(AndThenOrElseConcatenatedWorkWhenValid, std::move(const_cast<SutType&>(sut)));
}
} // namespace
