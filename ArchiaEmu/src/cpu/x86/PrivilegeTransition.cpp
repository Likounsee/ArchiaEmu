#include "PrivilegeTransition.hpp"
namespace myps5emu::x86 {
bool PrivilegeTransition::IsReturnAllowed(std::uint8_t current_cpl,std::uint8_t target_cpl) noexcept { return current_cpl<=3&&target_cpl<=3&&target_cpl>=current_cpl; }
bool PrivilegeTransition::IsCallAllowed(std::uint8_t current_cpl,std::uint8_t target_cpl) noexcept { return current_cpl<=3&&target_cpl<=current_cpl; }
UserKernelTransition PrivilegeTransition::EnterKernel(const UserKernelTransition& user,std::uint16_t cs,std::uint16_t ss,std::uint64_t rsp) noexcept {
 auto r=user; r.cs=cs; r.ss=ss; r.rsp=rsp; return r;
}
UserKernelTransition PrivilegeTransition::ReturnUser(const UserKernelTransition& kernel,const UserKernelTransition& user) noexcept {
 (void)kernel; return user;
}
}
