#include "BASE.h"

bool BASE::mem_read_word(uint32_t* data, uint32_t vaddr, const I_TYPE& ins, uint64_t pagetable) const {
    uint8_t* data_bytes = reinterpret_cast<uint8_t*>(data);
    int bytesToRead;
    bool addrOutofRangeError = false;

    // 确定读取的字节数
    if (ins.ddd.mem_whb == DecodeParams::MEM_W)
        bytesToRead = 4;
    else if (ins.ddd.mem_whb == DecodeParams::MEM_H)
        bytesToRead = 2;
    else if (ins.ddd.mem_whb == DecodeParams::MEM_B)
        bytesToRead = 1;
    else
        assert(0);

    if (vaddr >= ldsBaseAddr_core && vaddr < ldsBaseAddr_core + hw_lds_size) {
        // 读取局部内存
        addrOutofRangeError = false;
        for (int i = 0; i < bytesToRead; i++) {
            if (vaddr + i >= ldsBaseAddr_core + hw_lds_size) {
                return true;
            }
            data_bytes[i] = m_local_mem[vaddr - ldsBaseAddr_core + i];
        }
    } else { // 读取全局内存
        addrOutofRangeError = m_mem->readDataVirtual(pagetable, vaddr, bytesToRead, data);
    }

    // 如果不是读取4个字节，则根据mem_unsigned来决定如何处理剩余的位
    if (bytesToRead < 4) {
        if (ins.ddd.mem_unsigned == 1) {
            // 零扩展，data已正确设置
        } else {
            // 符号位扩展
            int shift = (4 - bytesToRead) * 8;
            int32_t signExtension = (static_cast<int32_t>(*data) << shift) >> shift;
            *data = static_cast<uint32_t>(signExtension);
        }
    }
    return addrOutofRangeError;
}

bool BASE::mem_write_word(uint32_t data, uint32_t vaddr, const I_TYPE& ins, uint64_t pagetable) {
    uint8_t* data_bytes = reinterpret_cast<uint8_t*>(&data);

    int bytesToWrite = 0; // 将要写入的字节数
    if (ins.ddd.mem_whb == DecodeParams::MEM_W)
        bytesToWrite = 4;
    else if (ins.ddd.mem_whb == DecodeParams::MEM_H)
        bytesToWrite = 2;
    else if (ins.ddd.mem_whb == DecodeParams::MEM_B)
        bytesToWrite = 1;
    else
        assert(0);

    if (vaddr >= ldsBaseAddr_core && vaddr < ldsBaseAddr_core + hw_lds_size) {
        // 写入局部内存
        for (int i = 0; i < bytesToWrite; i++) {
            if (vaddr + i >= ldsBaseAddr_core + hw_lds_size) {
                return true;
            }
            m_local_mem[vaddr - ldsBaseAddr_core + i] = data_bytes[i];
        }
    } else {
        // 写入全局内存
        return m_mem->writeDataVirtual(pagetable, vaddr, bytesToWrite, &data);
    }
    return false;
}

void BASE::LSU_IN() {
    lsu_in_t new_data;
    int a_delay, b_delay;
    while (true) {
        wait();
        if (emito_lsu) {
            if (lsu_ready_old == false) {
                std::cout << "lsu error: not ready at " << sc_time_stamp() << "," << sc_delta_count_at_current_time()
                          << std::endl;
            }
            lsu_unready.notify();

            new_data.ins = emit_ins;
            new_data.warp_id = emitins_warpid;
            for (int i = 0; i < m_hw_warps[new_data.warp_id]->CSR_reg[0x802]; i++) {
                new_data.rsv1_data[i] = tolsu_data1[i];
                new_data.rsv2_data[i] = tolsu_data2[i];
                new_data.rsv3_data[i] = tolsu_data3[i];
            }
            lsu_dq.push(new_data);
            a_delay = 15;
            b_delay = 5;
            if (a_delay == 0)
                lsu_eva.notify();
            else if (lsueqa_triggered)
                lsu_eqa.notify(sc_time((a_delay)*PERIOD, SC_NS));
            else {
                lsu_eqa.notify(sc_time((a_delay)*PERIOD, SC_NS));
                ev_lsufifo_pushed.notify();
            }
            if (b_delay == 0)
                lsu_evb.notify();
            else {
                lsu_eqb.notify(sc_time((b_delay)*PERIOD, SC_NS));
                ev_lsuready_updated.notify();
            }
#ifdef SPIKE_OUTPUT
            std::cout << "SM" << sm_id << " warp " << emitins_warpid << " 0x" << std::hex << emit_ins.read().currentpc
                      << " " << emit_ins << " LSU addr=" << std::hex << std::setw(8) << std::setfill('0');

            switch (emit_ins.read().op) {
            case LW_:
                std::cout << new_data.rsv1_data[0] << std::setw(0) << "+" << std::setw(8) << new_data.rsv2_data[0]
                          << "=" << (new_data.rsv1_data[0] + new_data.rsv2_data[0]);
                break;
            case SW_:
                std::cout << new_data.rsv1_data[0] << std::setw(0) << "+" << std::setw(8) << new_data.rsv2_data[0]
                          << "=" << (new_data.rsv1_data[0] + new_data.rsv2_data[0]);
                if (sm_id == 0 && emitins_warpid == 2) {
                    std::cout << "\nthis inst rs1_addr=" << std::dec << new_data.ins.s1
                              << ", rs2_addr=" << new_data.ins.s2 << std::hex
                              << ", s_regfile[rs1]=" << m_hw_warps[emitins_warpid]->s_regfile[new_data.ins.s1]
                              << ", s_regfile[rs2]=" << m_hw_warps[emitins_warpid]->s_regfile[new_data.ins.s2]
                              << std::endl;
                }
                break;
            case VLE32_V_:
                std::cout << new_data.rsv1_data[0];
                break;
            }
            std::cout << std::setw(0) << std::setfill(' ') << " at " << sc_time_stamp() << ","
                      << sc_delta_count_at_current_time() << std::endl;
#endif
        } else {
            if (!lsueqa_triggered)
                ev_lsufifo_pushed.notify();
            if (!lsueqb_triggered)
                ev_lsuready_updated.notify();
        }
    }
}

void BASE::LSU_CALC() {
    lsufifo_elem_num = 0;
    lsufifo_empty = 1;
    lsueqa_triggered = false;
    lsu_in_t lsutmp1;
    lsu_out_t lsutmp2;
    bool succeed;
    unsigned int external_addr;
    bool addrOutofRangeException;
    std::array<uint32_t, hw_num_thread> LSUaddr;
    while (true) {
        wait(lsu_eva | lsu_eqa.default_event());
        if (lsu_eqa.default_event().triggered()) {
            lsueqa_triggered = true;
            wait(SC_ZERO_TIME);
            lsueqa_triggered = false;
        }
        // std::cout << "LSU_OUT: triggered by eva/eqa at " << sc_time_stamp() << "," <<
        // sc_delta_count_at_current_time() << std::endl;
        lsutmp1 = lsu_dq.front();
        lsu_dq.pop();

        // 为warp中的每个线程计算访存地址
        for (int i = 0; i < m_hw_warps[lsutmp1.warp_id]->CSR_reg[0x802]; i++) {
            LSUaddr[i] = (lsutmp1.ins.ddd.isvec & lsutmp1.ins.ddd.disable_mask) ? lsutmp1.ins.ddd.is_vls12()
                    ? (lsutmp1.rsv1_data[i] + lsutmp1.rsv2_data[i])
                    : ((lsutmp1.rsv1_data[i] + lsutmp1.rsv2_data[i]) * hw_num_thread + (i << 2)
                       + m_hw_warps[lsutmp1.warp_id]->CSR_reg[0x807])
                : lsutmp1.ins.ddd.isvec
                ? (lsutmp1.rsv1_data[i] + (lsutmp1.ins.ddd.mop == 0 ? i << 2 : i * lsutmp1.rsv2_data[i]))
                : (lsutmp1.rsv1_data[0] + lsutmp1.rsv2_data[0]);
        }

        if (lsutmp1.ins.ddd.wvd || lsutmp1.ins.ddd.wxd) { // 读global/local mem，稍后要写回寄存器(write reg)
            lsutmp2.ins = lsutmp1.ins;
            lsutmp2.warp_id = lsutmp1.warp_id;
            if (lsutmp1.ins.ddd.isvec) { // vec instruction lw: check branch masks of each thread
                for (int i = 0; i < m_hw_warps[lsutmp1.warp_id]->CSR_reg[0x802]; i++) {
                    if (lsutmp1.ins.mask[i]) {
                        uint32_t data;
                        addrOutofRangeException
                            = mem_read_word(&data, LSUaddr[i], lsutmp2.ins, m_hw_warps[lsutmp1.warp_id]->pagetable);
                        lsutmp2.rdv1_data[i] = data;
                    } else {
                        lsutmp2.rdv1_data[i] = 0;   // don't care
                    }
                    if (addrOutofRangeException)
                        std::cout << "SM" << sm_id << " LSU read addrOutofRange error, ins=" << lsutmp1.ins
                                  << ",addr=" << LSUaddr[i] << " at " << sc_time_stamp() << ","
                                  << sc_delta_count_at_current_time() << std::endl;
                    // if(lsutmp1.ins.currentpc == 0x800000f8) {
                    //     std::cout << "SM" << sm_id << " warp " << lsutmp1.warp_id << " 0x" << std::hex << lsutmp1.ins.currentpc
                    //             << " " << lsutmp1.ins << std::hex << " addr=0x" << LSUaddr[0] << " data=0x" << lsutmp2.rdv1_data[0] << std::dec
                    //             << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
                    // }
                }
            } else {    // scalar instruction lw
                uint32_t data;
                addrOutofRangeException
                    = mem_read_word(&data, LSUaddr[0], lsutmp2.ins, m_hw_warps[lsutmp1.warp_id]->pagetable);
                lsutmp2.rdv1_data[0] = data;
                if (addrOutofRangeException)
                    std::cout << "SM" << sm_id << " LSU read addrOutofRange error, ins=" << lsutmp1.ins
                                << ",addr=" << LSUaddr[0] << " at " << sc_time_stamp() << ","
                                << sc_delta_count_at_current_time() << std::endl;
            }
            lsufifo.push(lsutmp2);
                
        } else { // 写global/local mem
            if(lsutmp1.ins.ddd.isvec) {     // vec instruction sw: check branch masks of each thread
                for (int i = 0; i < m_hw_warps[lsutmp1.warp_id]->CSR_reg[0x802]; i++) {
                    if (lsutmp1.ins.mask[i]) {
                        addrOutofRangeException = mem_write_word(lsutmp1.rsv3_data[i], LSUaddr[i], lsutmp1.ins,
                                                                 m_hw_warps[lsutmp1.warp_id]->pagetable);
                    }
                    if (addrOutofRangeException)
                        std::cout << "SM" << sm_id << " LSU write addrOutofRange error, ins=" << lsutmp1.ins
                                  << ",addr=0x" << std::hex << LSUaddr[i] << std::dec << " at " << sc_time_stamp()
                                  << "," << sc_delta_count_at_current_time() << std::endl;
                }
#ifdef SPIKE_OUTPUT
                std::cout << "SM" << sm_id << " warp " << lsutmp1.warp_id << " 0x" << std::hex << lsutmp1.ins.currentpc
                          << " " << lsutmp1.ins << std::hex << " data=" << std::setw(8) << std::setfill('0');
                for (int i = m_hw_warps[lsutmp1.warp_id]->CSR_reg[0x802] - 1; i >= 0; i--)
                    std::cout << lsutmp1.rsv3_data[i] << " ";
                std::cout << "@ ";
                for (int i = m_hw_warps[lsutmp1.warp_id]->CSR_reg[0x802] - 1; i >= 0; i--)
                    std::cout << LSUaddr[i] << " ";
                std::cout << std::setw(0) << std::setfill(' ') << " at " << sc_time_stamp() << ","
                          << sc_delta_count_at_current_time() << std::endl;
#endif
            } else { // scalar instruction sw
                addrOutofRangeException = mem_write_word(lsutmp1.rsv3_data[0], LSUaddr[0], lsutmp1.ins,
                                                         m_hw_warps[lsutmp1.warp_id]->pagetable);
                if (addrOutofRangeException)
                    std::cout << "SM" << sm_id << " LSU write addrOutofRange error, ins=" << lsutmp1.ins
                              << ",addr=0x" << std::hex << LSUaddr[0] << std::dec << " at " << sc_time_stamp() << ","
                              << sc_delta_count_at_current_time() << std::endl;
#ifdef SPIKE_OUTPUT
                std::cout << "SM" << sm_id << " warp " << lsutmp1.warp_id << " 0x" << std::hex << lsutmp1.ins.currentpc
                          << " " << lsutmp1.ins << std::hex << " data=" << std::setw(8) << std::setfill('0')
                          << lsutmp1.rsv3_data[0] << std::dec << std::setfill(' ') << " @ " << std::hex << LSUaddr[0]
                          << std::dec << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
#endif
            }
        }
        ev_lsufifo_pushed.notify();
    }
}

void BASE::LSU_CTRL() {
    lsu_ready = true;
    lsu_ready_old = true;
    lsueqb_triggered = false;
    while (true) {
        wait(lsu_eqb.default_event() | lsu_unready | lsu_evb);
        if (lsu_eqb.default_event().triggered()) {
            lsu_ready = true;
            lsu_ready_old = lsu_ready;
            lsueqb_triggered = true;
            wait(SC_ZERO_TIME);
            lsueqb_triggered = false;
            ev_lsuready_updated.notify();
        } else if (lsu_evb.triggered()) {
            lsu_ready = true;
            lsu_ready_old = lsu_ready;
            ev_lsuready_updated.notify();
        } else if (lsu_unready.triggered()) {
            lsu_ready = false;
            lsu_ready_old = lsu_ready;
            ev_lsuready_updated.notify();
        }
    }
}
