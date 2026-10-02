#include "Cpu.hpp"

#include "memory/Memory.hpp"

#include <iostream>
#include <limits>
#include <utility>


namespace {

struct U128DivResult {
    std::uint64_t quotient_hi;
    std::uint64_t quotient_lo;
    std::uint64_t remainder;
    bool quotient_overflow;
};

U128DivResult DivideU128ByU64(
    std::uint64_t dividend_hi,
    std::uint64_t dividend_lo,
    std::uint64_t divisor)
{
    U128DivResult result{};

    if (divisor == 0) {
        return result;
    }

    
    
    if (dividend_hi >= divisor) {
        result.quotient_overflow = true;
        return result;
    }

    std::uint64_t remainder = 0;

    
    
    
    
    
    
    
    
    
    
    for (int bit = 127; bit >= 0; --bit) {

        const std::uint64_t incoming =
            (bit >= 64)
                ? ((dividend_hi >> (bit - 64)) & 1ULL)
                : ((dividend_lo >> bit) & 1ULL);

        const bool high_bit =
            (remainder & 0x8000000000000000ULL) != 0;

        bool quotient_bit = false;

        if (high_bit) {

            
            
            
            quotient_bit = true;

            remainder =
                (remainder << 1) | incoming;

            remainder -= divisor;

        } else {

            const std::uint64_t shifted =
                (remainder << 1) | incoming;

            if (shifted >= divisor) {
                quotient_bit = true;
                remainder =
                    shifted - divisor;
            } else {
                remainder = shifted;
            }
        }

        if (quotient_bit) {

            if (bit >= 64) {
                result.quotient_hi |=
                    1ULL << (bit - 64);
            } else {
                result.quotient_lo |=
                    1ULL << bit;
            }
        }
    }

    result.remainder = remainder;
    return result;
}

} 
namespace myps5emu {

namespace {

constexpr std::uint64_t CF_MASK = 1ULL << 0;
constexpr std::uint64_t ZF_MASK = 1ULL << 6;
constexpr std::uint64_t SF_MASK = 1ULL << 7;
constexpr std::uint64_t OF_MASK = 1ULL << 11;
constexpr std::uint64_t PF_MASK = 1ULL << 2;
constexpr std::uint64_t AF_MASK = 1ULL << 4;

bool EvenParity8(std::uint8_t value) noexcept
{
    value ^= static_cast<std::uint8_t>(value >> 4);
    value ^= static_cast<std::uint8_t>(value >> 2);
    value ^= static_cast<std::uint8_t>(value >> 1);
    return (value & 1U) == 0;
}

bool ConditionHolds(std::uint8_t cc, std::uint64_t rflags) noexcept
{
    const bool cf=(rflags&CF_MASK)!=0, pf=(rflags&PF_MASK)!=0, zf=(rflags&ZF_MASK)!=0;
    const bool sf=(rflags&SF_MASK)!=0, of=(rflags&OF_MASK)!=0;
    switch(cc){
    case 0x0: return of; case 0x1: return !of; case 0x2: return cf; case 0x3: return !cf;
    case 0x4: return zf; case 0x5: return !zf; case 0x6: return cf||zf; case 0x7: return !cf&&!zf;
    case 0x8: return sf; case 0x9: return !sf; case 0xA: return pf; case 0xB: return !pf;
    case 0xC: return sf!=of; case 0xD: return sf==of; case 0xE: return zf||(sf!=of); case 0xF: return !zf&&(sf==of);
    default: return false;
    }
}

} 

void Cpu::ConnectMemory(Memory* memory) noexcept
{
    memory_ = memory;
}

void Cpu::SetInstructionPointer(std::uint64_t value) noexcept
{
    instruction_pointer_ = value;
}

std::uint64_t Cpu::InstructionPointer() const noexcept
{
    return instruction_pointer_;
}

std::uint64_t Cpu::ReadRegister64(
    std::uint8_t index) const noexcept
{
    return registers_.Read64(index);
}

void Cpu::WriteRegister64(
    std::uint8_t index,
    std::uint64_t value) noexcept
{
    registers_.Write64(index, value);
}

std::uint64_t Cpu::Rax() const noexcept
{
    return registers_.Rax();
}

std::uint64_t Cpu::Rsp() const noexcept
{
    return registers_.Rsp();
}

void Cpu::SetStackPointer(std::uint64_t value) noexcept
{
    registers_.SetRsp(value);
}

std::uint64_t Cpu::Rflags() const noexcept
{
    return rflags_;
}

void Cpu::SetRflags(std::uint64_t value) noexcept
{
    rflags_ = value;
}

void Cpu::SetCodeSegment(std::uint16_t value) noexcept
{
    code_segment_ = value;
}

std::uint16_t Cpu::CodeSegment() const noexcept
{
    return code_segment_;
}

void Cpu::SetStackSegment(std::uint16_t value) noexcept
{
    stack_segment_ = value;
}

std::uint16_t Cpu::StackSegment() const noexcept
{
    return stack_segment_;
}

void Cpu::SetFrameCallback(FrameCallback callback)
{
    frame_callback_ = std::move(callback);
}

void Cpu::SetSyscallHandler(SyscallHandler callback)
{
    syscall_handler_ = std::move(callback);
}

void Cpu::SetExceptionHandler(ExceptionHandler callback)
{
    exception_handler_ = std::move(callback);
}

void Cpu::SetExceptionReturnHandler(ExceptionReturnHandler callback)
{
    exception_return_handler_ = std::move(callback);
}

MemoryFault Cpu::LastMemoryFault() const noexcept
{
    return last_memory_fault_;
}

const CpuException& Cpu::LastException() const noexcept
{
    return last_exception_;
}

bool Cpu::RaiseException(const CpuException& exception)
{
    last_exception_ = exception;

    if (exception_handler_) {
        return exception_handler_(*this, last_exception_);
    }

    return false;
}

bool Cpu::RaiseMemoryFault()
{
    if (memory_ == nullptr) {
        return false;
    }

    last_memory_fault_ = memory_->LastFault();

    if (last_memory_fault_ == MemoryFault::None) {
        return false;
    }

    CpuException exception{};
    exception.kind = CpuExceptionKind::MemoryFault;
    exception.instruction_pointer = current_instruction_ip_;
    exception.memory_fault = last_memory_fault_;

    if (last_memory_fault_ == MemoryFault::Unmapped ||
        last_memory_fault_ == MemoryFault::PermissionDenied) {
        exception.vector = CpuExceptionVector::PageFault;
    }

    return RaiseException(exception);
}


bool Cpu::ReadMemory(std::uint64_t address, std::uint8_t* data, std::size_t size)
{
    if (memory_ == nullptr) {
        return false;
    }
    if (!memory_->Read(address, data, size)) {
        RaiseMemoryFault();
        return false;
    }
    return true;
}

bool Cpu::WriteMemory(std::uint64_t address, const std::uint8_t* data, std::size_t size)
{
    if (memory_ == nullptr) {
        return false;
    }
    if (!memory_->Write(address, data, size)) {
        RaiseMemoryFault();
        return false;
    }
    return true;
}
void Cpu::Halt() noexcept
{
    halted_ = true;
}

void Cpu::EnablePS5Features() {
    ps5_mode_ = true;
    SetupPS5MemoryLayout();
    SimulatePS5CacheBehavior();
    HandlePS5Interrupts();
}

bool Cpu::IsPS5Mode() const noexcept {
    return ps5_mode_;
}

void Cpu::SetupPS5MemoryLayout() {
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    ps5_memory_layout_initialized_ = true;
    
    
    
    std::cout << "PS5 Memory layout initialized" << std::endl;
}

void Cpu::SimulatePS5CacheBehavior() {
    
    
    
    
    
    
    
    
    
    
    
    
    
    cache_l1_size_ = 32 * 1024;  
    cache_l2_size_ = 512 * 1024; 
    cache_l3_size_ = 16 * 1024 * 1024; 
    
    
    cache_l1_associativity_ = 8;
    cache_l2_associativity_ = 8;
    cache_l3_associativity_ = 16;
    
    
    cache_line_size_ = 64; 
    
    
    ps5_cache_simulated_ = true;
    
    std::cout << "PS5 Cache behavior simulated (Zen 2 characteristics)" << std::endl;
}

void Cpu::HandlePS5Interrupts() {
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    ++ps5_interrupts_handled_;
    
    std::cout << "PS5 interrupts handled (" << ps5_interrupts_handled_ << ")" << std::endl;
}

bool Cpu::DecodePS5Instructions()
{
    if (!ps5_mode_) {
        std::cerr
            << "[CPU] Service PS5 demande mais le mode PS5 "
               "n'est pas active (EnablePS5Features())\n";
        return false;
    }

    // Convention SYSCALL standard : le numero de service est
    // dans RAX, exactement comme un vrai syscall Linux/BSD.
    // Ceci n'ajoute aucun opcode au decodeur x86 : c'est un
    // service logiciel invoque via la vraie instruction SYSCALL.
    const std::uint64_t serviceNumber =
        registers_.Read64(0);

    switch (serviceNumber) {

    // PS5_SIGNAL_INTERRUPT : simule une interruption materielle PS5
    case 0x01:
        HandlePS5Interrupts();
        return true;

    // PS5_QUERY_CACHE_L1 : empile la taille du cache L1 simule
    case 0x02:
        return Push64(cache_l1_size_);

    // PS5_QUERY_CACHE_L2 : empile la taille du cache L2 simule
    case 0x03:
        return Push64(cache_l2_size_);

    // PS5_QUERY_CACHE_L3 : empile la taille du cache L3 simule
    case 0x04:
        return Push64(cache_l3_size_);

    default:
        std::cerr
            << "[CPU] Service PS5 non supporte (RAX=0x"
            << std::hex
            << serviceNumber
            << std::dec
            << ")\n";
        return false;
    }
}

bool Cpu::Fetch8(std::uint8_t& value)
{
    if (memory_ == nullptr) {
        return false;
    }

    if (!memory_->ExecuteRead(
            instruction_pointer_,
            &value,
            sizeof(value))) {
        RaiseMemoryFault();
        return false;
    }

    ++instruction_pointer_;
    return true;
}

bool Cpu::Fetch32(std::uint32_t& value)
{
    if (memory_ == nullptr) {
        return false;
    }

    if (!memory_->ExecuteRead(
            instruction_pointer_,
            reinterpret_cast<std::uint8_t*>(&value),
            sizeof(value))) {
        RaiseMemoryFault();
        return false;
    }

    instruction_pointer_ += sizeof(value);
    return true;
}

bool Cpu::Fetch64(std::uint64_t& value)
{
    if (memory_ == nullptr) {
        return false;
    }

    if (!memory_->ExecuteRead(
            instruction_pointer_,
            reinterpret_cast<std::uint8_t*>(&value),
            sizeof(value))) {
        RaiseMemoryFault();
        return false;
    }

    instruction_pointer_ += sizeof(value);
    return true;
}

bool Cpu::FetchRel8(std::int8_t& value)
{
    std::uint8_t raw = 0;

    if (!Fetch8(raw)) {
        return false;
    }

    value = static_cast<std::int8_t>(raw);
    return true;
}

bool Cpu::FetchRel32(std::int32_t& value)
{
    std::uint32_t raw = 0;

    if (!Fetch32(raw)) {
        return false;
    }

    value = static_cast<std::int32_t>(raw);
    return true;
}

bool Cpu::Push64(std::uint64_t value)
{
    if (memory_ == nullptr) {
        return false;
    }
    constexpr std::uint64_t size = 8;

    const std::uint64_t rsp = Rsp();

    if (rsp < size) {
        return false;
    }

    const std::uint64_t newRsp = rsp - size;

    if (!WriteMemory(
            newRsp,
            reinterpret_cast<const std::uint8_t*>(&value),
            sizeof(value))) {
        return false;
    }

    registers_.SetRsp(newRsp);
    return true;
}

bool Cpu::Pop64(std::uint64_t& value)
{
    if (memory_ == nullptr) {
        return false;
    }
    const std::uint64_t rsp = Rsp();

    if (!ReadMemory(
            rsp,
            reinterpret_cast<std::uint8_t*>(&value),
            sizeof(value))) {
        return false;
    }

    if (rsp >
        std::numeric_limits<std::uint64_t>::max() - 8) {
        return false;
    }

    registers_.SetRsp(rsp + 8);
    return true;
}

void Cpu::SetZeroFlag(bool enabled) noexcept
{
    if (enabled) {
        rflags_ |= ZF_MASK;
    } else {
        rflags_ &= ~ZF_MASK;
    }
}

bool Cpu::ZeroFlag() const noexcept
{
    return (rflags_ & ZF_MASK) != 0;
}

void Cpu::SetSignFlag(bool enabled) noexcept
{
    if (enabled) {
        rflags_ |= SF_MASK;
    } else {
        rflags_ &= ~SF_MASK;
    }
}

bool Cpu::SignFlag() const noexcept
{
    return (rflags_ & SF_MASK) != 0;
}

void Cpu::SetLogicFlags32(std::uint32_t result) noexcept
{
    SetZeroFlag(result == 0);
    SetSignFlag((result & 0x80000000U) != 0);

    rflags_ &= ~CF_MASK;
    rflags_ &= ~OF_MASK;
    rflags_ &= ~AF_MASK;
    if (EvenParity8(static_cast<std::uint8_t>(result))) rflags_ |= PF_MASK;
    else rflags_ &= ~PF_MASK;
}

void Cpu::SetLogicFlags64(std::uint64_t result) noexcept
{
    SetZeroFlag(result == 0);
    SetSignFlag((result & 0x8000000000000000ULL) != 0);

    rflags_ &= ~CF_MASK;
    rflags_ &= ~OF_MASK;
    rflags_ &= ~AF_MASK;
    if (EvenParity8(static_cast<std::uint8_t>(result))) rflags_ |= PF_MASK;
    else rflags_ &= ~PF_MASK;
}

void Cpu::SetAddFlags32(
    std::uint32_t lhs,
    std::uint32_t rhs,
    std::uint32_t result) noexcept
{
    SetZeroFlag(result == 0);
    SetSignFlag((result & 0x80000000U) != 0);

    const bool carry =
        result < lhs || rhs > std::numeric_limits<std::uint32_t>::max() - lhs;

    if (carry) {
        rflags_ |= CF_MASK;
    } else {
        rflags_ &= ~CF_MASK;
    }

    const bool overflow =
        ((~(lhs ^ rhs) & (lhs ^ result)) & 0x80000000U) != 0;
    const bool auxiliary = ((lhs ^ rhs ^ result) & 0x10U) != 0;
    if (auxiliary) rflags_ |= AF_MASK; else rflags_ &= ~AF_MASK;
    if (EvenParity8(static_cast<std::uint8_t>(result))) rflags_ |= PF_MASK; else rflags_ &= ~PF_MASK;

    if (overflow) {
        rflags_ |= OF_MASK;
    } else {
        rflags_ &= ~OF_MASK;
    }
}

void Cpu::SetSubFlags32(
    std::uint32_t lhs,
    std::uint32_t rhs,
    std::uint32_t result) noexcept
{
    SetZeroFlag(result == 0);
    SetSignFlag((result & 0x80000000U) != 0);

    const bool borrow = lhs < rhs;

    if (borrow) {
        rflags_ |= CF_MASK;
    } else {
        rflags_ &= ~CF_MASK;
    }

    const bool overflow =
        (((lhs ^ rhs) & (lhs ^ result)) & 0x80000000U) != 0;
    const bool auxiliary = ((lhs ^ rhs ^ result) & 0x10U) != 0;
    if (auxiliary) rflags_ |= AF_MASK; else rflags_ &= ~AF_MASK;
    if (EvenParity8(static_cast<std::uint8_t>(result))) rflags_ |= PF_MASK; else rflags_ &= ~PF_MASK;

    if (overflow) {
        rflags_ |= OF_MASK;
    } else {
        rflags_ &= ~OF_MASK;
    }
}

void Cpu::SetAddFlags64(
    std::uint64_t lhs,
    std::uint64_t rhs,
    std::uint64_t result) noexcept
{
    SetZeroFlag(result == 0);
    SetSignFlag(
        (result & 0x8000000000000000ULL) != 0);

    const bool carry =
        result < lhs || rhs > std::numeric_limits<std::uint64_t>::max() - lhs;

    if (carry) {
        rflags_ |= CF_MASK;
    } else {
        rflags_ &= ~CF_MASK;
    }

    const bool overflow =
        ((~(lhs ^ rhs) &
          (lhs ^ result)) &
         0x8000000000000000ULL) != 0;
    const bool auxiliary = ((lhs ^ rhs ^ result) & 0x10ULL) != 0;
    if (auxiliary) rflags_ |= AF_MASK; else rflags_ &= ~AF_MASK;
    if (EvenParity8(static_cast<std::uint8_t>(result))) rflags_ |= PF_MASK; else rflags_ &= ~PF_MASK;

    if (overflow) {
        rflags_ |= OF_MASK;
    } else {
        rflags_ &= ~OF_MASK;
    }
}

void Cpu::SetSubFlags64(
    std::uint64_t lhs,
    std::uint64_t rhs,
    std::uint64_t result) noexcept
{
    SetZeroFlag(result == 0);
    SetSignFlag(
        (result & 0x8000000000000000ULL) != 0);

    const bool borrow =
        lhs < rhs;

    if (borrow) {
        rflags_ |= CF_MASK;
    } else {
        rflags_ &= ~CF_MASK;
    }

    const bool overflow =
        (((lhs ^ rhs) &
          (lhs ^ result)) &
         0x8000000000000000ULL) != 0;
    const bool auxiliary = ((lhs ^ rhs ^ result) & 0x10ULL) != 0;
    if (auxiliary) rflags_ |= AF_MASK; else rflags_ &= ~AF_MASK;
    if (EvenParity8(static_cast<std::uint8_t>(result))) rflags_ |= PF_MASK; else rflags_ &= ~PF_MASK;

    if (overflow) {
        rflags_ |= OF_MASK;
    } else {
        rflags_ &= ~OF_MASK;
    }
}

Cpu::RexPrefix Cpu::DecodeRex(
    std::uint8_t byte) const noexcept
{
    RexPrefix rex{};

    if (byte < 0x40 || byte > 0x4F) {
        return rex;
    }

    rex.present = true;
    rex.w = (byte & 0x08) != 0;
    rex.r = (byte & 0x04) != 0;
    rex.x = (byte & 0x02) != 0;
    rex.b = (byte & 0x01) != 0;

    return rex;
}

bool Cpu::DecodeModRMRegisterRegister32(
    std::uint8_t modrm,
    const RexPrefix& rex,
    std::uint8_t& reg,
    std::uint8_t& rm)
{
    const std::uint8_t mod =
        static_cast<std::uint8_t>((modrm >> 6) & 0x03);

    if (mod != 0x03) {
        return false;
    }

    reg =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);

    rm =
        static_cast<std::uint8_t>(modrm & 0x07);

    if (rex.r) {
        reg =
            static_cast<std::uint8_t>(reg + 8);
    }

    if (rex.b) {
        rm =
            static_cast<std::uint8_t>(rm + 8);
    }

    return true;
}

bool Cpu::DecodeSIBAddress(
    std::uint8_t mod,
    std::uint8_t sib,
    const RexPrefix& rex,
    std::uint64_t& address)
{
    const std::uint8_t scaleBits =
        static_cast<std::uint8_t>((sib >> 6) & 0x03);

    const std::uint8_t indexBits =
        static_cast<std::uint8_t>((sib >> 3) & 0x07);

    const std::uint8_t baseBits =
        static_cast<std::uint8_t>(sib & 0x07);

    const std::uint64_t scale =
        1ULL << scaleBits;

    const bool hasIndex =
        !(indexBits == 4 && !rex.x);

    std::uint8_t index = indexBits;

    if (rex.x) {
        index =
            static_cast<std::uint8_t>(index + 8);
    }

    const bool noBase =
        mod == 0x00 &&
        baseBits == 0x05 &&
        !rex.b;

    std::uint8_t base = baseBits;

    if (rex.b) {
        base =
            static_cast<std::uint8_t>(base + 8);
    }

    std::int64_t displacement = 0;

    if (mod == 0x01) {

        std::int8_t disp8 = 0;

        if (!FetchRel8(disp8)) {
            return false;
        }

        displacement =
            static_cast<std::int64_t>(disp8);
    }
    else if (mod == 0x02 || noBase) {

        std::int32_t disp32 = 0;

        if (!FetchRel32(disp32)) {
            return false;
        }

        displacement =
            static_cast<std::int64_t>(disp32);
    }

    std::uint64_t result = 0;

    if (!noBase) {
        result += registers_.Read64(base);
    }

    if (hasIndex) {
        result +=
            registers_.Read64(index) * scale;
    }

    result +=
        static_cast<std::uint64_t>(displacement);

    address = result;

    return true;
}
bool Cpu::DecodeMemoryOrRegister32(
    std::uint8_t modrm,
    const RexPrefix& rex,
    std::uint8_t& reg,
    std::uint8_t& rm,
    std::uint64_t& address,
    bool& memory)
{
    const std::uint8_t mod =
        static_cast<std::uint8_t>((modrm >> 6) & 0x03);

    reg =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);

    rm =
        static_cast<std::uint8_t>(modrm & 0x07);

    if (rex.r) {
        reg =
            static_cast<std::uint8_t>(reg + 8);
    }

    
    
    

    if (mod == 0x03) {

        if (rex.b) {
            rm =
                static_cast<std::uint8_t>(rm + 8);
        }

        memory = false;
        address = 0;

        return true;
    }

    
    
    

    memory = true;

    
    
    

    if (rm == 4) {

        std::uint8_t sib = 0;

        if (!Fetch8(sib)) {
            return false;
        }

        return DecodeSIBAddress(
            mod,
            sib,
            rex,
            address);
    }

    
    
    

    if (mod == 0x00 &&
        rm == 0x05 &&
        !rex.b) {

        std::int32_t displacement = 0;

        if (!FetchRel32(displacement)) {
            return false;
        }

        address =
            static_cast<std::uint64_t>(
                static_cast<std::int64_t>(
                    instruction_pointer_) +
                static_cast<std::int64_t>(
                    displacement));

        return true;
    }

    
    
    

    if (rex.b) {
        rm =
            static_cast<std::uint8_t>(rm + 8);
    }

    const std::uint64_t base =
        registers_.Read64(rm);

    
    
    

    if (mod == 0x00) {

        address = base;

        return true;
    }

    
    
    

    if (mod == 0x01) {

        std::int8_t displacement = 0;

        if (!FetchRel8(displacement)) {
            return false;
        }

        address =
            base +
            static_cast<std::uint64_t>(
                static_cast<std::int64_t>(
                    displacement));

        return true;
    }

    
    
    

    if (mod == 0x02) {

        std::int32_t displacement = 0;

        if (!FetchRel32(displacement)) {
            return false;
        }

        address =
            base +
            static_cast<std::uint64_t>(
                static_cast<std::int64_t>(
                    displacement));

        return true;
    }

    return false;
}
bool Cpu::DecodeAdd32(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    const std::uint32_t lhs =
        registers_.Read32(reg);

    std::uint32_t rhs = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&rhs),
                sizeof(rhs))) {
            return false;
        }
    } else {
        rhs =
            registers_.Read32(rm);
    }

    const std::uint32_t result =
        lhs + rhs;

    registers_.Write32(reg, result);

    SetAddFlags32(lhs, rhs, result);

    std::cout
        << "[CPU] ADD32 -> "
        << result
        << '\n';

    return true;
}

bool Cpu::DecodeSub32(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    const std::uint32_t lhs =
        registers_.Read32(reg);

    std::uint32_t rhs = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&rhs),
                sizeof(rhs))) {
            return false;
        }
    } else {
        rhs =
            registers_.Read32(rm);
    }

    const std::uint32_t result =
        lhs - rhs;

    registers_.Write32(reg, result);

    SetSubFlags32(lhs, rhs, result);

    std::cout
        << "[CPU] SUB32 -> "
        << result
        << '\n';

    return true;
}

bool Cpu::DecodeCmp32(
    std::uint8_t opcode,
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    std::uint32_t lhs = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&lhs),
                sizeof(lhs))) {
            return false;
        }
    } else {
        lhs =
            registers_.Read32(rm);
    }

    const std::uint32_t rmValue = lhs;
    const std::uint32_t regValue = registers_.Read32(reg);

    const std::uint32_t lhsValue =
        opcode == 0x3B ? regValue : rmValue;

    const std::uint32_t rhsValue =
        opcode == 0x3B ? rmValue : regValue;

    const std::uint32_t result =
        lhsValue - rhsValue;

    SetSubFlags32(lhsValue, rhsValue, result);

    std::cout
        << "[CPU] CMP32 ZF="
        << (ZeroFlag() ? 1 : 0)
        << " CF="
        << ((rflags_ & CF_MASK) ? 1 : 0)
        << " OF="
        << ((rflags_ & OF_MASK) ? 1 : 0)
        << '\n';

    return true;
}

bool Cpu::DecodeLogic32(
    std::uint8_t opcode,
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    std::uint32_t lhs = 0;
    std::uint32_t rhs = 0;

    const bool rmDestination =
        opcode == 0x09 ||
        opcode == 0x21 ||
        opcode == 0x31;

    if (memory) {
        if (rmDestination) {
            if (!ReadMemory(
                    address,
                    reinterpret_cast<std::uint8_t*>(&lhs),
                    sizeof(lhs))) {
                return false;
            }

            rhs = registers_.Read32(reg);
        }
        else {
            lhs = registers_.Read32(reg);

            if (!ReadMemory(
                    address,
                    reinterpret_cast<std::uint8_t*>(&rhs),
                    sizeof(rhs))) {
                return false;
            }
        }
    }
    else {
        if (rmDestination) {
            lhs = registers_.Read32(rm);
            rhs = registers_.Read32(reg);
        }
        else {
            lhs = registers_.Read32(reg);
            rhs = registers_.Read32(rm);
        }
    }

    std::uint32_t result = 0;

    switch (opcode) {
    case 0x09:
    case 0x0B:
        result = lhs | rhs;
        break;

    case 0x21:
    case 0x23:
        result = lhs & rhs;
        break;

    case 0x31:
    case 0x33:
        result = lhs ^ rhs;
        break;

    default:
        return false;
    }

    if (rmDestination) {
        if (memory) {
            if (!WriteMemory(
                    address,
                    reinterpret_cast<const std::uint8_t*>(&result),
                    sizeof(result))) {
                return false;
            }
        }
        else {
            registers_.Write32(rm, result);
        }
    }
    else {
        registers_.Write32(reg, result);
    }

    SetLogicFlags32(result);

    return true;
}
bool Cpu::DecodeTest32(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    const std::uint32_t lhs =
        registers_.Read32(reg);

    std::uint32_t rhs = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&rhs),
                sizeof(rhs))) {
            return false;
        }
    } else {
        rhs =
            registers_.Read32(rm);
    }

    SetLogicFlags32(lhs & rhs);
    return true;
}

bool Cpu::DecodeMov32Load(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    std::uint32_t value = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&value),
                sizeof(value))) {
            return false;
        }
    } else {
        value = registers_.Read32(rm);
    }

    registers_.Write32(reg, value);
    return true;
}

bool Cpu::DecodeMov32Store(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    const std::uint32_t value =
        registers_.Read32(reg);

    if (memory) {
        return WriteMemory(
            address,
            reinterpret_cast<const std::uint8_t*>(&value),
            sizeof(value));
    }

    registers_.Write32(rm, value);
    return true;
}

bool Cpu::DecodeAdd32Store(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    const std::uint32_t rhs =
        registers_.Read32(reg);

    std::uint32_t lhs = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&lhs),
                sizeof(lhs))) {
            return false;
        }

        const std::uint32_t result = lhs + rhs;

        if (!WriteMemory(
                address,
                reinterpret_cast<const std::uint8_t*>(&result),
                sizeof(result))) {
            return false;
        }

        SetAddFlags32(lhs, rhs, result);
    }
    else {
        lhs = registers_.Read32(rm);

        const std::uint32_t result = lhs + rhs;

        registers_.Write32(rm, result);
        SetAddFlags32(lhs, rhs, result);
    }

    return true;
}

bool Cpu::DecodeSub32Store(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    const std::uint32_t rhs =
        registers_.Read32(reg);

    std::uint32_t lhs = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&lhs),
                sizeof(lhs))) {
            return false;
        }

        const std::uint32_t result = lhs - rhs;

        if (!WriteMemory(
                address,
                reinterpret_cast<const std::uint8_t*>(&result),
                sizeof(result))) {
            return false;
        }

        SetSubFlags32(lhs, rhs, result);
    }
    else {
        lhs = registers_.Read32(rm);

        const std::uint32_t result = lhs - rhs;

        registers_.Write32(rm, result);
        SetSubFlags32(lhs, rhs, result);
    }

    return true;
}

bool Cpu::DecodeMov64Load(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!rex.w ||
        !DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    std::uint64_t value = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&value),
                sizeof(value))) {
            return false;
        }
    } else {
        value = registers_.Read64(rm);
    }

    registers_.Write64(reg, value);
    return true;
}

bool Cpu::DecodeMov64Store(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!rex.w ||
        !DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    const std::uint64_t value =
        registers_.Read64(reg);

    if (memory) {
        return WriteMemory(
            address,
            reinterpret_cast<const std::uint8_t*>(&value),
            sizeof(value));
    }

    registers_.Write64(rm, value);
    return true;
}

bool Cpu::DecodeAdd64(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    if (!rex.w) {
        return false;
    }

    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    const std::uint64_t lhs =
        registers_.Read64(reg);

    std::uint64_t rhs = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&rhs),
                sizeof(rhs))) {
            return false;
        }
    } else {
        rhs =
            registers_.Read64(rm);
    }

    const std::uint64_t result =
        lhs + rhs;

    registers_.Write64(reg, result);
    SetAddFlags64(lhs, rhs, result);

    return true;
}

bool Cpu::DecodeSub64(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    if (!rex.w) {
        return false;
    }

    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    const std::uint64_t lhs =
        registers_.Read64(reg);

    std::uint64_t rhs = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&rhs),
                sizeof(rhs))) {
            return false;
        }
    } else {
        rhs =
            registers_.Read64(rm);
    }

    const std::uint64_t result =
        lhs - rhs;

    registers_.Write64(reg, result);
    SetSubFlags64(lhs, rhs, result);

    return true;
}

bool Cpu::DecodeCmp64(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    if (!rex.w) {
        return false;
    }

    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    
    
    
    const std::uint64_t lhs =
        registers_.Read64(reg);

    std::uint64_t rhs = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&rhs),
                sizeof(rhs))) {
            return false;
        }
    }
    else {
        rhs =
            registers_.Read64(rm);
    }

    const std::uint64_t result =
        lhs - rhs;

    SetSubFlags64(lhs, rhs, result);

    std::cout
        << "[CPU] CMP64 lhs=0x"
        << std::hex
        << lhs
        << " rhs=0x"
        << rhs
        << " result=0x"
        << result
        << " ZF="
        << (ZeroFlag() ? 1 : 0)
        << " CF="
        << ((rflags_ & CF_MASK) ? 1 : 0)
        << " SF="
        << (SignFlag() ? 1 : 0)
        << " OF="
        << ((rflags_ & OF_MASK) ? 1 : 0)
        << std::dec
        << '\n';

    return true;
}
bool Cpu::DecodeLogic64(
    std::uint8_t opcode,
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    if (!rex.w) {
        return false;
    }

    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {

        return false;
    }

    
    
    
    
    
    
    
    
    
    
    
    

    const bool destinationIsMemory =
        opcode == 0x09 ||
        opcode == 0x21 ||
        opcode == 0x31;

    
    
    

    if (destinationIsMemory) {

        const std::uint64_t rhs =
            registers_.Read64(reg);

        std::uint64_t lhs = 0;

        if (memory) {

            if (!ReadMemory(
                    address,
                    reinterpret_cast<
                        std::uint8_t*>(&lhs),
                    sizeof(lhs))) {

                return false;
            }

        }
        else {

            lhs =
                registers_.Read64(rm);
        }

        std::uint64_t result = 0;

        switch (opcode) {

        case 0x09:
            result = lhs | rhs;
            break;

        case 0x21:
            result = lhs & rhs;
            break;

        case 0x31:
            result = lhs ^ rhs;
            break;

        default:
            return false;
        }

        if (memory) {

            if (!WriteMemory(
                    address,
                    reinterpret_cast<
                        const std::uint8_t*>(&result),
                    sizeof(result))) {

                return false;
            }

        }
        else {

            registers_.Write64(
                rm,
                result);
        }

        SetLogicFlags64(result);

        return true;
    }

    
    
    

    std::uint64_t rhs = 0;

    if (memory) {

        if (!ReadMemory(
                address,
                reinterpret_cast<
                    std::uint8_t*>(&rhs),
                sizeof(rhs))) {

            return false;
        }

    }
    else {

        rhs =
            registers_.Read64(rm);
    }

    const std::uint64_t lhs =
        registers_.Read64(reg);

    std::uint64_t result = 0;

    switch (opcode) {

    case 0x0B:
        result = lhs | rhs;
        break;

    case 0x23:
        result = lhs & rhs;
        break;

    case 0x33:
        result = lhs ^ rhs;
        break;

    default:
        return false;
    }

    registers_.Write64(
        reg,
        result);

    SetLogicFlags64(result);

    return true;
}
bool Cpu::DecodeTest64(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    if (!rex.w) {
        return false;
    }

    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    const std::uint64_t lhs =
        registers_.Read64(reg);

    std::uint64_t rhs = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&rhs),
                sizeof(rhs))) {
            return false;
        }
    } else {
        rhs =
            registers_.Read64(rm);
    }

    SetLogicFlags64(lhs & rhs);
    return true;
}

bool Cpu::DecodeLea64(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    
    
    

    if (!rex.w) {
        return false;
    }

    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {

        return false;
    }

    
    
    if (!memory) {
        return false;
    }

    registers_.Write64(
        reg,
        address);

    std::cout
        << "[CPU] LEA r64["
        << static_cast<unsigned>(reg)
        << "] = 0x"
        << std::hex
        << address
        << std::dec
        << '\n';

    return true;
}
bool Cpu::DecodeShiftLeft64Imm(
    std::uint8_t modrm,
    const RexPrefix& rex,
    std::uint8_t count,
    bool fetchCountAfterAddress)
{
    if (!rex.w) {
        return false;
    }

    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    if (fetchCountAfterAddress) {
        // Ordre x86 correct : [ModRM][SIB][disp][imm8].
        // L'immediat n'est lu qu'une fois le SIB/deplacement
        // eventuel deja consomme par DecodeMemoryOrRegister32.
        if (!Fetch8(count)) {
            return false;
        }
    }

    const std::uint8_t group =
        static_cast<std::uint8_t>(
            (modrm >> 3) & 0x07);

    if (group != 4 &&
        group != 5 &&
        group != 7) {
        return false;
    }

    const std::uint8_t shift =
        static_cast<std::uint8_t>(count & 0x3F);

    if (shift == 0) {
        return true;
    }

    std::uint64_t value = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&value),
                sizeof(value))) {
            return false;
        }
    }
    else {
        value = registers_.Read64(rm);
    }

    bool carry = false;
    std::uint64_t result = value;

    if (group == 4) {
        carry =
            ((value >> (64 - shift)) & 1ULL) != 0;

        result =
            value << shift;
    }
    else if (group == 5) {
        carry =
            ((value >> (shift - 1)) & 1ULL) != 0;

        result =
            value >> shift;
    }
    else {
        carry =
            ((value >> (shift - 1)) & 1ULL) != 0;

        const std::int64_t signedValue =
            static_cast<std::int64_t>(value);

        result =
            static_cast<std::uint64_t>(
                signedValue >> shift);
    }

    if (memory) {
        if (!WriteMemory(
                address,
                reinterpret_cast<const std::uint8_t*>(&result),
                sizeof(result))) {
            return false;
        }
    }
    else {
        registers_.Write64(rm, result);
    }

    SetZeroFlag(result == 0);
    SetSignFlag(
        (result & 0x8000000000000000ULL) != 0);

    if (carry) {
        rflags_ |= CF_MASK;
    }
    else {
        rflags_ &= ~CF_MASK;
    }

    if (shift == 1) {
    bool overflow = false;

    if (group == 4) {
        overflow =
            ((result >> 63) & 1ULL) !=
            (carry ? 1ULL : 0ULL);
    }
    else if (group == 5) {
        overflow =
            ((value >> 63) & 1ULL) != 0;
    }
    else {
        
        overflow = false;
    }

    if (overflow)
        rflags_ |= OF_MASK;
    else
        rflags_ &= ~OF_MASK;
}
else {
    rflags_ &= ~OF_MASK;
}

    return true;
}
bool Cpu::DecodeShiftLeft32Imm(
    std::uint8_t modrm,
    const RexPrefix& rex,
    std::uint8_t count,
    bool fetchCountAfterAddress)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    if (fetchCountAfterAddress) {
        if (!Fetch8(count)) {
            return false;
        }
    }

    const std::uint8_t group =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);

    if (group != 4) {
        return false;
    }

    const std::uint8_t shift =
        static_cast<std::uint8_t>(count & 0x1F);

    if (shift == 0) {
        return true;
    }

    std::uint32_t value = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&value),
                sizeof(value))) {
            return false;
        }
    }
    else {
        value = registers_.Read32(rm);
    }

    const bool carry =
        ((value >> (32 - shift)) & 1U) != 0;

    const std::uint32_t result =
        value << shift;

    if (memory) {
        if (!WriteMemory(
                address,
                reinterpret_cast<const std::uint8_t*>(&result),
                sizeof(result))) {
            return false;
        }
    }
    else {
        registers_.Write32(rm, result);
    }

    SetZeroFlag(result == 0);
    SetSignFlag((result & 0x80000000U) != 0);

    if (carry)
        rflags_ |= CF_MASK;
    else
        rflags_ &= ~CF_MASK;

    if (shift == 1) {
        const bool overflow =
            ((result >> 31) & 1U) !=
            (carry ? 1U : 0U);

        if (overflow)
            rflags_ |= OF_MASK;
        else
            rflags_ &= ~OF_MASK;
    }
    else {
        rflags_ &= ~OF_MASK;
    }

    return true;
}

bool Cpu::DecodeShiftRight32Imm(
    std::uint8_t modrm,
    const RexPrefix& rex,
    std::uint8_t count,
    bool fetchCountAfterAddress)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    if (fetchCountAfterAddress) {
        if (!Fetch8(count)) {
            return false;
        }
    }

    const std::uint8_t group =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);

    if (group != 5) {
        return false;
    }

    const std::uint8_t shift =
        static_cast<std::uint8_t>(count & 0x1F);

    if (shift == 0) {
        return true;
    }

    std::uint32_t value = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&value),
                sizeof(value))) {
            return false;
        }
    }
    else {
        value = registers_.Read32(rm);
    }

    const bool carry =
        ((value >> (shift - 1)) & 1U) != 0;

    const std::uint32_t result =
        value >> shift;

    if (memory) {
        if (!WriteMemory(
                address,
                reinterpret_cast<const std::uint8_t*>(&result),
                sizeof(result))) {
            return false;
        }
    }
    else {
        registers_.Write32(rm, result);
    }

    SetZeroFlag(result == 0);
    SetSignFlag((result & 0x80000000U) != 0);

    if (carry)
        rflags_ |= CF_MASK;
    else
        rflags_ &= ~CF_MASK;

    if (shift == 1) {
        const bool overflow =
            ((value >> 31) & 1U) != 0;

        if (overflow)
            rflags_ |= OF_MASK;
        else
            rflags_ &= ~OF_MASK;
    }
    else {
        rflags_ &= ~OF_MASK;
    }

    return true;
}

bool Cpu::DecodeShiftArithmetic32Imm(
    std::uint8_t modrm,
    const RexPrefix& rex,
    std::uint8_t count,
    bool fetchCountAfterAddress)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    if (fetchCountAfterAddress) {
        if (!Fetch8(count)) {
            return false;
        }
    }

    const std::uint8_t group =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);

    if (group != 7) {
        return false;
    }

    const std::uint8_t shift =
        static_cast<std::uint8_t>(count & 0x1F);

    if (shift == 0) {
        return true;
    }

    std::uint32_t value = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&value),
                sizeof(value))) {
            return false;
        }
    }
    else {
        value = registers_.Read32(rm);
    }

    const bool carry =
        ((value >> (shift - 1)) & 1U) != 0;

    const std::int32_t signedValue =
        static_cast<std::int32_t>(value);

    const std::uint32_t result =
        static_cast<std::uint32_t>(
            signedValue >> shift);

    if (memory) {
        if (!WriteMemory(
                address,
                reinterpret_cast<const std::uint8_t*>(&result),
                sizeof(result))) {
            return false;
        }
    }
    else {
        registers_.Write32(rm, result);
    }

    SetZeroFlag(result == 0);
    SetSignFlag((result & 0x80000000U) != 0);

    if (carry)
        rflags_ |= CF_MASK;
    else
        rflags_ &= ~CF_MASK;

    
    rflags_ &= ~OF_MASK;

    return true;
}

bool Cpu::DecodeRotate64Imm(
    std::uint8_t modrm,
    const RexPrefix& rex,
    std::uint8_t count,
    bool fetchCountAfterAddress)
{
    if (!rex.w) {
        return false;
    }

    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    if (fetchCountAfterAddress) {
        if (!Fetch8(count)) {
            return false;
        }
    }

    const std::uint8_t group =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);

    if (group != 0 && group != 1) {
        return false;
    }

    const std::uint8_t rotate =
        static_cast<std::uint8_t>(count & 0x3F);

    if (rotate == 0) {
        return true;
    }

    std::uint64_t value = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&value),
                sizeof(value))) {
            return false;
        }
    }
    else {
        value = registers_.Read64(rm);
    }

    std::uint64_t result = 0;
    bool carry = false;

    if (group == 0) {
        // ROL
        result =
            (value << rotate) |
            (value >> (64 - rotate));

        carry = (result & 1ULL) != 0;
    }
    else {
        // ROR
        result =
            (value >> rotate) |
            (value << (64 - rotate));

        carry = ((result >> 63) & 1ULL) != 0;
    }

    if (memory) {
        if (!WriteMemory(
                address,
                reinterpret_cast<const std::uint8_t*>(&result),
                sizeof(result))) {
            return false;
        }
    }
    else {
        registers_.Write64(rm, result);
    }

    if (carry) {
        rflags_ |= CF_MASK;
    }
    else {
        rflags_ &= ~CF_MASK;
    }

    if (rotate == 1) {
        bool overflow = false;

        if (group == 0) {
            overflow =
                ((result >> 63) & 1ULL) !=
                (carry ? 1ULL : 0ULL);
        }
        else {
            overflow =
                ((result >> 63) & 1ULL) !=
                ((result >> 62) & 1ULL);
        }

        if (overflow) {
            rflags_ |= OF_MASK;
        }
        else {
            rflags_ &= ~OF_MASK;
        }
    }
    else {
        rflags_ &= ~OF_MASK;
    }

    // ROL/ROR ne modifient ni ZF ni SF (contrairement aux
    // decalages) : conformes au comportement x86 reel.

    return true;
}

bool Cpu::DecodeRotate32Imm(
    std::uint8_t modrm,
    const RexPrefix& rex,
    std::uint8_t count,
    bool fetchCountAfterAddress)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    if (fetchCountAfterAddress) {
        if (!Fetch8(count)) {
            return false;
        }
    }

    const std::uint8_t group =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);

    if (group != 0 && group != 1) {
        return false;
    }

    const std::uint8_t rotate =
        static_cast<std::uint8_t>(count & 0x1F);

    if (rotate == 0) {
        return true;
    }

    std::uint32_t value = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&value),
                sizeof(value))) {
            return false;
        }
    }
    else {
        value = registers_.Read32(rm);
    }

    std::uint32_t result = 0;
    bool carry = false;

    if (group == 0) {
        // ROL
        result =
            (value << rotate) |
            (value >> (32 - rotate));

        carry = (result & 1U) != 0;
    }
    else {
        // ROR
        result =
            (value >> rotate) |
            (value << (32 - rotate));

        carry = ((result >> 31) & 1U) != 0;
    }

    if (memory) {
        if (!WriteMemory(
                address,
                reinterpret_cast<const std::uint8_t*>(&result),
                sizeof(result))) {
            return false;
        }
    }
    else {
        registers_.Write32(rm, result);
    }

    if (carry) {
        rflags_ |= CF_MASK;
    }
    else {
        rflags_ &= ~CF_MASK;
    }

    if (rotate == 1) {
        bool overflow = false;

        if (group == 0) {
            overflow =
                ((result >> 31) & 1U) !=
                (carry ? 1U : 0U);
        }
        else {
            overflow =
                ((result >> 31) & 1U) !=
                ((result >> 30) & 1U);
        }

        if (overflow) {
            rflags_ |= OF_MASK;
        }
        else {
            rflags_ &= ~OF_MASK;
        }
    }
    else {
        rflags_ &= ~OF_MASK;
    }

    return true;
}

bool Cpu::DecodeXchg(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    if (rex.w) {

        const std::uint64_t regValue =
            registers_.Read64(reg);

        std::uint64_t otherValue = 0;

        if (memory) {
            if (!ReadMemory(
                    address,
                    reinterpret_cast<std::uint8_t*>(&otherValue),
                    sizeof(otherValue))) {
                return false;
            }

            if (!WriteMemory(
                    address,
                    reinterpret_cast<const std::uint8_t*>(&regValue),
                    sizeof(regValue))) {
                return false;
            }
        }
        else {
            otherValue = registers_.Read64(rm);
            registers_.Write64(rm, regValue);
        }

        registers_.Write64(reg, otherValue);
    }
    else {

        const std::uint32_t regValue =
            registers_.Read32(reg);

        std::uint32_t otherValue = 0;

        if (memory) {
            if (!ReadMemory(
                    address,
                    reinterpret_cast<std::uint8_t*>(&otherValue),
                    sizeof(otherValue))) {
                return false;
            }

            if (!WriteMemory(
                    address,
                    reinterpret_cast<const std::uint8_t*>(&regValue),
                    sizeof(regValue))) {
                return false;
            }
        }
        else {
            otherValue = registers_.Read32(rm);
            registers_.Write32(rm, regValue);
        }

        registers_.Write32(reg, otherValue);
    }

    return true;
}

bool Cpu::DecodeMemoryOrRegister8(
    std::uint8_t modrm,
    const RexPrefix& rex,
    std::uint8_t& regIndex,
    bool& regHighByte,
    std::uint8_t& rmRegisterIndex,
    bool& rmHighByte,
    std::uint64_t& address,
    bool& memory)
{
    const std::uint8_t mod =
        static_cast<std::uint8_t>((modrm >> 6) & 0x03);

    const std::uint8_t regField =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);

    const std::uint8_t rmField =
        static_cast<std::uint8_t>(modrm & 0x07);

    const bool hasRex =
        rex.present;

    // Champ "reg" : AL/CL/DL/BL/AH/CH/DH/BH sans REX,
    // ou R8B..R15B / SPL/BPL/SIL/DIL des que REX est present
    // (le REX supprime l'acces aux octets hauts AH/CH/DH/BH).
    regIndex = regField;
    regHighByte = false;

    if (hasRex) {
        if (rex.r) {
            regIndex = static_cast<std::uint8_t>(regField + 8);
        }
    }
    else if (regField >= 4) {
        regIndex = static_cast<std::uint8_t>(regField - 4);
        regHighByte = true;
    }

    if (mod == 0x03) {

        memory = false;

        rmRegisterIndex = rmField;
        rmHighByte = false;

        if (hasRex) {
            if (rex.b) {
                rmRegisterIndex =
                    static_cast<std::uint8_t>(rmField + 8);
            }
        }
        else if (rmField >= 4) {
            rmRegisterIndex =
                static_cast<std::uint8_t>(rmField - 4);

            rmHighByte = true;
        }

        return true;
    }

    std::uint8_t reg32 = 0;
    std::uint8_t rm32 = 0;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg32,
            rm32,
            address,
            memory)) {
        return false;
    }

    return memory;
}

std::uint8_t Cpu::ReadReg8(
    std::uint8_t registerIndex,
    bool highByte) const noexcept
{
    const std::uint64_t value =
        registers_.Read64(registerIndex);

    if (highByte) {
        return static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    }

    return static_cast<std::uint8_t>(value & 0xFFU);
}

void Cpu::WriteReg8(
    std::uint8_t registerIndex,
    bool highByte,
    std::uint8_t value) noexcept
{
    const std::uint64_t oldValue =
        registers_.Read64(registerIndex);

    std::uint64_t newValue = oldValue;

    if (highByte) {
        newValue =
            (oldValue & ~(0xFFULL << 8)) |
            (static_cast<std::uint64_t>(value) << 8);
    }
    else {
        newValue =
            (oldValue & ~0xFFULL) |
            static_cast<std::uint64_t>(value);
    }

    registers_.Write64(registerIndex, newValue);
}

bool Cpu::DecodeXchg8(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t regIndex = 0;
    bool regHighByte = false;
    std::uint8_t rmRegisterIndex = 0;
    bool rmHighByte = false;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister8(
            modrm,
            rex,
            regIndex,
            regHighByte,
            rmRegisterIndex,
            rmHighByte,
            address,
            memory)) {
        return false;
    }

    const std::uint8_t regValue =
        ReadReg8(regIndex, regHighByte);

    std::uint8_t otherValue = 0;

    if (memory) {

        if (!ReadMemory(
                address,
                &otherValue,
                sizeof(otherValue))) {
            return false;
        }

        if (!WriteMemory(
                address,
                &regValue,
                sizeof(regValue))) {
            return false;
        }
    }
    else {

        otherValue =
            ReadReg8(rmRegisterIndex, rmHighByte);

        WriteReg8(
            rmRegisterIndex,
            rmHighByte,
            regValue);
    }

    WriteReg8(regIndex, regHighByte, otherValue);

    // XCHG ne modifie aucun flag (verifie explicitement par les tests).
    return true;
}

bool Cpu::DecodeTest8(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    std::uint8_t regIndex = 0;
    bool regHighByte = false;
    std::uint8_t rmRegisterIndex = 0;
    bool rmHighByte = false;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister8(
            modrm,
            rex,
            regIndex,
            regHighByte,
            rmRegisterIndex,
            rmHighByte,
            address,
            memory)) {
        return false;
    }

    // TEST r/m8, imm8 : seul /0 est defini. L'immediat n'est lu
    // qu'apres decodage complet de l'adresse (SIB/deplacement
    // deja consommes ci-dessus par DecodeMemoryOrRegister8).
    std::uint8_t imm8 = 0;

    if (!Fetch8(imm8)) {
        return false;
    }

    std::uint8_t value = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                &value,
                sizeof(value))) {
            return false;
        }
    }
    else {
        value = ReadReg8(rmRegisterIndex, rmHighByte);
    }

    const std::uint8_t result =
        static_cast<std::uint8_t>(value & imm8);

    SetZeroFlag(result == 0);
    SetSignFlag((result & 0x80U) != 0);
    rflags_ &= ~(CF_MASK | OF_MASK);

    // L'operande n'est jamais modifie (TEST, contrairement a AND).
    return true;
}

bool Cpu::DecodeShiftLeft32CL(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    const std::uint8_t count =
        static_cast<std::uint8_t>(
            registers_.Read32(1) & 0xFFU);

    return DecodeShiftLeft32Imm(
        modrm,
        rex,
        count);
}

bool Cpu::DecodeShiftRight32CL(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    const std::uint8_t count =
        static_cast<std::uint8_t>(
            registers_.Read32(1) & 0xFFU);

    return DecodeShiftRight32Imm(
        modrm,
        rex,
        count);
}

bool Cpu::DecodeShiftArithmetic32CL(
    std::uint8_t modrm,
    const RexPrefix& rex)
{
    const std::uint8_t count =
        static_cast<std::uint8_t>(
            registers_.Read32(1) & 0xFFU);

    return DecodeShiftArithmetic32Imm(
        modrm,
        rex,
        count);
}
int Cpu::Run()
{
    if (memory_ == nullptr) {
        std::cerr << "[CPU] No memory connected.\n";
        return 1;
    }

    constexpr std::uint8_t MOV_R32_IMM32_BASE = 0xB8;

    constexpr std::uint8_t ADD_RM32_R32 = 0x01;
    constexpr std::uint8_t ADD_R32_RM32 = 0x03;

    constexpr std::uint8_t SUB_RM32_R32 = 0x29;
    constexpr std::uint8_t SUB_R32_RM32 = 0x2B;

    constexpr std::uint8_t CMP_RM32_R32 = 0x39;
    constexpr std::uint8_t CMP_R32_RM32 = 0x3B;

    constexpr std::uint8_t AND_R32_RM32 = 0x23;
    constexpr std::uint8_t OR_R32_RM32 = 0x0B;
    constexpr std::uint8_t XOR_R32_RM32 = 0x33;
    constexpr std::uint8_t TEST_RM32_R32 = 0x85;

    constexpr std::uint8_t MOV_RM32_R32 = 0x89;
    constexpr std::uint8_t MOV_R32_RM32 = 0x8B;

    constexpr std::uint8_t JZ_REL8 = 0x74;
    constexpr std::uint8_t JNZ_REL8 = 0x75;

    constexpr std::uint8_t JMP_REL8 = 0xEB;
    constexpr std::uint8_t JMP_REL32 = 0xE9;

    constexpr std::uint8_t CALL_REL32 = 0xE8;

    constexpr std::uint8_t PUSH_R64_BASE = 0x50;
    constexpr std::uint8_t POP_R64_BASE = 0x58;

    constexpr std::uint8_t RET = 0xC3;

    halted_ = false;

    bool running = true;
    std::uint32_t call_depth = 0;

    std::uint64_t instruction_counter = 0;

    
    constexpr std::uint64_t FRAME_CALLBACK_INTERVAL = 30000;

    while (running && !halted_) {

        const std::uint64_t instruction_address =
            instruction_pointer_;
        current_instruction_ip_ = instruction_address;

        std::uint8_t opcode = 0;

        if (!Fetch8(opcode)) {
            std::cerr
                << "[CPU] Fetch failed at 0x"
                << std::hex
                << instruction_address
                << std::dec
                << '\n';
            return 1;
        }

        RexPrefix rex{};

        if (opcode >= 0x40 && opcode <= 0x4F) {
            rex = DecodeRex(opcode);

            if (!Fetch8(opcode)) {
                return 1;
            }
        }

        switch (opcode) {
        
        
        

        case 0xCF: {
            // In 64-bit mode, IRETQ requires a 64-bit operand size (REX.W).
            if (!rex.w || !exception_return_handler_) {
                return 1;
            }

            if (!exception_return_handler_(*this)) {
                return 1;
            }

            break;
        }

        case 0x0F: {
            
            
            
            
            

            std::uint8_t opcode2 = 0;

            if (!Fetch8(opcode2)) {
                return 1;
            }
            if (opcode2 == 0x05) {

                if (syscall_handler_) {
                    if (!syscall_handler_(*this)) {
                        return 1;
                    }
                }
                else if (ps5_mode_) {
                    // Pas de handler systeme fourni par l'hote :
                    // repli sur les services PS5 internes, invoques
                    // via le mecanisme SYSCALL standard x86-64
                    // (numero de service dans RAX, comme un vrai
                    // syscall Linux/BSD).
                    if (!DecodePS5Instructions()) {
                        return 1;
                    }
                }
                else {
                    return 1;
                }

                break;
            }
            if (opcode2 == 0xB6) {
                std::uint8_t modrmMovzx = 0;

                if (!Fetch8(modrmMovzx)) {
                    return 1;
                }

                if (rex.w) {
                    if (!DecodeMovzx64Reg32(modrmMovzx, rex)) {
                        return 1;
                    }
                } else {
                    if (!DecodeMovzx32Reg32(modrmMovzx, rex)) {
                        return 1;
                    }
                }

                break;
            }

            if (opcode2 == 0xB7) {
                std::uint8_t modrmMovzx16 = 0;

                if (!Fetch8(modrmMovzx16)) {
                    return 1;
                }

                if (!DecodeMovzx32Reg16(modrmMovzx16, rex)) {
                    return 1;
                }

                break;
            }

            if (opcode2 == 0xBF) {
                std::uint8_t modrmMovsx16 = 0;

                if (!Fetch8(modrmMovsx16)) {
                    return 1;
                }

                if (rex.w) {
                    if (!DecodeMovsx64Reg16(modrmMovsx16, rex)) {
                        return 1;
                    }
                } else {
                    if (!DecodeMovsx32Reg16(modrmMovsx16, rex)) {
                        return 1;
                    }
                }

                break;
            }
            if (opcode2 == 0xBE) {
                std::uint8_t modrmMovsx = 0;

                if (!Fetch8(modrmMovsx)) {
                    return 1;
                }

                if (!DecodeMovsx32Reg8(modrmMovsx, rex)) {
                    return 1;
                }

                break;
            }
            if (opcode2 == 0x84) {
                
                std::int32_t rel = 0;

                if (!FetchRel32(rel)) {
                    return 1;
                }if (ZeroFlag()) {
                    instruction_pointer_ =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                instruction_pointer_) + rel);
                }

                break;
            }
            if (opcode2 == 0x85) {

                std::int32_t rel = 0;

                if (!FetchRel32(rel)) {
                    return 1;
                }

                if (!ZeroFlag()) {
                    instruction_pointer_ =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                instruction_pointer_) + rel);
                }

                break;
            }
            if (opcode2 == 0x80 || opcode2 == 0x81) {
                std::int32_t rel=0; if(!FetchRel32(rel)) return 1;
                const std::uint8_t cc=static_cast<std::uint8_t>(opcode2-0x80);
                if(ConditionHolds(cc,rflags_)) instruction_pointer_=static_cast<std::uint64_t>(static_cast<std::int64_t>(instruction_pointer_)+rel);
                break;
            }
            if (opcode2 == 0x82) {

                std::int32_t rel = 0;

                if (!FetchRel32(rel)) {
                    return 1;
                }

                const bool cf =
                    (rflags_ & CF_MASK) != 0;

                if (cf) {
                    instruction_pointer_ =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                instruction_pointer_) + rel);
                }

                break;
            }
            if (opcode2 == 0x83) {

                std::int32_t rel = 0;

                if (!FetchRel32(rel)) {
                    return 1;
                }

                const bool cf =
                    (rflags_ & CF_MASK) != 0;

                if (!cf) {
                    instruction_pointer_ =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                instruction_pointer_) + rel);
                }

                break;
            }
            if (opcode2 == 0x86) {

                std::int32_t rel = 0;

                if (!FetchRel32(rel)) {
                    return 1;
                }

                const bool cf =
                    (rflags_ & CF_MASK) != 0;

                if (cf || ZeroFlag()) {
                    instruction_pointer_ =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                instruction_pointer_) + rel);
                }

                break;
            }
            if (opcode2 == 0x87) {

                std::int32_t rel = 0;

                if (!FetchRel32(rel)) {
                    return 1;
                }

                const bool cf =
                    (rflags_ & CF_MASK) != 0;

                if (!cf && !ZeroFlag()) {
                    instruction_pointer_ =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                instruction_pointer_) + rel);
                }

                break;
            }
            if (opcode2 == 0x8C) {

                std::int32_t rel = 0;

                if (!FetchRel32(rel)) {
                    return 1;
                }

                const bool sf = SignFlag();
                const bool of = (rflags_ & OF_MASK) != 0;

                if (sf != of) {
                    instruction_pointer_ =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                instruction_pointer_) + rel);
                }

                break;
            }
            if (opcode2 == 0x8D) {

                std::int32_t rel = 0;

                if (!FetchRel32(rel)) {
                    return 1;
                }

                const bool sf = SignFlag();
                const bool of = (rflags_ & OF_MASK) != 0;

                if (sf == of) {
                    instruction_pointer_ =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                instruction_pointer_) + rel);
                }

                break;
            }
            if (opcode2 == 0x8E) {

                std::int32_t rel = 0;

                if (!FetchRel32(rel)) {
                    return 1;
                }

                const bool sf = SignFlag();
                const bool of = (rflags_ & OF_MASK) != 0;

                if (ZeroFlag() || (sf != of)) {
                    instruction_pointer_ =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                instruction_pointer_) + rel);
                }

                break;
            }
            if (opcode2 == 0x8F) {

                std::int32_t rel = 0;

                if (!FetchRel32(rel)) {
                    return 1;
                }

                const bool sf = SignFlag();
                const bool of = (rflags_ & OF_MASK) != 0;

                if (!ZeroFlag() && (sf == of)) {
                    instruction_pointer_ =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                instruction_pointer_) + rel);
                }

                break;
            }
            if (
                opcode2 == 0x90 || opcode2 == 0x91 ||
                opcode2 == 0x92 || opcode2 == 0x93 ||
                opcode2 == 0x94 || opcode2 == 0x95 ||
                opcode2 == 0x96 || opcode2 == 0x97 ||
                opcode2 == 0x98 || opcode2 == 0x99 ||
                opcode2 == 0x9A || opcode2 == 0x9B ||
                opcode2 == 0x9C || opcode2 == 0x9D ||
                opcode2 == 0x9E || opcode2 == 0x9F) {
                std::uint8_t modrmSetcc = 0;
                if (!Fetch8(modrmSetcc)) return 1;
                const std::uint8_t mod = static_cast<std::uint8_t>((modrmSetcc >> 6) & 0x03);
                if (mod == 0x03) {
                    const std::uint8_t rawRm = static_cast<std::uint8_t>(modrmSetcc & 0x07);
                    std::uint8_t rm = static_cast<std::uint8_t>(rawRm | (rex.b ? 8 : 0));
                    const bool highByte = !rex.present && rawRm >= 4;
                    if (highByte) rm = static_cast<std::uint8_t>(rawRm - 4);
                    const bool cf = (rflags_ & CF_MASK) != 0;
                    const bool zf = ZeroFlag();
                    const bool sf = SignFlag();
                    const bool of = (rflags_ & OF_MASK) != 0;
                    const bool pf = (rflags_ & PF_MASK) != 0;
                    bool condition = false;
                    switch (opcode2) {
                    case 0x90: condition = of; break;
                    case 0x91: condition = !of; break;
                    case 0x92: condition = cf; break;
                    case 0x93: condition = !cf; break;
                    case 0x94: condition = zf; break;
                    case 0x95: condition = !zf; break;
                    case 0x96: condition = cf || zf; break;
                    case 0x97: condition = !cf && !zf; break;
                    case 0x98: condition = sf; break;
                    case 0x99: condition = !sf; break;
                    case 0x9A: condition = pf; break;
                    case 0x9B: condition = !pf; break;
                    case 0x9C: condition = sf != of; break;
                    case 0x9D: condition = sf == of; break;
                    case 0x9E: condition = zf || (sf != of); break;
                    case 0x9F: condition = !zf && (sf == of); break;
                    }
                    const std::uint8_t value = condition ? 1U : 0U;
                    if (highByte) {
                        std::uint64_t oldValue = registers_.Read64(rm);
                        oldValue = (oldValue & ~(0xFFULL << 8)) |
                                   (static_cast<std::uint64_t>(value) << 8);
                        registers_.Write64(rm, oldValue);
                    } else {
                        std::uint64_t oldValue = registers_.Read64(rm);
                        oldValue = (oldValue & ~0xFFULL) | value;
                        registers_.Write64(rm, oldValue);
                    }
                    break;
                }
                std::uint8_t reg = 0;
                std::uint8_t rm = 0;
                std::uint64_t address = 0;
                bool memory = false;
                if (!DecodeMemoryOrRegister32(modrmSetcc, rex, reg, rm, address, memory) || !memory) return 1;
                const bool cf = (rflags_ & CF_MASK) != 0;
                const bool zf = ZeroFlag();
                const bool sf = SignFlag();
                const bool of = (rflags_ & OF_MASK) != 0;
                const bool pf = (rflags_ & PF_MASK) != 0;
                bool condition = false;
                switch (opcode2) {
                case 0x90: condition = of; break; case 0x91: condition = !of; break;
                case 0x92: condition = cf; break; case 0x93: condition = !cf; break;
                case 0x94: condition = zf; break; case 0x95: condition = !zf; break;
                case 0x96: condition = cf || zf; break; case 0x97: condition = !cf && !zf; break;
                case 0x98: condition = sf; break; case 0x99: condition = !sf; break;
                case 0x9A: condition = pf; break; case 0x9B: condition = !pf; break;
                case 0x9C: condition = sf != of; break; case 0x9D: condition = sf == of; break;
                case 0x9E: condition = zf || (sf != of); break; case 0x9F: condition = !zf && (sf == of); break;
                }
                const std::uint8_t value = condition ? 1U : 0U;
                if (!WriteMemory(address, &value, sizeof(value))) return 1;
                break;
            }
            if (opcode2 == 0x0B) {
                RaiseException({
                    CpuExceptionKind::InvalidOpcode,
                    instruction_address,
                    MemoryFault::None,
                    CpuExceptionVector::InvalidOpcode
                });
                return 1;
            }

            if (opcode2 != 0xAF) {
                RaiseException({
                    CpuExceptionKind::InvalidOpcode,
                    instruction_address,
                    MemoryFault::None,
                    CpuExceptionVector::InvalidOpcode
                });
                return 1;
            }

            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            std::uint8_t reg = 0;
            std::uint8_t rm = 0;
            std::uint64_t address = 0;
            bool memory = false;

            if (!DecodeMemoryOrRegister32(
                    modrm,
                    rex,
                    reg,
                    rm,
                    address,
                    memory)) {
                return 1;
            }

            if (rex.w) {

                const std::int64_t lhs =
                    static_cast<std::int64_t>(
                        registers_.Read64(reg));

                std::uint64_t rhs_bits = 0;

                if (memory) {
                    if (!ReadMemory(
                            address,
                            reinterpret_cast<std::uint8_t*>(&rhs_bits),
                            sizeof(rhs_bits))) {
                        return 1;
                    }
                } else {
                    rhs_bits = registers_.Read64(rm);
                }

                const std::int64_t rhs =
                    static_cast<std::int64_t>(rhs_bits);

                
                
                const std::uint64_t result =
                    registers_.Read64(reg) *
                    rhs_bits;

                registers_.Write64(
                    reg,
                    result);

                
                
                bool overflow = false;

                if (lhs > 0) {
                    if (rhs > 0) {
                        overflow =
                            lhs >
                            (std::numeric_limits<std::int64_t>::max() / rhs);
                    } else if (rhs < 0) {
                        overflow =
                            rhs <
                            (std::numeric_limits<std::int64_t>::min() / lhs);
                    }
                } else if (lhs < 0) {
                    if (rhs > 0) {
                        overflow =
                            lhs <
                            (std::numeric_limits<std::int64_t>::min() / rhs);
                    } else if (rhs < 0) {
                        overflow =
                            lhs <
                            (std::numeric_limits<std::int64_t>::max() / rhs);
                    }
                }

                if (overflow) {
                    rflags_ |= CF_MASK;
                    rflags_ |= OF_MASK;
                } else {
                    rflags_ &= ~CF_MASK;
                    rflags_ &= ~OF_MASK;
                }

            } else {

                const std::int32_t lhs =
                    static_cast<std::int32_t>(
                        registers_.Read32(reg));

                std::uint32_t rhs_bits = 0;

                if (memory) {
                    if (!ReadMemory(
                            address,
                            reinterpret_cast<std::uint8_t*>(&rhs_bits),
                            sizeof(rhs_bits))) {
                        return 1;
                    }
                } else {
                    rhs_bits = registers_.Read32(rm);
                }

                const std::int32_t rhs =
                    static_cast<std::int32_t>(rhs_bits);

                const std::uint32_t result =
                    registers_.Read32(reg) *
                    rhs_bits;

                registers_.Write32(
                    reg,
                    result);

                bool overflow = false;

                if (lhs > 0) {
                    if (rhs > 0) {
                        overflow =
                            lhs >
                            (std::numeric_limits<std::int32_t>::max() / rhs);
                    } else if (rhs < 0) {
                        overflow =
                            rhs <
                            (std::numeric_limits<std::int32_t>::min() / lhs);
                    }
                } else if (lhs < 0) {
                    if (rhs > 0) {
                        overflow =
                            lhs <
                            (std::numeric_limits<std::int32_t>::min() / rhs);
                    } else if (rhs < 0) {
                        overflow =
                            lhs <
                            (std::numeric_limits<std::int32_t>::max() / rhs);
                    }
                }

                if (overflow) {
                    rflags_ |= CF_MASK;
                    rflags_ |= OF_MASK;
                } else {
                    rflags_ &= ~CF_MASK;
                    rflags_ &= ~OF_MASK;
                }
            }

            break;
        }
        case 0x14:
        case 0x1C: {
            std::uint8_t immediate = 0;
            if (!Fetch8(immediate)) return 1;

            const std::uint8_t lhs = ReadReg8(0, false);
            const bool cfIn = (rflags_ & CF_MASK) != 0;
            const std::uint8_t result =
                opcode == 0x14
                    ? static_cast<std::uint8_t>(lhs + immediate + (cfIn ? 1U : 0U))
                    : static_cast<std::uint8_t>(lhs - immediate - (cfIn ? 1U : 0U));

            WriteReg8(0, false, result);

            const bool cfOut =
                opcode == 0x14
                    ? (lhs > static_cast<std::uint8_t>(0xFFU - immediate) ||
                       (cfIn && lhs == static_cast<std::uint8_t>(0xFFU - immediate)))
                    : (lhs < immediate || (cfIn && lhs == immediate));
            const bool of =
                opcode == 0x14
                    ? ((~(lhs ^ immediate) & (lhs ^ result) & 0x80U) != 0)
                    : (((lhs ^ immediate) & (lhs ^ result) & 0x80U) != 0);

            if (cfOut) rflags_ |= CF_MASK; else rflags_ &= ~CF_MASK;
            SetZeroFlag(result == 0);
            SetSignFlag((result & 0x80U) != 0);
            if (of) rflags_ |= OF_MASK; else rflags_ &= ~OF_MASK;
            if (((lhs ^ immediate ^ result) & 0x10U) != 0) rflags_ |= AF_MASK; else rflags_ &= ~AF_MASK;
            if (EvenParity8(result)) rflags_ |= PF_MASK; else rflags_ &= ~PF_MASK;
            break;
        }

        case 0x15:
        case 0x1D: {
            std::uint32_t immediateRaw = 0;
            if (!Fetch32(immediateRaw)) return 1;
            const bool cfIn = (rflags_ & CF_MASK) != 0;

            if (rex.w) {
                const std::uint64_t lhs = registers_.Read64(0);
                const std::uint64_t immediate =
                    static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(immediateRaw)));
                const std::uint64_t result =
                    opcode == 0x15
                        ? lhs + immediate + (cfIn ? 1ULL : 0ULL)
                        : lhs - immediate - (cfIn ? 1ULL : 0ULL);
                registers_.Write64(0, result);
                const std::uint64_t sign = 0x8000000000000000ULL;
                const bool cfOut =
                    opcode == 0x15
                        ? (lhs > std::numeric_limits<std::uint64_t>::max() - immediate ||
                           (cfIn && lhs == std::numeric_limits<std::uint64_t>::max() - immediate))
                        : (lhs < immediate || (cfIn && lhs == immediate));
                const bool of =
                    opcode == 0x15
                        ? ((~(lhs ^ immediate) & (lhs ^ result) & sign) != 0)
                        : (((lhs ^ immediate) & (lhs ^ result) & sign) != 0);
                if (cfOut) rflags_ |= CF_MASK; else rflags_ &= ~CF_MASK;
                SetZeroFlag(result == 0);
                SetSignFlag((result & sign) != 0);
                if (of) rflags_ |= OF_MASK; else rflags_ &= ~OF_MASK;
                if (((lhs ^ immediate ^ result) & 0x10ULL) != 0) rflags_ |= AF_MASK; else rflags_ &= ~AF_MASK;
                if (EvenParity8(static_cast<std::uint8_t>(result))) rflags_ |= PF_MASK; else rflags_ &= ~PF_MASK;
            } else {
                const std::uint32_t lhs = registers_.Read32(0);
                const std::uint32_t immediate = immediateRaw;
                const std::uint32_t result =
                    opcode == 0x15
                        ? lhs + immediate + (cfIn ? 1U : 0U)
                        : lhs - immediate - (cfIn ? 1U : 0U);
                registers_.Write32(0, result);
                const std::uint32_t sign = 0x80000000U;
                const bool cfOut =
                    opcode == 0x15
                        ? (lhs > std::numeric_limits<std::uint32_t>::max() - immediate ||
                           (cfIn && lhs == std::numeric_limits<std::uint32_t>::max() - immediate))
                        : (lhs < immediate || (cfIn && lhs == immediate));
                const bool of =
                    opcode == 0x15
                        ? ((~(lhs ^ immediate) & (lhs ^ result) & sign) != 0)
                        : (((lhs ^ immediate) & (lhs ^ result) & sign) != 0);
                if (cfOut) rflags_ |= CF_MASK; else rflags_ &= ~CF_MASK;
                SetZeroFlag(result == 0);
                SetSignFlag((result & sign) != 0);
                if (of) rflags_ |= OF_MASK; else rflags_ &= ~OF_MASK;
                if (((lhs ^ immediate ^ result) & 0x10U) != 0) rflags_ |= AF_MASK; else rflags_ &= ~AF_MASK;
                if (EvenParity8(static_cast<std::uint8_t>(result))) rflags_ |= PF_MASK; else rflags_ &= ~PF_MASK;
            }
            break;
        }

        case 0x10:
        case 0x12:
        case 0x18:
        case 0x1A: {
            std::uint8_t modrm=0, reg=0, rm=0; bool regHigh=false, rmHigh=false; std::uint64_t address=0; bool memory=false;
            if(!Fetch8(modrm) || !DecodeMemoryOrRegister8(modrm,rex,reg,regHigh,rm,rmHigh,address,memory)) return 1;
            const bool isAdc=(opcode==0x10||opcode==0x12), destRm=(opcode==0x10||opcode==0x18), cfIn=(rflags_&CF_MASK)!=0;
            std::uint8_t lhs=destRm?(memory?0:ReadReg8(rm,rmHigh)):ReadReg8(reg,regHigh), rhs=destRm?ReadReg8(reg,regHigh):(memory?0:ReadReg8(rm,rmHigh));
            if(destRm&&memory){if(!ReadMemory(address,&lhs,1)) return 1;} if(!(!destRm)&&memory){if(!ReadMemory(address,&rhs,1)) return 1;}
            const std::uint8_t result=isAdc?static_cast<std::uint8_t>(lhs+rhs+(cfIn?1U:0U)):static_cast<std::uint8_t>(lhs-rhs-(cfIn?1U:0U));
            if(destRm){if(memory){if(!WriteMemory(address,&result,1)) return 1;}else WriteReg8(rm,rmHigh,result);}else WriteReg8(reg,regHigh,result);
            const bool cfOut=isAdc?(lhs>static_cast<std::uint8_t>(0xFFU-rhs)||(cfIn&&lhs==static_cast<std::uint8_t>(0xFFU-rhs))):(lhs<rhs||(cfIn&&lhs==rhs));
            const bool of=isAdc?((~(lhs^rhs)&(lhs^result)&0x80U)!=0):(((lhs^rhs)&(lhs^result)&0x80U)!=0);
            if(cfOut) rflags_|=CF_MASK; else rflags_&=~CF_MASK; SetZeroFlag(result==0); SetSignFlag((result&0x80U)!=0); if(of) rflags_|=OF_MASK; else rflags_&=~OF_MASK; if(((lhs^rhs^result)&0x10U)!=0) rflags_|=AF_MASK; else rflags_&=~AF_MASK; if(EvenParity8(result)) rflags_|=PF_MASK; else rflags_&=~PF_MASK;
            break;
        }
        case 0x11:
        case 0x13:
        case 0x19:
        case 0x1B: {
            std::uint8_t modrm=0, reg=0, rm=0; std::uint64_t address=0; bool memory=false;
            if(!Fetch8(modrm) || !DecodeMemoryOrRegister32(modrm,rex,reg,rm,address,memory)) return 1;
            const bool isAdc=(opcode==0x11||opcode==0x13);
            const bool destRm=(opcode==0x11||opcode==0x19);
            const bool cfIn=(rflags_&CF_MASK)!=0;
            if(rex.w){
                std::uint64_t lhs=destRm?(memory?0:registers_.Read64(rm)):registers_.Read64(reg);
                std::uint64_t rhs=destRm?registers_.Read64(reg):(memory?0:registers_.Read64(rm));
                if(destRm && memory){if(!ReadMemory(address,reinterpret_cast<std::uint8_t*>(&lhs),8)) return 1;}
                if(!destRm && memory){if(!ReadMemory(address,reinterpret_cast<std::uint8_t*>(&rhs),8)) return 1;}
                const std::uint64_t result=isAdc?lhs+rhs+(cfIn?1ULL:0ULL):lhs-rhs-(cfIn?1ULL:0ULL);
                if(destRm){if(memory){if(!WriteMemory(address,reinterpret_cast<const std::uint8_t*>(&result),8)) return 1;}else registers_.Write64(rm,result);}else registers_.Write64(reg,result);
                const std::uint64_t sign=0x8000000000000000ULL;
                const bool cfOut=isAdc?(lhs>std::numeric_limits<std::uint64_t>::max()-rhs || (cfIn&&lhs==std::numeric_limits<std::uint64_t>::max()-rhs)):(lhs<rhs || (cfIn&&lhs==rhs));
                const bool of=isAdc?((~(lhs^rhs)&(lhs^result)&sign)!=0):(((lhs^rhs)&(lhs^result)&sign)!=0);
                if(cfOut) rflags_|=CF_MASK; else rflags_&=~CF_MASK;
                SetZeroFlag(result==0); SetSignFlag((result&sign)!=0);
                if(of) rflags_|=OF_MASK; else rflags_&=~OF_MASK;
                if(((lhs^rhs^result)&0x10ULL)!=0) rflags_|=AF_MASK; else rflags_&=~AF_MASK;
                if(EvenParity8(static_cast<std::uint8_t>(result))) rflags_|=PF_MASK; else rflags_&=~PF_MASK;
            }else{
                std::uint32_t lhs=destRm?(memory?0:registers_.Read32(rm)):registers_.Read32(reg);
                std::uint32_t rhs=destRm?registers_.Read32(reg):(memory?0:registers_.Read32(rm));
                if(destRm&&memory){if(!ReadMemory(address,reinterpret_cast<std::uint8_t*>(&lhs),4)) return 1;}
                if(!destRm&&memory){if(!ReadMemory(address,reinterpret_cast<std::uint8_t*>(&rhs),4)) return 1;}
                const std::uint32_t result=isAdc?lhs+rhs+(cfIn?1U:0U):lhs-rhs-(cfIn?1U:0U);
                if(destRm){if(memory){if(!WriteMemory(address,reinterpret_cast<const std::uint8_t*>(&result),4)) return 1;}else registers_.Write32(rm,result);}else registers_.Write32(reg,result);
                const std::uint32_t sign=0x80000000U;
                const bool cfOut=isAdc?(lhs>std::numeric_limits<std::uint32_t>::max()-rhs || (cfIn&&lhs==std::numeric_limits<std::uint32_t>::max()-rhs)):(lhs<rhs || (cfIn&&lhs==rhs));
                const bool of=isAdc?((~(lhs^rhs)&(lhs^result)&sign)!=0):(((lhs^rhs)&(lhs^result)&sign)!=0);
                if(cfOut) rflags_|=CF_MASK; else rflags_&=~CF_MASK;
                SetZeroFlag(result==0); SetSignFlag((result&sign)!=0);
                if(of) rflags_|=OF_MASK; else rflags_&=~OF_MASK;
                if(((lhs^rhs^result)&0x10U)!=0) rflags_|=AF_MASK; else rflags_&=~AF_MASK;
                if(EvenParity8(static_cast<std::uint8_t>(result))) rflags_|=PF_MASK; else rflags_&=~PF_MASK;
            }
            break;
        }
        case 0xFF: { 
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            const std::uint8_t group = (modrm >> 3) & 0x7;
            const std::uint8_t mod = (modrm >> 6) & 0x3;

            
            if (mod != 0x3) {
                return 1;
            }

            const std::uint8_t rm =
                (modrm & 0x7) | (rex.b ? 8 : 0);

            
            
            if (group != 0 && group != 1) {
                return 1;
            }

            const bool old_cf = (rflags_ & CF_MASK) != 0;

            if (rex.w) {
                const std::uint64_t value = registers_.Read64(rm);
                const std::uint64_t result =
                    (group == 0) ? value + 1 : value - 1;

                registers_.Write64(rm, result);

                SetZeroFlag(result == 0);
                SetSignFlag((result & 0x8000000000000000ULL) != 0);

                const bool overflow =
                    (group == 0)
                        ? (value == 0x7FFFFFFFFFFFFFFFULL)
                        : (value == 0x8000000000000000ULL);

                if (overflow) {
                    rflags_ |= OF_MASK;
                } else {
                    rflags_ &= ~OF_MASK;
                }
            } else {
                const std::uint32_t value = registers_.Read32(rm);
                const std::uint32_t result =
                    (group == 0) ? value + 1U : value - 1U;

                registers_.Write32(rm, result);

                SetZeroFlag(result == 0);
                SetSignFlag((result & 0x80000000U) != 0);

                const bool overflow =
                    (group == 0)
                        ? (value == 0x7FFFFFFFU)
                        : (value == 0x80000000U);

                if (overflow) {
                    rflags_ |= OF_MASK;
                } else {
                    rflags_ &= ~OF_MASK;
                }
            }

            
            if (old_cf) {
                rflags_ |= CF_MASK;
            } else {
                rflags_ &= ~CF_MASK;
            }

            break;
        }

        case 0xF6: {

            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            const std::uint8_t group =
                static_cast<std::uint8_t>((modrm >> 3) & 0x07);

            // Seul /0 (TEST r/m8, imm8) est implemente pour le
            // moment. /2 (NOT), /3 (NEG), /4 (MUL), /5 (IMUL),
            // /6 (DIV), /7 (IDIV) restent hors perimetre.
            if (group != 0) {
                std::cerr
                    << "[CPU] F6 /"
                    << static_cast<unsigned>(group)
                    << " non supporte (seul TEST r/m8,imm8 l'est)\n";
                return 1;
            }

            if (!DecodeTest8(modrm, rex)) {
                return 1;
            }

            break;
        }

        case 0xF7: {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            const std::uint8_t group =
                static_cast<std::uint8_t>((modrm >> 3) & 0x07);

            std::uint8_t reg = 0;
            std::uint8_t rm = 0;
            std::uint64_t address = 0;
            bool memory = false;

            if (!DecodeMemoryOrRegister32(
                    modrm,
                    rex,
                    reg,
                    rm,
                    address,
                    memory)) {
                return 1;
            }

            if (group == 0) {

                std::uint32_t imm32 = 0;

                if (!Fetch32(imm32)) {
                    return 1;
                }

                if (rex.w) {

                    const std::uint64_t imm64 =
                        static_cast<std::uint64_t>(
                            static_cast<std::int64_t>(
                                static_cast<std::int32_t>(
                                    imm32)));

                    std::uint64_t value = 0;

                    if (memory) {
                        if (!ReadMemory(
                                address,
                                reinterpret_cast<std::uint8_t*>(&value),
                                sizeof(value))) {
                            return 1;
                        }
                    } else {
                        value = registers_.Read64(rm);
                    }

                    const std::uint64_t result =
                        value & imm64;

                    SetZeroFlag(result == 0);
                    SetSignFlag(
                        (result & 0x8000000000000000ULL) != 0);
                    rflags_ &= ~(CF_MASK | OF_MASK);

                } else {

                    std::uint32_t value = 0;

                    if (memory) {
                        if (!ReadMemory(
                                address,
                                reinterpret_cast<std::uint8_t*>(&value),
                                sizeof(value))) {
                            return 1;
                        }
                    } else {
                        value = registers_.Read32(rm);
                    }

                    const std::uint32_t result =
                        value & imm32;

                    SetZeroFlag(result == 0);
                    SetSignFlag((result & 0x80000000U) != 0);
                    rflags_ &= ~(CF_MASK | OF_MASK);
                }

                break;
            }

            
            
            
            if (group == 2) {

                // NOT : complement binaire, aucun flag affecte
                if (rex.w) {

                    std::uint64_t value = 0;

                    if (memory) {
                        if (!ReadMemory(
                                address,
                                reinterpret_cast<std::uint8_t*>(&value),
                                sizeof(value))) {
                            return 1;
                        }
                    } else {
                        value = registers_.Read64(rm);
                    }

                    const std::uint64_t result = ~value;

                    if (memory) {
                        if (!WriteMemory(
                                address,
                                reinterpret_cast<const std::uint8_t*>(&result),
                                sizeof(result))) {
                            return 1;
                        }
                    } else {
                        registers_.Write64(rm, result);
                    }

                } else {

                    std::uint32_t value = 0;

                    if (memory) {
                        if (!ReadMemory(
                                address,
                                reinterpret_cast<std::uint8_t*>(&value),
                                sizeof(value))) {
                            return 1;
                        }
                    } else {
                        value = registers_.Read32(rm);
                    }

                    const std::uint32_t result = ~value;

                    if (memory) {
                        if (!WriteMemory(
                                address,
                                reinterpret_cast<const std::uint8_t*>(&result),
                                sizeof(result))) {
                            return 1;
                        }
                    } else {
                        registers_.Write32(rm, result);
                    }
                }

                break;
            }

            if (group == 3) {

                if (rex.w) {

                    std::uint64_t value = 0;

                    if (memory) {
                        if (!ReadMemory(
                                address,
                                reinterpret_cast<std::uint8_t*>(&value),
                                sizeof(value))) {
                            return 1;
                        }
                    } else {
                        value = registers_.Read64(rm);
                    }

                    const std::uint64_t result =
                        0ULL - value;

                    if (memory) {
                        if (!WriteMemory(
                                address,
                                reinterpret_cast<const std::uint8_t*>(&result),
                                sizeof(result))) {
                            return 1;
                        }
                    } else {
                        registers_.Write64(
                            rm,
                            result);
                    }

                    SetZeroFlag(result == 0);

                    SetSignFlag(
                        (result & 0x8000000000000000ULL) != 0);

                    if (value != 0) {
                        rflags_ |= CF_MASK;
                    } else {
                        rflags_ &= ~CF_MASK;
                    }

                    if (value == 0x8000000000000000ULL) {
                        rflags_ |= OF_MASK;
                    } else {
                        rflags_ &= ~OF_MASK;
                    }

                } else {

                    std::uint32_t value = 0;

                    if (memory) {
                        if (!ReadMemory(
                                address,
                                reinterpret_cast<std::uint8_t*>(&value),
                                sizeof(value))) {
                            return 1;
                        }
                    } else {
                        value = registers_.Read32(rm);
                    }

                    const std::uint32_t result =
                        0U - value;

                    if (memory) {
                        if (!WriteMemory(
                                address,
                                reinterpret_cast<const std::uint8_t*>(&result),
                                sizeof(result))) {
                            return 1;
                        }
                    } else {
                        registers_.Write32(
                            rm,
                            result);
                    }

                    SetZeroFlag(result == 0);

                    SetSignFlag(
                        (result & 0x80000000U) != 0);

                    if (value != 0) {
                        rflags_ |= CF_MASK;
                    } else {
                        rflags_ &= ~CF_MASK;
                    }

                    if (value == 0x80000000U) {
                        rflags_ |= OF_MASK;
                    } else {
                        rflags_ &= ~OF_MASK;
                    }
                }

                break;
            }

            
                        
                        
            
            
            
            if (group == 4 || group == 5) {

                if (rex.w) {

                const std::uint64_t lhs =
                    registers_.Read64(0);

                std::uint64_t rhs = 0;

                if (memory) {
                    if (!ReadMemory(
                            address,
                            reinterpret_cast<std::uint8_t*>(&rhs),
                            sizeof(rhs))) {
                        return 1;
                    }
                } else {
                    rhs = registers_.Read64(rm);
                }

                const std::uint32_t lhs_lo =
                    static_cast<std::uint32_t>(lhs);

                const std::uint32_t lhs_hi =
                    static_cast<std::uint32_t>(lhs >> 32);

                const std::uint32_t rhs_lo =
                    static_cast<std::uint32_t>(rhs);

                const std::uint32_t rhs_hi =
                    static_cast<std::uint32_t>(rhs >> 32);

                const std::uint64_t p0 =
                    static_cast<std::uint64_t>(lhs_lo) * rhs_lo;

                const std::uint64_t p1 =
                    static_cast<std::uint64_t>(lhs_hi) * rhs_lo;

                const std::uint64_t p2 =
                    static_cast<std::uint64_t>(lhs_lo) * rhs_hi;

                const std::uint64_t p3 =
                    static_cast<std::uint64_t>(lhs_hi) * rhs_hi;

                const std::uint64_t middle =
                    (p0 >> 32) +
                    static_cast<std::uint32_t>(p1) +
                    static_cast<std::uint32_t>(p2);

                const std::uint64_t lo =
                    (p0 & 0xFFFFFFFFULL) |
                    (middle << 32);

                std::uint64_t hi =
                    p3 +
                    (p1 >> 32) +
                    (p2 >> 32) +
                    (middle >> 32);

                if (group == 5) {

                    if (lhs & 0x8000000000000000ULL) {
                        hi -= rhs;
                    }

                    if (rhs & 0x8000000000000000ULL) {
                        hi -= lhs;
                    }

                    const std::uint64_t expected_hi =
                        (lo & 0x8000000000000000ULL)
                            ? 0xFFFFFFFFFFFFFFFFULL
                            : 0ULL;

                    if (hi != expected_hi)
                        rflags_ |= CF_MASK | OF_MASK;
                    else
                        rflags_ &= ~(CF_MASK | OF_MASK);

                } else {

                    if (hi != 0)
                        rflags_ |= CF_MASK | OF_MASK;
                    else
                        rflags_ &= ~(CF_MASK | OF_MASK);
                }

                registers_.Write64(0, lo);
                registers_.Write64(2, hi);

                } // if (rex.w)
                else {

                    const std::uint32_t lhs32 =
                        registers_.Read32(0);

                    std::uint32_t rhs32 = 0;

                    if (memory) {
                        if (!ReadMemory(
                                address,
                                reinterpret_cast<std::uint8_t*>(&rhs32),
                                sizeof(rhs32))) {
                            return 1;
                        }
                    } else {
                        rhs32 = registers_.Read32(rm);
                    }

                    std::uint32_t lo32 = 0;
                    std::uint32_t hi32 = 0;

                    if (group == 4) {

                        // MUL r/m32 (non signe)
                        const std::uint64_t product =
                            static_cast<std::uint64_t>(lhs32) *
                            static_cast<std::uint64_t>(rhs32);

                        lo32 = static_cast<std::uint32_t>(product);
                        hi32 = static_cast<std::uint32_t>(product >> 32);

                        if (hi32 != 0) {
                            rflags_ |= CF_MASK | OF_MASK;
                        } else {
                            rflags_ &= ~(CF_MASK | OF_MASK);
                        }

                    } else {

                        // IMUL r/m32 (signe, forme 1 operande)
                        const std::int64_t product =
                            static_cast<std::int64_t>(
                                static_cast<std::int32_t>(lhs32)) *
                            static_cast<std::int64_t>(
                                static_cast<std::int32_t>(rhs32));

                        const std::uint64_t product_u =
                            static_cast<std::uint64_t>(product);

                        lo32 = static_cast<std::uint32_t>(product_u);
                        hi32 = static_cast<std::uint32_t>(product_u >> 32);

                        const std::uint32_t expected_hi32 =
                            (lo32 & 0x80000000U) ? 0xFFFFFFFFU : 0U;

                        if (hi32 != expected_hi32) {
                            rflags_ |= CF_MASK | OF_MASK;
                        } else {
                            rflags_ &= ~(CF_MASK | OF_MASK);
                        }
                    }

                    registers_.Write32(0, lo32);
                    registers_.Write32(2, hi32);
                }

                break;
            }

            
            
            if (group == 6 || group == 7) {

                const bool signed_division =
                    (group == 7);

                
                
                
                
                if (rex.w) {

                    std::uint64_t divisor_bits = 0;

                    if (memory) {
                        if (!ReadMemory(
                                address,
                                reinterpret_cast<std::uint8_t*>(&divisor_bits),
                                sizeof(divisor_bits))) {
                            return 1;
                        }
                    } else {
                        divisor_bits = registers_.Read64(rm);
                    }

                    if (divisor_bits == 0) {
                        std::cerr
                            << "[CPU] DIV/IDIV64 : division par zero\n";
                        RaiseException({
                            CpuExceptionKind::DivideError,
                            instruction_address,
                            MemoryFault::None,
                            CpuExceptionVector::DivideError
                        });
                        return 1;
                    }

                    const std::uint64_t dividend_lo =
                        registers_.Read64(0);

                    const std::uint64_t dividend_hi =
                        registers_.Read64(2);

                    
                    
                    
                    
                    
                    
                    if (!signed_division) {

                        const U128DivResult div =
                            DivideU128ByU64(
                                dividend_hi,
                                dividend_lo,
                                divisor_bits);

                        if (div.quotient_overflow) {
                            std::cerr
                                << "[CPU] DIV64 : overflow quotient\n";
                            RaiseException({
                                CpuExceptionKind::DivideError,
                                instruction_address,
                                MemoryFault::None,
                                CpuExceptionVector::DivideError
                            });
                            return 1;
                        }

                        registers_.Write64(
                            0,
                            div.quotient_lo);

                        registers_.Write64(
                            2,
                            div.remainder);

                    
                    
                    
                    
                    
                    } else {

                        const bool dividend_negative =
                            (dividend_hi &
                             0x8000000000000000ULL) != 0;

                        const bool divisor_negative =
                            (divisor_bits &
                             0x8000000000000000ULL) != 0;

                        
                        std::uint64_t magnitude_hi =
                            dividend_hi;

                        std::uint64_t magnitude_lo =
                            dividend_lo;

                        if (dividend_negative) {

                            magnitude_hi =
                                ~magnitude_hi;

                            magnitude_lo =
                                ~magnitude_lo;

                            ++magnitude_lo;

                            if (magnitude_lo == 0) {
                                ++magnitude_hi;
                            }
                        }

                        
                        std::uint64_t divisor_magnitude =
                            divisor_bits;

                        if (divisor_negative) {
                            divisor_magnitude =
                                0ULL - divisor_magnitude;
                        }

                        const U128DivResult div =
                            DivideU128ByU64(
                                magnitude_hi,
                                magnitude_lo,
                                divisor_magnitude);

                        const bool quotient_negative =
                            dividend_negative != divisor_negative;

                        if (div.quotient_overflow ||            div.quotient_hi != 0) {
                            std::cerr
                                << "[CPU] IDIV64 : overflow quotient\n";
                            RaiseException({
                                CpuExceptionKind::DivideError,
                                instruction_address,
                                MemoryFault::None,
                                CpuExceptionVector::DivideError
                            });
                            return 1;
                        }

                        
                        if (!quotient_negative) {

                            if (div.quotient_lo >
                                0x7FFFFFFFFFFFFFFFULL) {

                                std::cerr
                                    << "[CPU] IDIV64 : overflow quotient\n";
                                RaiseException({
                                    CpuExceptionKind::DivideError,
                                    instruction_address,
                                    MemoryFault::None,
                                CpuExceptionVector::DivideError
                                });
                                return 1;
                            }

                        
                        } else {

                            if (div.quotient_lo >
                                0x8000000000000000ULL) {

                                std::cerr
                                    << "[CPU] IDIV64 : overflow quotient\n";
                                RaiseException({
                                    CpuExceptionKind::DivideError,
                                    instruction_address,
                                    MemoryFault::None,
                                CpuExceptionVector::DivideError
                                });
                                return 1;
                            }
                        }

                        std::uint64_t quotient_bits =
                            div.quotient_lo;

                        if (quotient_negative) {
                            quotient_bits =
                                0ULL - quotient_bits;
                        }

                        
                        std::uint64_t remainder_bits =
                            div.remainder;

                        if (dividend_negative &&
                            remainder_bits != 0) {

                            remainder_bits =
                                0ULL - remainder_bits;
                        }

                        registers_.Write64(
                            0,
                            quotient_bits);

                        registers_.Write64(
                            2,
                            remainder_bits);
                    }

                
                
                } else {

                    std::uint32_t divisor_bits = 0;

                    if (memory) {
                        if (!ReadMemory(
                                address,
                                reinterpret_cast<std::uint8_t*>(&divisor_bits),
                                sizeof(divisor_bits))) {
                            return 1;
                        }
                    } else {
                        divisor_bits = registers_.Read32(rm);
                    }

                    if (divisor_bits == 0) {
                        std::cerr
                            << "[CPU] DIV/IDIV32 : division par zero\n";
                        RaiseException({
                            CpuExceptionKind::DivideError,
                            instruction_address,
                            MemoryFault::None,
                                CpuExceptionVector::DivideError
                        });
                        return 1;
                    }

                    const std::uint32_t eax =
                        registers_.Read32(0);

                    const std::uint32_t edx =
                        registers_.Read32(2);

                    
                    
                    
                    
                    
                    if (!signed_division) {

                        const std::uint64_t dividend =
                            (static_cast<std::uint64_t>(edx) << 32) |
                            static_cast<std::uint64_t>(eax);

                        const std::uint64_t divisor =
                            static_cast<std::uint64_t>(
                                divisor_bits);

                        const std::uint64_t quotient =
                            dividend / divisor;

                        const std::uint64_t remainder =
                            dividend % divisor;

                        
                        if (quotient > 0xFFFFFFFFULL) {
                            std::cerr
                                << "[CPU] DIV32 : overflow quotient\n";
                            RaiseException({
                                CpuExceptionKind::DivideError,
                                instruction_address,
                                MemoryFault::None,
                                CpuExceptionVector::DivideError
                            });
                            return 1;
                        }

                        registers_.Write32(
                            0,
                            static_cast<std::uint32_t>(
                                quotient));

                        registers_.Write32(
                            2,
                            static_cast<std::uint32_t>(
                                remainder));

                    
                    
                    
                    
                    
                    } else {

                        const std::int32_t high =
                            static_cast<std::int32_t>(edx);

                        const std::int32_t low =
                            static_cast<std::int32_t>(eax);

                        const std::int64_t dividend =
                            (static_cast<std::int64_t>(high) << 32) |
                            static_cast<std::uint32_t>(low);

                        const std::int32_t divisor =
                            static_cast<std::int32_t>(
                                divisor_bits);

                        
                        if (divisor == -1 &&
                            dividend ==
                                static_cast<std::int64_t>(
                                    std::numeric_limits<
                                        std::int32_t>::min())) {

                            std::cerr
                                << "[CPU] IDIV32 : overflow quotient\n";
                            RaiseException({
                                CpuExceptionKind::DivideError,
                                instruction_address,
                                MemoryFault::None,
                                CpuExceptionVector::DivideError
                            });
                            return 1;
                        }

                        const std::int64_t quotient =
                            dividend /
                            static_cast<std::int64_t>(divisor);

                        const std::int64_t remainder =
                            dividend %
                            static_cast<std::int64_t>(divisor);

                        if (quotient <
                                static_cast<std::int64_t>(
                                    std::numeric_limits<
                                        std::int32_t>::min()) ||
                            quotient >
                                static_cast<std::int64_t>(
                                    std::numeric_limits<
                                        std::int32_t>::max())) {

                            std::cerr
                                << "[CPU] IDIV32 : overflow quotient\n";
                            RaiseException({
                                CpuExceptionKind::DivideError,
                                instruction_address,
                                MemoryFault::None,
                                CpuExceptionVector::DivideError
                            });
                            return 1;
                        }

                        registers_.Write32(
                            0,
                            static_cast<std::uint32_t>(
                                static_cast<std::int32_t>(
                                    quotient)));

                        registers_.Write32(
                            2,
                            static_cast<std::uint32_t>(
                                static_cast<std::int32_t>(
                                    remainder)));
                    }
                }

                
                

                break;
            }

            return 1;
        }
case 0xD0:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (!DecodeShift8Imm(modrm, rex, 1)) {
                return 1;
            }

            break;
        }

        case 0xC0:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            const std::uint8_t modC0 =
                static_cast<std::uint8_t>((modrm >> 6) & 0x03);

            if (modC0 == 0x03) {

                std::uint8_t imm8 = 0;

                if (!Fetch8(imm8)) {
                    return 1;
                }

                if (!DecodeShift8Imm(modrm, rex, imm8)) {
                    return 1;
                }

            }
            else {

                std::uint8_t reg = 0;
                std::uint8_t rm = 0;
                std::uint64_t address = 0;
                bool memory = false;

                if (!DecodeMemoryOrRegister32(
                        modrm,
                        rex,
                        reg,
                        rm,
                        address,
                        memory) ||
                    !memory) {
                    return 1;
                }

                std::uint8_t imm8 = 0;

                if (!Fetch8(imm8)) {
                    return 1;
                }

                const std::uint8_t group =
                    static_cast<std::uint8_t>(
                        (modrm >> 3) & 0x07);

                if (!DecodeShift8Memory(
                        rm,
                        address,
                        group,
                        imm8)) {
                    return 1;
                }
            }

            break;
        }

        case 0xD2:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (!DecodeShift8CL(modrm, rex)) {
                return 1;
            }

            break;
        }
        case 0xD3:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            const std::uint8_t clCount =
                static_cast<std::uint8_t>(
                    registers_.Read32(1) & 0xFF);

            if (rex.w) {
                const std::uint8_t group64 =
                    static_cast<std::uint8_t>(
                        (modrm >> 3) & 0x07);

                if (group64 == 0 || group64 == 1) {
                    if (!DecodeRotate64Imm(
                            modrm,
                            rex,
                            clCount)) {
                        return 1;
                    }
                }
                else if (!DecodeShiftLeft64CL(modrm, rex)) {
                    return 1;
                }
            }
            else {
                const std::uint8_t group =
                    static_cast<std::uint8_t>(
                        (modrm >> 3) & 0x07);

                if (group == 0 || group == 1) {
                    if (!DecodeRotate32Imm(
                            modrm,
                            rex,
                            clCount)) {
                        return 1;
                    }
                }
                else if (group == 4) {
                    if (!DecodeShiftLeft32CL(modrm, rex)) {
                        return 1;
                    }
                }
                else if (group == 5) {
                    if (!DecodeShiftRight32CL(modrm, rex)) {
                        return 1;
                    }
                }
                else if (group == 7) {
                    if (!DecodeShiftArithmetic32CL(modrm, rex)) {
                        return 1;
                    }
                }
                else {
                    return 1;
                }
            }

            break;
        }
        case 0xD1:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {

                const std::uint8_t group64 =
                    static_cast<std::uint8_t>(
                        (modrm >> 3) & 0x07);

                if (group64 == 0 || group64 == 1) {
                    if (!DecodeRotate64Imm(
                            modrm,
                            rex,
                            1)) {
                        return 1;
                    }
                }
                else if (!DecodeShiftLeft64Imm(
                        modrm,
                        rex,
                        1)) {
                    return 1;
                }

            }
            else {

                const std::uint8_t group =
                    static_cast<std::uint8_t>(
                        (modrm >> 3) & 0x07);

                if (group == 0 || group == 1) {

                    if (!DecodeRotate32Imm(
                            modrm,
                            rex,
                            1)) {
                        return 1;
                    }

                }
                else if (group == 4) {

                    if (!DecodeShiftLeft32Imm(
                            modrm,
                            rex,
                            1)) {
                        return 1;
                    }

                }
                else if (group == 5) {

                    if (!DecodeShiftRight32Imm(
                            modrm,
                            rex,
                            1)) {
                        return 1;
                    }

                }
                else if (group == 7) {

                    if (!DecodeShiftArithmetic32Imm(
                            modrm,
                            rex,
                            1)) {
                        return 1;
                    }

                }
                else {
                    return 1;
                }
            }

            break;
        }
        case 0xC1:
        {
            std::uint8_t modrm = 0;
            std::uint8_t count = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {
                const std::uint8_t group64 =
                    static_cast<std::uint8_t>(
                        (modrm >> 3) & 0x07);

                if (group64 == 0 || group64 == 1) {
                    if (!DecodeRotate64Imm(
                            modrm,
                            rex,
                            count,
                            true)) {
                        return 1;
                    }
                }
                else if (!DecodeShiftLeft64Imm(
                        modrm,
                        rex,
                        count,
                        true)) {
                    return 1;
                }
            }
            else {
                std::uint8_t group = (modrm >> 3) & 0x07;

                if (group == 0 || group == 1) {
                    if (!DecodeRotate32Imm(
                            modrm,
                            rex,
                            count,
                            true)) {
                        return 1;
                    }
                }
                else if (group == 4) {
                    if (!DecodeShiftLeft32Imm(
                            modrm,
                            rex,
                            count,
                            true)) {
                        return 1;
                    }
                }
                else if (group == 5) {
                    if (!DecodeShiftRight32Imm(
                            modrm,
                            rex,
                            count,
                            true)) {
                        return 1;
                    }
                }
                else if (group == 7) {
                    if (!DecodeShiftArithmetic32Imm(
                            modrm,
                            rex,
                            count,
                            true)) {
                        return 1;
                    }
                }
                else {
                    return 1;
                }
            }

            break;
        }
        case 0x05:
        case 0x0D:
        case 0x25:
        case 0x2D:
        case 0x35:
        case 0x3D:
        case 0xA9:
        {
            std::uint32_t immediate32 = 0;

            if (!Fetch32(immediate32)) {
                return 1;
            }

            if (rex.w) {

                const std::uint64_t immediate64 =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            static_cast<std::int32_t>(immediate32)));

                const std::uint64_t lhs =
                    registers_.Read64(0);

                std::uint64_t result = 0;

                switch (opcode) {
                case 0x05: result = lhs + immediate64; break;
                case 0x0D: result = lhs | immediate64; break;
                case 0x25: result = lhs & immediate64; break;
                case 0x2D: result = lhs - immediate64; break;
                case 0x35: result = lhs ^ immediate64; break;
                case 0x3D: result = lhs - immediate64; break;
                case 0xA9: result = lhs & immediate64; break;
                }

                if (opcode != 0x3D && opcode != 0xA9) {
                    registers_.Write64(0, result);
                }

                if (opcode == 0x05) {
                    SetAddFlags64(lhs, immediate64, result);
                } else if (opcode == 0x2D || opcode == 0x3D) {
                    SetSubFlags64(lhs, immediate64, result);
                } else {
                    SetLogicFlags64(result);
                }

            } else {

                const std::uint32_t lhs32 =
                    registers_.Read32(0);

                std::uint32_t result32 = 0;

                switch (opcode) {
                case 0x05: result32 = lhs32 + immediate32; break;
                case 0x0D: result32 = lhs32 | immediate32; break;
                case 0x25: result32 = lhs32 & immediate32; break;
                case 0x2D: result32 = lhs32 - immediate32; break;
                case 0x35: result32 = lhs32 ^ immediate32; break;
                case 0x3D: result32 = lhs32 - immediate32; break;
                case 0xA9: result32 = lhs32 & immediate32; break;
                }

                if (opcode != 0x3D && opcode != 0xA9) {
                    registers_.Write32(0, result32);
                }

                if (opcode == 0x05) {
                    SetAddFlags32(lhs32, immediate32, result32);
                } else if (opcode == 0x2D || opcode == 0x3D) {
                    SetSubFlags32(lhs32, immediate32, result32);
                } else {
                    SetLogicFlags32(result32);
                }
            }

            break;
        }
        case 0xC7:
        {
            
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            const std::uint8_t group =
                static_cast<std::uint8_t>((modrm >> 3) & 0x07);

            if (group != 0) {
                return 1;
            }

            std::uint8_t reg = 0;
            std::uint8_t rm = 0;
            std::uint64_t address = 0;
            bool memory = false;

            if (!DecodeMemoryOrRegister32(
                    modrm,
                    rex,
                    reg,
                    rm,
                    address,
                    memory)) {
                return 1;
            }

            std::uint32_t immediate = 0;

            if (!Fetch32(immediate)) {
                return 1;
            }

            if (rex.w) {
                const std::uint64_t value =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            static_cast<std::int32_t>(immediate)));

                if (memory) {
                    if (!WriteMemory(
                            address,
                            reinterpret_cast<const std::uint8_t*>(&value),
                            sizeof(value))) {
                        return 1;
                    }
                }
                else {
                    registers_.Write64(rm, value);
                }
            }
            else if (memory) {
                if (!WriteMemory(
                        address,
                        reinterpret_cast<const std::uint8_t*>(&immediate),
                        sizeof(immediate))) {
                    return 1;
                }
            }
            else {
                registers_.Write32(rm, immediate);
            }

            break;
        }
        case 0x81:
        case 0x83:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (!rex.w) {
                const std::uint8_t group =
                    static_cast<std::uint8_t>((modrm >> 3) & 0x07);

                std::uint8_t reg = 0;
                std::uint8_t rm = 0;
                std::uint64_t address = 0;
                bool memory = false;

                if (!DecodeMemoryOrRegister32(
                        modrm, rex, reg, rm, address, memory)) {
                    return 1;
                }

                std::uint32_t immediate32 = 0;

                if (opcode == 0x81) {
                    if (!Fetch32(immediate32)) {
                        return 1;
                    }
                }
                else {
                    std::uint8_t imm8 = 0;
                    if (!Fetch8(imm8)) {
                        return 1;
                    }
                    immediate32 = static_cast<std::uint32_t>(
                        static_cast<std::int32_t>(
                            static_cast<std::int8_t>(imm8)));
                }

                std::uint32_t lhs32 = memory
                    ? 0
                    : registers_.Read32(rm);

                if (memory) {
                    if (!ReadMemory(
                            address,
                            reinterpret_cast<std::uint8_t*>(&lhs32),
                            sizeof(lhs32))) {
                        return 1;
                    }
                }

                std::uint32_t result32 = 0;

                switch (group) {
                case 0:
                    result32 = lhs32 + immediate32;
                    SetAddFlags32(lhs32, immediate32, result32);
                    break;

                case 1:
                    result32 = lhs32 | immediate32;
                    SetLogicFlags32(result32);
                    break;

                case 2:
                {
                    const std::uint32_t carry =
                        (rflags_ & CF_MASK) ? 1u : 0u;

                    result32 =
                        lhs32 + immediate32 + carry;

                    SetAddFlags32(
                        lhs32,
                        immediate32 + carry,
                        result32);
                    break;
                }

                case 3:
                {
                    const std::uint32_t borrow =
                        (rflags_ & CF_MASK) ? 1u : 0u;

                    result32 =
                        lhs32 - immediate32 - borrow;

                    SetSubFlags32(
                        lhs32,
                        immediate32 + borrow,
                        result32);
                    break;
                }

                case 4:
                    result32 = lhs32 & immediate32;
                    SetLogicFlags32(result32);
                    break;

                case 5:
                    result32 = lhs32 - immediate32;
                    SetSubFlags32(lhs32, immediate32, result32);
                    break;

                case 6:
                    result32 = lhs32 ^ immediate32;
                    SetLogicFlags32(result32);
                    break;

                case 7:
                    result32 = lhs32 - immediate32;
                    SetSubFlags32(lhs32, immediate32, result32);
                    break;

                default:
                    std::cerr << "[CPU] Unsupported Group1 immediate /"
                              << static_cast<unsigned>(group) << '\n';
                    return 1;
                }

                if (group != 7) {
                    if (memory) {
                        if (!WriteMemory(
                                address,
                                reinterpret_cast<const std::uint8_t*>(&result32),
                                sizeof(result32))) {
                            return 1;
                        }
                    }
                    else {
                        registers_.Write32(rm, result32);
                    }
                }

                break;
            }

            
            
            
            
            
            std::uint8_t reg = 0;
            std::uint8_t rm = 0;
            std::uint64_t address = 0;
            bool memory = false;

            if (!DecodeMemoryOrRegister32(
                    modrm,
                    rex,
                    reg,
                    rm,
                    address,
                    memory)) {

                return 1;
            }

            std::uint64_t immediate = 0;

            if (opcode == 0x81) {

                std::uint32_t imm32 = 0;

                if (!Fetch32(imm32)) {
                    return 1;
                }

                immediate =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            static_cast<std::int32_t>(
                                imm32)));

            }
            else {

                std::uint8_t imm8 = 0;

                if (!Fetch8(imm8)) {
                    return 1;
                }

                immediate =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            static_cast<std::int8_t>(
                                imm8)));
            }

            
            
            

            std::uint64_t lhs = 0;

            if (memory) {

                if (!ReadMemory(
                        address,
                        reinterpret_cast<
                            std::uint8_t*>(&lhs),
                        sizeof(lhs))) {

                    return 1;
                }

            }
            else {

                lhs =
                    registers_.Read64(rm);
            }

            
            
            

            const std::uint8_t group =
                static_cast<std::uint8_t>((modrm >> 3) & 0x07);

            std::uint64_t result = 0;

            switch (group) {

            
            case 0:
                result =
                    lhs + immediate;

                if (memory) {

                    if (!WriteMemory(
                            address,
                            reinterpret_cast<
                                const std::uint8_t*>(&result),
                            sizeof(result))) {

                        return 1;
                    }

                }
                else {

                    registers_.Write64(
                        rm,
                        result);
                }

                SetAddFlags64(
                    lhs,
                    immediate,
                    result);

                std::cout
                    << "[CPU] ADD64 IMM -> 0x"
                    << std::hex
                    << result
                    << std::dec
                    << '\n';

                break;

            
            case 1:
                result =
                    lhs | immediate;

                if (memory) {

                    if (!WriteMemory(
                            address,
                            reinterpret_cast<
                                const std::uint8_t*>(&result),
                            sizeof(result))) {

                        return 1;
                    }

                }
                else {

                    registers_.Write64(
                        rm,
                        result);
                }

                SetLogicFlags64(result);

                std::cout
                    << "[CPU] OR64 IMM -> 0x"
                    << std::hex
                    << result
                    << std::dec
                    << '\n';

                break;

            
            case 4:
                result =
                    lhs & immediate;

                if (memory) {

                    if (!WriteMemory(
                            address,
                            reinterpret_cast<
                                const std::uint8_t*>(&result),
                            sizeof(result))) {

                        return 1;
                    }

                }
                else {

                    registers_.Write64(
                        rm,
                        result);
                }

                SetLogicFlags64(result);

                std::cout
                    << "[CPU] AND64 IMM -> 0x"
                    << std::hex
                    << result
                    << std::dec
                    << '\n';

                break;

            
            case 5:
                result =
                    lhs - immediate;

                if (memory) {

                    if (!WriteMemory(
                            address,
                            reinterpret_cast<
                                const std::uint8_t*>(&result),
                            sizeof(result))) {

                        return 1;
                    }

                }
                else {

                    registers_.Write64(
                        rm,
                        result);
                }

                SetSubFlags64(
                    lhs,
                    immediate,
                    result);

                std::cout
                    << "[CPU] SUB64 IMM -> 0x"
                    << std::hex
                    << result
                    << std::dec
                    << '\n';

                break;

            
            case 6:
                result =
                    lhs ^ immediate;

                if (memory) {

                    if (!WriteMemory(
                            address,
                            reinterpret_cast<
                                const std::uint8_t*>(&result),
                            sizeof(result))) {

                        return 1;
                    }

                }
                else {

                    registers_.Write64(
                        rm,
                        result);
                }

                SetLogicFlags64(result);

                std::cout
                    << "[CPU] XOR64 IMM -> 0x"
                    << std::hex
                    << result
                    << std::dec
                    << '\n';

                break;

            
            case 7:
                result =
                    lhs - immediate;

                SetSubFlags64(
                    lhs,
                    immediate,
                    result);

                std::cout
                    << "[CPU] CMP64 IMM lhs=0x"
                    << std::hex
                    << lhs
                    << " imm=0x"
                    << immediate
                    << " result=0x"
                    << result
                    << " ZF="
                    << (ZeroFlag() ? 1 : 0)
                    << " CF="
                    << ((rflags_ & CF_MASK) ? 1 : 0)
                    << " SF="
                    << (SignFlag() ? 1 : 0)
                    << " OF="
                    << ((rflags_ & OF_MASK) ? 1 : 0)
                    << std::dec
                    << '\n';

                break;

            default:
                std::cerr
                    << "[CPU] Unsupported Group1 immediate /"
                    << static_cast<unsigned>(group)
                    << '\n';

                return 1;
            }

            break;
        }

        
        
        

        case 0x74: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            if (ZeroFlag()) {
                instruction_pointer_ =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(instruction_pointer_) + rel);
            }

            break;
        }

        case 0x75: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            if (!ZeroFlag()) {
                instruction_pointer_ =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(instruction_pointer_) + rel);
            }

            break;
        }

        case 0xE9: 
        {
            std::int32_t rel = 0;

            if (!FetchRel32(rel)) {
                return 1;
            }

            instruction_pointer_ =
                static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(instruction_pointer_) + rel);

            break;
        }

        case 0xEB: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            instruction_pointer_ =
                static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(instruction_pointer_) + rel);

            break;
        }
        case 0x72: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            const bool cf =
                (rflags_ & CF_MASK) != 0;

            if (cf) {
                instruction_pointer_ =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            instruction_pointer_) + rel);
            }

            break;
        }

        case 0x73: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            const bool cf =
                (rflags_ & CF_MASK) != 0;

            if (!cf) {
                instruction_pointer_ =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            instruction_pointer_) + rel);
            }

            break;
        }

        case 0x76: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            const bool cf =
                (rflags_ & CF_MASK) != 0;

            if (cf || ZeroFlag()) {
                instruction_pointer_ =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            instruction_pointer_) + rel);
            }

            break;
        }

        case 0x77: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            const bool cf =
                (rflags_ & CF_MASK) != 0;

            if (!cf && !ZeroFlag()) {
                instruction_pointer_ =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            instruction_pointer_) + rel);
            }

            break;
        }

        case 0x7C: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            const bool sf =
                SignFlag();

            const bool of =
                (rflags_ & OF_MASK) != 0;

            if (sf != of) {
                instruction_pointer_ =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            instruction_pointer_) + rel);
            }

            break;
        }

        case 0x7D: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            const bool sf =
                SignFlag();

            const bool of =
                (rflags_ & OF_MASK) != 0;

            if (sf == of) {
                instruction_pointer_ =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            instruction_pointer_) + rel);
            }

            break;
        }

        case 0x7E: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            const bool sf =
                SignFlag();

            const bool of =
                (rflags_ & OF_MASK) != 0;

            if (ZeroFlag() || (sf != of)) {
                instruction_pointer_ =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            instruction_pointer_) + rel);
            }

            break;
        }

        case 0x7F: 
        {
            std::int8_t rel = 0;

            if (!FetchRel8(rel)) {
                return 1;
            }

            const bool sf =
                SignFlag();

            const bool of =
                (rflags_ & OF_MASK) != 0;

            if (!ZeroFlag() && (sf == of)) {
                instruction_pointer_ =
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            instruction_pointer_) + rel);
            }

            break;
        }


        
        
        

        case 0xB8:
        case 0xB9:
        case 0xBA:
        case 0xBB:
        case 0xBC:
        case 0xBD:
        case 0xBE:
        case 0xBF:
        {
            std::uint8_t reg =
                static_cast<std::uint8_t>(
                    opcode - MOV_R32_IMM32_BASE);

            if (rex.b) {
                reg =
                    static_cast<std::uint8_t>(reg + 8);
            }

            if (rex.w) {

                std::uint64_t value = 0;

                if (!Fetch64(value)) {
                    return 1;
                }

                registers_.Write64(reg, value);

                std::cout
                    << "[CPU] MOV r64["
                    << static_cast<unsigned>(reg)
                    << "] = 0x"
                    << std::hex
                    << value
                    << std::dec
                    << '\n';
            }
            else {

                std::uint32_t value = 0;

                if (!Fetch32(value)) {
                    return 1;
                }

                registers_.Write32(reg, value);

                std::cout
                    << "[CPU] MOV r32["
                    << static_cast<unsigned>(reg)
                    << "] = "
                    << value
                    << '\n';
            }

            break;
        }

        
        
        case 0x87:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (!DecodeXchg(modrm, rex)) {
                return 1;
            }

            break;
        }

        case 0x86:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (!DecodeXchg8(modrm, rex)) {
                return 1;
            }

            break;
        }

        

        case MOV_RM32_R32:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {

                if (!DecodeMov64Store(modrm, rex)) {
                    return 1;
                }
            }
            else {
                if (!DecodeMov32Store(modrm, rex)) {
                    return 1;
                }
            }

            break;
        }

        case MOV_R32_RM32:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {

                if (!DecodeMov64Load(modrm, rex)) {
                    return 1;
                }

            }
            else {
                if (!DecodeMov32Load(modrm, rex)) {
                    return 1;
                }
            }

            break;
        }

        
        
        

        case ADD_RM32_R32:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {

                std::uint8_t reg = 0;
                std::uint8_t rm = 0;
                std::uint64_t address = 0;
                bool memory = false;

                if (!DecodeMemoryOrRegister32(
                        modrm,
                        rex,
                        reg,
                        rm,
                        address,
                        memory)) {
                    return 1;
                }

                const std::uint64_t rhs =
                    registers_.Read64(reg);

                if (memory) {

                    std::uint64_t lhs = 0;

                    if (!ReadMemory(
                            address,
                            reinterpret_cast<std::uint8_t*>(&lhs),
                            sizeof(lhs))) {
                        return 1;
                    }

                    const std::uint64_t result =
                        lhs + rhs;

                    if (!WriteMemory(
                            address,
                            reinterpret_cast<const std::uint8_t*>(&result),
                            sizeof(result))) {
                        return 1;
                    }

                    SetAddFlags64(lhs, rhs, result);
                }
                else {

                    const std::uint64_t lhs =
                        registers_.Read64(rm);

                    const std::uint64_t result =
                        lhs + rhs;

                    registers_.Write64(rm, result);

                    SetAddFlags64(lhs, rhs, result);

                    std::cout
                        << "[CPU] ADD64 r64["
                        << static_cast<unsigned>(rm)
                        << "] += r64["
                        << static_cast<unsigned>(reg)
                        << "] -> 0x"
                        << std::hex
                        << result
                        << std::dec
                        << '\n';
                }
            }
            else {

                if (!DecodeAdd32Store(modrm, rex)) {
                    return 1;
                }
            }

            break;
        }

        case ADD_R32_RM32:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {
                if (!DecodeAdd64(modrm, rex)) {
                    return 1;
                }
            }
            else {
                if (!DecodeAdd32(modrm, rex)) {
                    return 1;
                }
            }

            break;
        }

        
        
        

        case SUB_RM32_R32:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {

                std::uint8_t reg = 0;
                std::uint8_t rm = 0;
                std::uint64_t address = 0;
                bool memory = false;

                if (!DecodeMemoryOrRegister32(
                        modrm,
                        rex,
                        reg,
                        rm,
                        address,
                        memory)) {
                    return 1;
                }

                const std::uint64_t rhs =
                    registers_.Read64(reg);

                if (memory) {

                    std::uint64_t lhs = 0;

                    if (!ReadMemory(
                            address,
                            reinterpret_cast<std::uint8_t*>(&lhs),
                            sizeof(lhs))) {
                        return 1;
                    }

                    const std::uint64_t result =
                        lhs - rhs;

                    if (!WriteMemory(
                            address,
                            reinterpret_cast<const std::uint8_t*>(&result),
                            sizeof(result))) {
                        return 1;
                    }

                    SetSubFlags64(lhs, rhs, result);
                }
                else {

                    const std::uint64_t lhs =
                        registers_.Read64(rm);

                    const std::uint64_t result =
                        lhs - rhs;

                    registers_.Write64(rm, result);

                    SetSubFlags64(lhs, rhs, result);

                    std::cout
                        << "[CPU] SUB64 r64["
                        << static_cast<unsigned>(rm)
                        << "] -= r64["
                        << static_cast<unsigned>(reg)
                        << "] -> 0x"
                        << std::hex
                        << result
                        << std::dec
                        << '\n';
                }
            }
            else {

                if (!DecodeSub32Store(modrm, rex)) {
                    return 1;
                }
            }

            break;
        }

        case SUB_R32_RM32:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {
                if (!DecodeSub64(modrm, rex)) {
                    return 1;
                }
            }
            else {
                if (!DecodeSub32(modrm, rex)) {
                    return 1;
                }
            }

            break;
        }

        
        
        

        case CMP_RM32_R32:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            
            
            
            
            

            if (rex.w) {

                std::uint8_t reg = 0;
                std::uint8_t rm = 0;

                std::uint64_t address = 0;
                bool memory = false;

                if (!DecodeMemoryOrRegister32(
                        modrm,
                        rex,
                        reg,
                        rm,
                        address,
                        memory)) {

                    return 1;
                }

                std::uint64_t lhs = 0;
                std::uint64_t rhs = 0;

                if (memory) {

                    if (!ReadMemory(
                            address,
                            reinterpret_cast<std::uint8_t*>(&lhs),
                            sizeof(lhs))) {

                        std::cerr
                            << "[CPU] CMP64 memory read failed at 0x"
                            << std::hex
                            << address
                            << std::dec
                            << '\n';

                        return 1;
                    }

                }
                else {

                    lhs = registers_.Read64(rm);
                }

                rhs = registers_.Read64(reg);

                const std::uint64_t result =
                    lhs - rhs;

                SetSubFlags64(
                    lhs,
                    rhs,
                    result);

                std::cout
                    << "[CPU] CMP64 r/m64,r64 lhs=0x"
                    << std::hex
                    << lhs
                    << " rhs=0x"
                    << rhs
                    << " result=0x"
                    << result
                    << " ZF="
                    << (ZeroFlag() ? 1 : 0)
                    << " SF="
                    << (SignFlag() ? 1 : 0)
                    << std::dec
                    << '\n';

                break;
            }

            
            
            

            if (!DecodeCmp32(
                    opcode,
                    modrm,
                    rex)) {

                return 1;
            }

            break;
        }
        case CMP_R32_RM32:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {
                if (!DecodeCmp64(modrm, rex)) {
                    return 1;
                }
            }
            else {
                if (!DecodeCmp32(opcode, modrm, rex)) {
                    return 1;
                }
            }

            break;
        }

        
        
        
        case 0x09: 
        case 0x0B: 
        case 0x21: 
        case 0x23: 
        case 0x31: 
        case 0x33: 
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {
                if (!DecodeLogic64(
                        opcode,
                        modrm,
                        rex)) {
                    return 1;
                }
            }
            else {
                if (!DecodeLogic32(
                        opcode,
                        modrm,
                        rex)) {
                    return 1;
                }
            }

            break;
        }

        
        
        


        case 0x8D:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {

                if (!DecodeLea64(
                        modrm,
                        rex)) {

                    return 1;
                }

            }
            else {

                std::uint8_t reg = 0;
                std::uint8_t rm = 0;
                std::uint64_t address = 0;
                bool memory = false;

                if (!DecodeMemoryOrRegister32(
                        modrm, rex, reg, rm, address, memory)) {
                    return 1;
                }

                if (!memory) {
                    return 1;
                }

                const std::uint32_t address32 =
                    static_cast<std::uint32_t>(address);

                registers_.Write32(
                    reg,
                    address32);

                std::cout
                    << "[CPU] LEA r32["
                    << static_cast<unsigned>(reg)
                    << "] = 0x"
                    << std::hex
                    << address32
                    << std::dec
                    << '\n';
            }

            break;
        }

        
        
        

        case TEST_RM32_R32:
        {
            std::uint8_t modrm = 0;

            if (!Fetch8(modrm)) {
                return 1;
            }

            if (rex.w) {

                if (!DecodeTest64(
                        modrm,
                        rex)) {
                    return 1;
                }
            }
            else {

                if (!DecodeTest32(
                        modrm,
                        rex)) {
                    return 1;
                }
            }

            break;
        }

        
        
        

        
        
        

        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
        {
            std::uint8_t reg =
                static_cast<std::uint8_t>(
                    opcode - PUSH_R64_BASE);

            if (rex.b) {
                reg =
                    static_cast<std::uint8_t>(reg + 8);
            }

            if (!Push64(registers_.Read64(reg))) {
                return 1;
            }

            break;
        }

        
        
        

        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D:
        case 0x5E:
        case 0x5F:
        {
            std::uint8_t reg =
                static_cast<std::uint8_t>(
                    opcode - POP_R64_BASE);

            if (rex.b) {
                reg =
                    static_cast<std::uint8_t>(reg + 8);
            }

            std::uint64_t value = 0;

            if (!Pop64(value)) {
                return 1;
            }

            registers_.Write64(reg, value);
            break;
        }

        
        
        
        

        case 0xC9:
        {
            
            
            

            const std::uint64_t rbp =
                registers_.Read64(5);

            registers_.SetRsp(rbp);

            std::uint64_t value = 0;

            if (!Pop64(value)) {
                return 1;
            }

            registers_.Write64(5, value);

            break;
        }

        
        
        

        case 0x6A:
        {
            std::uint8_t immediate = 0;

            if (!Fetch8(immediate)) {
                return 1;
            }

            const auto value =
                static_cast<std::int64_t>(
                    static_cast<std::int8_t>(immediate));

            if (!Push64(
                    static_cast<std::uint64_t>(value))) {
                return 1;
            }

            break;
        }

        
        
        

        case 0x68:
        {
            std::uint32_t immediate = 0;

            if (!Fetch32(immediate)) {
                return 1;
            }

            const auto value =
                static_cast<std::int64_t>(
                    static_cast<std::int32_t>(immediate));

            if (!Push64(
                    static_cast<std::uint64_t>(value))) {
                return 1;
            }

            break;
        }

        
        
        

        case CALL_REL32:
        {
            std::int32_t displacement = 0;

            if (!FetchRel32(displacement)) {
                return 1;
            }

            const std::uint64_t returnAddress =
                instruction_pointer_;

            if (!Push64(returnAddress)) {
                return 1;
            }

            instruction_pointer_ =
                static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(
                        instruction_pointer_) +
                    static_cast<std::int64_t>(
                        displacement));

            ++call_depth;

            break;
        }

        
        
        

        case RET:
        {
            if (call_depth == 0) {
                running = false;
                break;
            }

            std::uint64_t returnAddress = 0;

            if (!Pop64(returnAddress)) {
                return 1;
            }

            instruction_pointer_ = returnAddress;
            --call_depth;

            break;
        }

        case 0x90: {
            // NOP
            break;
        }

        case 0x98: {
            if (rex.w) {
                // CDQE : sign-extend EAX -> RAX
                const std::int32_t eax =
                    static_cast<std::int32_t>(
                        registers_.Read32(0));

                registers_.Write64(
                    0,
                    static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(eax)));
            }
            else {
                // CWDE : sign-extend AX -> EAX
                const std::int16_t ax =
                    static_cast<std::int16_t>(
                        registers_.Read32(0) & 0xFFFF);

                registers_.Write32(
                    0,
                    static_cast<std::uint32_t>(
                        static_cast<std::int32_t>(ax)));
            }

            break;
        }

        case 0x99: {
            if (rex.w) {
                // CQO : sign-extend RAX -> RDX:RAX
                const std::int64_t rax =
                    static_cast<std::int64_t>(
                        registers_.Read64(0));

                registers_.Write64(
                    2,
                    (rax < 0) ? 0xFFFFFFFFFFFFFFFFULL : 0ULL);
            }
            else {
                // CDQ : sign-extend EAX -> EDX:EAX
                const std::int32_t eax =
                    static_cast<std::int32_t>(
                        registers_.Read32(0));

                registers_.Write32(
                    2,
                    (eax < 0) ? 0xFFFFFFFFU : 0U);
            }

            break;
        }

        case 0xF4: {
            // HLT : arret propre du CPU
            Halt();
            break;
        }

        default:
            std::cerr
                << "[CPU] Unsupported opcode 0x"
                << std::hex
                << static_cast<unsigned>(opcode)
                << std::dec
                << '\n';

            RaiseException({
                CpuExceptionKind::InvalidOpcode,
                instruction_address,
                MemoryFault::None,
                CpuExceptionVector::InvalidOpcode
            });

            return 1;
        }

        ++instruction_counter;

        if (frame_callback_ &&
            (instruction_counter % FRAME_CALLBACK_INTERVAL) == 0) {

            if (!frame_callback_()) {
                running = false;
            }
        }
    }

    std::cout
        << "[CPU] Final registers:\n"
        << "      RAX = 0x"
        << std::hex
        << registers_.Rax()
        << "\n      RSP = 0x"
        << registers_.Rsp()
        << "\n      RIP = 0x"
        << instruction_pointer_
        << "\n      RFLAGS = 0x"
        << rflags_
        << std::dec
        << '\n';

    return 0;
}





bool Cpu::DecodeMovzx32Reg32(std::uint8_t modrm, const RexPrefix& rex)
{
    
    
    
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(modrm, rex, reg, rm, address, memory)) {
        return false;
    }

    if (memory) {
        
        std::uint8_t value = 0;
        if (!ReadMemory(address, &value, sizeof(value))) {
            return false;
        }
        
        
        registers_.Write32(reg, static_cast<std::uint32_t>(value));
    } else {
        
        std::uint8_t value = static_cast<std::uint8_t>(registers_.Read32(rm) & 0xFF);
        registers_.Write32(reg, static_cast<std::uint32_t>(value));
    }
    
    return true;
}

bool Cpu::DecodeMovzx64Reg32(std::uint8_t modrm, const RexPrefix& rex)
{
    
    
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(modrm, rex, reg, rm, address, memory)) {
        return false;
    }

    if (memory) {
        std::uint32_t value = 0;
        if (!ReadMemory(address, reinterpret_cast<std::uint8_t*>(&value), sizeof(value))) {
            return false;
        }
        
        
        registers_.Write64(reg, static_cast<std::uint64_t>(value));
    } else {
        
        std::uint32_t value = registers_.Read32(rm);
        registers_.Write64(reg, static_cast<std::uint64_t>(value));
    }
    
    return true;
}

bool Cpu::DecodeMovsx32Reg8(std::uint8_t modrm, const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(modrm, rex, reg, rm, address, memory)) {
        return false;
    }

    std::int8_t value = 0;

    if (memory) {
        std::uint8_t raw = 0;

        if (!ReadMemory(
                address,
                &raw,
                sizeof(raw))) {
            return false;
        }

        value =
            static_cast<std::int8_t>(raw);
    } else {
        value =
            static_cast<std::int8_t>(
                registers_.Read64(rm) & 0xFF);
    }

    if (rex.w) {
        registers_.Write64(
            reg,
            static_cast<std::uint64_t>(
                static_cast<std::int64_t>(value)));
    } else {
        registers_.Write32(
            reg,
            static_cast<std::uint32_t>(
                static_cast<std::int32_t>(value)));
    }

    return true;
}

bool Cpu::DecodeMovzx32Reg16(std::uint8_t modrm, const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(modrm, rex, reg, rm, address, memory)) {
        return false;
    }

    std::uint16_t value = 0;

    if (memory) {
        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&value),
                sizeof(value))) {
            return false;
        }
    } else {
        value =
            static_cast<std::uint16_t>(
                registers_.Read64(rm) & 0xFFFF);
    }

    if (rex.w) {
        registers_.Write64(
            reg,
            static_cast<std::uint64_t>(value));
    } else {
        registers_.Write32(
            reg,
            static_cast<std::uint32_t>(value));
    }

    return true;
}

bool Cpu::DecodeMovsx32Reg16(std::uint8_t modrm, const RexPrefix& rex)
{
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(modrm, rex, reg, rm, address, memory)) {
        return false;
    }

    std::int16_t value = 0;

    if (memory) {
        std::uint16_t raw = 0;

        if (!ReadMemory(
                address,
                reinterpret_cast<std::uint8_t*>(&raw),
                sizeof(raw))) {
            return false;
        }

        value = static_cast<std::int16_t>(raw);
    } else {
        value =
            static_cast<std::int16_t>(
                registers_.Read64(rm) & 0xFFFF);
    }

    if (rex.w) {
        registers_.Write64(
            reg,
            static_cast<std::uint64_t>(
                static_cast<std::int64_t>(value)));
    } else {
        registers_.Write32(
            reg,
            static_cast<std::uint32_t>(
                static_cast<std::int32_t>(value)));
    }

    return true;
}
bool Cpu::DecodeMovsx64Reg16(std::uint8_t modrm, const RexPrefix& rex)
{
    
    
    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(modrm, rex, reg, rm, address, memory)) {
        return false;
    }

    if (memory) {
        std::uint16_t value = 0;
        if (!ReadMemory(address, reinterpret_cast<std::uint8_t*>(&value), sizeof(value))) {
            return false;
        }
        
        
        registers_.Write64(reg, static_cast<std::int64_t>(static_cast<std::int16_t>(value)));
    } else {
        
        std::uint16_t value = static_cast<std::uint16_t>(registers_.Read32(rm) & 0xFFFF);
        registers_.Write64(reg, static_cast<std::int64_t>(static_cast<std::int16_t>(value)));
    }
    
    return true;
}

bool Cpu::DecodeShift8Memory(
    std::uint8_t rm,
    std::uint64_t address,
    std::uint8_t group,
    std::uint8_t count)
{
    (void)rm;

    if (group != 0 && group != 1 &&
        group != 4 && group != 5 && group != 7) {
        return false;
    }

    const std::uint8_t maskedCount =
        static_cast<std::uint8_t>(count & 0x1FU);

    if (maskedCount == 0) {
        return true;
    }

    const bool isRotate = (group == 0 || group == 1);

    // Une rotation 8 bits se repete tous les 8 bits : on reduit le
    // compte pour eviter un decalage >= largeur (UB en C++) et
    // obtenir le meme resultat que le vrai CPU (rotation par un
    // multiple de 8 = valeur inchangee).
    const std::uint8_t effectiveCount =
        isRotate
            ? static_cast<std::uint8_t>(maskedCount % 8)
            : maskedCount;

    if (isRotate && effectiveCount == 0) {
        return true;
    }

    std::uint8_t value = 0;

    if (!ReadMemory(
            address,
            &value,
            sizeof(value))) {
        return false;
    }

    const std::uint8_t original = value;

    bool carry = false;
    std::uint8_t result = value;

    if (group == 0) {
        // ROL
        result = static_cast<std::uint8_t>(
            (value << effectiveCount) |
            (value >> (8 - effectiveCount)));

        carry = (result & 1U) != 0;
    }
    else if (group == 1) {
        // ROR
        result = static_cast<std::uint8_t>(
            (value >> effectiveCount) |
            (value << (8 - effectiveCount)));

        carry = ((result >> 7) & 1U) != 0;
    }
    else if (group == 4) {

        carry =
            ((value >> (8 - maskedCount)) & 1U) != 0;

        result =
            static_cast<std::uint8_t>(
                value << maskedCount);
    }
    else if (group == 5) {

        carry =
            ((value >> (maskedCount - 1)) & 1U) != 0;

        result =
            static_cast<std::uint8_t>(
                value >> maskedCount);
    }
    else {

        carry =
            ((value >> (maskedCount - 1)) & 1U) != 0;

        const std::int8_t signedValue =
            static_cast<std::int8_t>(value);

        result =
            static_cast<std::uint8_t>(
                signedValue >> maskedCount);
    }

    if (!WriteMemory(
            address,
            &result,
            sizeof(result))) {
        return false;
    }

    if (!isRotate) {
        SetZeroFlag(result == 0);
        SetSignFlag((result & 0x80U) != 0);
    }

    if (carry) {
        rflags_ |= CF_MASK;
    }
    else {
        rflags_ &= ~CF_MASK;
    }

    const std::uint8_t ofCount =
        isRotate ? effectiveCount : maskedCount;

    if (ofCount == 1) {

        bool overflow = false;

        if (group == 0) {
            overflow = ((result >> 7) & 1U) != carry;
        }
        else if (group == 1) {
            overflow =
                ((result >> 7) & 1U) != ((result >> 6) & 1U);
        }
        else if (group == 4) {
            overflow = ((result & 0x80U) != 0) != carry;
        }
        else if (group == 5) {
            overflow = (original & 0x80U) != 0;
        }
        else {
            overflow = false;
        }

        if (overflow) {
            rflags_ |= OF_MASK;
        }
        else {
            rflags_ &= ~OF_MASK;
        }
    }
    else {
        rflags_ &= ~OF_MASK;
    }

    return true;
}

bool Cpu::DecodeShift8Imm(
    std::uint8_t modrm,
    const RexPrefix& rex,
    std::uint8_t rawCount)
{
    const std::uint8_t group =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);

    if (group != 0 && group != 1 &&
        group != 4 && group != 5 && group != 7) {
        return false;
    }

    const std::uint8_t count =
        static_cast<std::uint8_t>(rawCount & 0x1FU);

    if (count == 0) {
        return true;
    }

    const bool isRotate = (group == 0 || group == 1);

    const std::uint8_t effectiveCount =
        isRotate
            ? static_cast<std::uint8_t>(count % 8)
            : count;

    if (isRotate && effectiveCount == 0) {
        return true;
    }

    const std::uint8_t mod =
        static_cast<std::uint8_t>((modrm >> 6) & 0x03);

    std::uint8_t rm =
        static_cast<std::uint8_t>(modrm & 0x07);

    if (mod == 0x03) {

        const bool hasRex =
            rex.present;

        std::uint8_t registerIndex = rm;
        bool highByte = false;

        if (hasRex) {

            if (rex.b) {
                registerIndex =
                    static_cast<std::uint8_t>(
                        registerIndex + 8);
            }

        }
        else if (rm >= 4) {

            registerIndex =
                static_cast<std::uint8_t>(rm - 4);

            highByte = true;
        }

        const std::uint64_t oldValue =
            registers_.Read64(registerIndex);

        std::uint8_t value = 0;

        if (highByte) {
            value =
                static_cast<std::uint8_t>(
                    (oldValue >> 8) & 0xFFU);
        }
        else {
            value =
                static_cast<std::uint8_t>(
                    oldValue & 0xFFU);
        }

        const std::uint8_t original = value;

        bool carry = false;
        std::uint8_t result = value;

        if (group == 0) {
            // ROL
            result = static_cast<std::uint8_t>(
                (value << effectiveCount) |
                (value >> (8 - effectiveCount)));

            carry = (result & 1U) != 0;
        }
        else if (group == 1) {
            // ROR
            result = static_cast<std::uint8_t>(
                (value >> effectiveCount) |
                (value << (8 - effectiveCount)));

            carry = ((result >> 7) & 1U) != 0;
        }
        else if (group == 4) {

            carry =
                ((value >> (8 - count)) & 1U) != 0;

            result =
                static_cast<std::uint8_t>(
                    value << count);
        }
        else if (group == 5) {

            carry =
                ((value >> (count - 1)) & 1U) != 0;

            result =
                static_cast<std::uint8_t>(
                    value >> count);
        }
        else {

            carry =
                ((value >> (count - 1)) & 1U) != 0;

            const std::int8_t signedValue =
                static_cast<std::int8_t>(value);

            result =
                static_cast<std::uint8_t>(
                    signedValue >> count);
        }

        std::uint64_t newValue = oldValue;

        if (highByte) {

            newValue =
                (newValue & ~(0xFFULL << 8)) |
                (static_cast<std::uint64_t>(result) << 8);
        }
        else {

            newValue =
                (newValue & ~0xFFULL) |
                static_cast<std::uint64_t>(result);
        }

        registers_.Write64(
            registerIndex,
            newValue);

        if (!isRotate) {
            SetZeroFlag(result == 0);
            SetSignFlag((result & 0x80U) != 0);
        }

        if (carry) {
            rflags_ |= CF_MASK;
        }
        else {
            rflags_ &= ~CF_MASK;
        }

        const std::uint8_t ofCount =
            isRotate ? effectiveCount : count;

        if (ofCount == 1) {

            bool overflow = false;

            if (group == 0) {
                overflow = ((result >> 7) & 1U) != carry;
            }
            else if (group == 1) {
                overflow =
                    ((result >> 7) & 1U) != ((result >> 6) & 1U);
            }
            else if (group == 4) {

                overflow =
                    ((result & 0x80U) != 0) != carry;
            }
            else if (group == 5) {

                overflow =
                    (original & 0x80U) != 0;
            }
            else {

                overflow = false;
            }

            if (overflow) {
                rflags_ |= OF_MASK;
            }
            else {
                rflags_ &= ~OF_MASK;
            }
        }
        else {

            rflags_ &= ~OF_MASK;
        }

        return true;
    }

    std::uint8_t reg = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(
            modrm,
            rex,
            reg,
            rm,
            address,
            memory)) {
        return false;
    }

    if (!memory) {
        return false;
    }

    return DecodeShift8Memory(rm, address, group, rawCount);
}

bool Cpu::DecodeShift8CL(std::uint8_t modrm, const RexPrefix& rex)
{
    const std::uint8_t count =
        static_cast<std::uint8_t>(
            registers_.Read32(1) & 0xFFU);

    return DecodeShift8Imm(modrm, rex, count);
}

bool Cpu::DecodeShiftLeft64CL(std::uint8_t modrm, const RexPrefix& rex)
{
    if (!rex.w) {
        return false;
    }

    std::uint8_t reg = 0;
    std::uint8_t rm = 0;
    std::uint64_t address = 0;
    bool memory = false;

    if (!DecodeMemoryOrRegister32(modrm, rex, reg, rm, address, memory)) {
        return false;
    }

    const std::uint8_t group = (modrm >> 3) & 0x07;

    if (group != 4 && group != 5 && group != 7) {
        return false;
    }

    
    const std::uint8_t shift = static_cast<std::uint8_t>(registers_.Read32(1) & 0x3F);

    if (shift == 0) {
        return true;
    }

    std::uint64_t value = 0;

    if (memory) {
        if (!ReadMemory(address, reinterpret_cast<std::uint8_t*>(&value), sizeof(value))) {
            return false;
        }
    } else {
        value = registers_.Read64(rm);
    }

    bool carry = false;
    std::uint64_t result = value;

    if (group == 4) {
        
        carry = ((value >> (64 - shift)) & 1ULL) != 0;
        result = value << shift;
    }
    else if (group == 5) {
        
        carry = ((value >> (shift - 1)) & 1ULL) != 0;
        result = value >> shift;
    }
    else {
        
        carry = ((value >> (shift - 1)) & 1ULL) != 0;
        const std::int64_t signedValue = static_cast<std::int64_t>(value);
        result = static_cast<std::uint64_t>(signedValue >> shift);
    }

    if (memory) {
        if (!WriteMemory(address, reinterpret_cast<const std::uint8_t*>(&result), sizeof(result))) {
            return false;
        }
    } else {
        registers_.Write64(rm, result);
    }

    SetZeroFlag(result == 0);
    SetSignFlag((result & 0x8000000000000000ULL) != 0);

    if (carry) {
        rflags_ |= CF_MASK;
    } else {
        rflags_ &= ~CF_MASK;
    }

    
    if (shift == 1) {
    bool overflow = false;

    if (group == 4) {
        overflow =
            ((result >> 63) & 1ULL) !=
            (carry ? 1ULL : 0ULL);
    }
    else if (group == 5) {
        overflow =
            ((value >> 63) & 1ULL) != 0;
    }
    else {
        
        overflow = false;
    }

    if (overflow)
        rflags_ |= OF_MASK;
    else
        rflags_ &= ~OF_MASK;
}
else {
    rflags_ &= ~OF_MASK;
}

    return true;
}

} 






































