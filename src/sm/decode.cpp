#include "BASE.h"

void BASE::DECODE(int warp_id)
{
    I_TYPE tmpins;
    sc_bv<32> scinsbit;
    bool WILLregext = false;
    int ext1, ext2, ext3, extd, extimm;
    auto& hwarp = m_hw_warps[warp_id];
    while (true)
    {
        // std::cout << "SM" << sm_id << " warp" << warp_id << " DECODE: finish at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
        wait(hwarp->ev_decode);
            
        if (hwarp->jump == 1 ||
            hwarp->simtstk_jump == 1||
            hwarp->endprg_flush_pipe)
        {
            hwarp->fetch_valid2 = false;
            WILLregext = false;
        }
        else
        { // std::cout << "SM" << sm_id << " warp" << warp_id << " DECODE: start at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
            tmpins = I_TYPE(hwarp->fetch_ins, hwarp->pc.read());
            // if (sm_id == 0 && warp_id == 0)
            //     std::cout << "SM" << sm_id << " warp" << warp_id << " DECODE ins.bit=" << std::hex << tmpins.origin32bit << std::dec << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;

            bool foundBitIns = 0;
            for (const auto &instable_item : instable_vec)
            {
                std::bitset<32> masked_ins = std::bitset<32>(tmpins.origin32bit) & instable_item.mask;
                // std::cout << "warp" << warp_id << " DECODE: mask=" << instable_item.mask << ", masked_ins=" << masked_ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
                auto it = instable_item.itable.find(masked_ins);
                if (it != instable_item.itable.end())
                {
                    tmpins.op = it->second;
                    foundBitIns = true;
                    break;
                }
            }
            if (!foundBitIns)
            {
                tmpins.op = INVALID_;
                std::cout << "warp" << warp_id << " DECODE error: invalid bit ins " << tmpins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
            }
            else
            {
                // std::cout << "warp" << warp_id << " DECODE: match ins bit=" << std::bitset<32>(tmpins.origin32bit) << " with " << magic_enum::enum_name((OP_TYPE)tmpins.op) << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
            }

            tmpins.ddd = decode_table[(OP_TYPE)tmpins.op];

            if (tmpins.op == (int)REGEXT_)
            {
                hwarp->fetch_valid2 = false;
                WILLregext = true;

                extimm = 0;
                ext3 = extractBits32(tmpins.origin32bit, 31, 29);
                ext2 = extractBits32(tmpins.origin32bit, 28, 26);
                ext1 = extractBits32(tmpins.origin32bit, 25, 23);
                extd = extractBits32(tmpins.origin32bit, 22, 20);
#ifdef SPIKE_OUTPUT
                std::cout << "SM" << sm_id << " warp " << warp_id << " 0x" << std::hex << tmpins.currentpc << tmpins
                    << " DECODE: set regext(s3,s2,s1,d)=" << ext3 << "," << ext2 << "," << ext1 << "," << extd << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
#endif
            }
            else if (tmpins.op == (int)REGEXTI_)
            {
                hwarp->fetch_valid2 = false;
                WILLregext = true;

                extimm = extractBits32(tmpins.origin32bit, 31, 26);
                ext3 = 0;
                ext2 = extractBits32(tmpins.origin32bit, 25, 23);
                ext1 = 0;
                extd = extractBits32(tmpins.origin32bit, 22, 20);
#ifdef SPIKE_OUTPUT
                std::cout << "SM" << sm_id << " warp " << warp_id << " 0x" << std::hex << tmpins.currentpc << tmpins
                    << " DECODE: set regexti(s3,s2,s1,d)=" << ext3 << "," << ext2 << "," << ext1 << "," << extd << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
#endif
            }
            else
            {
                hwarp->fetch_valid2 = hwarp->fetch_valid12;
                if (tmpins.ddd.tc)
                    tmpins.ddd.sel_execunit = DecodeParams::TC;
                else if (tmpins.ddd.sfu)
                    tmpins.ddd.sel_execunit = DecodeParams::SFU;
                else if (tmpins.ddd.fp)
                    tmpins.ddd.sel_execunit = DecodeParams::VFPU;
                else if (tmpins.ddd.csr != 0)
                    tmpins.ddd.sel_execunit = DecodeParams::CSR;
                else if (tmpins.ddd.mul)
                    tmpins.ddd.sel_execunit = DecodeParams::MUL;
                else if (tmpins.ddd.mem_cmd != 0)
                    tmpins.ddd.sel_execunit = DecodeParams::LSU;
                else if (tmpins.ddd.isvec)
                {
                    if (tmpins.op == JOIN_)
                        tmpins.ddd.sel_execunit = DecodeParams::SIMTSTK;
                    else
                        tmpins.ddd.sel_execunit = DecodeParams::VALU;
                }
                else if (tmpins.ddd.barrier)
                    tmpins.ddd.sel_execunit = DecodeParams::WPSCHEDLER;
                else
                    tmpins.ddd.sel_execunit = DecodeParams::SALU;

                tmpins.s1 = extractBits32(tmpins.origin32bit, 19, 15);
                tmpins.s2 = extractBits32(tmpins.origin32bit, 24, 20);
                tmpins.s3 = (tmpins.ddd.fp & !tmpins.ddd.isvec)
                                ? extractBits32(tmpins.origin32bit, 31, 27)
                                : extractBits32(tmpins.origin32bit, 11, 7);
                tmpins.d = extractBits32(tmpins.origin32bit, 11, 7);
                if (WILLregext)
                {
                    tmpins.imm += extimm << 5;
                    tmpins.s1 += ext1 << 5;
                    tmpins.s2 += ext2 << 5;
                    tmpins.s3 += ext3 << 5;
                    tmpins.d += extd << 5;
                    WILLregext = false;
#ifdef SPIKE_OUTPUT
                std::cout << "SM" << sm_id << " warp " << warp_id << " 0x" << std::hex << tmpins.currentpc << tmpins
                    << " DECODE: regext(s3,s2,s1,d)=" << ext3 << "," << ext2 << "," << ext1 << "," << extd
                    << " is used to set s3,s2,s1,d=" << tmpins.s3 << "," << tmpins.s2 << "," << tmpins.s1 << "," << tmpins.d
                    << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
#endif
                }
                scinsbit = tmpins.origin32bit;
                tmpins.ddd.mop = tmpins.ddd.readmask ? 3 : (scinsbit.range(27, 26)).to_uint();

                switch (tmpins.ddd.sel_imm)
                {
                case DecodeParams::sel_imm_t::IMM_I:
                    tmpins.imm = scinsbit.range(31, 20).to_int(); // to_int()会自动补符号位，to_uint()补0
                    break;
                case DecodeParams::sel_imm_t::IMM_S:
                    tmpins.imm = (scinsbit.range(31, 25), scinsbit.range(11, 7)).to_int();
                    break;
                case DecodeParams::sel_imm_t::IMM_B:
                    tmpins.imm = (scinsbit.range(31, 31), scinsbit.range(7, 7), scinsbit.range(30, 25), scinsbit.range(11, 8)).to_int() << 1;
                    break;
                case DecodeParams::sel_imm_t::IMM_U:
                    tmpins.imm = (scinsbit.range(31, 12)).to_int() << 12;
                    break;
                case DecodeParams::sel_imm_t::IMM_J:
                    tmpins.imm = (scinsbit.range(31, 31), scinsbit.range(19, 12), scinsbit.range(20, 20), scinsbit.range(30, 21)).to_int() << 1;
                    break;
                case DecodeParams::sel_imm_t::IMM_Z:
                    tmpins.imm = (scinsbit.range(19, 15)).to_uint();
                    break;
                case DecodeParams::sel_imm_t::IMM_2:
                    tmpins.imm = (scinsbit.range(24, 20)).to_int();
                    break;
                case DecodeParams::sel_imm_t::IMM_V: // 和scala不一样，需要修改，加位拓展
                    tmpins.imm = (scinsbit.range(19, 15)).to_int();
                    break;
                case DecodeParams::sel_imm_t::IMM_L11:
                    tmpins.imm = (scinsbit.range(30, 20)).to_int();
                    break;
                case DecodeParams::sel_imm_t::IMM_S11:
                    tmpins.imm = (scinsbit.range(30, 25), scinsbit.range(11, 7)).to_int();
                    break;
                default:
                    break;
                }

                hwarp->decode_ins = tmpins;
            }
        }
    }
}
