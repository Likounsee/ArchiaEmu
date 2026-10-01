#include "cpu/x86/Idt.hpp"
#include "cpu/x86/ExceptionDispatcher.hpp"
#include "cpu/CpuException.hpp"

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
    Idt idt;

    if (Idt::kEntryCount != 256 ||
        idt.Limit() != 4095 ||
        idt.IsPresent(0) ||
        idt.IsPresent(255)) {
        return Fail("IDT defaults or limit are invalid") ? 0 : 1;
    }

    IdtGate64 gate{};
    gate.offset = 0x1122334455667788ULL;
    gate.selector = 0x0028;
    gate.ist = 3;
    gate.type = IdtGateType::Trap;
    gate.dpl = 2;
    gate.present = true;

    if (!gate.IsValid() || !idt.SetGate(14, gate)) {
        return Fail("Valid IDT gate was rejected") ? 0 : 1;
    }

    if (!idt.IsPresent(14) ||
        idt.Gate(14).offset != gate.offset ||
        idt.Gate(14).selector != gate.selector ||
        idt.Gate(14).ist != gate.ist ||
        idt.Gate(14).type != gate.type ||
        idt.Gate(14).dpl != gate.dpl) {
        return Fail("IDT gate storage failed") ? 0 : 1;
    }

    const auto bytes = gate.Encode();

    const std::uint8_t expected[] = {
        0x88, 0x77,
        0x28, 0x00,
        0x03,
        0xCF,
        0x66, 0x55,
        0x44, 0x33, 0x22, 0x11,
        0x00, 0x00, 0x00, 0x00
    };

    for (std::size_t i = 0; i < 16; ++i) {
        if (bytes[i] != expected[i]) {
            return Fail("64-bit IDT gate encoding is incorrect") ? 0 : 1;
        }
    }

    const IdtGate64 decoded = IdtGate64::Decode(bytes);
    if (!decoded.IsValid() ||
        decoded.offset != gate.offset ||
        decoded.selector != gate.selector ||
        decoded.ist != gate.ist ||
        decoded.type != gate.type ||
        decoded.dpl != gate.dpl ||
        !decoded.present) {
        return Fail("64-bit IDT gate decoding failed") ? 0 : 1;
    }

    auto invalidBytes = bytes;
    invalidBytes[5] = 0x81; // Present, DPL 0, reserved gate type 1.
    const IdtGate64 invalidDecoded = IdtGate64::Decode(invalidBytes);
    if (invalidDecoded.IsValid() ||
        static_cast<std::uint8_t>(invalidDecoded.type) != 0x01U) {
        return Fail("IDT decode accepted or rewrote an invalid gate type") ? 0 : 1;
    }

    IdtGate64 invalid = gate;
    invalid.dpl = 4;
    if (invalid.IsValid() || idt.SetGate(15, invalid)) {
        return Fail("Invalid IDT DPL was accepted") ? 0 : 1;
    }

    invalid = gate;
    invalid.ist = 8;
    if (invalid.IsValid() || idt.SetGate(15, invalid)) {
        return Fail("Invalid IDT IST was accepted") ? 0 : 1;
    }

    invalid = gate;
    invalid.type = static_cast<IdtGateType>(0x01);
    if (invalid.IsValid() || idt.SetGate(15, invalid)) {
        return Fail("Invalid IDT gate type was accepted") ? 0 : 1;
    }

    IdtGate64 divideGate = gate;
    divideGate.offset = 0x0000000012345678ULL;
    divideGate.type = IdtGateType::Interrupt;
    divideGate.dpl = 0;
    if (!idt.SetGate(
            static_cast<std::uint8_t>(CpuExceptionVector::DivideError),
            divideGate)) {
        return Fail("Failed to install #DE gate") ? 0 : 1;
    }

    ExceptionDispatcher dispatcher(idt);

    CpuException divideException{};
    divideException.kind = CpuExceptionKind::DivideError;
    divideException.vector = CpuExceptionVector::DivideError;

    const auto resolved = dispatcher.Resolve(divideException);
    if (resolved.status != IdtDispatchStatus::Delivered ||
        resolved.vector != 0 ||
        resolved.gate.offset != divideGate.offset) {
        return Fail("#DE was not resolved through the IDT") ? 0 : 1;
    }

    CpuException noVector{};
    noVector.kind = CpuExceptionKind::None;
    noVector.vector = CpuExceptionVector::None;
    if (dispatcher.Resolve(noVector).status != IdtDispatchStatus::NoVector) {
        return Fail("No-vector exception was incorrectly dispatched") ? 0 : 1;
    }

    CpuException missing{};
    missing.kind = CpuExceptionKind::InvalidOpcode;
    missing.vector = CpuExceptionVector::InvalidOpcode;
    if (dispatcher.Resolve(missing).status != IdtDispatchStatus::NotPresent) {
        return Fail("Missing #UD gate was not detected") ? 0 : 1;
    }

    idt.ClearGate(0);

    idt.ClearGate(14);
    if (idt.IsPresent(14) ||
        idt.Gate(14).offset != 0 ||
        idt.Gate(14).selector != 0) {
        return Fail("IDT gate clear failed") ? 0 : 1;
    }

    std::cout << "x86-64 IDT architecture test: PASS\n";
    return 0;
}
