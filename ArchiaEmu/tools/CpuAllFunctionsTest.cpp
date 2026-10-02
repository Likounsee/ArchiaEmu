        "SHL8 count >= width is defined by emulator policy",
        RunCode(cpu, mem, code) &&
        (cpu.ReadRegister64(0) & 0xFFU) == 0);
}

void TestOperandSizeOverride()
{
    Memory mem;
    mem.Map(CODE, 0x2000);
    mem.Map(DATA, 0x2000);
    mem.Map(STACK, 0x2000);

    Cpu cpu = MakeCpu(mem);

    // 66 B8 34 12 = MOV AX,1234h. The upper RAX bits must survive.
    auto code = MovR64(0, 0x1122334455667788ULL);
    code.insert(code.end(), {0x66, 0xB8, 0x34, 0x12});

    // 66 BB ABCDh = MOV BX,ABCDh; 66 89 02 = MOV [RDX],AX;
    // 66 8B 0A = MOV CX,[RDX].
    code.insert(code.end(), {0x66, 0xBB, 0xCD, 0xAB});
    code.insert(code.end(), {0x66, 0x89, 0xD8});
    Append(code, MovR64(2, DATA));
    code.insert(code.end(), {0x66, 0x89, 0x02});
    code.insert(code.end(), {0x66, 0x8B, 0x0A});
    code = Finish(code);

    CHECK(
        "66h MOV register/memory 16-bit width",
        RunCode(cpu, mem, code) &&
        cpu.Rax() == 0x112233445566ABCDULL &&
        cpu.ReadRegister64(1) == 0xABCD &&
        Read16(mem, DATA) == 0xABCD);

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x1122334455667788ULL);
        stageCode.insert(stageCode.end(), {0x66, 0xB8, 0x34, 0x12});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV r16,imm16 preserves upper register bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.Rax() == 0x1122334455661234ULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(1, 0x1122334455667788ULL);
        stageCode.insert(stageCode.end(), {0x66, 0xBB, 0xCD, 0xAB});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV r16,imm16 preserves upper register bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.ReadRegister64(1) == 0x112233445566ABCDULL);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        auto stageCode = MovR64(0, 0x112233445566ABCDULL);
        Append(stageCode, MovR64(2, DATA));
        stageCode.insert(stageCode.end(), {0x66, 0x89, 0x02});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV r/m16,r16 stores exactly 16 bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            Read16(stageMem, DATA) == 0xABCD);
    }

    {
        Memory stageMem;
        stageMem.Map(CODE, 0x2000);
        stageMem.Map(DATA, 0x2000);
        stageMem.Map(STACK, 0x2000);
        Cpu stageCpu = MakeCpu(stageMem);
        Write64(stageMem, DATA, 0x112233445566ABCDULL);
        Append(stageCode, MovR64(2, DATA));
        stageCode.insert(stageCode.end(), {0x66, 0x8B, 0x0A});
        stageCode = Finish(stageCode);
        CHECK(
            "66h MOV r16,r/m16 loads exactly 16 bits",
            RunCode(stageCpu, stageMem, stageCode) &&
            stageCpu.ReadRegister64(1) == 0xABCD);
    }
}

int main()
{
    TestCpuAudit();