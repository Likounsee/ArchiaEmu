#include "cpu/x86/PrivilegeTransition.hpp"
#include <iostream>
using namespace myps5emu::x86;
int main(){
 if(!PrivilegeTransition::IsCallAllowed(3,0)||PrivilegeTransition::IsCallAllowed(0,3)) return 1;
 if(!PrivilegeTransition::IsReturnAllowed(0,3)||PrivilegeTransition::IsReturnAllowed(3,0)) return 2;
 UserKernelTransition u{0x1b,0x23,0x7000,0x401000,0x202};
 auto k=PrivilegeTransition::EnterKernel(u,0x8,0x10,0x9000);
 if(k.cs!=8||k.ss!=0x10||k.rsp!=0x9000||k.rip!=u.rip) return 3;
 auto back=PrivilegeTransition::ReturnUser(k,u);
 if(back.cs!=u.cs||back.ss!=u.ss||back.rsp!=u.rsp||back.rip!=u.rip) return 4;
 std::cout<<"x86 privilege transition test: PASS\n"; return 0;
}
