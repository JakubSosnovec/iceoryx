// Copyright (c) 2020 - 2022 by Apex.AI Inc. All rights reserved.
// Copyright (c) 2020 - 2021 by Robert Bosch GmbH. All rights reserved.
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

#include "iceoryx_hoofs/cxx/helplets.hpp"
#include "iceoryx_hoofs/testing/timing_test.hpp"
#include "iceoryx_hoofs/testing/watch_dog.hpp"
#include "iceoryx_posh/iceoryx_posh_types.hpp"
#include "iceoryx_posh/popo/listener.hpp"
#include "iceoryx_posh/popo/untyped_publisher.hpp"
#include "iceoryx_posh/popo/untyped_server.hpp"
#include "iceoryx_posh/popo/wait_set.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "iceoryx_posh/runtime/service_discovery.hpp"
#include "iceoryx_posh/testing/mocks/posh_runtime_mock.hpp"
#include "iceoryx_posh/testing/roudi_gtest.hpp"
#include "test.hpp"

#include <random>
#include <set>
#include <type_traits>
#include <vector>


namespace
{
using namespace ::testing;
using namespace iox::runtime;
using namespace iox::cxx;
using namespace iox::popo;
using namespace iox::capro;
using iox::capro::IdString_t;
using iox::capro::ServiceDescription;
using iox::popo::MessagingPattern;
using iox::roudi::RouDiEnvironment;

using ServiceContainer = std::vector<ServiceDescription>;

struct Publisher
{
    using Producer = iox::popo::UntypedPublisher;
    static constexpr MessagingPattern KIND{MessagingPattern::PUB_SUB};
    static constexpr auto MAX_PRODUCERS{iox::MAX_PUBLISHERS};
    static constexpr auto MAX_USER_PRODUCERS{iox::MAX_PUBLISHERS - iox::NUMBER_OF_INTERNAL_PUBLISHERS};
};

struct Server
{
    using Producer = iox::popo::UntypedServer;
    static constexpr MessagingPattern KIND{MessagingPattern::REQ_RES};
    static constexpr auto MAX_PRODUCERS{iox::MAX_SERVERS};
    static constexpr auto MAX_USER_PRODUCERS{iox::MAX_SERVERS};
};

std::atomic_bool callbackWasCalled{false};
ServiceContainer serviceContainer;
class ServiceDiscoveryPubSub_test : public RouDi_GTest
{
  public:
    void SetUp() override
    {
        callbackWasCalled = false;
        m_watchdog.watchAndActOnFailure([] { std::terminate(); });
    }

    void TearDown() override
    {
    }

    void findService(const optional<IdString_t>& service,
                     const optional<IdString_t>& instance,
                     const optional<IdString_t>& event,
                     const iox::popo::MessagingPattern pattern) noexcept
    {
        serviceContainer.clear();
        sut.findService(service, instance, event, findHandler, pattern);
    }

    static void findHandler(const ServiceDescription& s)
    {
        serviceContainer.emplace_back(s);
    };

    static void testCallback(ServiceDiscovery* const)
    {
        callbackWasCalled = true;
    }

    static void searchForPublisher(ServiceDiscovery* const serviceDiscovery, ServiceDescription* service)
    {
        serviceContainer.clear();
        serviceDiscovery->findService(service->getServiceIDString(),
                                      service->getInstanceIDString(),
                                      service->getEventIDString(),
                                      findHandler,
                                      MessagingPattern::PUB_SUB);
        callbackWasCalled = true;
    }

    iox::runtime::PoshRuntime* runtime{&iox::runtime::PoshRuntime::initRuntime("Runtime")};
    ServiceDiscovery sut;

    const iox::units::Duration m_fatalTimeout = 10_s;
    Watchdog m_watchdog{m_fatalTimeout};
};

template <typename T>
class ServiceDiscovery_test : public ServiceDiscoveryPubSub_test
{
  public:
    using CommunicationKind = T;

    void findService(const optional<IdString_t>& service,
                     const optional<IdString_t>& instance,
                     const optional<IdString_t>& event) noexcept
    {
        ServiceDiscoveryPubSub_test::findService(service, instance, event, CommunicationKind::KIND);
    }
};

///
/// Publish-Subscribe || Request-Response
///

using CommunicationKind = Types<Publisher, Server>;
TYPED_TEST_SUITE(ServiceDiscovery_test, CommunicationKind);

// tests with multiple offer stages, we should not vary the search kind here (exact search suffices)

TYPED_TEST(ServiceDiscovery_test, ReofferedServiceCanBeFound)
{
    ::testing::Test::RecordProperty("TEST_ID", "b67b4990-e2fd-4efa-ab5d-e53c4ee55972");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("service", "instance", "event");
    typename TestFixture::CommunicationKind::Producer producer(SERVICE_DESCRIPTION);

    this->findService(SERVICE_DESCRIPTION.getServiceIDString(),
                      SERVICE_DESCRIPTION.getInstanceIDString(),
                      SERVICE_DESCRIPTION.getEventIDString());

    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION));

    producer.stopOffer();
    this->InterOpWait();
    this->findService(SERVICE_DESCRIPTION.getServiceIDString(),
                      SERVICE_DESCRIPTION.getInstanceIDString(),
                      SERVICE_DESCRIPTION.getEventIDString());

    EXPECT_TRUE(serviceContainer.empty());

    producer.offer();
    this->InterOpWait();
    this->findService(SERVICE_DESCRIPTION.getServiceIDString(),
                      SERVICE_DESCRIPTION.getInstanceIDString(),
                      SERVICE_DESCRIPTION.getEventIDString());

    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION));
}

TYPED_TEST(ServiceDiscovery_test, ServiceOfferedMultipleTimesCanBeFound)
{
    ::testing::Test::RecordProperty("TEST_ID", "ae0790ed-4e1b-4f12-94b3-c9e56433c935");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("service", "instance", "event");
    typename TestFixture::CommunicationKind::Producer producer(SERVICE_DESCRIPTION);
    producer.offer();
    this->InterOpWait();

    this->findService(SERVICE_DESCRIPTION.getServiceIDString(),
                      SERVICE_DESCRIPTION.getInstanceIDString(),
                      SERVICE_DESCRIPTION.getEventIDString());

    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION));
}


TYPED_TEST(ServiceDiscovery_test, ServicesWhichStoppedOfferingCannotBeFound)
{
    ::testing::Test::RecordProperty("TEST_ID", "e4f99eb1-7496-4a1e-bbd1-ebdb07e1ec9b");

    const IdString_t INSTANCE = "instance";
    const IdString_t EVENT = "event";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1("service1", INSTANCE, EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("service2", INSTANCE, EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3("service3", INSTANCE, EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd3(SERVICE_DESCRIPTION3);

    producer_sd1.stopOffer();
    producer_sd3.stopOffer();
    this->InterOpWait();

    this->findService(SERVICE_DESCRIPTION1.getServiceIDString(), INSTANCE, EVENT);
    EXPECT_THAT(serviceContainer.size(), Eq(0U));

    this->findService(SERVICE_DESCRIPTION2.getServiceIDString(), INSTANCE, EVENT);
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION2));

    this->findService(SERVICE_DESCRIPTION3.getServiceIDString(), INSTANCE, EVENT);
    EXPECT_THAT(serviceContainer.size(), Eq(0U));
}

///
/// Publisher-Subscriber && Request-Response
///

///
/// Publisher-Subscriber
///

TEST_F(ServiceDiscoveryPubSub_test, FindServiceWithWildcardsReturnsOnlyIntrospectionServicesAndServiceRegistry)
{
    ::testing::Test::RecordProperty("TEST_ID", "d944f32c-edef-44f5-a6eb-c19ee73c98eb");
    findService(iox::capro::Wildcard, iox::capro::Wildcard, iox::capro::Wildcard, MessagingPattern::PUB_SUB);

    constexpr uint32_t NUM_INTERNAL_SERVICES = 6U;
    EXPECT_EQ(serviceContainer.size(), NUM_INTERNAL_SERVICES);
    for (auto& service : serviceContainer)
    {
        EXPECT_THAT(service.getInstanceIDString().c_str(), StrEq("RouDi_ID"));
    }
}

TEST_F(ServiceDiscoveryPubSub_test, FindServiceWithEmptyCallableDoesNotDie)
{
    ::testing::Test::RecordProperty("TEST_ID", "7e1bf253-ce81-47cc-9b4a-605de7e49b64");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("ninjababy", "pow", "pow");
    iox::popo::UntypedPublisher publisher(SERVICE_DESCRIPTION);
    iox::cxx::function_ref<void(const ServiceDescription&)> searchFunction;
    this->sut.findService(
        iox::capro::Wildcard, iox::capro::Wildcard, iox::capro::Wildcard, searchFunction, MessagingPattern::PUB_SUB);
}

TEST_F(ServiceDiscoveryPubSub_test, ServiceDiscoveryIsAttachableToWaitSet)
{
    ::testing::Test::RecordProperty("TEST_ID", "fc0eeb7a-6f2a-481f-ae8a-1e17460e261f");
    iox::popo::WaitSet<10U> waitSet;

    waitSet
        .attachEvent(this->sut,
                     ServiceDiscoveryEvent::SERVICE_REGISTRY_CHANGED,
                     0U,
                     iox::popo::createNotificationCallback(testCallback))
        .and_then([]() { GTEST_SUCCEED(); })
        .or_else([](auto) { GTEST_FAIL() << "Could not attach to wait set"; });
}

TEST_F(ServiceDiscoveryPubSub_test, ServiceDiscoveryIsNotifiedbyWaitSetAboutSingleService)
{
    ::testing::Test::RecordProperty("TEST_ID", "f1cf36b5-3db2-4e6f-8e05-e7e449530ec0");
    iox::popo::WaitSet<1U> waitSet;

    waitSet
        .attachEvent(this->sut,
                     ServiceDiscoveryEvent::SERVICE_REGISTRY_CHANGED,
                     0U,
                     iox::popo::createNotificationCallback(testCallback))
        .or_else([](auto) { GTEST_FAIL() << "Could not attach to wait set"; });

    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("Moep", "Fluepp", "Shoezzel");
    iox::popo::UntypedPublisher publisher(SERVICE_DESCRIPTION);

    auto notificationVector = waitSet.timedWait(1_s);

    for (auto& notification : notificationVector)
    {
        (*notification)();
    }

    EXPECT_TRUE(callbackWasCalled);
}

TEST_F(ServiceDiscoveryPubSub_test, ServiceDiscoveryNotifiedbyWaitSetFindsSingleService)
{
    ::testing::Test::RecordProperty("TEST_ID", "1ecde7e0-f5b2-4721-b309-66f32f40a7bf");
    iox::popo::WaitSet<1U> waitSet;
    iox::capro::ServiceDescription serviceDescriptionToSearchFor("Soep", "Moemi", "Luela");

    waitSet
        .attachEvent(this->sut,
                     ServiceDiscoveryEvent::SERVICE_REGISTRY_CHANGED,
                     0U,
                     iox::popo::createNotificationCallback(searchForPublisher, serviceDescriptionToSearchFor))
        .or_else([](auto) { GTEST_FAIL() << "Could not attach to wait set"; });

    iox::popo::UntypedPublisher publisher(serviceDescriptionToSearchFor);

    auto notificationVector = waitSet.timedWait(1_s);

    for (auto& notification : notificationVector)
    {
        (*notification)();
    }

    ASSERT_FALSE(serviceContainer.empty());
    EXPECT_THAT(serviceContainer.front(), Eq(serviceDescriptionToSearchFor));
}

TEST_F(ServiceDiscoveryPubSub_test, ServiceDiscoveryIsAttachableToListener)
{
    ::testing::Test::RecordProperty("TEST_ID", "def201f7-d1bf-4031-8e50-a2ad22ee303c");
    iox::popo::Listener listener;

    listener
        .attachEvent(this->sut,
                     ServiceDiscoveryEvent::SERVICE_REGISTRY_CHANGED,
                     iox::popo::createNotificationCallback(testCallback))
        .and_then([]() { GTEST_SUCCEED(); })
        .or_else([](auto) { GTEST_FAIL() << "Could not attach to listener"; });
}

TEST_F(ServiceDiscoveryPubSub_test, ServiceDiscoveryIsNotifiedByListenerAboutSingleService)
{
    ::testing::Test::RecordProperty("TEST_ID", "305107fc-41dd-431c-8032-ed5e82f93038");
    iox::popo::Listener listener;

    listener
        .attachEvent(this->sut,
                     ServiceDiscoveryEvent::SERVICE_REGISTRY_CHANGED,
                     iox::popo::createNotificationCallback(testCallback))
        .or_else([](auto) { GTEST_FAIL() << "Could not attach to listener"; });

    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("Moep", "Fluepp", "Shoezzel");
    iox::popo::UntypedPublisher publisher(SERVICE_DESCRIPTION);

    while (!callbackWasCalled.load())
    {
        std::this_thread::yield();
    }

    EXPECT_TRUE(callbackWasCalled.load());
}

TEST_F(ServiceDiscoveryPubSub_test, ServiceDiscoveryNotifiedbyListenerFindsSingleService)
{
    ::testing::Test::RecordProperty("TEST_ID", "b38ba8a4-ff27-437a-b376-13125cb419cb");
    iox::popo::Listener listener;
    iox::capro::ServiceDescription serviceDescriptionToSearchFor("Gimbel", "Seggel", "Doedel");

    listener
        .attachEvent(this->sut,
                     ServiceDiscoveryEvent::SERVICE_REGISTRY_CHANGED,
                     iox::popo::createNotificationCallback(searchForPublisher, serviceDescriptionToSearchFor))
        .or_else([](auto) { GTEST_FAIL() << "Could not attach to listener"; });

    iox::popo::UntypedPublisher publisher(serviceDescriptionToSearchFor);

    while (!callbackWasCalled.load())
    {
        std::this_thread::yield();
    }

    ASSERT_FALSE(serviceContainer.empty());
    EXPECT_THAT(serviceContainer.front(), Eq(serviceDescriptionToSearchFor));
}

struct Comparator
{
    bool operator()(const ServiceDescription& a, const ServiceDescription& b)
    {
        if (a.getServiceIDString() < b.getServiceIDString())
        {
            return true;
        }
        if (b.getServiceIDString() < a.getServiceIDString())
        {
            return false;
        }

        if (a.getInstanceIDString() < b.getInstanceIDString())
        {
            return true;
        }
        if (b.getInstanceIDString() < a.getInstanceIDString())
        {
            return false;
        }

        if (a.getEventIDString() < b.getEventIDString())
        {
            return true;
        }
        return false;
    }
};

bool sortAndCompare(ServiceContainer& a, ServiceContainer& b)
{
    std::sort(a.begin(), a.end(), Comparator());
    std::sort(b.begin(), b.end(), Comparator());
    return a == b;
}

struct ReferenceDiscovery
{
    std::set<ServiceDescription> services;

    ReferenceDiscovery(const MessagingPattern pattern = MessagingPattern::PUB_SUB)
    {
        if (pattern == MessagingPattern::PUB_SUB)
        {
            services.emplace(iox::roudi::IntrospectionMempoolService);
            services.emplace(iox::roudi::IntrospectionPortService);
            services.emplace(iox::roudi::IntrospectionPortThroughputService);
            services.emplace(iox::roudi::IntrospectionSubscriberPortChangingDataService);
            services.emplace(iox::roudi::IntrospectionProcessService);
            services.emplace(iox::SERVICE_DISCOVERY_SERVICE_NAME,
                             iox::SERVICE_DISCOVERY_INSTANCE_NAME,
                             iox::SERVICE_DISCOVERY_EVENT_NAME);
        }
    }

    bool contains(const ServiceDescription& s)
    {
        return services.find(s) != services.end();
    }

    void add(const ServiceDescription& s)
    {
        services.emplace(s);
    }

    // brute force reference implementation
    ServiceContainer findService(const optional<IdString_t>& service,
                                 const optional<IdString_t>& instance,
                                 const optional<IdString_t>& event)
    {
        ServiceContainer result;
        for (auto& s : services)
        {
            bool match = (service) ? (s.getServiceIDString() == *service) : true;
            match &= (instance) ? (s.getInstanceIDString() == *instance) : true;
            match &= (event) ? (s.getEventIDString() == *event) : true;

            if (match)
            {
                result.emplace_back(s);
            }
        }
        return result;
    }

    auto size()
    {
        return services.size();
    }
};

template <typename T>
class ReferenceServiceDiscovery_test : public ServiceDiscoveryPubSub_test
{
  public:
    using Variation = T;

    static constexpr uint32_t MAX_PUBLISHERS = iox::MAX_PUBLISHERS;
    static constexpr uint32_t MAX_SERVERS = iox::MAX_SERVERS;

    // std::vector<iox::popo::UntypedPublisher> publishers;
    iox::cxx::vector<iox::popo::UntypedPublisher, MAX_PUBLISHERS> publishers;
    iox::cxx::vector<iox::popo::UntypedServer, MAX_SERVERS> servers;

    ReferenceDiscovery publisherDiscovery{MessagingPattern::PUB_SUB};
    ReferenceDiscovery serverDiscovery{MessagingPattern::REQ_RES};

    ServiceContainer expectedResult;

    void addPublisher(const ServiceDescription& s)
    {
        // does not work with std::vector, but probably with std::array
        // probably because vector grows dynamically and may require copying (though the error is about move)
        publishers.emplace_back(s);
        publisherDiscovery.add(s);
    }

    void addServer(const ServiceDescription& s)
    {
        // servers are unique (contrary to publishers)
        if (!serverDiscovery.contains(s))
        {
            servers.emplace_back(s);
            serverDiscovery.add(s);
        }
    }

    void add(const ServiceDescription& s, MessagingPattern pattern)
    {
        if (pattern == MessagingPattern::PUB_SUB)
        {
            addPublisher(s);
        }
        else
        {
            addServer(s);
        }
    }

    void add(const ServiceDescription& s)
    {
        add(s, Variation::PATTERN);
    }

    void addOther(const ServiceDescription& s)
    {
        add(s, otherPattern());
    }

    void testFindService(const optional<IdString_t>& service,
                         const optional<IdString_t>& instance,
                         const optional<IdString_t>& event) noexcept
    {
        optional<IdString_t> s(service);
        optional<IdString_t> i(instance);
        optional<IdString_t> e(event);

        Variation::setSearchArgs(s, i, e); // modify inputs with (partial) wildcards

        ServiceDiscoveryPubSub_test::findService(s, i, e, Variation::PATTERN);

        if (Variation::PATTERN == MessagingPattern::PUB_SUB)
        {
            expectedResult = publisherDiscovery.findService(s, i, e);
        }
        else
        {
            expectedResult = serverDiscovery.findService(s, i, e);
        }

        // size check is redundant but provides more information in case of error
        // EXPECT_EQ(serviceContainer.size(), expectedResult.size());
        EXPECT_TRUE(sortAndCompare(serviceContainer, expectedResult));
    }

    void testFindService(const ServiceDescription& s)
    {
        testFindService(s.getServiceIDString(), s.getInstanceIDString(), s.getEventIDString());
    }

    // assumes we have only two pattern which holds for now
    static constexpr MessagingPattern otherPattern()
    {
        if (Variation::PATTERN == MessagingPattern::PUB_SUB)
        {
            return MessagingPattern::REQ_RES;
        }
        else
        {
            return MessagingPattern::PUB_SUB;
        }
    }

    static constexpr uint32_t maxProducers(MessagingPattern pattern)
    {
        if (pattern == MessagingPattern::PUB_SUB)
        {
            return iox::MAX_PUBLISHERS - iox::NUMBER_OF_INTERNAL_PUBLISHERS;
        }
        else
        {
            return iox::MAX_SERVERS;
        }
    }
}; // for (; created < OTHER_MAX; ++created)
   // {
   //     this->addOther(randomService("Ferdinand", "Spitz"));
   // }

template <typename T>
T uniform(T n)
{
    static auto seed = std::random_device()();
    static std::mt19937 mt(seed);
    std::uniform_int_distribution<T> dist(0, n);
    return dist(mt);
}

using string_t = iox::capro::IdString_t;

string_t randomString(uint64_t size = string_t::capacity())
{
    // deliberately contains no `0` (need to exclude some char)
    static const char chars[] = "123456789"
                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz";

    constexpr auto N = string_t::capacity();
    size = std::min(N, size);

    char a[N + 1U];
    for (uint64_t i = 0U; i < size; ++i)
    {
        // -2 is needed to avoid generating the '\0' terminator of chars
        a[i] = chars[uniform(sizeof(chars) - 2U)];
    }
    a[size] = '\0';

    return string_t(a);
}

ServiceDescription randomService()
{
    auto service = randomString();
    auto instance = randomString();
    auto event = randomString();
    return {service, instance, event};
}

ServiceDescription randomService(const string_t& service)
{
    auto instance = randomString();
    auto event = randomString();
    return {service, instance, event};
}

ServiceDescription randomService(const string_t& service, const string_t& instance)
{
    auto event = randomString();
    return {service, instance, event};
}


// 8 variations (S)ervice, (I)nstance, (E)vent, (W)ildcard

struct SIE
{
    static void setSearchArgs(optional<IdString_t>&, optional<IdString_t>&, optional<IdString_t>&)
    {
    }
};

struct WIE
{
    static void setSearchArgs(optional<IdString_t>& service, optional<IdString_t>&, optional<IdString_t>&)
    {
        service.reset();
    }
};

struct SWE
{
    static void setSearchArgs(optional<IdString_t>&, optional<IdString_t>& instance, optional<IdString_t>&)
    {
        instance.reset();
    }
};

struct SIW
{
    static void setSearchArgs(optional<IdString_t>&, optional<IdString_t>&, optional<IdString_t>& event)
    {
        event.reset();
    }
};

struct WWE
{
    static void setSearchArgs(optional<IdString_t>& service, optional<IdString_t>& instance, optional<IdString_t>&)
    {
        service.reset();
        instance.reset();
    }
};

struct WIW
{
    static void setSearchArgs(optional<IdString_t>& service, optional<IdString_t>&, optional<IdString_t>& event)
    {
        service.reset();
        event.reset();
    }
};

struct SWW
{
    static void setSearchArgs(optional<IdString_t>&, optional<IdString_t>& instance, optional<IdString_t>& event)
    {
        instance.reset();
        event.reset();
    }
};

struct WWW
{
    static void
    setSearchArgs(optional<IdString_t>& service, optional<IdString_t>& instance, optional<IdString_t>& event)
    {
        service.reset();
        instance.reset();
        event.reset();
    }
};

struct PubSub
{
    static constexpr MessagingPattern PATTERN{MessagingPattern::PUB_SUB};
    static constexpr uint32_t MAX_PRODUCERS{iox::MAX_PUBLISHERS - iox::NUMBER_OF_INTERNAL_PUBLISHERS};
};

struct ReqRes
{
    static constexpr MessagingPattern PATTERN{MessagingPattern::REQ_RES};
    static constexpr auto MAX_PRODUCERS{iox::MAX_SERVERS};
};


template <typename S, typename T>
struct Variation : public S, public T
{
};


// We could also have the variation of search directly in (a) testFindService method,
// but this would add test interference and make it impossible to compare
// subsequent test results with each other

using PS_SIE = Variation<SIE, PubSub>;
using PS_WIE = Variation<WIE, PubSub>;
using PS_SWE = Variation<SWE, PubSub>;
using PS_SIW = Variation<SIW, PubSub>;
using PS_WWE = Variation<WWE, PubSub>;
using PS_WIW = Variation<WIW, PubSub>;
using PS_SWW = Variation<SWW, PubSub>;
using PS_WWW = Variation<WWW, PubSub>;

using RR_SIE = Variation<SIE, ReqRes>;
using RR_WIE = Variation<WIE, ReqRes>;
using RR_SWE = Variation<SWE, ReqRes>;
using RR_SIW = Variation<SIW, ReqRes>;
using RR_WWE = Variation<WWE, ReqRes>;
using RR_WIW = Variation<WIW, ReqRes>;
using RR_SWW = Variation<SWW, ReqRes>;
using RR_WWW = Variation<WWW, ReqRes>;
using TestVariations = Types<PS_SIE,
                             PS_WIE,
                             PS_SWE,
                             PS_SIW,
                             PS_WWE,
                             PS_WIW,
                             PS_SWW,
                             PS_WWW,
                             RR_SIE,
                             RR_WIE,
                             RR_SWE,
                             RR_SIW,
                             RR_WWE,
                             RR_WIW,
                             RR_SWW,
                             RR_WWW>;

// using TestVariations = Types < Variation<SIE, PubSub>, Variation<WIE, PubSub>, Variation<SWE, PubSub>,
//       Variation<SIW, PubSub>, Variation<WWE, PubSub>, Variation<WIW, PubSub>, Variation<SWW, PubSub>,
//       Variation<WWW, PubSub>, Variation<SIE, ReqRes>, Variation<WIE, ReqRes>, Variation<SWE, ReqRes>,
//       Variation<SIW, ReqRes>, Variation<WWE, ReqRes>, Variation<WIW, ReqRes>, Variation<SWW, ReqRes>,
//       Variation<WWW, ReqRes>;

TYPED_TEST_SUITE(ReferenceServiceDiscovery_test, TestVariations);

// All tests run for publishers and servers as well as all 8 search variations.
// Each test sets up the discovery state and, runs a search and compares the result
// with a (trivial, brute force) reference implementation.
// There is some overlap/redundancy in the tests due to the generative scheme
// (the price for elegance and structure).

// TODO: test ids

#if 1
TYPED_TEST(ReferenceServiceDiscovery_test, FindIfNothingOffered)
{
    this->testFindService({"a"}, {"b"}, {"c"});
}

TYPED_TEST(ReferenceServiceDiscovery_test, FindIfSingleServiceOffered)
{
    this->add({"a", "b", "c"});

    this->testFindService({"a"}, {"b"}, {"c"});
}

TYPED_TEST(ReferenceServiceDiscovery_test, FindIfSingleServiceOfferedMultipleTimes)
{
    this->add({"a", "b", "c"});
    this->add({"a", "b", "c"});

    this->testFindService({"a"}, {"b"}, {"c"});
}

TYPED_TEST(ReferenceServiceDiscovery_test, FindIfMultipleServicesOffered)
{
    this->add({"a", "b", "c"});
    this->add({"a", "b", "aa"});
    this->add({"aa", "a", "c"});
    this->add({"a", "ab", "a"});

    this->testFindService({"aa"}, {"a"}, {"c"});
}

TYPED_TEST(ReferenceServiceDiscovery_test, RepeatedSearchYieldsSameResult)
{
    this->add({"a", "b", "c"});
    this->add({"a", "b", "aa"});
    this->add({"aa", "a", "c"});
    this->add({"a", "ab", "a"});

    this->testFindService({"a"}, {"b"}, {"aa"});
    auto previousResult = serviceContainer;

    this->testFindService({"a"}, {"b"}, {"aa"});
    sortAndCompare(previousResult, serviceContainer);
}

TYPED_TEST(ReferenceServiceDiscovery_test, FindNonExistingService)
{
    this->add({"a", "b", "c"});
    this->add({"a", "b", "aa"});
    this->add({"aa", "a", "c"});
    this->add({"a", "ab", "a"});

    this->testFindService({"x"}, {"y"}, {"z"});

    this->testFindService({"x"}, {"y"}, {"aa"});
    this->testFindService({"x"}, {"b"}, {"z"});
    this->testFindService({"a"}, {"y"}, {"z"});

    this->testFindService({"a"}, {"b"}, {"z"});
    this->testFindService({"a"}, {"y"}, {"aa"});
    this->testFindService({"x"}, {"b"}, {"aa"});
}
#endif

// only one kind (MessagingPattern) of service
TYPED_TEST(ReferenceServiceDiscovery_test, FindInMaximumProducers)
{
    auto constexpr MAX = TestFixture::maxProducers(TestFixture::Variation::PATTERN);
    auto constexpr N1 = MAX / 3;
    auto constexpr N2 = 2 * N1;

    // completely random
    auto s1 = randomService();
    this->add(s1);
    uint32_t created = 1;

    for (; created < N1; ++created)
    {
        this->add(randomService());
    }

    // presented by Umlaut (and hence unique)
    ServiceDescription s2{"Ferdinand", "Spitz", "Schnüffler"};
    this->add(s2);
    ++created;

    // partially random
    for (; created < N2; ++created)
    {
        this->add(randomService("Ferdinand"));
    }

    for (; created < MAX - 1; ++created)
    {
        this->add(randomService("Ferdinand", "Spitz"));
    }

    auto s3 = randomService("Ferdinand", "Spitz");
    this->add(s3);
    ++created;

    EXPECT_EQ(created, MAX);


    // find first offered, last offered and some inbetween
    this->testFindService(s1);
    this->testFindService(s2);
    this->testFindService(s3);
};

// mixed tests

TYPED_TEST(ReferenceServiceDiscovery_test, SameServerAndPublisherCanBeFound)
{
    this->add({"Ferdinand", "Schnüffel", "Spitz"});
    this->addOther({"Ferdinand", "Schnüffel", "Spitz"});

    this->testFindService({"Ferdinand"}, {"Schnüffel"}, {"Spitz"});
}

TYPED_TEST(ReferenceServiceDiscovery_test, OtherServiceKindWithMatchingNameIsNotFound)
{
    this->add({"Schnüffel", "Ferdinand", "Spitz"});
    this->addOther({"Ferdinand", "Schnüffel", "Spitz"});

    this->testFindService({"Ferdinand"}, {"Schnüffel"}, {"Spitz"});
}

// both kinds (MessagingPattern) of service
TYPED_TEST(ReferenceServiceDiscovery_test, FindInMaximumServices)
{
    auto constexpr MAX = TestFixture::maxProducers(TestFixture::Variation::PATTERN);
    auto constexpr N1 = MAX / 3;
    auto constexpr N2 = 2 * N1;

    // completely random
    auto s1 = randomService();
    this->add(s1);

    uint32_t created = 1;

    for (; created < N1; ++created)
    {
        this->add(randomService());
    }

    // presented by Umlaut (and hence unique)
    ServiceDescription s2{"Ferdinand", "Spitz", "Schnüffler"};
    this->add(s2);
    ++created;

    // partially random
    for (; created < N2; ++created)
    {
        this->add(randomService("Ferdinand"));
    }

    for (; created < MAX - 1; ++created)
    {
        this->add(randomService("Ferdinand", "Spitz"));
    }

    auto s3 = randomService("Ferdinand", "Spitz");
    this->add(s3);
    ++created;

    EXPECT_EQ(created, MAX);

    // create some services of the other kind
    created = 0;

    auto constexpr OTHER_MAX = TestFixture::maxProducers(TestFixture::otherPattern());

    // thresholds N1, N2 are chosen in a way that we can reuse them
    for (; created < N1; ++created)
    {
        this->addOther(randomService());
    }

    for (; created < N2; ++created)
    {
        this->addOther(randomService("Spitz"));
    }

    for (; created < OTHER_MAX; ++created)
    {
        this->addOther(randomService("Ferdinand", "Spitz"));
    }

    EXPECT_EQ(created, OTHER_MAX);

    // now we have the maximum of services of both kinds
    // with semi-random services

    this->testFindService(s1);
    this->testFindService(s2);
    this->testFindService(s3);
};


} // namespace


#if 0

// redundant

TYPED_TEST(ServiceDiscovery_test, FindServiceReturnsOfferedService)
{
    ::testing::Test::RecordProperty("TEST_ID", "30f0e255-3584-4ab2-b7a6-85c16026852d");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("service", "instance", "event");

    typename TestFixture::CommunicationKind::Producer producer(SERVICE_DESCRIPTION);

    this->findService(SERVICE_DESCRIPTION.getServiceIDString(),
                      SERVICE_DESCRIPTION.getInstanceIDString(),
                      SERVICE_DESCRIPTION.getEventIDString());

    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION));
}

TYPED_TEST(ServiceDiscovery_test, FindSameServiceMultipleTimesReturnsSingleInstance)
{
    ::testing::Test::RecordProperty("TEST_ID", "21948bcf-fe7e-44b4-b93b-f46303e3e050");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("service", "instance", "event");
    typename TestFixture::CommunicationKind::Producer producer(SERVICE_DESCRIPTION);

    this->findService(SERVICE_DESCRIPTION.getServiceIDString(),
                      SERVICE_DESCRIPTION.getInstanceIDString(),
                      SERVICE_DESCRIPTION.getEventIDString());
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION));

    this->findService(SERVICE_DESCRIPTION.getServiceIDString(),
                      SERVICE_DESCRIPTION.getInstanceIDString(),
                      SERVICE_DESCRIPTION.getEventIDString());

    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION));
}

TYPED_TEST(ServiceDiscovery_test, OfferDifferentServicesWithSameInstanceAndEvent)
{
    ::testing::Test::RecordProperty("TEST_ID", "25bf794d-450e-47ce-a920-ab2ea479af39");

    const IdString_t INSTANCE = "instance";
    const IdString_t EVENT = "event";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1("service1", INSTANCE, EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("service2", INSTANCE, EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3("service3", INSTANCE, EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd3(SERVICE_DESCRIPTION3);

    this->findService(SERVICE_DESCRIPTION1.getServiceIDString(), INSTANCE, EVENT);
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION1));

    this->findService(SERVICE_DESCRIPTION2.getServiceIDString(), INSTANCE, EVENT);
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION2));

    this->findService(SERVICE_DESCRIPTION3.getServiceIDString(), INSTANCE, EVENT);
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION3));
}

TYPED_TEST(ServiceDiscovery_test, FindServiceDoesNotReturnServiceWhenStringsDoNotMatch)
{
    ::testing::Test::RecordProperty("TEST_ID", "1984e907-e990-48b2-8cbd-eab3f67cd162");
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1("service1", "instance1", "event1");
    typename TestFixture::CommunicationKind::Producer producer_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("service2", "instance2", "event2");
    typename TestFixture::CommunicationKind::Producer producer_sd2(SERVICE_DESCRIPTION2);

    this->findService(SERVICE_DESCRIPTION1.getServiceIDString(),
                      SERVICE_DESCRIPTION1.getInstanceIDString(),
                      SERVICE_DESCRIPTION2.getEventIDString());
    ASSERT_THAT(serviceContainer.size(), Eq(0U));

    this->findService(SERVICE_DESCRIPTION1.getServiceIDString(),
                      SERVICE_DESCRIPTION2.getInstanceIDString(),
                      SERVICE_DESCRIPTION1.getEventIDString());

    EXPECT_THAT(serviceContainer.size(), Eq(0U));

    this->findService(SERVICE_DESCRIPTION1.getServiceIDString(),
                      SERVICE_DESCRIPTION2.getInstanceIDString(),
                      SERVICE_DESCRIPTION2.getEventIDString());
    EXPECT_THAT(serviceContainer.size(), Eq(0U));
}

TYPED_TEST(ServiceDiscovery_test, FindServiceWithInstanceAndEventWildcardReturnsAllMatchingServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "6e0b1a12-6995-45f4-8fd8-59acbca9bfa8");

    const IdString_t SERVICE = "service";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1(SERVICE, "instance1", "event1");
    typename TestFixture::CommunicationKind::Producer producer_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2(SERVICE, "instance2", "event2");
    typename TestFixture::CommunicationKind::Producer producer_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3(SERVICE, "instance3", "event3");
    typename TestFixture::CommunicationKind::Producer producer_sd3(SERVICE_DESCRIPTION3);

    ServiceContainer serviceContainerExp;
    serviceContainerExp.push_back(SERVICE_DESCRIPTION1);
    serviceContainerExp.push_back(SERVICE_DESCRIPTION2);
    serviceContainerExp.push_back(SERVICE_DESCRIPTION3);

    this->findService(SERVICE, iox::capro::Wildcard, iox::capro::Wildcard);
    ASSERT_THAT(serviceContainer.size(), Eq(3U));
    EXPECT_TRUE(serviceContainer == serviceContainerExp);
}

TYPED_TEST(ServiceDiscovery_test, FindServiceWithServiceWildcardReturnsCorrectServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "f4731a52-39d8-4d49-b247-008a2e9181f9");
    const IdString_t INSTANCE = "instance";
    const IdString_t EVENT = "event";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1("service1", INSTANCE, EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("service2", "another_instance", EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3("service3", INSTANCE, EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd3(SERVICE_DESCRIPTION3);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION4("service4", INSTANCE, "another_event");
    typename TestFixture::CommunicationKind::Producer producer_sd4(SERVICE_DESCRIPTION4);

    ServiceContainer serviceContainerExp;
    serviceContainerExp.push_back(SERVICE_DESCRIPTION1);
    serviceContainerExp.push_back(SERVICE_DESCRIPTION3);

    this->findService(iox::capro::Wildcard, INSTANCE, EVENT);
    ASSERT_THAT(serviceContainer.size(), Eq(2U));
    EXPECT_TRUE(serviceContainer == serviceContainerExp);
}

TYPED_TEST(ServiceDiscovery_test, FindServiceWithEventWildcardReturnsCorrectServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "e78b35f4-b3c3-4c39-b10a-67712c72e28a");
    const IdString_t SERVICE = "service";
    const IdString_t INSTANCE = "instance";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1(SERVICE, INSTANCE, "event1");
    typename TestFixture::CommunicationKind::Producer producer_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("another_service", INSTANCE, "event2");
    typename TestFixture::CommunicationKind::Producer producer_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3(SERVICE, INSTANCE, "event3");
    typename TestFixture::CommunicationKind::Producer producer_sd3(SERVICE_DESCRIPTION3);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION4(SERVICE, "another_instance", "event4");
    typename TestFixture::CommunicationKind::Producer producer_sd4(SERVICE_DESCRIPTION4);

    ServiceContainer serviceContainerExp;
    serviceContainerExp.push_back(SERVICE_DESCRIPTION1);
    serviceContainerExp.push_back(SERVICE_DESCRIPTION3);

    this->findService(SERVICE, INSTANCE, iox::capro::Wildcard);
    ASSERT_THAT(serviceContainer.size(), Eq(2U));
    EXPECT_TRUE(serviceContainer == serviceContainerExp);
}

TYPED_TEST(ServiceDiscovery_test, FindServiceWithInstanceWildcardReturnsCorrectServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "2ec4b422-3ded-4af3-9e72-3b870c55031c");
    const IdString_t SERVICE = "service";
    const IdString_t EVENT = "event";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1(SERVICE, "instance1", EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2("another_service", "instance2", EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3(SERVICE, "instance3", EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd3(SERVICE_DESCRIPTION3);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION4(SERVICE, "instance4", "another_event");
    typename TestFixture::CommunicationKind::Producer producer_sd4(SERVICE_DESCRIPTION4);

    ServiceContainer serviceContainerExp;
    serviceContainerExp.push_back(SERVICE_DESCRIPTION1);
    serviceContainerExp.push_back(SERVICE_DESCRIPTION3);

    this->findService(SERVICE, iox::capro::Wildcard, EVENT);
    ASSERT_THAT(serviceContainer.size(), Eq(2U));
    EXPECT_TRUE(serviceContainer == serviceContainerExp);
}

TYPED_TEST(ServiceDiscovery_test, FindServiceReturnsCorrectServiceInstanceCombinations)
{
    ::testing::Test::RecordProperty("TEST_ID", "360839a7-9309-4e7e-8e89-892097a87f7a");

    const IdString_t SERVICE1 = "service1";
    const IdString_t INSTANCE1 = "instance1";
    const IdString_t INSTANCE2 = "instance2";
    const IdString_t EVENT1 = "event1";
    const IdString_t EVENT2 = "event2";
    const IdString_t EVENT3 = "event3";

    const iox::capro::ServiceDescription SERVICE_DESCRIPTION_1_1_1(SERVICE1, INSTANCE1, EVENT1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION_1_1_2(SERVICE1, INSTANCE1, EVENT2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION_1_2_1(SERVICE1, INSTANCE2, EVENT1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION_1_2_2(SERVICE1, INSTANCE2, EVENT2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION_1_2_3(SERVICE1, INSTANCE2, EVENT3);

    typename TestFixture::CommunicationKind::Producer producer_sd_1_1_1(SERVICE_DESCRIPTION_1_1_1);
    typename TestFixture::CommunicationKind::Producer producer_sd_1_1_2(SERVICE_DESCRIPTION_1_1_2);
    typename TestFixture::CommunicationKind::Producer producer_sd_1_2_1(SERVICE_DESCRIPTION_1_2_1);
    typename TestFixture::CommunicationKind::Producer producer_sd_1_2_2(SERVICE_DESCRIPTION_1_2_2);
    typename TestFixture::CommunicationKind::Producer producer_sd_1_2_3(SERVICE_DESCRIPTION_1_2_3);

    this->findService(SERVICE1, INSTANCE1, EVENT1);
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION_1_1_1));

    this->findService(SERVICE1, INSTANCE1, EVENT2);
    ASSERT_THAT(serviceContainer.size(), Eq(1U));

    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION_1_1_2));

    this->findService(SERVICE1, INSTANCE2, EVENT1);
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION_1_2_1));

    this->findService(SERVICE1, INSTANCE2, EVENT2);
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION_1_2_2));

    this->findService(SERVICE1, INSTANCE2, EVENT3);
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION_1_2_3));
}

TYPED_TEST(ServiceDiscovery_test, NonExistingServicesAreNotFound)
{
    ::testing::Test::RecordProperty("TEST_ID", "86b87264-4df4-4d20-9357-06391ca1d57f");
    const IdString_t SERVICE = "service";
    const IdString_t NONEXISTENT_SERVICE = "ignatz";
    const IdString_t INSTANCE = "instance";
    const IdString_t NONEXISTENT_INSTANCE = "schlomo";
    const IdString_t EVENT = "event";
    const IdString_t NONEXISTENT_EVENT = "hypnotoad";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION(SERVICE, INSTANCE, EVENT);
    typename TestFixture::CommunicationKind::Producer producer_sd(SERVICE_DESCRIPTION);

    this->findService(NONEXISTENT_SERVICE, NONEXISTENT_INSTANCE, NONEXISTENT_EVENT);
    EXPECT_THAT(serviceContainer.size(), Eq(0U));

    this->findService(NONEXISTENT_SERVICE, NONEXISTENT_INSTANCE, EVENT);
    EXPECT_THAT(serviceContainer.size(), Eq(0U));

    this->findService(NONEXISTENT_SERVICE, INSTANCE, NONEXISTENT_EVENT);
    EXPECT_THAT(serviceContainer.size(), Eq(0U));

    this->findService(NONEXISTENT_SERVICE, INSTANCE, EVENT);
    EXPECT_THAT(serviceContainer.size(), Eq(0U));

    this->findService(SERVICE, NONEXISTENT_INSTANCE, NONEXISTENT_EVENT);
    EXPECT_THAT(serviceContainer.size(), Eq(0U));

    this->findService(SERVICE, NONEXISTENT_INSTANCE, NONEXISTENT_EVENT);
    EXPECT_THAT(serviceContainer.size(), Eq(0U));

    this->findService(SERVICE, INSTANCE, NONEXISTENT_EVENT);
    EXPECT_THAT(serviceContainer.size(), Eq(0U));
}

TYPED_TEST(ServiceDiscovery_test, FindServiceReturnsMaxProducerServices)
{
    ::testing::Test::RecordProperty("TEST_ID", "68628cc2-df6d-46e4-8586-7563f43bf10c");
    const IdString_t SERVICE = "s";


    // if the result size is limited to be lower than the number of producers in the registry,
    // there is no way to retrieve them all
    constexpr auto NUM_PRODUCERS =
        std::min(TestFixture::CommunicationKind::MAX_USER_PRODUCERS, iox::MAX_FINDSERVICE_RESULT_SIZE);
    iox::cxx::vector<typename TestFixture::CommunicationKind::Producer, NUM_PRODUCERS> producers;

    // we want to check whether we find all the services, including internal ones like introspection,
    // so we first search for them without creating other services to obtain the internal ones

    ServiceContainer serviceContainerExp;
    auto findInternalServices = [&](const ServiceDescription& s) { serviceContainerExp.emplace_back(s); };
    this->sut.findService(iox::capro::Wildcard,
                          iox::capro::Wildcard,
                          iox::capro::Wildcard,
                          findInternalServices,
                          TestFixture::CommunicationKind::KIND);

    for (size_t i = 0; i < NUM_PRODUCERS; i++)
    {
        std::string instance = "i" + iox::cxx::convert::toString(i);
        iox::capro::ServiceDescription SERVICE_DESCRIPTION(
            SERVICE, IdString_t(iox::cxx::TruncateToCapacity, instance), "foo");
        producers.emplace_back(SERVICE_DESCRIPTION);
        serviceContainerExp.push_back(SERVICE_DESCRIPTION);
    }

    // now we should find the maximum number of services we can search for,
    // i.e. internal services and those we just created (iox::MAX_PUBLISHERS combined)
    this->findService(iox::capro::Wildcard, iox::capro::Wildcard, iox::capro::Wildcard);

    constexpr auto EXPECTED_NUM_PRODUCERS =
        std::min(TestFixture::CommunicationKind::MAX_PRODUCERS, iox::MAX_FINDSERVICE_RESULT_SIZE);
    EXPECT_EQ(serviceContainer.size(), EXPECTED_NUM_PRODUCERS);
    EXPECT_TRUE(serviceContainer == serviceContainerExp);
}

TYPED_TEST(ServiceDiscovery_test, OfferSingleServiceMultiInstance)
{
    ::testing::Test::RecordProperty("TEST_ID", "538bec69-ea02-400e-8643-c833d6e84972");

    const IdString_t SERVICE = "service";
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION1(SERVICE, "instance1", "event1");
    typename TestFixture::CommunicationKind::Producer producer_sd1(SERVICE_DESCRIPTION1);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION2(SERVICE, "instance2", "event2");
    typename TestFixture::CommunicationKind::Producer producer_sd2(SERVICE_DESCRIPTION2);
    const iox::capro::ServiceDescription SERVICE_DESCRIPTION3(SERVICE, "instance3", "event3");
    typename TestFixture::CommunicationKind::Producer producer_sd3(SERVICE_DESCRIPTION3);

    this->findService(SERVICE, SERVICE_DESCRIPTION1.getInstanceIDString(), SERVICE_DESCRIPTION1.getEventIDString());
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION1));

    this->findService(SERVICE, SERVICE_DESCRIPTION2.getInstanceIDString(), SERVICE_DESCRIPTION2.getEventIDString());
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION2));

    this->findService(SERVICE, SERVICE_DESCRIPTION3.getInstanceIDString(), SERVICE_DESCRIPTION3.getEventIDString());
    ASSERT_THAT(serviceContainer.size(), Eq(1U));
    EXPECT_THAT(*serviceContainer.begin(), Eq(SERVICE_DESCRIPTION3));
}

TEST_F(ServiceDiscoveryPubSub_test, FindServiceWithPublisherAndServerWithTheSameServiceDescriptionAreBothFound)
{
    ::testing::Test::RecordProperty("TEST_ID", "1ea59629-2fc7-4ef4-9acb-9892a9ec640c");

    const iox::capro::ServiceDescription SERVICE_DESCRIPTION("Curry", "Chicken tikka", "Ginger");

    iox::popo::UntypedPublisher publisher(SERVICE_DESCRIPTION);
    iox::popo::UntypedServer server(SERVICE_DESCRIPTION);

    ServiceContainer serviceContainerPubSub;
    sut.findService(
        SERVICE_DESCRIPTION.getServiceIDString(),
        SERVICE_DESCRIPTION.getInstanceIDString(),
        SERVICE_DESCRIPTION.getEventIDString(),
        [&](const ServiceDescription& s) { serviceContainerPubSub.emplace_back(s); },
        MessagingPattern::PUB_SUB);

    ServiceContainer serviceContainerReqRes;
    sut.findService(
        SERVICE_DESCRIPTION.getServiceIDString(),
        SERVICE_DESCRIPTION.getInstanceIDString(),
        SERVICE_DESCRIPTION.getEventIDString(),
        [&](const ServiceDescription& s) { serviceContainerReqRes.emplace_back(s); },
        MessagingPattern::REQ_RES);

    ASSERT_THAT(serviceContainerPubSub.size(), Eq(1U));
    EXPECT_THAT(*serviceContainerPubSub.begin(), Eq(SERVICE_DESCRIPTION));

    ASSERT_THAT(serviceContainerReqRes.size(), Eq(1U));
    EXPECT_THAT(*serviceContainerReqRes.begin(), Eq(SERVICE_DESCRIPTION));
}

#endif
