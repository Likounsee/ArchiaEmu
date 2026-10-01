#include "ExceptionFrame64.hpp"

namespace myps5emu::x86 {

bool ExceptionFrame64::HasHardwareErrorCode(
    CpuExceptionVector vector) noexcept
{
    // The emulator currently models #PF as the only exception whose
    // architectural error code is represented in CpuException.
    return vector == CpuExceptionVector::PageFault;
}

ExceptionFrame64 ExceptionFrame64::Build(
    const CpuException& exception,
    std::uint64_t saved_cs,
    std::uint64_t saved_rflags,
    std::uint64_t error_code) noexcept
{
    ExceptionFrame64 frame{};
    frame.rip = exception.instruction_pointer;
    frame.cs = saved_cs;
    frame.rflags = saved_rflags;
    frame.has_error_code = HasHardwareErrorCode(exception.vector);
    frame.error_code = frame.has_error_code ? error_code : 0;
    return frame;
}

} // namespace myps5emu::x86
