#pragma once

#include <cstdint>
#include <functional>

#include "RegisterFile.hpp"
#include "CpuException.hpp"

namespace myps5emu {

class Memory;
class Paging;
class Cpu {
public:
    void SetPaging(Paging* paging) noexcept;
    void SetCr0(std::uint64_t value) noexcept;
    std::uint64_t Cr0() const noexcept;
    void SetCr2(std::uint64_t value) noexcept;
    std::uint64_t Cr2() const noexcept;
    void SetCr3(std::uint64_t value) noexcept;
    std::uint64_t Cr3() const noexcept;
    void SetCr4(std::uint64_t value) noexcept;
    std::uint64_t Cr4() const noexcept;
    void SetEfer(std::uint64_t value) noexcept;
    std::uint64_t Efer() const noexcept;
    void SetMsrStar(std::uint64_t value) noexcept;
    std::uint64_t MsrStar() const noexcept;
    void SetMsrLstar(std::uint64_t value) noexcept;
    std::uint64_t MsrLstar() const noexcept;
    void SetMsrFmask(std::uint64_t value) noexcept;
    std::uint64_t MsrFmask() const noexcept;
    void ConnectMemory(Memory* memory) noexcept;

    void SetInstructionPointer(std::uint64_t value) noexcept;
    std::uint64_t InstructionPointer() const noexcept;

    std::uint64_t ReadRegister64(std::uint8_t index) const noexcept;
    void WriteRegister64(
        std::uint8_t index,
        std::uint64_t value) noexcept;

    std::uint64_t Rax() const noexcept;
    std::uint64_t Rsp() const noexcept;

    void SetStackPointer(std::uint64_t value) noexcept;

    std::uint64_t Rflags() const noexcept;

        void SetRflags(std::uint64_t value) noexcept;

    void SetCodeSegment(std::uint16_t value) noexcept;
    std::uint16_t CodeSegment() const noexcept;

    void SetStackSegment(std::uint16_t value) noexcept;
    std::uint16_t StackSegment() const noexcept;
using FrameCallback = std::function<bool()>;
    using SyscallHandler = std::function<bool(Cpu&)>;
    using ExceptionHandler = std::function<bool(Cpu&, const CpuException&)>;
    using ExceptionReturnHandler = std::function<bool(Cpu&)>;

    void SetFrameCallback(FrameCallback callback);
    void SetSyscallHandler(SyscallHandler callback);
    void SetExceptionHandler(ExceptionHandler callback);
    void SetExceptionReturnHandler(ExceptionReturnHandler callback);
    void SetExceptionArchitecture(
        const x86::Idt* idt,
        const x86::Gdt64* gdt,
        const x86::Tss64* tss) noexcept;
    MemoryFault LastMemoryFault() const noexcept;
    const CpuException& LastException() const noexcept;
    void Halt() noexcept;

    int Run();

    // PS5-specific features
    void EnablePS5Features();
    bool IsPS5Mode() const noexcept;

private:
    struct RexPrefix {
        bool present = false;
        bool w = false;
        bool r = false;
        bool x = false;
        bool b = false;
    };

    // CPU state
    bool ps5_mode_ = false;

    // PS5 hardware / cache simulation state
    bool ps5_memory_layout_initialized_ = false;
    std::uint64_t cache_l1_size_ = 0;
    std::uint64_t cache_l2_size_ = 0;
    std::uint64_t cache_l3_size_ = 0;
    std::uint32_t cache_l1_associativity_ = 0;
    std::uint32_t cache_l2_associativity_ = 0;
    std::uint32_t cache_l3_associativity_ = 0;
    std::uint32_t cache_line_size_ = 64;
    bool ps5_cache_simulated_ = false;
    std::uint64_t ps5_interrupts_handled_ = 0;
    std::uint64_t instruction_pointer_ = 0;
    std::uint64_t rflags_ = 0;
        std::uint16_t code_segment_ = 0;
    std::uint16_t stack_segment_ = 0;
Memory* memory_ = nullptr;
    Paging* paging_ = nullptr;
    std::uint64_t cr0_ = 0;
    std::uint64_t cr2_ = 0;
    std::uint64_t cr3_ = 0;
    std::uint64_t cr4_ = 0;
    std::uint64_t efer_ = 0;
    std::uint64_t msr_star_ = 0;
    std::uint64_t msr_lstar_ = 0;
    std::uint64_t msr_fmask_ = 0;
    FrameCallback frame_callback_;
    SyscallHandler syscall_handler_;
    ExceptionHandler exception_handler_;
    ExceptionReturnHandler exception_return_handler_;
    MemoryFault last_memory_fault_ = static_cast<MemoryFault>(0);
    CpuException last_exception_{};
    std::uint64_t current_instruction_ip_ = 0;
    bool halted_ = false;

    // Register file
    RegisterFile registers_;

    bool Fetch8(std::uint8_t& value);
    bool RaiseException(const CpuException& exception);
    bool RaiseMemoryFault();
    bool TranslateMemoryAddress(std::uint64_t address, bool write, bool instruction, std::uint64_t& physical);
    bool ReadMemory(std::uint64_t address, std::uint8_t* data, std::size_t size);
    bool WriteMemory(std::uint64_t address, const std::uint8_t* data, std::size_t size);
    bool Fetch32(std::uint32_t& value);
    bool Fetch64(std::uint64_t& value);
    bool FetchRel8(std::int8_t& value);
    bool FetchRel32(std::int32_t& value);

    bool Push64(std::uint64_t value);
    bool Pop64(std::uint64_t& value);

    RexPrefix DecodeRex(std::uint8_t byte) const noexcept;

    bool DecodeModRMRegisterRegister32(
        std::uint8_t modrm,
        const RexPrefix& rex,
        std::uint8_t& reg,
        std::uint8_t& rm);

    bool DecodeMemoryOrRegister32(
        std::uint8_t modrm,
        const RexPrefix& rex,
        std::uint8_t& reg,
        std::uint8_t& rm,
        std::uint64_t& address,
        bool& memory);

    bool DecodeSIBAddress(
        std::uint8_t mod,
        std::uint8_t sib,
        const RexPrefix& rex,
        std::uint64_t& address);

    bool DecodeAdd32(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeSub32(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeCmp32(
        std::uint8_t opcode,
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeLogic32(
        std::uint8_t opcode,
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeTest32(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeMov32Load(
        std::uint8_t modrm,
        const RexPrefix& rex);

    // New PS5-specific methods
    bool DecodePS5Instructions();
    void HandlePS5Interrupts();
    void SimulatePS5CacheBehavior();
    void SetupPS5MemoryLayout();

    bool DecodeMov32Store(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeAdd32Store(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeSub32Store(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeMov64Load(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeMov64Store(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeAdd64(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeSub64(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeCmp64(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeLogic64(
        std::uint8_t opcode,
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeTest64(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeMovzx32Reg32(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeMovzx64Reg32(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeMovsx32Reg8(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeMovsx64Reg16(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeMovzx32Reg16(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeMovsx32Reg16(
        std::uint8_t modrm,
        const RexPrefix& rex);
    bool DecodeShift8CL(
        std::uint8_t modrm,
        const RexPrefix& rex);
    bool DecodeShift8Imm(
        std::uint8_t modrm,
        const RexPrefix& rex,
        std::uint8_t count);
    bool DecodeShift8Memory(
        std::uint8_t rm,
        std::uint64_t address,
        std::uint8_t group,
        std::uint8_t count);
    bool DecodeShiftLeft64CL(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeShiftLeft32CL(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeShiftRight32CL(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeShiftArithmetic32CL(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeLea64(
        std::uint8_t modrm,
        const RexPrefix& rex);

    bool DecodeShiftLeft64Imm(
        std::uint8_t modrm,
        const RexPrefix& rex,
        std::uint8_t count,
        bool fetchCountAfterAddress = false);
    bool DecodeShiftLeft32Imm(
        std::uint8_t modrm,
        const RexPrefix& rex,
        std::uint8_t count,
        bool fetchCountAfterAddress = false);
    bool DecodeShiftRight32Imm(
        std::uint8_t modrm,
        const RexPrefix& rex,
        std::uint8_t count,
        bool fetchCountAfterAddress = false);
    bool DecodeShiftArithmetic32Imm(
        std::uint8_t modrm,
        const RexPrefix& rex,
        std::uint8_t count,
        bool fetchCountAfterAddress = false);
    bool DecodeRotate64Imm(
        std::uint8_t modrm,
        const RexPrefix& rex,
        std::uint8_t count,
        bool fetchCountAfterAddress = false);
    bool DecodeRotate32Imm(
        std::uint8_t modrm,
        const RexPrefix& rex,
        std::uint8_t count,
        bool fetchCountAfterAddress = false);
    bool DecodeXchg(
        std::uint8_t modrm,
        const RexPrefix& rex);
    bool DecodeXchg8(
        std::uint8_t modrm,
        const RexPrefix& rex);
    bool DecodeMemoryOrRegister8(
        std::uint8_t modrm,
        const RexPrefix& rex,
        std::uint8_t& regIndex,
        bool& regHighByte,
        std::uint8_t& rmRegisterIndex,
        bool& rmHighByte,
        std::uint64_t& address,
        bool& memory);
    std::uint8_t ReadReg8(
        std::uint8_t registerIndex,
        bool highByte) const noexcept;
    void WriteReg8(
        std::uint8_t registerIndex,
        bool highByte,
        std::uint8_t value) noexcept;
    bool DecodeTest8(
        std::uint8_t modrm,
        const RexPrefix& rex);
    void SetLogicFlags32(std::uint32_t result) noexcept;
    void SetLogicFlags64(std::uint64_t result) noexcept;

    void SetAddFlags32(
        std::uint32_t lhs,
        std::uint32_t rhs,
        std::uint32_t result) noexcept;

    void SetSubFlags32(
        std::uint32_t lhs,
        std::uint32_t rhs,
        std::uint32_t result) noexcept;

    void SetAddFlags64(
        std::uint64_t lhs,
        std::uint64_t rhs,
        std::uint64_t result) noexcept;

    void SetSubFlags64(
        std::uint64_t lhs,
        std::uint64_t rhs,
        std::uint64_t result) noexcept;

    void SetZeroFlag(bool enabled) noexcept;
    bool ZeroFlag() const noexcept;

    void SetSignFlag(bool enabled) noexcept;
    bool SignFlag() const noexcept;
};

} // namespace myps5emu



