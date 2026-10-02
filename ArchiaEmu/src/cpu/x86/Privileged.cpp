#include "Privileged.hpp"
namespace myps5emu::x86 {
namespace { constexpr std::uint64_t IF=1ULL<<9; }
PrivilegedResult Privileged::Cli(std::uint8_t cpl,std::uint64_t flags,std::uint64_t iopl) noexcept {
 if(cpl>iopl) return {PrivilegedStatus::PrivilegeViolation,flags};
 return {PrivilegedStatus::Success,flags&~IF};
}
PrivilegedResult Privileged::Sti(std::uint8_t cpl,std::uint64_t flags,std::uint64_t iopl) noexcept {
 if(cpl>iopl) return {PrivilegedStatus::PrivilegeViolation,flags};
 return {PrivilegedStatus::Success,flags|IF};
}
PrivilegedResult Privileged::Hlt(std::uint8_t cpl) noexcept {
 return cpl==0 ? PrivilegedResult{PrivilegedStatus::Success,0} : PrivilegedResult{PrivilegedStatus::PrivilegeViolation,0};
}
PrivilegedResult Privileged::MovCrTo(std::uint8_t cpl,std::uint8_t cr,std::uint64_t value) noexcept {
 if(cpl!=0 || cr==1 || cr>4) return {PrivilegedStatus::PrivilegeViolation,0};
 return {PrivilegedStatus::Success,value};
}
PrivilegedResult Privileged::Invlpg(std::uint8_t cpl) noexcept {
 return cpl==0 ? PrivilegedResult{PrivilegedStatus::Success,0} : PrivilegedResult{PrivilegedStatus::PrivilegeViolation,0};
}
PrivilegedResult Privileged::MovCrFrom(std::uint8_t cpl,std::uint8_t cr,std::uint64_t value) noexcept {
 if(cpl!=0 || cr==1 || cr>4) return {PrivilegedStatus::PrivilegeViolation,0};
 return {PrivilegedStatus::Success,value};
}
}
