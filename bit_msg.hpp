#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <tuple>

#include "field.hpp"

namespace cgx {
namespace bit {

struct msg {
   public:
    bool unmarshal(const std::uint8_t* bytes, const std::size_t len,
                   const std::size_t bit_offset = 0) noexcept {
        return on_unmarshal(bytes, len, bit_offset);
    }

    template <std::size_t NBytes>
    bool unmarshal(const std::array<std::uint8_t, NBytes>& bytes,
                   const std::size_t bit_offset = 0) noexcept {
        return on_unmarshal(bytes.data(), NBytes, bit_offset);
    }

    virtual std::uint32_t id() const noexcept       = 0;
    virtual bool          is_valid() const noexcept = 0;

    operator bool() const noexcept { return is_valid(); }

   private:
    virtual bool on_unmarshal(const std::uint8_t* bytes, const std::size_t len,
                              const std::size_t bit_offset) noexcept = 0;
};

class msg_logger {
   public:
    template <typename TMsg>
    void on_error_bit_size(const TMsg& msg, const std::size_t got,
                           const std::size_t expected) const noexcept {
        (void)msg;
        (void)got;
        (void)expected;
    }

    template <typename TMsg>
    void on_error_condition(const TMsg&         msg,
                            const std::uint32_t field_id) const noexcept {
        (void)msg;
        (void)field_id;
    }

    template <typename TMsg>
    void on_error_unmarshal(const TMsg&         msg,
                            const std::uint32_t field_id) const noexcept {
        (void)msg;
        (void)field_id;
    }

    template <typename TMsg>
    void on_unmarshal_start(const TMsg& msg) const noexcept {
        (void)msg;
    }

    template <typename TMsg>
    void on_unmarshal_end(const TMsg& msg) const noexcept {
        (void)msg;
    }
};

template <typename TLogger, typename TID, typename... TFields>
struct msg_t : public msg {
   private:
    static constexpr TLogger logger{};

    // unmarshal
    template <std::size_t I = 0, typename... TArgs>
    typename std::enable_if<I == sizeof...(TArgs), bool>::
        type constexpr unmarshal_tuple(const std::uint8_t*, const std::size_t,
                                       std::tuple<TArgs...>&) noexcept {
        return true;
    }

    template <std::size_t I = 0, typename... TArgs>
        typename std::enable_if <
        I<sizeof...(TArgs), bool>::type constexpr unmarshal_tuple(
            const std::uint8_t* bytes, const std::size_t bit_offset,
            std::tuple<TArgs...>& args) noexcept {
        auto& field = std::get<I>(args);
        if (!field.unmarshal(bytes, bit_offset)) {
            logger.on_error_unmarshal(*this, field.id());
            return false;
        }
        return unmarshal_tuple<I + 1>(bytes, bit_offset + field.n_bits, args);
    }

    // marshal
    template <std::size_t I = 0, typename... TArgs>
    typename std::enable_if<I == sizeof...(TArgs), void>::type marshal_tuple(
        const std::tuple<TArgs...>&, std::uint8_t*,
        std::size_t&) const noexcept {
        return;
    }

    template <std::size_t I = 0, typename... TArgs>
        typename std::enable_if <
        I<sizeof...(TArgs), void>::type marshal_tuple(
            const std::tuple<TArgs...>& args, std::uint8_t* bytes,
            std::size_t& bit_offset) const noexcept {
        const auto& field       = std::get<I>(args);
        const auto  byte_offset = bit_offset / 8;
        const auto  byte_shift  = bit_offset % 8;

        auto data = field.marshal();

        std::uint16_t overflow = 0;
        for (std::size_t i = 0; i < field.n_bytes; i++) {
            std::uint16_t byte = bytes[byte_offset + i];
            byte |= overflow;
            byte |= data[i] << byte_shift;
            overflow = byte >> 8;

            bytes[byte_offset + i] = byte;
        }
        if (overflow != 0) {
            bytes[byte_offset + field.n_bytes] |= overflow;
        }

        bit_offset += field.n_bits;
        marshal_tuple<I + 1>(args, bytes, bit_offset);
    }

    // sum_bits
    template <typename TArg, typename... TArgs>
    static typename std::enable_if<sizeof...(TArgs) == 0, std::size_t>::
        type constexpr sum_bits() noexcept {
        return TArg::n_bits;
    }

    template <typename TArg, typename... TArgs>
    static typename std::enable_if<sizeof...(TArgs) != 0, std::size_t>::
        type constexpr sum_bits() noexcept {
        return TArg::n_bits + sum_bits<TArgs...>();
    }

    template <std::size_t I = 0, typename... TArgs>
    typename std::enable_if<I == sizeof...(TArgs), bool>::
        type constexpr is_valid_tuple(
            const std::tuple<TArgs...>&) const noexcept {
        return true;
    }

    template <std::size_t I = 0, typename... TArgs>
        typename std::enable_if <
        I<sizeof...(TArgs), bool>::type constexpr is_valid_tuple(
            const std::tuple<TArgs...>& args) const noexcept {
        auto& field = std::get<I>(args);
        if (!field.is_valid()) {
            logger.on_error_condition(*this, field.id());
            return false;
        }
        return is_valid_tuple<I + 1>(args);
    }

    bool valid = false;

    std::function<void(const msg_t&)> m_callback         = nullptr;
    std::function<bool(const msg_t&)> m_custom_validator = nullptr;

   public:
    static constexpr auto n_fields = sizeof...(TFields);
    static constexpr auto n_bits   = sum_bits<TFields...>();
    static constexpr auto n_bytes  = (n_bits + 7) / 8;

    using fields_type = std::tuple<TFields...>;
    std::tuple<TFields...> fields;
    using logger_type = TLogger;

    std::uint32_t id() const noexcept override {
        return static_cast<std::uint32_t>(TID{});
    }

    constexpr msg_t() noexcept : fields() {}
    constexpr msg_t(const TFields&... fields) noexcept : fields(fields...) {}
    constexpr msg_t(const std::tuple<TFields...>& fields) noexcept
        : fields(fields) {}
    constexpr msg_t(const std::function<void(const msg_t&)>& callback,
                    const TFields&... fields) noexcept
        : m_callback(callback), fields(fields...) {}
    constexpr msg_t(const std::function<void(const msg_t&)>& callback,
                    const std::tuple<TFields...>&            fields) noexcept
        : m_callback(callback), fields(fields) {}
    constexpr msg_t(const msg_t& other) noexcept
        : m_callback(other.m_callback),
          m_custom_validator(other.m_custom_validator),
          fields(other.fields) {}
    constexpr msg_t(msg_t&& other) noexcept
        : m_callback(other.m_callback),
          m_custom_validator(other.m_custom_validator),
          fields(other.fields) {}

    constexpr auto operator==(const msg_t& other) const noexcept -> bool {
        return fields == other.fields;
    }
    constexpr auto operator!=(const msg_t& other) const noexcept -> bool {
        return fields != other.fields;
    }

    auto set_callback(const std::function<void(const msg_t&)>& callback) {
        m_callback = callback;
        return *this;
    }

    auto set_custom_validator(
        const std::function<bool(const msg_t&)>& custom_validator) {
        m_custom_validator = custom_validator;
        return *this;
    }

    bool is_valid() const noexcept override {
        const bool valid = is_valid_tuple(fields);
        if (m_custom_validator != nullptr) {
            return valid && m_custom_validator(*this);
        }
        return valid;
    }

    void run_callback() const noexcept {
        if (m_callback != nullptr) {
            m_callback(*this);
        }
    }

    template <typename TField>
    constexpr auto& get() noexcept {
        return std::get<TField>(fields);
    }

    template <typename TField>
    constexpr auto& get() const noexcept {
        return std::get<TField>(fields);
    }

    template <typename TField>
    constexpr auto value_of() const noexcept {
        return get<TField>().value();
    }

    template <typename TField>
    auto set_condition(
        const std::function<bool(TField&, const typename TField::value_type&)>&
            condition) noexcept {
        get<TField>().set_condition(condition);
        return *this;
    }

    template <typename TField>
    auto& set_condition(const TField& other) noexcept {
        get<TField>() = other;
        return *this;
    }

    const TLogger& get_logger() const noexcept { return logger; }

    bool on_unmarshal(const std::uint8_t* bytes, const std::size_t len,
                      const std::size_t bit_offset) noexcept override {
        logger.on_unmarshal_start(*this);
        if (len * 8 - bit_offset < n_bits) {
            valid = false;
            logger.on_error_bit_size(*this, len * 8 - bit_offset, n_bits);
            return false;
        }
        std::size_t offset = bit_offset;
        valid              = unmarshal_tuple(bytes, offset, fields);
        logger.on_unmarshal_end(*this);
        if (valid && m_custom_validator != nullptr) {
            valid = m_custom_validator(*this);
        }
        if (valid && m_callback != nullptr) {
            m_callback(*this);
        }
        return valid;
    }

    constexpr auto marshal() const noexcept {
        std::array<std::uint8_t, n_bytes> bytes{};

        std::size_t bit_offset = 0;
        marshal_tuple(fields, bytes.data(), bit_offset);

        return bytes;
    }
};

template <typename TID, typename TCallback, typename... TFields>
msg_t<msg_logger, TID, TFields...> make_msg(const TID&, TCallback&& cb,
                                            const TFields&... fields) noexcept {
    return msg_t<msg_logger, TID, TFields...>(cb, fields...);
}

template <typename TLogger, typename TID, typename TCallback,
          typename... TFields>
msg_t<TLogger, TID, TFields...> make_msg_with_log(
    const TLogger&, const TID&, TCallback&& cb,
    const TFields&... fields) noexcept {
    return msg_t<TLogger, TID, TFields...>(cb, fields...);
}

template <typename TLogger, typename TID, typename TCallback,
          typename... TOtherfields, typename... TFields>
cgx::bit::msg_t<TLogger, TID, TOtherfields..., TFields...> _make_msg_cat(
    const TLogger&, const TID&, TCallback&& cb,
    std::tuple<TOtherfields...>& other, const TFields&... fields) noexcept {
    auto tuple  = std::make_tuple(fields...);
    auto concat = std::tuple_cat(other, tuple);
    return cgx::bit::msg_t<TLogger, TID, TOtherfields..., TFields...>(
        cb, concat);
}

template <typename TID, typename TCallback, typename TOther,
          typename... TFields>
auto make_msg_cat(const TID& id, TCallback&& cb, TOther& other,
                  const TFields&... fields) noexcept {
    return _make_msg_cat(other.get_logger(), id, cb, other.fields, fields...);
}

}  // namespace bit
}  // namespace cgx
