#pragma once
#include <cstdint>
namespace myps5emu::x86 {
enum class PrivilegedInstruction : std::uint8_t { Cli, Sti, Hlt, Invlpg, MovCr };
enum class PrivilegedStatus : std::uint8_t { Success, GeneralProtection, InvalidOpcode, PrivilegeViolation };
struct PrivilegedResult { PrivilegedStatus status=PrivilegedStatus::InvalidOpcode; std::uint64_t value=0; };
class Privileged {
public:
 static PrivilegedResult Cli(std::uint8_t cpl,std::uint64_t rflags,std::uint64_t iopl) noexcept;
 static PrivilegedResult Sti(std::uint8_t cpl,std::uint64_t rflags,std::uint64_t iopl) noexcept;
 static PrivilegedResult Hlt(std::uint8_t cpl) noexcept;
 static PrivilegedResult Invlpg(std::uint8_t cpl) noexcept;
 static PrivilegedResult MovCrTo(std::uint8_t cpl,std::uint8_t cr,std::uint64_t value) noexcept;
 static PrivilegedResult MovCrFrom(std::uint8_t cpl,std::uint8_t cr,std::uint64_t value) noexcept;
};
}
