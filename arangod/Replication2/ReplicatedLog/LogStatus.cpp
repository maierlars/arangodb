////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <Basics/debugging.h>
#include <Basics/Exceptions.h>
#include <Basics/StaticStrings.h>
#include <Basics/overload.h>
#include <Basics/application-exit.h>
#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include "LogStatus.h"

using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

constexpr static std::string_view kUnconfiguredString = "unconfigured";

void UnconfiguredStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue(kUnconfiguredString));
}

auto UnconfiguredStatus::fromVelocyPack(velocypack::Slice slice)
    -> UnconfiguredStatus {
  TRI_ASSERT(slice.get("role").isEqualString(kUnconfiguredString));
  return {};
}

auto QuickLogStatus::getCurrentTerm() const noexcept -> std::optional<LogTerm> {
  if (role == ParticipantRole::kUnconfigured) {
    return std::nullopt;
  }
  return term;
}

auto QuickLogStatus::getLocalStatistics() const noexcept
    -> std::optional<LogStatistics> {
  if (role == ParticipantRole::kUnconfigured) {
    return std::nullopt;
  }
  return local;
}

void FollowerStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue(StaticStrings::Follower));
  if (leader.has_value()) {
    builder.add(StaticStrings::Leader, VPackValue(*leader));
  }
  builder.add(StaticStrings::Term, VPackValue(term.value));
  builder.add("lowestIndexToKeep", VPackValue(lowestIndexToKeep.value));
  builder.add(VPackValue("local"));
  local.toVelocyPack(builder);
}

auto FollowerStatus::fromVelocyPack(velocypack::Slice slice) -> FollowerStatus {
  TRI_ASSERT(slice.get("role").isEqualString(StaticStrings::Follower));
  FollowerStatus status;
  status.term = slice.get(StaticStrings::Term).extract<LogTerm>();
  status.lowestIndexToKeep = slice.get("lowestIndexToKeep").extract<LogIndex>();
  status.local = LogStatistics::fromVelocyPack(slice.get("local"));
  if (auto leader = slice.get(StaticStrings::Leader); !leader.isNone()) {
    status.leader = leader.copyString();
  }
  return status;
}

void LeaderStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue(StaticStrings::Leader));
  builder.add(StaticStrings::Term, VPackValue(term.value));
  builder.add("lowestIndexToKeep", VPackValue(lowestIndexToKeep.value));
  builder.add("commitLagMS", VPackValue(commitLagMS.count()));
  builder.add("leadershipEstablished", VPackValue(leadershipEstablished));
  builder.add(VPackValue("local"));
  local.toVelocyPack(builder);
  builder.add(VPackValue("lastCommitStatus"));
  lastCommitStatus.toVelocyPack(builder);
  builder.add(VPackValue("activeParticipantsConfig"));
  activeParticipantsConfig.toVelocyPack(builder);
  builder.add(VPackValue("committedParticipantsConfig"));
  if (committedParticipantsConfig.has_value()) {
    committedParticipantsConfig->toVelocyPack(builder);
  } else {
    builder.add(VPackSlice::nullSlice());
  }
  {
    VPackObjectBuilder ob2(&builder, StaticStrings::Follower);
    for (auto const& [id, stat] : follower) {
      builder.add(VPackValue(id));
      stat.toVelocyPack(builder);
    }
  }
}

auto LeaderStatus::fromVelocyPack(velocypack::Slice slice) -> LeaderStatus {
  TRI_ASSERT(slice.get("role").isEqualString(StaticStrings::Leader));
  LeaderStatus status;
  status.term = slice.get(StaticStrings::Term).extract<LogTerm>();
  status.local = LogStatistics::fromVelocyPack(slice.get("local"));
  status.lowestIndexToKeep = slice.get("lowestIndexToKeep").extract<LogIndex>();
  status.leadershipEstablished = slice.get("leadershipEstablished").isTrue();
  status.commitLagMS = std::chrono::duration<double, std::milli>{
      slice.get("commitLagMS").extract<double>()};
  status.lastCommitStatus =
      CommitFailReason::fromVelocyPack(slice.get("lastCommitStatus"));
  status.activeParticipantsConfig =
      ParticipantsConfig::fromVelocyPack(slice.get("activeParticipantsConfig"));
  if (auto const committedParticipantsConfigSlice =
          slice.get("committedParticipantsConfig");
      !committedParticipantsConfigSlice.isNull()) {
    status.committedParticipantsConfig =
        ParticipantsConfig::fromVelocyPack(committedParticipantsConfigSlice);
  }
  for (auto [key, value] :
       VPackObjectIterator(slice.get(StaticStrings::Follower))) {
    auto id = ParticipantId{key.copyString()};
    auto stat = FollowerStatistics::fromVelocyPack(value);
    status.follower.emplace(std::move(id), stat);
  }
  return status;
}

void FollowerStatistics::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::CommitIndex, VPackValue(commitIndex.value));
  builder.add(VPackValue(StaticStrings::Spearhead));
  spearHead.toVelocyPack(builder);
  builder.add(VPackValue("lastErrorReason"));
  lastErrorReason.toVelocyPack(builder);
  builder.add("lastRequestLatencyMS", VPackValue(lastRequestLatencyMS.count()));
  builder.add(VPackValue("state"));
  internalState.toVelocyPack(builder);
}

auto FollowerStatistics::fromVelocyPack(velocypack::Slice slice)
    -> FollowerStatistics {
  FollowerStatistics stats;
  stats.commitIndex = slice.get(StaticStrings::CommitIndex).extract<LogIndex>();
  stats.spearHead =
      TermIndexPair::fromVelocyPack(slice.get(StaticStrings::Spearhead));
  stats.lastErrorReason =
      AppendEntriesErrorReason::fromVelocyPack(slice.get("lastErrorReason"));
  stats.lastRequestLatencyMS = std::chrono::duration<double, std::milli>{
      slice.get("lastRequestLatencyMS").getDouble()};
  stats.internalState = FollowerState::fromVelocyPack(slice.get("state"));
  return stats;
}

auto replicated_log::operator==(FollowerStatistics const& left,
                                FollowerStatistics const& right) noexcept
    -> bool {
  return left.lastErrorReason == right.lastErrorReason &&
         left.lastRequestLatencyMS == right.lastRequestLatencyMS &&
         left.internalState.value.index() == right.internalState.value.index();
}

auto replicated_log::operator!=(FollowerStatistics const& left,
                                FollowerStatistics const& right) noexcept
    -> bool {
  return !(left == right);
}

auto LogStatus::getCurrentTerm() const noexcept -> std::optional<LogTerm> {
  return std::visit(
      overload{
          [&](replicated_log::UnconfiguredStatus) -> std::optional<LogTerm> {
            return std::nullopt;
          },
          [&](replicated_log::LeaderStatus const& s) -> std::optional<LogTerm> {
            return s.term;
          },
          [&](replicated_log::FollowerStatus const& s)
              -> std::optional<LogTerm> { return s.term; },
      },
      _variant);
}

auto LogStatus::getLocalStatistics() const noexcept
    -> std::optional<LogStatistics> {
  return std::visit(
      overload{
          [&](replicated_log::UnconfiguredStatus const& s)
              -> std::optional<LogStatistics> { return std::nullopt; },
          [&](replicated_log::LeaderStatus const& s)
              -> std::optional<LogStatistics> { return s.local; },
          [&](replicated_log::FollowerStatus const& s)
              -> std::optional<LogStatistics> { return s.local; },
      },
      _variant);
}

auto LogStatus::fromVelocyPack(VPackSlice slice) -> LogStatus {
  auto role = slice.get("role");
  if (role.isEqualString(StaticStrings::Leader)) {
    return LogStatus{LeaderStatus::fromVelocyPack(slice)};
  } else if (role.isEqualString(StaticStrings::Follower)) {
    return LogStatus{FollowerStatus::fromVelocyPack(slice)};
  } else {
    return LogStatus{UnconfiguredStatus::fromVelocyPack(slice)};
  }
}

LogStatus::LogStatus(UnconfiguredStatus status) noexcept : _variant(status) {}
LogStatus::LogStatus(LeaderStatus status) noexcept
    : _variant(std::move(status)) {}
LogStatus::LogStatus(FollowerStatus status) noexcept
    : _variant(std::move(status)) {}

auto LogStatus::getVariant() const noexcept -> VariantType const& {
  return _variant;
}

auto LogStatus::toVelocyPack(velocypack::Builder& builder) const -> void {
  std::visit([&](auto const& s) { s.toVelocyPack(builder); }, _variant);
}

auto LogStatus::asLeaderStatus() const noexcept -> LeaderStatus const* {
  return std::get_if<LeaderStatus>(&_variant);
}

namespace {
inline constexpr std::string_view kSupervision = "supervision";
inline constexpr std::string_view kLeaderId = "leaderId";
}  // namespace

auto GlobalStatus::toVelocyPack(velocypack::Builder& builder) const -> void {
  VPackObjectBuilder ob(&builder);
  builder.add(VPackValue(kSupervision));
  supervision.toVelocyPack(builder);
  {
    VPackObjectBuilder ob2(&builder, StaticStrings::Participants);
    for (auto const& [id, status] : participants) {
      builder.add(VPackValue(id));
      status.toVelocyPack(builder);
    }
  }
  builder.add(VPackValue("specification"));
  specification.toVelocyPack(builder);
  if (leaderId.has_value()) {
    builder.add(kLeaderId, VPackValue(*leaderId));
  }
}

auto GlobalStatus::fromVelocyPack(VPackSlice slice) -> GlobalStatus {
  GlobalStatus status;
  auto sup = slice.get(kSupervision);
  TRI_ASSERT(!sup.isNone())
      << "expected " << kSupervision << " key in GlobalStatus";
  status.supervision = SupervisionStatus::fromVelocyPack(sup);
  status.specification =
      Specification::fromVelocyPack(slice.get("specification"));
  for (auto [key, value] :
       VPackObjectIterator(slice.get(StaticStrings::Participants))) {
    auto id = ParticipantId{key.copyString()};
    auto stat = ParticipantStatus::fromVelocyPack(value);
    status.participants.emplace(std::move(id), stat);
  }
  if (auto leaderId = slice.get(kLeaderId); !leaderId.isNone()) {
    status.leaderId = leaderId.copyString();
  }
  return status;
}

void GlobalStatus::Connection::toVelocyPack(
    arangodb::velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add(StaticStrings::ErrorCode, VPackValue(error));
  if (!errorMessage.empty()) {
    b.add(StaticStrings::ErrorMessage, VPackValue(errorMessage));
  }
}

auto GlobalStatus::Connection::fromVelocyPack(arangodb::velocypack::Slice slice)
    -> GlobalStatus::Connection {
  auto code = ErrorCode(slice.get(StaticStrings::ErrorCode).extract<int>());
  auto message = std::string{};
  if (auto ms = slice.get(StaticStrings::ErrorMessage); !ms.isNone()) {
    message = ms.copyString();
  }
  return Connection{.error = code, .errorMessage = message};
}

void GlobalStatus::ParticipantStatus::Response::toVelocyPack(
    arangodb::velocypack::Builder& b) const {
  if (std::holds_alternative<LogStatus>(value)) {
    std::get<LogStatus>(value).toVelocyPack(b);
  } else {
    TRI_ASSERT(std::holds_alternative<velocypack::UInt8Buffer>(value));
    auto slice = VPackSlice(std::get<velocypack::UInt8Buffer>(value).data());
    b.add(slice);
  }
}

auto GlobalStatus::ParticipantStatus::Response::fromVelocyPack(
    arangodb::velocypack::Slice s)
    -> GlobalStatus::ParticipantStatus::Response {
  if (s.hasKey("role")) {
    return Response{.value = LogStatus::fromVelocyPack(s)};
  } else {
    auto buffer = velocypack::UInt8Buffer(s.byteSize());
    buffer.append(s.start(), s.byteSize());
    return Response{.value = buffer};
  }
}

void GlobalStatus::ParticipantStatus::toVelocyPack(
    arangodb::velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add(VPackValue("connection"));
  connection.toVelocyPack(b);
  if (response.has_value()) {
    b.add(VPackValue("response"));
    response->toVelocyPack(b);
  }
}

auto GlobalStatus::ParticipantStatus::fromVelocyPack(
    arangodb::velocypack::Slice s) -> GlobalStatus::ParticipantStatus {
  auto connection = Connection::fromVelocyPack(s.get("connection"));
  auto response = std::optional<Response>{};
  if (auto rs = s.get("response"); !rs.isNone()) {
    response = Response::fromVelocyPack(rs);
  }
  return ParticipantStatus{.connection = std::move(connection),
                           .response = std::move(response)};
}

void GlobalStatus::SupervisionStatus::toVelocyPack(
    arangodb::velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add(VPackValue("connection"));
  connection.toVelocyPack(b);
  if (response.has_value()) {
    b.add(VPackValue("response"));
    response->toVelocyPack(b);
  }
}

auto GlobalStatus::SupervisionStatus::fromVelocyPack(
    arangodb::velocypack::Slice s) -> GlobalStatus::SupervisionStatus {
  auto connection = Connection::fromVelocyPack(s.get("connection"));
  auto response = std::optional<agency::LogCurrentSupervision>{};
  if (auto rs = s.get("response"); !rs.isNone()) {
    response = agency::LogCurrentSupervision::fromVelocyPack(rs);
  }
  return SupervisionStatus{.connection = std::move(connection),
                           .response = std::move(response)};
}

auto replicated_log::to_string(GlobalStatus::SpecificationSource source)
    -> std::string_view {
  switch (source) {
    case GlobalStatus::SpecificationSource::kLocalCache:
      return "LocalCache";
    case GlobalStatus::SpecificationSource::kRemoteAgency:
      return "RemoteAgency";
    default:
      return "(unknown)";
  }
}

void GlobalStatus::Specification::toVelocyPack(
    arangodb::velocypack::Builder& b) const {
  velocypack::ObjectBuilder ob(&b);
  b.add(VPackValue("plan"));
  plan.toVelocyPack(b);
  b.add("source", VPackValue(to_string(source)));
}

auto GlobalStatus::Specification::fromVelocyPack(arangodb::velocypack::Slice s)
    -> Specification {
  auto plan = agency::LogPlanSpecification::fromVelocyPack(s.get("plan"));
  auto source = GlobalStatus::SpecificationSource::kLocalCache;
  if (s.get("source").isEqualString("RemoteAgency")) {
    source = GlobalStatus::SpecificationSource::kRemoteAgency;
  }
  return Specification{.source = source, .plan = std::move(plan)};
}
