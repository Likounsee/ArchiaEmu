#include "cpu/x86/Privileged.hpp"
#include <iostream>
using namespace myps5emu::x86;
int main(){
 const auto cli=Privileged::Cli(0,1ULL<<9,0); if(cli.status!=PrivilegedStatus::Success||cli.value!=(0)) return 1;
 const auto userCli=Privileged::Cli(3,1ULL<<9,0); if(userCli.status!=PrivilegedStatus::PrivilegeViolation) return 2;
 const auto sti=Privileged::Sti(0,0,0); if(sti.status!=PrivilegedStatus::Success||(sti.value&(1ULL<<9))==0) return 3;
 if(Privileged::Hlt(3).status!=PrivilegedStatus::PrivilegeViolation) return 4;
 if(Privileged::Hlt(0).status!=PrivilegedStatus::Success) return 5;
 if(Privileged::MovCrTo(3,3,1).status!=PrivilegedStatus::PrivilegeViolation) return 6;
 if(Privileged::MovCrTo(0,3,1).status!=PrivilegedStatus::Success) return 7;
 std::cout<<"x86 privileged test: PASS\n"; return 0;
}
