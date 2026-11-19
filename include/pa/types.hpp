#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include <functional>

namespace pa {

// Строгие ID (newtype pattern)
struct UserId {
    uint64_t value;
    bool operator==(const UserId& o) const noexcept { return value == o.value; }
    bool operator!=(const UserId& o) const noexcept { return value != o.value; }
    bool operator<(const UserId& o) const noexcept { return value < o.value; }
};

struct TaskId {
    uint64_t value;
    bool operator==(const TaskId& o) const noexcept { return value == o.value; }
    bool operator!=(const TaskId& o) const noexcept { return value != o.value; }
    bool operator<(const TaskId& o) const noexcept { return value < o.value; }
};

struct PrivilegeId {
    uint64_t value;
    bool operator==(const PrivilegeId& o) const noexcept { return value == o.value; }
    bool operator!=(const PrivilegeId& o) const noexcept { return value != o.value; }
    bool operator<(const PrivilegeId& o) const noexcept { return value < o.value; }
};

struct AuctionId {
    uint64_t value;
    bool operator==(const AuctionId& o) const noexcept { return value == o.value; }
    bool operator!=(const AuctionId& o) const noexcept { return value != o.value; }
    bool operator<(const AuctionId& o) const noexcept { return value < o.value; }
};

using Points = uint64_t;
using Timestamp = std::chrono::system_clock::time_point;

// Ранг задачи
enum class TaskRank : uint8_t {
    D = 0,  // Низший: до 50,    M = 1
    C = 1,  // Средний: 51-200,  M = 3
    B = 2,  // Высокий: 201-500, M = 5
    A = 3   // Эпический: 501+,  M = 10
};

inline double rank_multiplier(TaskRank r) noexcept {
    switch (r) {
        case TaskRank::D: return 1.0;
        case TaskRank::C: return 3.0;
        case TaskRank::B: return 5.0;
        case TaskRank::A: return 10.0;
    }
    return 1.0;
}

inline Points rank_max_base_price(TaskRank r) noexcept {
    switch (r) {
        case TaskRank::D: return 50;
        case TaskRank::C: return 200;
        case TaskRank::B: return 500;
        case TaskRank::A: return UINT64_MAX;
    }
    return 0;
}

inline Points rank_min_base_price(TaskRank r) noexcept {
    switch (r) {
        case TaskRank::D: return 0;
        case TaskRank::C: return 51;
        case TaskRank::B: return 201;
        case TaskRank::A: return 501;
    }
    return 0;
}

// Коэффициент «Силы» (K2)
struct TaskComplexity {
    double volume;
    double difficulty;
    double risk;

    double w_volume;
    double w_difficulty;
    double w_risk;
};

} // namespace pa

// Специализации std::hash — в ГЛОБАЛЬНОМ namespace std, не внутри pa
namespace std {
    template<> struct hash<pa::UserId> {
        size_t operator()(const pa::UserId& id) const noexcept {
            return hash<uint64_t>{}(id.value);
        }
    };
    template<> struct hash<pa::TaskId> {
        size_t operator()(const pa::TaskId& id) const noexcept {
            return hash<uint64_t>{}(id.value);
        }
    };
    template<> struct hash<pa::PrivilegeId> {
        size_t operator()(const pa::PrivilegeId& id) const noexcept {
            return hash<uint64_t>{}(id.value);
        }
    };
    template<> struct hash<pa::AuctionId> {
        size_t operator()(const pa::AuctionId& id) const noexcept {
            return hash<uint64_t>{}(id.value);
        }
    };
} // namespace std
