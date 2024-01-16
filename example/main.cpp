#include <array>
#include <iostream>

#include "../bit_msg.hpp"

int main() {
    using first_f = cgx::bit::field_t<std::uint16_t, 4>;
    using second_f = cgx::bit::field_t<std::uint16_t, 12>;

    auto msg = cgx::bit::make_msg(
        0,
        [](const auto& fields) {
            std::cout << "first_f: " << fields.template value_of<first_f>()
                      << std::endl;
            std::cout << "second_f: " << fields.template value_of<second_f>()
                      << std::endl;
        },
        first_f::between(0, 2), second_f::any());

    std::array<std::uint8_t, 2> bytes{0x84, 0x0A};
    std::array<std::uint8_t, 2> bytes2{0x80, 0x0A};

    std::cout << "unmarshalling bytes: ";
    msg.unmarshal(bytes);
    std::cout << (msg ? "valid" : "invalid") << std::endl;

    std::cout << "unmarshalling bytes2: ";
    msg.unmarshal(bytes2);
    std::cout << (msg ? "valid" : "invalid") << std::endl;
}
