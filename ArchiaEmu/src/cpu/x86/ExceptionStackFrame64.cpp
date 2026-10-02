#include "ExceptionStackFrame64.hpp"

namespace myps5emu::x86 {

namespace {
void Store64(std::uint8_t* out, std::uint64_t value) noexcept
{
    for (std::size_t i = 0; i < 8; ++i) {
        out[i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}
}

std::size_t ExceptionStackFrame64::QwordCount() const noexcept
{
    return (3 + (has_stack_switch ? 2 : 0)) +
           (has_error_code ? 1 : 0);
}

bool ExceptionStackFrame64::Encode(
    std::uint8_t* out,
    std::size_t size) const noexcept
{
    if (out == nullptr || size < QwordCount() * 8) {
        return false;
    }

    std::size_t offset = 0;
    if (has_error_code) {
        Store64(out + offset, error_code); offset += 8;
    }
    Store64(out + offset, rip); offset += 8;
    Store64(out + offset, cs); offset += 8;
    Store64(out + offset, rflags); offset += 8;
    if (has_stack_switch) {
        Store64(out + offset, rsp); offset += 8;
        Store64(out + offset, ss);
    }
    return true;
}

} // namespace myps5emu::x86
