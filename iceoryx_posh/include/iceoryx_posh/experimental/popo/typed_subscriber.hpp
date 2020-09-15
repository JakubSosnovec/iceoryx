// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
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

#ifndef IOX_EXPERIMENTAL_POSH_POPO_TYPED_SUBSCRIBER_HPP
#define IOX_EXPERIMENTAL_POSH_POPO_TYPED_SUBSCRIBER_HPP

#include "iceoryx_posh/experimental/popo/base_subscriber.hpp"

namespace iox {
namespace popo {

template<typename T>
class TypedSubscriber : protected BaseSubscriber<T>
{
public:

    TypedSubscriber(const capro::ServiceDescription& service);
    TypedSubscriber(const TypedSubscriber& other) = delete;
    TypedSubscriber& operator=(const TypedSubscriber&) = delete;
    TypedSubscriber(TypedSubscriber&& rhs) = delete;
    TypedSubscriber& operator=(TypedSubscriber&& rhs) = delete;
    ~TypedSubscriber() = default;

    capro::ServiceDescription getServiceDescription() const noexcept;
    uid_t uid() const noexcept;

    cxx::expected<SubscriberError> subscribe(const uint64_t queueCapacity = MAX_RECEIVER_QUEUE_CAPACITY) noexcept;
    SubscriptionState getSubscriptionState() const noexcept;
    void unsubscribe() noexcept;

    bool hasData() const noexcept;
    cxx::optional<cxx::unique_ptr<T>> receive() noexcept;
    cxx::optional<cxx::unique_ptr<mepoo::ChunkHeader>> receiveWithHeader() noexcept;
    void clearReceiveBuffer() noexcept;

};

} // namespace popo
} // namespace iox

#include "iceoryx_posh/experimental/internal/popo/typed_subscriber.inl"

#endif // IOX_EXPERIMENTAL_POSH_POPO_TYPED_SUBSCRIBER_HPP
