#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>

namespace cgx {
namespace bit {

enum class endianess { little, big };

template <typename T, std::size_t NBitsT, typename TID = std::size_t,
          endianess EndianessT = endianess::little>
struct field_t {
   private:
    std::function<bool(field_t&, const T&)> m_condition =
        [](field_t& var, const value_type& v) {
            (void)var;
            constexpr auto min = std::numeric_limits<T>::min();
            constexpr auto max =
                static_cast<T>(NBitsT == 32 ? std::numeric_limits<T>::max()
                                            : (1 << NBitsT) - 1);
            bool ok = v >= min && v <= max;
            return ok;
        };

    bool           m_valid = false;
    constexpr auto validate() noexcept {
        if (m_condition == nullptr) {
            m_valid = true;
            return;
        }
        m_valid = m_condition(*this, m_value);
    }

   public:
    using value_type                = T;
    static constexpr auto n_bits    = NBitsT;
    static constexpr auto endianess = EndianessT;
    static constexpr auto n_bytes   = (n_bits + 7) / 8;
    static_assert(NBitsT <= sizeof(T) * 8, "n_bits exceeds the size of T");
    static_assert(sizeof(T) <= sizeof(std::uint32_t), "T exceeds 32 bits");

   private:
    value_type m_value{};

   public:
    constexpr auto id() const noexcept {
        return static_cast<std::uint32_t>(TID{});
    }

    constexpr field_t() noexcept {}
    constexpr field_t(const value_type& value) noexcept : m_value(value) {}
    constexpr field_t(const field_t& other) noexcept
        : m_condition(other.m_condition), m_value(other.m_value) {}
    constexpr field_t(field_t&& other) noexcept
        : m_condition(other.m_condition), m_value(other.m_value) {}
    constexpr field_t(const std::uint8_t* bytes,
                      const std::size_t   bit_offset = 0) noexcept
        : m_value(unmarshal(bytes, bit_offset)) {}

    constexpr field_t(
        const std::function<bool(field_t&, const T&)>& condition) noexcept
        : m_condition(condition) {}

    constexpr auto operator=(const field_t& other) noexcept -> field_t& {
        m_value     = other.m_value;
        m_condition = other.m_condition;
        validate();
        return *this;
    }

    constexpr auto operator=(const value_type& other) noexcept -> field_t& {
        m_value = other;
        validate();
        return *this;
    }

    constexpr void force(const value_type& other) noexcept {
        m_value = other;
        m_valid = true;
    }

    constexpr value_type value() const noexcept { return m_value; }
    constexpr operator bool() const noexcept { return is_valid(); }

    constexpr auto operator==(const value_type& other) const noexcept -> bool {
        return m_value == other;
    }
    constexpr auto operator==(const field_t& other) const noexcept -> bool {
        return m_value == other.m_value;
    }

    constexpr auto operator!=(const value_type& other) const noexcept -> bool {
        return m_value != other;
    }
    constexpr auto operator!=(const field_t& other) const noexcept -> bool {
        return m_value != other.m_value;
    }

    constexpr auto operator>(const value_type& other) const noexcept -> bool {
        return m_value > other;
    }
    constexpr auto operator>(const field_t& other) const noexcept -> bool {
        return m_value > other.m_value;
    }

    constexpr auto operator<(const value_type& other) const noexcept -> bool {
        return m_value < other;
    }
    constexpr auto operator<(const field_t& other) const noexcept -> bool {
        return m_value < other.m_value;
    }

    constexpr auto operator>=(const value_type& other) const noexcept -> bool {
        return m_value >= other;
    }
    constexpr auto operator>=(const field_t& other) const noexcept -> bool {
        return m_value >= other.m_value;
    }

    constexpr auto operator<=(const value_type& other) const noexcept -> bool {
        return m_value <= other;
    }
    constexpr auto operator<=(const field_t& other) const noexcept -> bool {
        return m_value <= other.m_value;
    }

    constexpr auto lsb_unmarshal(const std::uint8_t* bytes,
                                 const std::size_t   bit_offset = 0) noexcept {
        static_assert(NBitsT <= sizeof(std::uint32_t) * 8,
                      "n_bits exceeds the size of std::uint32_t");
        std::uint32_t raw = 0;
        for (std::size_t i = bit_offset; i < bit_offset + n_bits; i++) {
            raw |= ((bytes[i / 8] >> (i % 8)) & 1) << (i - bit_offset);
        }
        return raw;
    }

    constexpr auto msb_unmarshal(const std::uint8_t* bytes,
                                 const std::size_t   bit_offset = 0) noexcept {
        static_assert(NBitsT <= sizeof(std::uint32_t) * 8,
                      "n_bits exceeds the size of std::uint32_t");
        std::uint32_t raw = 0;
        for (std::size_t i = bit_offset; i < bit_offset + n_bits; i++) {
            raw <<= 1;
            raw |= (bytes[i / 8] >> (7 - (i % 8))) & 1;
        }
        return raw;
    }

    // unmarshal takes a pointer to an array of bytes and an optional bit offset
    // and decodes the underlying variable. It does not check for out of bounds
    // access.
    constexpr auto unmarshal(const std::uint8_t* bytes,
                             const std::size_t   bit_offset = 0) noexcept
        -> bool {
        uint32_t raw = 0;
        if (EndianessT == endianess::little) {
            raw = lsb_unmarshal(bytes, bit_offset);
        } else {
            raw = msb_unmarshal(bytes, bit_offset);
        }
        memcpy(&m_value, &raw, sizeof(m_value));
        validate();
        return is_valid();
    }

    constexpr auto marshal() const noexcept {
        std::array<std::uint8_t, n_bytes> bytes = {0};
        std::uint32_t                     raw   = 0;
        memcpy(&raw, &m_value, sizeof(m_value));
        if (EndianessT == endianess::little) {
            for (std::size_t i = 0; i < n_bits; i++) {
                bytes[i / 8] |= ((raw >> i) & 1) << (i % 8);
            }
        } else {
            for (std::size_t i = 0; i < n_bits; i++) {
                bytes[i / 8] |= ((raw >> (n_bits - i - 1)) & 1) << (i % 8);
            }
        }
        return bytes;
    }

    constexpr auto is_valid() const noexcept { return m_valid; }

    static constexpr auto any() noexcept { return field_t{}; }

    template <std::size_t N>
    static constexpr auto equal_to(
        const std::array<value_type, N>& values) noexcept {
        return field_t([values](field_t& var, const value_type& v) {
            (void)var;
            for (const auto& value : values) {
                if (v == value) {
                    return true;
                }
            }
            return false;
        });
    }

    static constexpr auto equal_to(const value_type& value) noexcept {
        return field_t([value](field_t& var, const value_type& v) {
            (void)var;
            bool ok = v == value;
            return ok;
        });
    }

    static constexpr auto not_equal_to(const value_type& value) noexcept {
        return field_t([value](field_t& var, const value_type& v) {
            (void)var;
            bool ok = v != value;
            return ok;
        });
    }

    static constexpr auto greater_than(const value_type& value) noexcept {
        return field_t([value](field_t& var, const value_type& v) {
            (void)var;
            bool ok = v > value;
            return ok;
        });
    }

    static constexpr auto less_than(const value_type& value) noexcept {
        return field_t([value](field_t& var, const value_type& v) {
            (void)var;
            bool ok = v < value;
            return ok;
        });
    }

    static constexpr auto greater_equal_to(const value_type& value) noexcept {
        return field_t([value](field_t& var, const value_type& v) {
            (void)var;
            bool ok = v >= value;
            return ok;
        });
    }

    static constexpr auto less_equal_to(const value_type& value) noexcept {
        return field_t([value](field_t& var, const value_type& v) {
            (void)var;
            bool ok = v <= value;
            return ok;
        });
    }

    static constexpr auto between(const value_type& min,
                                  const value_type& max) noexcept {
        return field_t([min, max](field_t& var, const value_type& v) {
            (void)var;
            bool ok = v >= min && v <= max;
            return ok;
        });
    }

    static constexpr auto clamp(const value_type& min,
                                const value_type& max) noexcept {
        return field_t([min, max](field_t& var, const value_type& v) {
            if (v < min) {
                var.force(min);
                return true;
            }

            if (v > max) {
                var.force(max);
                return true;
            }

            return true;
        });
    }

    static constexpr auto condition(
        const std::function<bool(field_t&, const value_type&)>&
            condition) noexcept {
        return field_t(condition);
    }

    constexpr auto set_condition(
        const std::function<bool(field_t&, const value_type&)>&
            condition) noexcept {
        m_condition = condition;
        return *this;
    }

    constexpr auto init(const value_type& value) noexcept -> field_t& {
        m_value = value;
        validate();
        return *this;
    }
};

}  // namespace bit
}  // namespace cgx
