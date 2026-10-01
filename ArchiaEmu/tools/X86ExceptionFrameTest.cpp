#include "cpu/CpuException.hpp"
#include "cpu/x86/ExceptionFrame64.hpp"

#include <cstdint>
#include <iostream>

using namespace myps5emu;
using namespace myps5emu::x86;

namespace {

bool Fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

} // namespace

int main()
{
    CpuException pageFault{};
    pageFault.kind = CpuExceptionKind::MemoryFault;
    pageFault.instruction_pointer = 0x123456789ABCDEF0ULL;
    pageFault.vector = CpuExceptionVector::PageFault;

    constexpr std::uint64_t cs = 0x0028;
    constexpr std::uint64_t rflags = 0x202;
    constexpr std::uint64_t pageFaultError = 0x5;

    const auto pageFrame =
        ExceptionFrame64::Build(pageFault, cs, rflags, pageFaultError);

    if (pageFrame.rip != pageFault.instruction_pointer ||
        pageFrame.cs != cs ||
        pageFrame.rflags != rflags ||
        !pageFrame.has_error_code ||
        pageFrame.error_code != pageFaultError) {
        return Fail("Page-fault exception frame was built incorrectly")
            ? 0 : 1;
    }

    CpuException divideError{};
    divideError.kind = CpuExceptionKind::DivideError;
    divideError.instruction_pointer = 0x4000;
    divideError.vector = CpuExceptionVector::DivideError;

    const auto divideFrame =
        ExceptionFrame64::Build(divideError, cs, rflags, 0xDEADBEEF);

    if (divideFrame.rip != divideError.instruction_pointer ||
        divideFrame.cs != cs ||
        divideFrame.rflags != rflags ||
        divideFrame.has_error_code ||
        divideFrame.error_code != 0) {
        return Fail("Divide-error exception frame was built incorrectly")
            ? 0 : 1;
    }

    if (!ExceptionFrame64::HasHardwareErrorCode(
            CpuExceptionVector::PageFault) ||
        ExceptionFrame64::HasHardwareErrorCode(
            CpuExceptionVector::DivideError) ||
        ExceptionFrame64::HasHardwareErrorCode(
            CpuExceptionVector::InvalidOpcode)) {
        return Fail("Exception error-code classification is incorrect")
            ? 0 : 1;
    }

    std::cout << "x86-64 exception frame test: PASS\n";
    return 0;
}
