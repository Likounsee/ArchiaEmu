#pragma once
#include <cstdint>
namespace myps5emu::x86 {
struct UserKernelTransition {
 std::uint16_t cs=0, ss=0;
 std::uint64_t rsp=0, rip=0, rflags=0;
};
class PrivilegeTransition {
public:
 static bool IsReturnAllowed(std::uint8_t current_cpl,std::uint8_t target_cpl) noexcept;
 static bool IsCallAllowed(std::uint8_t current_cpl,std::uint8_t target_cpl) noexcept;
 static UserKernelTransition EnterKernel(const UserKernelTransition& user,std::uint16_t kernel_cs,std::uint16_t kernel_ss,std::uint64_t kernel_rsp) noexcept;
 static UserKernelTransition ReturnUser(const UserKernelTransition& kernel,const UserKernelTransition& user) noexcept;
};
}
