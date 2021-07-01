#include <functional> 
#include "jitunittests.h"

#include "jitblock.h"
#include "jitops.h"
#include "jithelper.h"

namespace dsp56k
{
	JitUnittests::JitUnittests()
	: mem(m_defaultMemoryValidator, 0x100)
	, dsp(mem, &peripherals, &peripherals)
	, m_checks({})
	{
		runTest(&JitUnittests::conversion_build, &JitUnittests::conversion_verify);
		runTest(&JitUnittests::signextend_build, &JitUnittests::signextend_verify);

		runTest(&JitUnittests::ccr_u_build, &JitUnittests::ccr_u_verify);
		runTest(&JitUnittests::ccr_e_build, &JitUnittests::ccr_e_verify);
		runTest(&JitUnittests::ccr_n_build, &JitUnittests::ccr_n_verify);
		runTest(&JitUnittests::ccr_s_build, &JitUnittests::ccr_s_verify);

		runTest(&JitUnittests::agu_build, &JitUnittests::agu_verify);
		runTest(&JitUnittests::agu_modulo_build, &JitUnittests::agu_modulo_verify);
		runTest(&JitUnittests::agu_modulo2_build, &JitUnittests::agu_modulo2_verify);

		runTest(&JitUnittests::transferSaturation_build, &JitUnittests::transferSaturation_verify);

		{
			constexpr auto T=true;
			constexpr auto F=false;

			//                            <  <= =  >= >  != 
			testCCCC(0xff000000000000, 0, T, T, F, F, F, T);
			testCCCC(0x00ff0000000000, 0, F, F, F, T, T, T);
			testCCCC(0x00000000000000, 0, F, T, T, T ,F ,F);
		}

		decode_dddddd_write();
		decode_dddddd_read();

		runTest(&JitUnittests::getSS_build, &JitUnittests::getSS_verify);
		runTest(&JitUnittests::getSetRegs_build, &JitUnittests::getSetRegs_verify);

		runTest(&JitUnittests::abs_build, &JitUnittests::abs_verify);
		
		runTest(&JitUnittests::add_build, &JitUnittests::add_verify);
		runTest(&JitUnittests::addShortImmediate_build, &JitUnittests::addShortImmediate_verify);
		runTest(&JitUnittests::addLongImmediate_build, &JitUnittests::addLongImmediate_verify);
		runTest(&JitUnittests::addl_build, &JitUnittests::addl_verify);
		runTest(&JitUnittests::addr_build, &JitUnittests::addr_verify);

		runTest(&JitUnittests::and_build, &JitUnittests::and_verify);

		runTest(&JitUnittests::andi_build, &JitUnittests::andi_verify);

		asl_D();
		runTest(&JitUnittests::asl_ii_build, &JitUnittests::asl_ii_verify);
		runTest(&JitUnittests::asl_S1S2D_build, &JitUnittests::asl_S1S2D_verify);
		runTest(&JitUnittests::asr_D_build, &JitUnittests::asr_D_verify);
		asr_ii();
		runTest(&JitUnittests::asr_S1S2D_build, &JitUnittests::asr_S1S2D_verify);

		runTest(&JitUnittests::bchg_aa_build, &JitUnittests::bchg_aa_verify);

		runTest(&JitUnittests::bclr_ea_build, &JitUnittests::bclr_ea_verify);
		runTest(&JitUnittests::bclr_aa_build, &JitUnittests::bclr_aa_verify);
		runTest(&JitUnittests::bclr_qqpp_build, &JitUnittests::bclr_qqpp_verify);
		runTest(&JitUnittests::bclr_D_build, &JitUnittests::bclr_D_verify);

		runTest(&JitUnittests::bset_aa_build, &JitUnittests::bset_aa_verify);

		runTest(&JitUnittests::btst_aa_build, &JitUnittests::btst_aa_verify);

		cmp();
		dec();
		div();
		dmac();
		extractu();
		ifcc();
		inc();
		lra();
		lsl();
		lsr();
		lua_ea();
		lua_rn();
		mac_S();
		mpy();
		mpyr();
		mpy_SD();
		neg();
		not_();
		or_();
		rnd();
		rol();
		sub();
		tfr();
		move();
		parallel();
		
		runTest(&JitUnittests::ori_build, &JitUnittests::ori_verify);
		
		runTest(&JitUnittests::clr_build, &JitUnittests::clr_verify);
	}

	void JitUnittests::runTest(void( JitUnittests::* _build)(JitBlock&, JitOps&), void( JitUnittests::* _verify)())
	{
		runTest([&](JitBlock& _b, JitOps& _o)
		{
			(this->*_build)(_b, _o);			
		},[&]()
		{
			(this->*_verify)();
		});
	}

	void JitUnittests::runTest(std::function<void(JitBlock&, JitOps&)> _build, std::function<void()> _verify)
	{
		AsmJitErrorHandler errorHandler;
		asmjit::CodeHolder code;
		AsmJitLogger logger;
		code.init(m_rt.environment());
		code.setLogger(&logger);
		code.setErrorHandler(&errorHandler);

		asmjit::x86::Assembler m_asm(&code);

		{
			JitBlock block(m_asm, dsp);

			JitOps ops(block);

			_build(block, ops);

			ops.updateDirtyCCR();
		}

		m_asm.ret();

		typedef void (*Func)();
		Func func;
		const auto err = m_rt.add(&func, &code);
		if(err)
		{
			const auto* const errString = asmjit::DebugUtils::errorAsString(err);
			LOG("JIT failed: " << err << " - " << errString);
			return;
		}

		func();

		_verify();

		m_rt.release(&func);
	}

	void JitUnittests::nop(JitBlock& _block, size_t _count)
	{
		for(size_t i=0; i<_count; ++i)
			_block.asm_().nop();
	}

	void JitUnittests::conversion_build(JitBlock& _block, JitOps& _ops)
	{
		_block.asm_().bind(_block.asm_().newNamedLabel("test_conv"));

		dsp.regs().x.var = 0xffeedd112233;
		dsp.regs().y.var = 0x112233445566;

		const RegGP r0(_block);
		const RegGP r1(_block);

		_ops.XY0to56(r0, 0);
		_ops.XY1to56(r1, 0);

		_block.mem().mov(m_checks[0], r0);
		_block.mem().mov(m_checks[1], r1);
	}

	void JitUnittests::conversion_verify()
	{
		assert(m_checks[0] == 0x0000112233000000);
		assert(m_checks[1] == 0x00ffffeedd000000);
	}

	void JitUnittests::signextend_build(JitBlock& _block, JitOps& _ops)
	{
		m_checks[0] = 0xabcdef;
		m_checks[1] = 0x123456;
		m_checks[2] = 0xabcdef123456;
		m_checks[3] = 0x123456abcdef;
		m_checks[4] = 0xab123456abcdef;
		m_checks[5] = 0x12123456abcdef;

		const DSPReg regLA(_block, JitDspRegPool::DspLA, true, false);
		const DSPReg regLC(_block, JitDspRegPool::DspLC, true, false);
		const DSPReg regSR(_block, JitDspRegPool::DspSR, true, false);
		const DSPReg regA(_block, JitDspRegPool::DspA, true, false);
		const RegGP ra(_block);
		const RegGP rb(_block);

		_block.mem().mov(regLA, m_checks[0]);
		_block.mem().mov(regLC, m_checks[1]);
		_block.mem().mov(regSR, m_checks[2]);
		_block.mem().mov(regA, m_checks[3]);
		_block.mem().mov(ra, m_checks[4]);
		_block.mem().mov(rb, m_checks[5]);

		_ops.signextend24to56(regLA.get().r64());
		_ops.signextend24to56(regLC.get().r64());
		_ops.signextend48to56(regSR.get().r64());
		_ops.signextend48to56(regA.get().r64());
		_ops.signextend56to64(ra);
		_ops.signextend56to64(rb);

		_block.mem().mov(m_checks[0], regLA);
		_block.mem().mov(m_checks[1], regLC);
		_block.mem().mov(m_checks[2], regSR);
		_block.mem().mov(m_checks[3], regA);
		_block.mem().mov(m_checks[4], ra);
		_block.mem().mov(m_checks[5], rb);
	}

	void JitUnittests::signextend_verify()
	{
		assert(m_checks[0] == 0xffffffffabcdef);
		assert(m_checks[1] == 0x00000000123456);

		assert(m_checks[2] == 0xffabcdef123456);
		assert(m_checks[3] == 0x00123456abcdef);

		assert(m_checks[4] == 0xffab123456abcdef);
		assert(m_checks[5] == 0x0012123456abcdef);
	}

	void JitUnittests::ccr_u_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.sr.var = 0;

		m_checks[0] = 0xee012233445566;
		m_checks[1] = 0xee412233445566;
		m_checks[2] = 0xee812233445566;
		m_checks[3] = 0xeec12233445566;

		for(auto i=0; i<4; ++i)
		{
			RegGP r(_block);

			_block.asm_().mov(r, m_checks[i]);
			_ops.ccr_u_update(r);
			_block.mem().mov(m_checks[i], _block.regs().getSR(JitDspRegs::Read));
		}
	}

	void JitUnittests::ccr_u_verify()
	{
		assert((m_checks[0] & CCR_U));
		assert(!(m_checks[1] & CCR_U));
		assert(!(m_checks[2] & CCR_U));
		assert((m_checks[3] & CCR_U));
	}

	void JitUnittests::ccr_e_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.sr.var = 0;

		m_checks[0] = 0xff812233445566;
		m_checks[1] = 0xff712233445566;
		m_checks[2] = 0x00712233445566;
		m_checks[3] = 0x00812233445566;

		for(auto i=0; i<4; ++i)
		{
			RegGP r(_block);

			_block.asm_().mov(r, m_checks[i]);
			_ops.ccr_e_update(r);
			_block.mem().mov(m_checks[i], _block.regs().getSR(JitDspRegs::Read));

		}
	}

	void JitUnittests::ccr_e_verify()
	{
		assert(!(m_checks[0] & CCR_E));
		assert((m_checks[1] & CCR_E));
		assert(!(m_checks[2] & CCR_E));
		assert((m_checks[3] & CCR_E));
	}

	void JitUnittests::ccr_n_build(JitBlock& _block, JitOps& _ops)
	{
		m_checks[0] = 0xff812233445566;
		m_checks[1] = 0x7f812233445566;

		for(auto i=0; i<2; ++i)
		{
			RegGP r(_block);

			_block.asm_().mov(r, m_checks[i]);
			_ops.ccr_n_update_by55(r);
			_block.mem().mov(m_checks[i], _block.regs().getSR(JitDspRegs::Read));
		}
	}

	void JitUnittests::ccr_n_verify()
	{
		assert((m_checks[0] & CCR_N));
		assert(!(m_checks[1] & CCR_N));
	}

	void JitUnittests::ccr_s_build(JitBlock& _block, JitOps& _ops)
	{
		m_checks[0] = 0xff0fffffffffff;
		m_checks[1] = 0xff2fffffffffff;
		m_checks[2] = 0xff4fffffffffff;
		m_checks[3] = 0xff6fffffffffff;

		dsp.reg.sr.var = 0;

		for(auto i=0; i<4; ++i)
		{
			RegGP r(_block);

			_block.asm_().mov(r, m_checks[i]);
			_block.asm_().mov(_block.regs().getSR(JitDspRegs::Write), asmjit::Imm(0));
			_ops.ccr_s_update(r);
			_block.mem().mov(m_checks[i], _block.regs().getSR(JitDspRegs::Read));
		}
	}

	void JitUnittests::ccr_s_verify()
	{
		assert(m_checks[0] == 0);
		assert(m_checks[1] == CCR_S);
		assert(m_checks[2] == CCR_S);
		assert(m_checks[3] == 0);
	}

	void JitUnittests::agu_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.r[0].var = 0x1000;
		dsp.reg.n[0].var = 0x10;
		dsp.reg.m[0].var = 0xffffff;

		uint32_t ci=0;

		const RegGP temp(_block);

		_ops.updateAddressRegister(temp.get(), MMM_Rn, 0);
		_block.mem().mov(m_checks[ci++], temp);
		_block.regs().getR(temp, 0);
		_block.mem().mov(m_checks[ci++], temp);

		_ops.updateAddressRegister(temp.get(), MMM_RnPlus, 0);
		_block.mem().mov(m_checks[ci++], temp);
		_block.regs().getR(temp, 0);
		_block.mem().mov(m_checks[ci++], temp);

		_ops.updateAddressRegister(temp.get(), MMM_RnMinus, 0);
		_block.mem().mov(m_checks[ci++], temp);
		_block.regs().getR(temp, 0);
		_block.mem().mov(m_checks[ci++], temp);

		_ops.updateAddressRegister(temp.get(), MMM_RnPlusNn, 0);
		_block.mem().mov(m_checks[ci++], temp);
		_block.regs().getR(temp, 0);
		_block.mem().mov(m_checks[ci++], temp);

		_ops.updateAddressRegister(temp.get(), MMM_RnMinusNn, 0);
		_block.mem().mov(m_checks[ci++], temp);
		_block.regs().getR(temp, 0);
		_block.mem().mov(m_checks[ci++], temp);

		_ops.updateAddressRegister(temp.get(), MMM_RnPlusNnUpdate, 0);
		_block.mem().mov(m_checks[ci++], temp);
		_block.regs().getR(temp, 0);
		_block.mem().mov(m_checks[ci++], temp);

		_ops.updateAddressRegister(temp.get(), MMM_MinusRn, 0);
		_block.mem().mov(m_checks[ci++], temp);
		_block.regs().getR(temp, 0);
		_block.mem().mov(m_checks[ci++], temp);
	}

	void JitUnittests::agu_verify()
	{
		assert(m_checks[0 ] == 0x1000);	assert(m_checks[1 ] == 0x1000);
		assert(m_checks[2 ] == 0x1000);	assert(m_checks[3 ] == 0x1001);
		assert(m_checks[4 ] == 0x1001);	assert(m_checks[5 ] == 0x1000);
		assert(m_checks[6 ] == 0x1000);	assert(m_checks[7 ] == 0x1010);
		assert(m_checks[8 ] == 0x1010);	assert(m_checks[9 ] == 0x1000);
		assert(m_checks[10] == 0x1010);	assert(m_checks[11] == 0x1000);
		assert(m_checks[12] == 0x0fff);	assert(m_checks[13] == 0x0fff);
	}

	void JitUnittests::agu_modulo_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.r[0].var = 0x100;
		dsp.reg.n[0].var = 0x200;
		dsp.reg.m[0].var = 0xfff;

		const RegGP temp(_block);

		for(size_t i=0; i<8; ++i)
		{
			m_checks[i] = 0;
			_ops.updateAddressRegister(temp.get(), MMM_RnPlusNn, 0);
			_block.regs().getR(temp, 0);
			_block.mem().mov(m_checks[i], temp);
		}
	}

	void JitUnittests::agu_modulo_verify()
	{
		assert(m_checks[0] == 0x300);
		assert(m_checks[1] == 0x500);
		assert(m_checks[2] == 0x700);
		assert(m_checks[3] == 0x900);
		assert(m_checks[4] == 0xb00);
		assert(m_checks[5] == 0xd00);
		assert(m_checks[6] == 0xf00);
		assert(m_checks[7] == 0x100);
	}

	void JitUnittests::agu_modulo2_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.r[0].var = 0x70;
		dsp.reg.n[0].var = 0x20;
		dsp.reg.m[0].var = 0x100;

		const RegGP temp(_block);

		for(size_t i=0; i<8; ++i)
		{
			_ops.updateAddressRegister(temp.get(), MMM_RnMinusNn, 0);
			_block.regs().getR(temp, 0);
			_block.mem().mov(m_checks[i], temp);
		}
	}

	void JitUnittests::agu_modulo2_verify()
	{
		assert(m_checks[0] == 0x50);
		assert(m_checks[1] == 0x30);
		assert(m_checks[2] == 0x10);
		assert(m_checks[3] == 0xf1);
		assert(m_checks[4] == 0xd1);
		assert(m_checks[5] == 0xb1);
		assert(m_checks[6] == 0x91);
		assert(m_checks[7] == 0x71);
	}

	void JitUnittests::transferSaturation_build(JitBlock& _block, JitOps& _ops)
	{
		const RegGP temp(_block);

		_block.asm_().mov(temp, asmjit::Imm(0x00ff700000555555));
		_ops.transferSaturation(temp);
		_block.mem().mov(m_checks[0], temp);

		_block.asm_().mov(temp, asmjit::Imm(0x00008abbcc555555));
		_ops.transferSaturation(temp);
		_block.mem().mov(m_checks[1], temp);

		_block.asm_().mov(temp, asmjit::Imm(0x0000334455667788));
		_ops.transferSaturation(temp);
		_block.mem().mov(m_checks[2], temp);
	}

	void JitUnittests::transferSaturation_verify()
	{
		assert(m_checks[0] == 0x800000);
		assert(m_checks[1] == 0x7fffff);
		assert(m_checks[2] == 0x334455);
	}

	void JitUnittests::testCCCC(const int64_t _value, const int64_t _compareValue, const bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq)
	{
		dsp.reg.a.var = _value;
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			const RegGP r(_block);
			_block.asm_().mov(r, asmjit::Imm(_compareValue));
			_ops.alu_cmp(0, r, false);

			_ops.decode_cccc(r.get(), CCCC_LessThan);			_block.mem().mov(m_checks[0], r.get());
			_ops.decode_cccc(r.get(), CCCC_LessEqual);			_block.mem().mov(m_checks[1], r.get());
			_ops.decode_cccc(r.get(), CCCC_Equal);				_block.mem().mov(m_checks[2], r.get());
			_ops.decode_cccc(r.get(), CCCC_GreaterEqual);		_block.mem().mov(m_checks[3], r.get());
			_ops.decode_cccc(r.get(), CCCC_GreaterThan);		_block.mem().mov(m_checks[4], r.get());
			_ops.decode_cccc(r.get(), CCCC_NotEqual);			_block.mem().mov(m_checks[5], r.get());
		}, [&]()
		{
			assert(_lt == (dsp.decode_cccc(CCCC_LessThan) != 0));
			assert(_le == (dsp.decode_cccc(CCCC_LessEqual) != 0));
			assert(_eq == (dsp.decode_cccc(CCCC_Equal) != 0));
			assert(_ge == (dsp.decode_cccc(CCCC_GreaterEqual) != 0));
			assert(_gt == (dsp.decode_cccc(CCCC_GreaterThan) != 0));
			assert(_neq == (dsp.decode_cccc(CCCC_NotEqual) != 0));	

			assert(_lt  == (m_checks[0] != 0));
			assert(_le  == (m_checks[1] != 0));
			assert(_eq  == (m_checks[2] != 0));
			assert(_ge  == (m_checks[3] != 0));
			assert(_gt  == (m_checks[4] != 0));
			assert(_neq == (m_checks[5] != 0));	
		}
		);
	}

	void JitUnittests::decode_dddddd_write()
	{
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			m_checks.fill(0);

			const RegGP r(_block);

			for(int i=0; i<8; ++i)
			{
				const TWord inc = i * 0x10000;

				_ops.emit(0, 0x60f400 + inc, 0x110000 * (i+1));		// move #$110000,ri
				_block.regs().getR(r, i);
				_block.mem().mov(m_checks[i], r.get().r32());

				_ops.emit(0, 0x70f400 + inc, 0x001100 * (i+1));		// move #$001100,ni
				_block.regs().getN(r, i);
				_block.mem().mov(m_checks[i+8], r.get().r32());

				_ops.emit(0, 0x05f420 + i, 0x000011 * (i+1));		// move #$000011,mi
				_block.regs().getM(r, i);
				_block.mem().mov(m_checks[i+16], r.get().r32());
			}
		}, [&]()
		{
			for(size_t i=0; i<8; ++i)
			{
				assert(m_checks[i   ] == 0x110000 * (i+1));
				assert(m_checks[i+8 ] == 0x001100 * (i+1));
				assert(m_checks[i+16] == 0x000011 * (i+1));
			}
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			m_checks.fill(0);

			int i=0;
			const RegGP r(_block);

			_ops.emit(0, 0x50f400, 0x111111);	// move #$111111,a0
			_ops.emit(0, 0x51f400, 0x222222);	// move #$222222,b0

			_block.regs().getALU(r, 0);	_block.mem().mov(m_checks[i++], r.get());
			_block.regs().getALU(r, 1);	_block.mem().mov(m_checks[i++], r.get());

			_ops.emit(0, 0x54f400, 0x111111);	// move #$111111,a1
			_ops.emit(0, 0x55f400, 0x222222);	// move #$222222,b1

			_block.regs().getALU(r, 0);	_block.mem().mov(m_checks[i++], r.get());
			_block.regs().getALU(r, 1);	_block.mem().mov(m_checks[i++], r.get());

			_ops.emit(0, 0x56f400, 0x111111);	// move #$111111,a
			_ops.emit(0, 0x57f400, 0x222222);	// move #$222222,b

			_block.regs().getALU(r, 0);	_block.mem().mov(m_checks[i++], r.get());
			_block.regs().getALU(r, 1);	_block.mem().mov(m_checks[i++], r.get());

			_ops.emit(0, 0x52f400, 0x111111);	// move #$111111,a2
			_ops.emit(0, 0x53f400, 0x222222);	// move #$222222,b2

			_block.regs().getALU(r, 0);	_block.mem().mov(m_checks[i++], r.get());
			_block.regs().getALU(r, 1);	_block.mem().mov(m_checks[i++], r.get());
		}, [&]()
		{
			int i=0;
			assert(m_checks[i++] == 0x00000000111111);
			assert(m_checks[i++] == 0x00000000222222);
			assert(m_checks[i++] == 0x00111111111111);
			assert(m_checks[i++] == 0x00222222222222);
			assert(m_checks[i++] == 0x00111111000000);
			assert(m_checks[i++] == 0x00222222000000);
			assert(m_checks[i++] == 0x11111111000000);
			assert(m_checks[i++] == 0x22222222000000);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			m_checks.fill(0);
			dsp.x0(0xaaaaaa);
			dsp.x1(0xbbbbbb);
			dsp.y0(0xcccccc);
			dsp.y1(0xdddddd);

			int i=0;
			const RegGP r(_block);

			_ops.emit(0, 0x44f400, 0x111111);	// move #$111111,x0
			_block.regs().getXY(r, 0);
			_block.mem().mov(m_checks[i++], r.get());

			_ops.emit(0, 0x45f400, 0x222222);	// move #$222222,x1
			_block.regs().getXY(r, 0);
			_block.mem().mov(m_checks[i++], r.get());

			_ops.emit(0, 0x46f400, 0x333333);	// move #$333333,y0
			_block.regs().getXY(r, 1);
			_block.mem().mov(m_checks[i++], r.get());

			_ops.emit(0, 0x47f400, 0x444444);	// move #$444444,y1
			_block.regs().getXY(r, 1);
			_block.mem().mov(m_checks[i++], r.get());

		}, [&]()
		{
			int i=0;
			assert(m_checks[i++] == 0xbbbbbb111111);
			assert(m_checks[i++] == 0x222222111111);
			assert(m_checks[i++] == 0xdddddd333333);
			assert(m_checks[i++] == 0x444444333333);
		});
	}

	void JitUnittests::decode_dddddd_read()
	{
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			m_checks.fill(0);

			const RegGP r(_block);

			for(int i=0; i<8; ++i)
			{
				const TWord inc = i * 0x10000;

				dsp.reg.r[i].var = (i+1) * 0x110000;
				dsp.reg.n[i].var = (i+1) * 0x001100;
				dsp.reg.m[i].var = (i+1) * 0x000011;

				_ops.emit(0, 0x600500 + inc);						// asm move ri,x:$5
				_block.mem().readDspMemory(r, MemArea_X, 0x5);
				_block.mem().mov(m_checks[i], r.get().r32());

				_ops.emit(0, 0x700500 + inc);						// asm move ni,x:$5
				_block.mem().readDspMemory(r, MemArea_X, 0x5);
				_block.mem().mov(m_checks[i+8], r.get().r32());

				_ops.emit(0, 0x050520 + i);							// asm move mi,x:$5
				_block.mem().readDspMemory(r, MemArea_X, 0x5);
				_block.mem().mov(m_checks[i+16], r.get().r32());
			}
		}, [&]()
		{
			for(size_t i=0; i<8; ++i)
			{
				assert(m_checks[i   ] == 0x110000 * (i+1));
				assert(m_checks[i+8 ] == 0x001100 * (i+1));
				assert(m_checks[i+16] == 0x000011 * (i+1));
			}
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			m_checks.fill(0);
			dsp.reg.a.var = 0x11112233445566;
			dsp.reg.b.var = 0xff5566778899aa;

			int i=0;
			const RegGP r(_block);

			_ops.emit(0, 0x560500);								// a,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());

			_ops.emit(0, 0x570500);								// b,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());

			_ops.emit(0, 0x500500);								// a0,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());

			_ops.emit(0, 0x540500);								// a1,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());

			_ops.emit(0, 0x520500);								// a2,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());

			_ops.emit(0, 0x510500);								// b0,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());

			_ops.emit(0, 0x550500);								// b1,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());

			_ops.emit(0, 0x530500);								// b2,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());
		}, [&]()
		{
			int i=0;
			assert(m_checks[i++] == 0x7fffff); // a
			assert(m_checks[i++] == 0x800000); // b
			assert(m_checks[i++] == 0x445566); // a0
			assert(m_checks[i++] == 0x112233); // a1
			assert(m_checks[i++] == 0x000011); // a2
			assert(m_checks[i++] == 0x8899aa); // b0
			assert(m_checks[i++] == 0x556677); // b1
			assert(m_checks[i++] == 0xffffff); // b2
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			m_checks.fill(0);
			dsp.x0(0xaaabbb);
			dsp.x1(0xcccddd);
			dsp.y0(0xeeefff);
			dsp.y1(0x111222);

			int i=0;
			const RegGP r(_block);

			_ops.emit(0, 0x440500);								// move x0,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());
			_ops.emit(0, 0x450500);								// move x1,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());
			_ops.emit(0, 0x460500);								// move y0,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());
			_ops.emit(0, 0x470500);								// move y1,x:$5
			_block.mem().readDspMemory(r, MemArea_X, 0x5);
			_block.mem().mov(m_checks[i++], r.get().r32());
		}, [&]()
		{
			int i=0;
			assert(m_checks[i++] == 0xaaabbb);
			assert(m_checks[i++] == 0xcccddd);
			assert(m_checks[i++] == 0xeeefff);
			assert(m_checks[i++] == 0x111222);
		});
	}

	void JitUnittests::getSS_build(JitBlock& _block, JitOps& _ops)
	{
		const RegGP temp(_block);

		dsp.reg.sp.var = 0xf0;

		for(size_t i=0; i<dsp.reg.ss.eSize; ++i)
		{			
			dsp.reg.ss[i].var = 0x111111111111 * i;

			_block.regs().getSS(temp);
			_block.regs().incSP();
			_block.mem().mov(m_checks[i], temp);
		}
	}

	void JitUnittests::getSS_verify()
	{
		for(size_t i=0; i<dsp.reg.ss.eSize; ++i)
			assert(dsp.reg.ss[i].var == 0x111111111111 * i);
	}

	void JitUnittests::getSetRegs_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.ep.var = 0x112233;
		dsp.reg.vba.var = 0x223344;
		dsp.reg.sc.var = 0x33;
		dsp.reg.sz.var = 0x556677;
		dsp.reg.omr.var = 0x667788;
		dsp.reg.sp.var = 0x778899;
		dsp.reg.la.var = 0x8899aa;
		dsp.reg.lc.var = 0x99aabb;

		dsp.reg.a.var = 0x00ffaabbcc112233;
		dsp.reg.b.var = 0x00ee112233445566;

		dsp.reg.x.var = 0x0000aabbccddeeff;
		dsp.reg.y.var = 0x0000112233445566;

		const RegGP temp(_block);
		const auto r = temp.get().r32();

		auto& regs = _block.regs();

		auto modify = [&]()
		{
			_block.asm_().shl(temp, asmjit::Imm(4));
			_block.asm_().and_(temp, asmjit::Imm(0xffffff));
		};

		auto modify64 = [&]()
		{
			_block.asm_().shl(temp, asmjit::Imm(4));
		};

		regs.getEP(r);				modify();		regs.setEP(r);
		regs.getVBA(r);				modify();		regs.setVBA(r);
		regs.getSC(r);				modify();		regs.setSC(r);
		regs.getSZ(r);				modify();		regs.setSZ(r);
		regs.getOMR(r);				modify();		regs.setOMR(r);
		regs.getSP(r);				modify();		regs.setSP(r);
		regs.getLA(r);				modify();		regs.setLA(r);
		regs.getLC(r);				modify();		regs.setLC(r);

		regs.getALU0(r, 0);			modify64();		regs.setALU0(0,r);
		regs.getALU1(r, 0);			modify64();		regs.setALU1(0,r);
		regs.getALU2signed(r, 0);	modify64();		regs.setALU2(0,r);

		regs.getALU(temp, 1);		modify64();		regs.setALU(1, temp);

		regs.getXY0(r, 0);			modify();		regs.setXY0(0, r);
		regs.getXY1(r, 0);			modify();		regs.setXY1(0, r);

		regs.getXY(r, 1);			modify64();		regs.setXY(1, r);
	}

	void JitUnittests::getSetRegs_verify()
	{
		auto& r = dsp.regs();

		assert(r.ep.var == 0x122330);
		assert(r.vba.var == 0x233440);
		assert(r.sc.var == 0x30);
		assert(r.sz.var == 0x566770);
		assert(r.omr.var == 0x677880);
		assert(r.sp.var == 0x788990);
		assert(r.la.var == 0x899aa0);
		assert(r.lc.var == 0x9aabb0);

		assert(r.a.var == 0x00f0abbcc0122330);
		assert(r.b.var == 0x00e1122334455660);

		assert(r.x.var == 0x0000abbcc0deeff0);
		assert(r.y.var == 0x0000122334455660);
	}

	void JitUnittests::abs_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.regs().a.var = 0x00ff112233445566;
		dsp.regs().b.var = 0x0000aabbccddeeff;

		_ops.op_Abs(0x000000);
		_ops.op_Abs(0xffffff);
	}

	void JitUnittests::abs_verify()
	{
		assert(dsp.regs().a == 0x00EEDDCCBBAA9A);
		assert(dsp.regs().b == 0x0000aabbccddeeff);
	}

	void JitUnittests::add_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.regs().a.var = 0x0001e000000000;
		dsp.regs().b.var = 0xfffe2000000000;

		// add b,a
		_ops.emit(0, 0x200010);
	}

	void JitUnittests::add_verify()
	{
		assert(dsp.regs().a.var == 0);
		assert(dsp.sr_test(CCR_C));
		assert(dsp.sr_test(CCR_Z));
		assert(!dsp.sr_test(CCR_V));
	}

	void JitUnittests::addShortImmediate_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0;

		// add #<32,a
		_ops.emit(0, 0x017280);
	}

	void JitUnittests::addShortImmediate_verify()
	{
		assert(dsp.regs().a.var == 0x00000032000000);
		assert(!dsp.sr_test(CCR_C));
		assert(!dsp.sr_test(CCR_Z));
		assert(!dsp.sr_test(CCR_V));
	}

	void JitUnittests::addLongImmediate_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0;
		dsp.reg.pc.var = 0;

		// add #>32,a, two op add with immediate in extension word
		_ops.emit(0, 0x0140c0, 0x000032);
	}

	void JitUnittests::addLongImmediate_verify()
	{
		assert(dsp.regs().a.var == 0x00000032000000);
		assert(!dsp.sr_test(CCR_C));
		assert(!dsp.sr_test(CCR_Z));
		assert(!dsp.sr_test(CCR_V));
	}

	void JitUnittests::addl_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0x222222;
		dsp.reg.b.var = 0x333333;

		_ops.emit(0, 0x20001a);
	}

	void JitUnittests::addl_verify()
	{
		assert(dsp.reg.b.var == 0x888888);
		assert(!dsp.sr_test(CCR_C));
		assert(!dsp.sr_test(CCR_Z));
		assert(!dsp.sr_test(CCR_V));
	}

	void JitUnittests::addr_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0x004edffe000000;
		dsp.reg.b.var = 0xff89fe13000000;
		dsp.setSR(0x0800d0);							// (S L) U

		_ops.emit(0, 0x200002);	// addr b,a
	}

	void JitUnittests::addr_verify()
	{
		assert(dsp.reg.a.var == 0x0ffb16e12000000);
		assert(dsp.getSR().var == 0x0800c8);			// (S L) N
	}

	void JitUnittests::and_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0xffcccccc112233;
		dsp.reg.x.var = 0x777777;

		dsp.reg.b.var = 0xaaaabbcc334455;
		dsp.reg.y.var = 0x667788000000;

		_ops.emit(0, 0x200046);	// and x0,a
		_ops.emit(0, 0x20007e);	// and y1,b
	}

	void JitUnittests::and_verify()
	{
		assert(dsp.reg.a.var == 0xff444444112233);
		assert(dsp.reg.b.var == 0xaa223388334455);
	}

	void JitUnittests::andi_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.omr.var = 0xff6666;
		dsp.reg.sr.var = 0xff6666;

		_ops.emit(0, 0x0033ba);	// andi #$33,omr
		_ops.emit(0, 0x0033bb);	// andi #$33,eom
		_ops.emit(0, 0x0033b9);	// andi #$33,ccr
		_ops.emit(0, 0x0033b8);	// andi #$33,mr
	}

	void JitUnittests::andi_verify()
	{
		assert(dsp.reg.omr.var == 0xff2222);
		assert(dsp.reg.sr.var == 0xff2222);
	}

	void JitUnittests::asl_D()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0xaaabcdef123456;
			dsp.reg.sr.var = 0;

			_ops.emit(0, 0x200032);	// asl a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x55579bde2468ac);
			assert(!dsp.sr_test_noCache(CCR_Z));
			assert(dsp.sr_test_noCache(CCR_V));
			assert(dsp.sr_test_noCache(CCR_C));
		});		

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0x00400000000000;
			dsp.reg.sr.var = 0;
			_ops.emit(0, 0x200032);	// asl a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x00800000000000);
			assert(!dsp.sr_test_noCache(CCR_Z));
			assert(!dsp.sr_test_noCache(CCR_V));
			assert(!dsp.sr_test_noCache(CCR_C));
		});		
	}

	void JitUnittests::asl_ii_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0xaaabcdef123456;
		dsp.reg.sr.var = 0;
		_ops.emit(0, 0x0c1d02);	// asl #1,a,a
	}

	void JitUnittests::asl_ii_verify()
	{
		assert(dsp.reg.a.var == 0x55579bde2468ac);
		assert(!dsp.sr_test_noCache(CCR_Z));
		assert(dsp.sr_test_noCache(CCR_V));
		assert(dsp.sr_test_noCache(CCR_C));
	}

	void JitUnittests::asl_S1S2D_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.x.var = ~0;
		dsp.reg.y.var = ~0;
		dsp.x0(0x4);
		dsp.y1(0x8);

		dsp.reg.a.var = 0x0011aabbccddeeff;
		dsp.reg.b.var = 0x00ff112233445566;
		
		_ops.emit(0, 0x0c1e48);	// asl x0,a,a
		_ops.emit(0, 0x0c1e5f);	// asl y1,b,b
	}

	void JitUnittests::asl_S1S2D_verify()
	{
		assert(dsp.reg.a.var == 0x001aabbccddeeff0);
		assert(dsp.reg.b.var == 0x0011223344556600);
	}

	void JitUnittests::asr_D_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0x000599f2204000;
		dsp.reg.sr.var = 0;

		_ops.emit(0, 0x200022);	// asr a
	}

	void JitUnittests::asr_D_verify()
	{
		assert(dsp.reg.a.var == 0x0002ccf9102000);
	}

	void JitUnittests::asr_ii()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0x000599f2204000;
			_ops.emit(0, 0x0c1c02);	// asr #1,a,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x0002ccf9102000);
		});		

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0xfffffdff000000;
			_ops.emit(0, 0x0c1c2a);	// asr #15,a,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xffffffffffeff8);
		});		
	}

	void JitUnittests::asr_S1S2D_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.x.var = ~0;
		dsp.reg.y.var = ~0;
		dsp.x0(0x4);
		dsp.y1(0x8);

		dsp.reg.a.var = 0x0011aabbccddeeff;
		dsp.reg.b.var = 0x00ff112233445566;
		
		_ops.emit(0, 0x0c1e68);	// asr x0,a,a
		_ops.emit(0, 0x0c1e7f);	// asr y1,b,b
	}

	void JitUnittests::asr_S1S2D_verify()
	{
		assert(dsp.reg.a.var == 0x00011aabbccddeef);
		assert(dsp.reg.b.var == 0x00ffff1122334455);
	}

	void JitUnittests::bchg_aa_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.mem.set(MemArea_X, 0x2, 0x556677);
		dsp.mem.set(MemArea_Y, 0x3, 0xddeeff);

		_ops.emit(0, 0x0b0203);	// bchg #$3,x:<$2
		_ops.emit(0, 0x0b0343);	// bchg #$3,y:<$3
	}

	void JitUnittests::bchg_aa_verify()
	{
		const auto x = dsp.mem.get(MemArea_X, 0x2);
		const auto y = dsp.mem.get(MemArea_Y, 0x3);
		assert(x == 0x55667f);
		assert(y == 0xddeef7);
	}

	void JitUnittests::bclr_ea_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.mem.set(MemArea_X, 0x11, 0xffffff);
		dsp.mem.set(MemArea_Y, 0x22, 0xffffff);

		dsp.reg.r[0].var = 0x11;
		dsp.reg.r[1].var = 0x22;

		dsp.reg.n[0].var = dsp.reg.n[1].var = 0;
		dsp.reg.m[0].var = dsp.reg.m[1].var = 0xffffff;

		_ops.emit(0, 0xa6014);	// bclr #$14,x:(r0)
		_ops.emit(0, 0xa6150);	// bclr #$10,y:(r1)
	}

	void JitUnittests::bclr_ea_verify()
	{
		const auto x = dsp.mem.get(MemArea_X, 0x11);
		const auto y = dsp.mem.get(MemArea_Y, 0x22);
		assert(x == 0xefffff);
		assert(y == 0xfeffff);
	}

	void JitUnittests::bclr_aa_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.mem.set(MemArea_X, 0x11, 0xffaaaa);
		dsp.mem.set(MemArea_Y, 0x22, 0xffbbbb);

		_ops.emit(0, 0xa1114);	// bclr #$14,x:<$11
		_ops.emit(0, 0xa2250);	// bclr #$10,y:<$22
	}

	void JitUnittests::bclr_aa_verify()
	{
		const auto x = dsp.mem.get(MemArea_X, 0x11);
		const auto y = dsp.mem.get(MemArea_Y, 0x22);
		assert(x == 0xefaaaa);
		assert(y == 0xfebbbb);
	}

	void JitUnittests::bclr_qqpp_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.getPeriph(0)->write(0xffff90, 0x334455);
		dsp.getPeriph(0)->write(0xffffd0, 0x556677);

		_ops.emit(0, 0x11002);	// bclr #$2,x:<<$ffff90	- bclr_qq
		_ops.emit(0, 0xa9004);	// bclr #$4,x:<<$ffffd0 - bclr_pp
	}

	void JitUnittests::bclr_qqpp_verify()
	{
		const auto a = dsp.getPeriph(0)->read(0xffff90);
		const auto b = dsp.getPeriph(0)->read(0xffffd0);
		assert(a == 0x334451);	// bit 2 cleared
		assert(b == 0x556667);	// bit 4 cleared
	}

	void JitUnittests::bclr_D_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.omr.var = 0xddeeff;
		_ops.emit(0, 0x0afa47);	// bclr #$7,omr
	}

	void JitUnittests::bclr_D_verify()
	{
		assert(dsp.reg.omr.var == 0xddee7f);
	}

	void JitUnittests::bset_aa_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.mem.set(MemArea_X, 0x2, 0x55667f);
		dsp.mem.set(MemArea_Y, 0x3, 0xddeef0);

		_ops.emit(0, 0x0a0223);	// bset #$3,x:<$2
		_ops.emit(0, 0x0a0363);	// bset #$3,y:<$3
	}

	void JitUnittests::bset_aa_verify()
	{
		const auto x = dsp.mem.get(MemArea_X, 0x2);
		const auto y = dsp.mem.get(MemArea_Y, 0x3);
		assert(x == 0x55667f);
		assert(y == 0xddeef8);
	}

	void JitUnittests::btst_aa_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.mem.set(MemArea_X, 0x2, 0xaabbc4);

		_ops.emit(0, 0x0b0222);	// bset #$2,x:<$2
		_block.asm_().mov(_block.regs().getSR(JitDspRegs::ReadWrite), m_checks[0]);
		_ops.emit(0, 0x0b0223);	// bset #$3,x:<$2
		_block.asm_().mov(_block.regs().getSR(JitDspRegs::ReadWrite), m_checks[1]);	// TODO: args swapped! Why doesn't the test fail???
	}

	void JitUnittests::btst_aa_verify()
	{
		assert((m_checks[0] & CCR_C) == 0);
		assert((m_checks[1] & CCR_C) != 0);
	}

	void JitUnittests::cmp()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.b.var = 0;
			dsp.b1(TReg24(0x123456));

			dsp.reg.x.var = 0;
			dsp.x0(TReg24(0x123456));

			_ops.emit(0, 0x20004d);		// cmp x0,b
		},
		[&]()
		{
			assert(dsp.sr_test(CCR_Z));
			assert(!dsp.sr_test(static_cast<CCRMask>(CCR_N | CCR_E | CCR_V | CCR_C)));			
		});
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.x0(0xf00000);
			dsp.reg.a.var = 0xfff40000000000;
			dsp.setSR(0x0800d8);

			_ops.emit(0, 0x200045);	// cmp x0,a
		},
		[&]()
		{
			assert(dsp.getSR().var == 0x0800d0);
		});
		runTest([&](auto& _block, auto& _ops)
		{
			
			dsp.setSR(0x080099);
			dsp.reg.a.var = 0xfffffc6c000000;
			_ops.emit(0, 0x0140c5, 0x0000aa);	// cmp #>$aa,a
		},
		[&]()
		{
			assert(dsp.getSR().var == 0x080098);
		});
	}

	void JitUnittests::dec()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 2;
			_ops.emit(0, 0x00000a);		// dec a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 1);
			assert(!dsp.sr_test(static_cast<CCRMask>(CCR_Z | CCR_N | CCR_E | CCR_V | CCR_C)));
		});
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 1;
			_ops.emit(0, 0x00000a);		// dec a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0);
			assert(dsp.sr_test(CCR_Z));
			assert(!dsp.sr_test(static_cast<CCRMask>(CCR_N | CCR_E | CCR_V | CCR_C)));
		});
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0;
			_ops.emit(0, 0x00000a);		// dec a
		},
		[&]()
		{
			assert(dsp.sr_test(static_cast<CCRMask>(CCR_N | CCR_C)));
			assert(!dsp.sr_test(static_cast<CCRMask>(CCR_Z | CCR_E | CCR_V)));
		});
	}

	void JitUnittests::div()
	{
		constexpr uint64_t expectedValues[24] =
		{
			0xffef590e000000,
			0xffef790e000000,
			0xffefb90e000000,
			0xfff0390e000000,
			0xfff1390e000000,
			0xfff3390e000000,
			0xfff7390e000000,
			0xffff390e000000,
			0x000f390e000000,
			0x000dab2a000001,
			0x000a8f62000003,
			0x000457d2000007,
			0xfff7e8b200000f,
			0x0000985600001e,
			0xfff069ba00003d,
			0xfff19a6600007a,
			0xfff3fbbe0000f4,
			0xfff8be6e0001e8,
			0x000243ce0003d0,
			0xfff3c0aa0007a1,
			0xfff84846000f42,
			0x0001577e001e84,
			0xfff1e80a003d09,
			0xfff49706007a12
		};

		dsp.setSR(dsp.getSR().var & 0xfe);
		dsp.reg.a.var = 0x00001000000000;
		dsp.reg.y.var =   0x04444410c6f2;

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			for(size_t i=0; i<24; ++i)
			{
				_ops.emit(0, 0x018050);		// div y0,a
				RegGP r(_block);
				_block.regs().getALU(r, 0);
				_block.mem().mov(m_checks[i], r.get());
			}
		},
		[&]()
		{
			for(size_t i=0; i<24; ++i)
			{
				assert(m_checks[i] == expectedValues[i]);
			}
		});

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.y0(0x218dec);
			dsp.reg.a.var = 0x00008000000000;
			dsp.setSR(0x0800d4);
			_ops.emit(0, 0x018050);		// div y0,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xffdf7214000000);
			assert(dsp.getSR().var == 0x0800d4);		
		});		
	}

	void JitUnittests::dmac()
	{
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0;
			dsp.x1(0x000020);
			dsp.y1(0x000020);
			_ops.emit(0, 0x01248f);		// dmacss x1,y1,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x800);
		});
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0xfff00000000000;
			dsp.x1(0x000020);
			dsp.y1(0x000020);
			_ops.emit(0, 0x01248f);		// dmacss x1,y1,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xfffffffff00800);
		});
	}

	void JitUnittests::extractu()
	{
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.x.var = 0x4008000000;  // x1 = 0x4008  (width=4, offset=8)
			dsp.reg.a.var = 0xff00;
			dsp.reg.b.var = 0;

			// extractu x1,a,b  (width = 0x8, offset = 0x28)
			_ops.emit(0, 0x0c1a8d);
		},
		[&]()
		{
			assert(dsp.reg.b.var == 0xf);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0;
			dsp.reg.b.var = 0xfff47555000000;
			dsp.setSR(0x0800d9);

			// extractu $8028,b,a
			_ops.emit(0, 0x0c1890, 0x008028);
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xf4);
			assert(dsp.getSR().var == 0x0800d0);
		});
	}

	void JitUnittests::inc()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0x00ffffffffffffff;
			_ops.emit(0, 0x000008);		// inc a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0);
			assert(dsp.sr_test(static_cast<CCRMask>(CCR_C | CCR_Z)));
			assert(!dsp.sr_test(static_cast<CCRMask>(CCR_N | CCR_E | CCR_V)));
		});
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 1;
			_ops.emit(0, 0x000008);		// inc a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 2);
			assert(!dsp.sr_test(static_cast<CCRMask>(CCR_Z | CCR_N | CCR_E | CCR_V | CCR_C)));
		});
	}

	void JitUnittests::lra()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.n[0].var = 0x4711;
			_ops.emit(0x20, 0x044058, 0x00000a);				// lra >*+$a,n0
		},
		[&]()
		{
			assert(dsp.reg.n[0].var == 0x2a);
		});
	}

	void JitUnittests::lsl()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0xffaabbcc112233;
			_ops.emit(0, 0x200033);				// lsl a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xff557798112233);
			assert(dsp.sr_test(CCR_C));
		});

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0xffaabbcc112233;
			_ops.emit(0, 0x0c1e88);				// lsl #$4,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xffabbcc0112233);
			assert(!dsp.sr_test(CCR_C));
		});
	}

	void JitUnittests::lsr()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0xffaabbcc112233;
			_ops.emit(0, 0x200023);				// lsr a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xff555de6112233);
			assert(!dsp.sr_test(CCR_C));
		});

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0xffaabbcc112233;
			_ops.emit(0, 0x0c1ec8);				// lsr #$4,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xff0aabbc112233);
			assert(dsp.sr_test(CCR_C));
		});
	}

	void JitUnittests::lua_ea()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.r[0].var = 0x112233;
			dsp.reg.n[0].var = 0x001111;
			_ops.emit(0, 0x045818);				// lua (r0)+,n0
		},
		[&]()
		{
			assert(dsp.reg.r[0].var == 0x112233);
			assert(dsp.reg.n[0].var == 0x112234);
		});

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.r[0].var = 0x112233;
			dsp.reg.n[0].var = 0x001111;
			_ops.emit(0, 0x044818);				// lua (r0)+n0,n0
		},
		[&]()
		{
			assert(dsp.reg.r[0].var == 0x112233);
			assert(dsp.reg.n[0].var == 0x113344);
		});
	}

	void JitUnittests::lua_rn()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.r[0].var = 0x0000f0;
			dsp.reg.m[0].var = 0xffffff;

			_ops.emit(0, 0x04180b);				// lua (r0+$30),n3
		},
		[&]()
		{
			assert(dsp.reg.r[0].var == 0x0000f0);
			assert(dsp.reg.n[3].var == 0x000120);
		});
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.r[0].var = 0x0000f0;
			dsp.reg.m[0].var = 0x0000ff;

			_ops.emit(0, 0x04180b);				// lua (r0+$30),n3
		},
		[&]()
		{
			assert(dsp.reg.r[0].var == 0x0000f0);
			assert(dsp.reg.n[3].var == 0x000020);
		});
	}

	void JitUnittests::mac_S()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.x1(0x2);
			dsp.reg.a.var = 0x100;

			_ops.emit(0, 0x0102f2);				// mac x1,#$2,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x00000000800100);
		});
	}

	void JitUnittests::mpy()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.x0(0x20);
			dsp.x1(0x20);

			_ops.emit(0, 0x2000a0);				// mpy x0,x1,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x000800);
		});

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.x0(0xffffff);
			dsp.x1(0xffffff);

			_ops.emit(0, 0x2000a0);				// mpy x0,x1,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x2);
		});		
	}

	void JitUnittests::mpyr()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.x0(0xef4e);
			dsp.y0(0x600000);
			dsp.setSR(0x0880d0);
			dsp.reg.omr.var = 0x004380;

			_ops.emit(0, 0x2000d1);				// mpyr y0,x0,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x0000b37a000000);
		});		
	}

	void JitUnittests::mpy_SD()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.x1(0x2);
			dsp.reg.a.var = 0;

			_ops.emit(0, 0x0102f0);				// mpy x1,#$2,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x00000000800000);
		});
	}

	void JitUnittests::neg()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 1;

			_ops.emit(0, 0x200036);				// neg a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xffffffffffffff);
			assert(dsp.sr_test(CCR_N));
			assert(!dsp.sr_test(CCR_Z));
		});

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0xfffffffffffffe;

			_ops.emit(0, 0x200036);				// neg a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 2);
			assert(!dsp.sr_test(CCR_N));
			assert(!dsp.sr_test(CCR_Z));
		});
	}

	void JitUnittests::not_()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0x12555555123456;
			_ops.emit(0, 0x200017);	// not a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x12aaaaaa123456);
		});

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0xffd8b38b000000;
			dsp.setSR(0x0800e8);
			_ops.emit(0, 0x200017);	// not a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xff274c74000000);
			assert(dsp.reg.sr.var == 0x0800e0);
		});
	}

	void JitUnittests::or_()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0xee222222555555;
			dsp.x0(0x444444);
			_ops.emit(0, 0x200042);				// or x0,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xee666666555555);
		});
	}

	void JitUnittests::rnd()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0x00222222333333;

			_ops.emit(0, 0x200011);				// rnd a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x00222222000000);
		});

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0x00222222999999;

			_ops.emit(0, 0x200011);				// rnd a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x00222223000000);
		});		

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.b.var = 0xffff9538000000;

			_ops.emit(0, 0x200019);				// rnd b
		},
		[&]()
		{
			assert(dsp.reg.b.var == 0xffff9538000000);
			assert(dsp.sr_test(CCR_N));
			assert(!dsp.sr_test(CCR_Z));
			assert(!dsp.sr_test(CCR_V));
		});		
	}

	void JitUnittests::rol()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.sr.var = 0;
			dsp.reg.a.var = 0xee112233ffeedd;

			_ops.emit(0, 0x200037);				// rol a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xee224466ffeedd);
			assert(!dsp.sr_test(CCR_C));
		});		
	}

	void JitUnittests::sub()
	{
		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0x00000000000001;
			dsp.reg.b.var = 0x00000000000002;

			_ops.emit(0, 0x200014);		// sub b,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xffffffffffffff);
			assert(dsp.sr_test(CCR_C));
			assert(!dsp.sr_test(CCR_V));
		});		

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0x80000000000000;
			dsp.reg.b.var = 0x00000000000001;

			_ops.emit(0, 0x200014);		// sub b,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x7fffffffffffff);
			assert(!dsp.sr_test(CCR_C));
			assert(!dsp.sr_test(CCR_V));
		});

		runTest([&](auto& _block, auto& _ops)
		{
			dsp.reg.a.var = 0;
			dsp.x0(0x800000);

			_ops.emit(0, 0x200044);		// sub x0,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x00800000000000);
			assert(dsp.sr_test(CCR_C));
			assert(!dsp.sr_test(CCR_N));
		});
	}

	void JitUnittests::tfr()
	{
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0x11223344556677;
			dsp.reg.b.var = 0;
			_ops.emit(0, 0x200009);	// tfr a,b
		},
		[&]()
		{
			assert(dsp.reg.b.var == 0x11223344556677);
		});
	}

	void JitUnittests::move()
	{
		// op_Mover
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0x00112233445566;
			dsp.reg.n[2].var = 0;
			_ops.emit(0, 0x21da00);	// move a,n2
		},
		[&]()
		{
			assert(dsp.reg.n[2].var == 0x112233);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0x00445566aabbcc;
			dsp.reg.r[0].var = 0;
			_ops.emit(0, 0x21d000);	// move a,r0
		},
		[&]()
		{
			assert(dsp.reg.r[0].var == 0x445566);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0;
			dsp.reg.b.var = 0x44aabbccddeeff;
			_ops.emit(0, 0x21ee00);	// move b,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x007fffff000000);
			assert(dsp.reg.b.var == 0x44aabbccddeeff);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0xff000000000000;
			dsp.reg.b.var = 0x77000000000000;
			_ops.emit(0, 0x214400);	// move a2,x0
			_ops.emit(0, 0x216600);	// move b2,y0
		},
		[&]()
		{
			assert(dsp.x0() == 0xffffff);
			assert(dsp.y0() == 0x000077);
		});

		// op_Movem_ea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[2].var = 0xa;
			dsp.reg.n[2].var = 0x5;
			dsp.mem.set(MemArea_P, 0xa + 0x5, 0x123456);
			_ops.emit(0, 0x07ea92);	// move p:(r2+n2),r2
		},
		[&]()
		{
			assert(dsp.reg.r[2].var == 0x123456);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0x00223344556677;
			dsp.y1(0xaabbcc);
			_ops.emit(0, 0x21c700);	// move a,y1
		},
		[&]()
		{
			assert(dsp.y1() == 0x223344);
		});

		// op_Movex_ea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0;
			dsp.mem.set(MemArea_X, 0x10, 0x223344);
			_ops.emit(0, 0x56f000, 0x000010);	// move x:<<$10,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x00223344000000);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[0].var = 0x11;
			dsp.mem.set(MemArea_X, 0x19, 0x11abcd);
			_ops.emit(0, 0x02209f);	// move x:(r0+$8),b
		},
		[&]()
		{
			assert(dsp.reg.b.var == 0x0011abcd000000);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.b.var = 0x0011aabb000000;
			dsp.mem.set(MemArea_X, 0x07, 0);
			dsp.reg.r[0].var = 0x3;
			_ops.emit(0, 0x02108f);	// move b,x:(r0+$4)
		},
		[&]()
		{
			const auto r = dsp.mem.get(MemArea_X, 0x7);
			assert(r == 0x11aabb);
		});

		// op_Move_xx
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.x.var = 0;
			_ops.emit(0, 0x24ff00);	// move #$ff,x0
		},
		[&]()
		{
			assert(dsp.reg.x.var == 0x000000ff0000);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0;
			_ops.emit(0, 0x2eff00);	// move #$ff,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xffff0000000000);
		});

		// op_Movel_ea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.x.var = 0xbadbadbadbad;
			dsp.reg.r[1].var = 0x10;
			dsp.mem.set(MemArea_X, 0x10, 0xaabbcc);
			dsp.mem.set(MemArea_Y, 0x10, 0xddeeff);

			_ops.emit(0, 0x42d900);	// move l:(r1)+,x

			dsp.mem.set(MemArea_X, 0x3, 0x7f0000);
			dsp.mem.set(MemArea_Y, 0x3, 0x112233);
			dsp.reg.b.var = 0xffffeeddccbbaa;

			_ops.emit(0, 0x498300);	// move l:$3,b
		},
		[&]()
		{
			assert(dsp.reg.b.var == 0x007f0000112233);
			assert(dsp.reg.r[1].var == 0x11);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.x.var = 0xaabbccddeeff;
			dsp.reg.y.var = 0x112233445566;
			dsp.reg.a.var = 0x00abcdef123456;
			dsp.reg.b.var = 0x00654321fedcba;
			dsp.reg.r[1].var = 0x10;
			dsp.reg.r[2].var = 0x15;
			dsp.reg.r[3].var = 0x20;
			dsp.reg.r[4].var = 0x25;
			dsp.mem.set(MemArea_X, 0x10, 0);	dsp.mem.set(MemArea_Y, 0x10, 0);
			dsp.mem.set(MemArea_X, 0x15, 0);	dsp.mem.set(MemArea_Y, 0x15, 0);
			dsp.mem.set(MemArea_X, 0x20, 0);	dsp.mem.set(MemArea_Y, 0x20, 0);
			dsp.mem.set(MemArea_X, 0x25, 0);	dsp.mem.set(MemArea_Y, 0x25, 0);
			_ops.emit(0, 0x426100);	// move x,l:(r1)
			_ops.emit(0, 0x436200);	// move y,l:(r2)
			_ops.emit(0, 0x486300);	// move a,l:(r3)
			_ops.emit(0, 0x496400);	// move b,l:(r4)
		},
		[&]()
		{
			assert(dsp.mem.get(MemArea_X, 0x10) == 0xaabbcc);	assert(dsp.mem.get(MemArea_Y, 0x10) == 0xddeeff);
			assert(dsp.mem.get(MemArea_X, 0x15) == 0x112233);	assert(dsp.mem.get(MemArea_Y, 0x15) == 0x445566);
			assert(dsp.mem.get(MemArea_X, 0x20) == 0xabcdef);	assert(dsp.mem.get(MemArea_Y, 0x20) == 0x123456);
			assert(dsp.mem.get(MemArea_X, 0x25) == 0x654321);	assert(dsp.mem.get(MemArea_Y, 0x25) == 0xfedcba);
		});

		// op_Movel_aa
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0;
			dsp.reg.b.var = 0;
			dsp.mem.set(MemArea_X, 0x3, 0x123456);
			dsp.mem.set(MemArea_Y, 0x3, 0x789abc);
			_ops.emit(0, 0x4a8300);	// move l:<$3,ab
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x00123456000000);
			assert(dsp.reg.b.var == 0x00789abc000000);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.y.var = 0;
			dsp.mem.set(MemArea_X, 0x4, 0x123456);
			dsp.mem.set(MemArea_Y, 0x4, 0x789abc);
			_ops.emit(0, 0x438400);	// move l:<$4,y
		},
		[&]()
		{
			assert(dsp.reg.y.var == 0x00123456789abc);
		});

		// op_Movey_ea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.mem.set(MemArea_Y, 0x20, 0x334455);
			_ops.emit(0, 0x4ff000, 0x000020);	// move y:>$2daf2,y1
		},
		[&]()
		{
			assert(dsp.y1() == 0x334455);
		});

		// op_Move_ea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[4].var = 0x10;
			dsp.reg.n[4].var = 0x3;
			_ops.emit(0, 0x204c00);	// move (r4)+n4
		},
		[&]()
		{
			assert(dsp.reg.r[4].var == 0x13);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[4].var = 0x13;
			_ops.emit(0, 0x205c00);	// move (r4)+
		},
		[&]()
		{
			assert(dsp.reg.r[4].var == 0x14);
		});

		// op_Movex_aa
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.mem.set(MemArea_X, 0x7, 0x654321);
			dsp.reg.r[2].var = 0;
			_ops.emit(0, 0x628700);	// move x:<$7,r2
		},
		[&]()
		{
			assert(dsp.reg.r[2].var == 0x654321);
		});

		// op_Movey_aa
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[2].var = 0xfedcba;
			dsp.mem.set(MemArea_Y, 0x6, 0);
			_ops.emit(0, 0x6a0600);	// move r2,y:<$6
		},
		[&]()
		{
			assert(dsp.mem.get(MemArea_Y, 0x6) == 0xfedcba);
		});

		// op_Movex_Rnxxxx
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[3].var = 0x3;
			dsp.reg.n[5].var = 0;
			dsp.mem.set(MemArea_X, 0x7, 0x223344);
			_ops.emit(0, 0x0a73dd, 000004);	// move x:(r3+$4),n5
		},
		[&]()
		{
			assert(dsp.reg.r[3].var == 0x3);
			assert(dsp.reg.n[5].var == 0x223344);
		});

		// op_Movey_Rnxxxx
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[2].var = 0x5;
			dsp.reg.n[3].var = 0x778899;
			dsp.mem.set(MemArea_Y, 0x9, 0);
			_ops.emit(0, 0x0b729b, 000004);	// move n3,y:(r2+$4)
		},
		[&]()
		{
			assert(dsp.reg.r[2].var == 0x5);
			assert(dsp.mem.get(MemArea_Y, 0x9) == 0x778899);
		});

		// op_Movex_Rnxxx
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[3].var = 0x3;
			dsp.reg.a.var = 0;
			dsp.mem.set(MemArea_X, 0x7, 0x223344);
			_ops.emit(0, 0x02139e);	// move x:(r3+$4),a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x00223344000000);
		});

		// op_Movey_Rnxxx
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[2].var = 0x5;
			dsp.reg.a.var = 0x00334455667788;
			dsp.mem.set(MemArea_Y, 0x9, 0);
			_ops.emit(0, 0x0212ae);	// move a,y:(r2+$4)
		},
		[&]()
		{
			assert(dsp.mem.get(MemArea_Y, 0x9) == 0x334455);
		});

		// op_Movexr_ea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[2].var = 0x5;
			dsp.reg.a.var = 0;
			dsp.reg.b.var = 0x00223344556677;
			dsp.reg.y.var =   0x111111222222;
			dsp.mem.set(MemArea_X, 0x5, 0xaabbcc);
			_ops.emit(0, 0x1a9a00);	// move x:(r2)+,a b,y0
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xffaabbcc000000);
			assert(dsp.reg.y.var ==   0x111111223344);
			assert(dsp.reg.r[2].var == 0x6);
		});

		// op_Moveyr_ea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[2].var = 0x5;
			dsp.reg.a.var = 0;
			dsp.reg.b.var = 0x00223344556677;
			dsp.reg.x.var =   0x111111222222;
			dsp.mem.set(MemArea_Y, 0x5, 0xddeeff);
			_ops.emit(0, 0x1ada00);	// move b,x0 y:(r2)+,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0xffddeeff000000);
			assert(dsp.reg.x.var ==   0x111111223344);
			assert(dsp.reg.r[2].var == 0x6);
		});

		// op_Movexr_A
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[1].var = 0x3;
			dsp.reg.a.var = 0x00223344556677;
			dsp.reg.x.var =   0x111111222222;
			dsp.mem.set(MemArea_X, 3, 0);
			_ops.emit(0, 0x082100);	// move a,x:(r1) x0,a
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0x00222222000000);
			assert(dsp.mem.get(MemArea_X, 3) == 0x223344);
		});

		// op_Moveyr_A
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[6].var = 0x4;
			dsp.reg.b.var = 0x00334455667788;
			dsp.reg.y.var =   0x444444555555;
			dsp.mem.set(MemArea_Y, 4, 0);
			_ops.emit(0, 0x09a600);	// move b,y:(r6) y0,b
		},
		[&]()
		{
			assert(dsp.reg.b.var == 0x00555555000000);
			assert(dsp.mem.get(MemArea_Y, 4) == 0x334455);
		});

		// op_Movexy
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[2].var = 0x2;
			dsp.reg.r[6].var = 0x3;
			dsp.reg.n[2].var = 0x3;
			dsp.x0(0);
			dsp.y0(0);
			dsp.mem.set(MemArea_X, 2, 0x223344);
			dsp.mem.set(MemArea_Y, 3, 0xccddee);

			_ops.emit(0, 0xf0ca00);	// move x:(r2)+n2,x0 y:(r6)+,y0
		},
		[&]()
		{
			assert(dsp.x0() == 0x223344);
			assert(dsp.y0() == 0xccddee);
			assert(dsp.reg.r[2].var == 0x5);
			assert(dsp.reg.r[6].var == 0x4);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[3].var = 0x6;
			dsp.reg.r[7].var = 0x7;
			dsp.x0(0x112233);
			dsp.y0(0x445566);
			dsp.mem.set(MemArea_X, 6, 0);
			dsp.mem.set(MemArea_Y, 7, 0);

			_ops.emit(0, 0x806300);	// move x0,x:(r3) y0,y:(r7)
		},
		[&]()
		{
			assert(dsp.mem.get(MemArea_X, 6) == 0x112233);
			assert(dsp.mem.get(MemArea_Y, 7) == 0x445566);
		});

		// op_Movec_ea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.r[0].var = 3;
			dsp.reg.omr.var = 0;
			dsp.mem.set(MemArea_X, 3, 0x112233);
			_ops.emit(0, 0x05e03a);	// move x:(r0),omr
		},
		[&]()
		{
			assert(dsp.reg.omr == 0x112233);
		});

		// op_Movec_aa
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.sr.var = 0;
			dsp.mem.set(MemArea_X, 3, 0x223344);
			_ops.emit(0, 0x058339);	// move x:<$3,sr
		},
		[&]()
		{
			assert(dsp.reg.sr == 0x223344);
		});

		// op_Movec_S1D2
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.vba.var = 0;
			dsp.y1(0x334455);
			_ops.emit(0, 0x04c7b0);	// move y1,vba
		},
		[&]()
		{
			assert(dsp.reg.vba.var == 0x334455);
		});

		// op_Movec_S1D2
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.ep.var = 0xaabbdd;
			dsp.x1(0);
			_ops.emit(0, 0x445aa);	// move ep,x1
		},
		[&]()
		{
			assert(dsp.x1() == 0xaabbdd);
		});

		// op_Movec_ea with immediate data
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.lc.var = 0;
			_ops.emit(0, 0x05f43f, 0xaabbcc);	// move #$aabbcc,lc
		},
		[&]()
		{
			assert(dsp.reg.lc.var == 0xaabbcc);
		});

		// op_Movec_xx
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.la.var = 0;
			_ops.emit(0, 0x0555be);	// move #$55,la
		},
		[&]()
		{
			assert(dsp.reg.la.var == 0x55);
		});

		// op_Movep_ppea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.memWritePeriph(MemArea_X, 0xffffc5, 0);
			_ops.emit(0, 0x08f485, 0xffeeff);	// movep #>$112233,x:<<$ffffc5
		},
		[&]()
		{
			assert(dsp.memReadPeriph(MemArea_X, 0xffffc5) == 0xffeeff);
		});

		// op_Movep_Xqqea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.memWritePeriph(MemArea_X, 0xffff85, 0);
			_ops.emit(0, 0x07f405, 0x334455);	// movep #>$334455,x:<<$ffff85
		},
		[&]()
		{
			assert(dsp.memReadPeriph(MemArea_X, 0xffff85) == 0x334455);
		});

		// op_Movep_Yqqea
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.memWritePeriph(MemArea_Y, 0xffff82, 0);
			_ops.emit(0, 0x07b482, 0x556677);	// movep #>$556677,y:<<$ffff82
		},
		[&]()
		{
			assert(dsp.memReadPeriph(MemArea_Y, 0xffff82) == 0x556677);
		});

		// op_Movep_SXqq
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.memWritePeriph(MemArea_X, 0xffff84, 0);
			dsp.y1(0x334455);
			_ops.emit(0, 0x04c784);	// movep y1,x:<<$ffff84
		},
		[&]()
		{
			assert(dsp.memReadPeriph(MemArea_X, 0xffff84) == 0x334455);
		});

		// op_Movep_SYqq
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.memWritePeriph(MemArea_Y, 0xffff86, 0x112233);
			dsp.reg.b.var = 0;
			_ops.emit(0, 0x044f26);	// movep y:<<$ffff86,b
		},
		[&]()
		{
			assert(dsp.reg.b.var == 0x00112233000000);
		});

		// op_Movep_Spp
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.memWritePeriph(MemArea_Y, 0xffffc5, 0x8899aa);
			dsp.y1(0);
			_ops.emit(0, 0x094705);	// asm movep y:<<$ffffc5,y1
		},
		[&]()
		{
			assert(dsp.y1() == 0x8899aa);
		});
	}

	void JitUnittests::parallel()
	{
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			_block.dsp().reg.x.var =   0x000000010000;
			_block.dsp().reg.a.var = 0x006c0000000000;
			_block.dsp().reg.b.var = 0xbbbbbbbbbbbbbb;
			_block.dsp().reg.y.var =   0x222222222222;

			_ops.emit(0, 0x243c44);	// sub x0,a #$3c,x0
		},
		[&]()
		{
			assert(dsp.x0().var  ==   0x3c0000);
			assert(dsp.reg.a.var == 0x006b0000000000);
			assert(dsp.reg.b.var == 0xbbbbbbbbbbbbbb);
			assert(dsp.reg.y.var ==   0x222222222222);
		});
		
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			_block.dsp().reg.x.var =   0x100000080000;
			_block.dsp().reg.y.var =   0x000000200000;
			_block.dsp().reg.a.var = 0x0002cdd6000000;
			_block.dsp().reg.b.var = 0x0002a0a5000000;

			_ops.emit(0, 0x210541);	// tfr x0,a a0,x1
		},
		[&]()
		{
			assert(dsp.reg.x.var ==   0x000000080000);
			assert(dsp.reg.y.var ==   0x000000200000);
			assert(dsp.reg.a.var == 0x00080000000000);
			assert(dsp.reg.b.var == 0x0002a0a5000000);
		});
		
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			_block.dsp().reg.x.var =   0x000000003339;
			_block.dsp().reg.y.var =   0x65a1cb000000;
			_block.dsp().reg.a.var = 0x00000000000000;
			_block.dsp().reg.b.var = 0x00196871f4bc6a;

			_ops.emit(0, 0x21cf51);	// tfr y0,a a,b
		},
		[&]()
		{
			assert(dsp.reg.x.var ==   0x000000003339);
			assert(dsp.reg.y.var ==   0x65a1cb000000);
			assert(dsp.reg.a.var == 0x00000000000000);
			assert(dsp.reg.b.var == 0x00000000000000);
		});
		
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			_block.dsp().reg.x.var =   0x111111222222;
			_block.dsp().reg.y.var =   0x333333444444;
			_block.dsp().reg.a.var = 0x55666666777777;
			_block.dsp().reg.b.var = 0x88999999aaaaaa;

			_ops.emit(0, 0x21ee59);	// tfr y0,b b,a
		},
		[&]()
		{
			assert(dsp.reg.x.var ==   0x111111222222);
			assert(dsp.reg.y.var ==   0x333333444444);
			assert(dsp.reg.a.var == 0xff800000000000);
			assert(dsp.reg.b.var == 0x00444444000000);
		});
		
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			_block.dsp().reg.x.var =   0x111111222222;
			_block.dsp().reg.y.var =   0x333333444444;
			_block.dsp().reg.a.var = 0x55666666777777;
			_block.dsp().reg.b.var = 0x88999999aaaaaa;

			_ops.emit(0, 0x210741);	// tfr x0,a a0,y1
		},
		[&]()
		{
			assert(dsp.reg.x.var ==   0x111111222222);
			assert(dsp.reg.y.var ==   0x777777444444);
			assert(dsp.reg.a.var == 0x00222222000000);
			assert(dsp.reg.b.var == 0x88999999aaaaaa);
		});
	}

	void JitUnittests::ifcc()
	{
		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0;
			dsp.reg.b.var = 1;

			dsp.setSR(0);

			_ops.emit(0, 0x202a10);	// add b,a ifeq
		},
		[&]()
		{
			assert(dsp.reg.a.var == 0);
		});

		runTest([&](JitBlock& _block, JitOps& _ops)
		{
			dsp.reg.a.var = 0;
			dsp.reg.b.var = 1;

			dsp.setSR(CCR_Z);

			_ops.emit(0, 0x202a10);	// add b,a ifeq
		},
		[&]()
		{
			assert(dsp.reg.a.var == 1);
		});
	}

	void JitUnittests::ori_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.omr.var = 0xff1111;
		dsp.reg.sr.var = 0xff1111;

		_ops.emit(0, 0x0022fa);	// ori #$33,omr
		_ops.emit(0, 0x0022fb);	// ori #$33,eom
		_ops.emit(0, 0x0022f9);	// ori #$33,ccr
		_ops.emit(0, 0x0022f8);	// ori #$33,mr
	}

	void JitUnittests::ori_verify()
	{
		assert(dsp.reg.omr.var == 0xff3333);
		assert(dsp.reg.sr.var == 0xff3333);
	}

	void JitUnittests::clr_build(JitBlock& _block, JitOps& _ops)
	{
		dsp.reg.a.var = 0xbada55c0deba5e;

		// ensure that ALU is loaded, otherwise it is not written back to DSP registers
		{
			const RegGP dummy(_block);
			_block.regs().getALU(dummy, 0);			
		}
		_ops.emit(0, 0x200013);
	}

	void JitUnittests::clr_verify()
	{
		assert(dsp.reg.a.var == 0);
	}
}
