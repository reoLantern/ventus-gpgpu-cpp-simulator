#include "BASE.h"

BASE::BASE(sc_core::sc_module_name name, int _sm_id, Memory* mem)
    : sc_module(name)
    , sm_id(_sm_id)
    , m_mem(mem) {
    for (int warp_id = 0; warp_id < hw_num_warp; warp_id++) {
        WARP_BONE* new_warp_bone_ = new WARP_BONE(warp_id);
        m_hw_warps[warp_id] = new_warp_bone_;
    }
    SC_HAS_PROCESS(BASE);

    SC_THREAD(debug_sti);
    // SC_THREAD(debug_display);
    // SC_THREAD(debug_display1);
    // SC_THREAD(debug_display2);
    // SC_THREAD(debug_display3);
    SC_THREAD(INIT_INSTABLE);
    SC_THREAD(INIT_DECODETABLE);

    for (int i = 0; i < hw_num_warp; i++) {
        sc_core::sc_spawn(sc_bind(&BASE::PROGRAM_COUNTER, this, i),
                          ("warp" + std::to_string(i) + "_PROGRAM_COUNTER").c_str());
        sc_core::sc_spawn(sc_bind(&BASE::INSTRUCTION_REG, this, i),
                          ("warp" + std::to_string(i) + "_INSTRUCTION_REG").c_str());
        sc_core::sc_spawn(sc_bind(&BASE::DECODE, this, i), ("warp" + std::to_string(i) + "_DECODE").c_str());
        // sc_core::sc_spawn(sc_bind(&BASE::IBUF_ACTION, this, i), ("warp" + std::to_string(i) +
        // "_IBUF_ACTION").c_str()); sc_core::sc_spawn(sc_bind(&BASE::JUDGE_DISPATCH, this, i), ("warp" +
        // std::to_string(i) + "_JUDGE_DISPATCH").c_str());
        sc_core::sc_spawn(sc_bind(&BASE::BEFORE_DISPATCH, this, i),
                          ("warp" + std::to_string(i) + "_BEFORE_DISPATCH").c_str());
        // sc_core::sc_spawn(sc_bind(&BASE::INIT_REG, this, i), ("warp" + std::to_string(i) + "_INIT_REG").c_str());
        sc_core::sc_spawn(sc_bind(&BASE::SIMT_STACK, this, i), ("warp" + std::to_string(i) + "_SIMT_STACK").c_str());
        sc_core::sc_spawn(sc_bind(&BASE::WRITE_REG, this, i), ("warp" + std::to_string(i) + "_WRITE_REG").c_str());
    }

    // issue
    SC_THREAD(WARP_SCHEDULER);
    // opc
    SC_THREAD(OPC_FIFO);
    sensitive << clk.pos();
    SC_THREAD(OPC_FETCH);
    sensitive << clk.pos();
    SC_THREAD(OPC_EMIT);
    sensitive << clk.pos();
    // regfile
    SC_THREAD(READ_REG);
    // exec
    SC_THREAD(SALU_IN);
    sensitive << clk.pos();
    SC_THREAD(SALU_CALC);
    sensitive << clk.pos();
    SC_THREAD(SALU_CTRL);

    SC_THREAD(VALU_IN);
    sensitive << clk.pos();
    SC_THREAD(VALU_CALC);
    sensitive << clk.pos();
    SC_THREAD(VALU_CTRL);

    SC_THREAD(VFPU_IN);
    sensitive << clk.pos();
    SC_THREAD(VFPU_CALC);
    sensitive << clk.pos();
    SC_THREAD(VFPU_CTRL);

    SC_THREAD(LSU_IN);
    sensitive << clk.pos();
    SC_THREAD(LSU_CALC);
    sensitive << clk.pos();
    SC_THREAD(LSU_CTRL);

    SC_THREAD(CSR_IN);
    sensitive << clk.pos();
    SC_THREAD(CSR_CALC);
    sensitive << clk.pos();
    SC_THREAD(CSR_CTRL);

    SC_THREAD(MUL_IN);
    sensitive << clk.pos();
    SC_THREAD(MUL_CALC);
    sensitive << clk.pos();
    SC_THREAD(MUL_CTRL);

    SC_THREAD(SFU_IN);
    sensitive << clk.pos();
    SC_THREAD(SFU_CALC);
    sensitive << clk.pos();
    SC_THREAD(SFU_CTRL);

    SC_THREAD(TC_IN);
    sensitive << clk.pos();
    SC_THREAD(TC_CALC);
    sensitive << clk.pos();
    SC_THREAD(TC_CTRL);

    // writeback
    SC_THREAD(WRITE_BACK);
    sensitive << clk.pos();
}

void BASE::debug_sti() {
    while (true) {
        wait(clk.posedge_event());
        wait(SC_ZERO_TIME);
        dispatch_ready = !opc_full | doemit;
    }
}
void BASE::debug_display() {
    while (true) {
        wait(ev_salufifo_pushed);
        std::cout << "ev_salufifo_pushed triggered at " << sc_time_stamp() << "," << sc_delta_count_at_current_time()
                  << std::endl;
    }
}
void BASE::debug_display1() {
    while (true) {
        wait(ev_valufifo_pushed);
        std::cout << "ev_valufifo_pushed triggered at " << sc_time_stamp() << "," << sc_delta_count_at_current_time()
                  << std::endl;
    }
}
void BASE::debug_display2() {
    while (true) {
        wait(ev_vfpufifo_pushed);
        std::cout << "ev_vfpufifo_pushed triggered at " << sc_time_stamp() << "," << sc_delta_count_at_current_time()
                  << std::endl;
    }
}
void BASE::debug_display3() {
    while (true) {
        wait(ev_lsufifo_pushed);
        std::cout << "ev_lsufifo_pushed triggered at " << sc_time_stamp() << "," << sc_delta_count_at_current_time()
                  << std::endl;
    }
}

void BASE::PROGRAM_COUNTER(int warp_id) {
    // m_hw_warps[warp_id]->pc = 0x80000000 - 4;
    while (true) {
        // std::cout << "SM" << sm_id << " warp" << warp_id << " PC: finish at " << sc_time_stamp() << "," <<
        // sc_delta_count_at_current_time() << std::endl;
        wait(clk.posedge_event());
        auto& hwarp = m_hw_warps[warp_id];
        // std::cout << "SM" << sm_id << " warp" << warp_id << " PC: start at " << sc_time_stamp() << "," <<
        // sc_delta_count_at_current_time() << std::endl; std::cout << "PC warp" << warp_id << " start at " <<
        // sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
        // wait(hwarp->ev_ibuf_inout); // ibuf判断swallow后，fetch新指令
        // std::cout << "PC start, ibuf_swallow=" << ibuf_swallow << " at " << sc_time_stamp() << "," <<
        // sc_delta_count_at_current_time() << std::endl;
        if (hwarp->is_warp_activated.read()) {
            // std::cout << "warp " << warp_id << " sup at " << sc_time_stamp() << "," <<
            // sc_delta_count_at_current_time() << std::endl;
            if (rst_n == 0) {
                hwarp->pc = 0;
                hwarp->fetch_valid = false;
            } else if (hwarp->jump == 1) {
                hwarp->pc = hwarp->jump_addr;
                hwarp->fetch_valid = true;
#ifdef SPIKE_OUTPUT
                std::cout << "SM" << sm_id << " warp " << warp_id << " pc jumps to 0x" << std::hex
                          << hwarp->jump_addr << std::dec << " at " << sc_time_stamp() << ","
                          << sc_delta_count_at_current_time() << std::endl;
#endif
            } else if (hwarp->simtstk_jump == 1) {
                hwarp->pc = hwarp->simtstk_jumpaddr;
                hwarp->fetch_valid = true;
            } else if (hwarp->ibuf_empty
                       | (!hwarp->ibuf_full
                          | (hwarp->dispatch_warp_valid && (!opc_full | doemit)))) {
                // std::cout << "pc will +1 at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() <<
                // std::endl;
                hwarp->pc = hwarp->pc.read() + 4;
                hwarp->fetch_valid = true;
            }
        }
        hwarp->ev_fetchpc.notify(); // Not used
        if (hwarp->endprg_flush_pipe) {
            hwarp->fetch_valid = false;
        }
    }
}

void BASE::INSTRUCTION_REG(int warp_id) {
    // initialize
    // ireg[0] = I_TYPE(ADD_, 0, 1, 2);
    // ireg[1] = I_TYPE(ADD_, 1, 3, 3);
    // ireg[2] = I_TYPE(ADD_, 0, 4, 5);
    // ireg[3] = I_TYPE(BEQ_, 0, 7, 2);
    // ireg[4] = I_TYPE(VADD_VX_, 0, 1, 4);
    // ireg[5] = I_TYPE(VFADD_VV_, 3, 4, 2);
    // ireg[6] = I_TYPE(BEQ_, 0, 7, 5);

    bool addrOutofRangeException;

    while (true) {
        // std::cout << "SM" << sm_id << " warp" << warp_id << " INSTRUCTION_REG: finish at " << sc_time_stamp() << ","
        // << sc_delta_count_at_current_time() << std::endl;
        wait(clk.posedge_event());
        auto& hwarp = m_hw_warps[warp_id];
        // std::cout << "SM" << sm_id << " warp" << warp_id << " INSTRUCTION_REG: start at " << sc_time_stamp() << ","
        // << sc_delta_count_at_current_time() << std::endl;
        if (hwarp->is_warp_activated && rst_n != 0) {
            if (hwarp->jump == 1 | hwarp->simtstk_jump == 1) {
                hwarp->fetch_valid12 = false;
                hwarp->ev_decode.notify();
            } else if (hwarp->ibuf_empty
                       | (!hwarp->ibuf_full
                          | (hwarp->dispatch_warp_valid && (!opc_full | doemit)))) {
                hwarp->fetch_valid12 = hwarp->fetch_valid;

                // if (sm_id == 0 && warp_id == 0)
                //     std::cout << "SM" << sm_id << " warp" << warp_id << " INSTRUCTION_REG: fetch_ins pc=" << std::hex
                //     << hwarp->pc.read() << std::dec << " at " << sc_time_stamp() << "," <<
                //     sc_delta_count_at_current_time() << std::endl;
                // hwarp->fetch_ins = m_kernel->readInsBuffer(hwarp->pc.read(),
                // addrOutofRangeException);
                addrOutofRangeException = m_mem->readDataVirtual(
                    hwarp->pagetable, hwarp->pc.read(), 4, &hwarp->fetch_ins);
                if (addrOutofRangeException)
                    std::cout << "SM" << sm_id << " warp" << warp_id << "INS_REG error: pc(" << std::hex
                              << hwarp->pc.read() << std::dec << ") out of range at " << sc_time_stamp()
                              << "," << sc_delta_count_at_current_time() << std::endl;

                // if (sm_id == 0 && warp_id == 0)
                //     std::cout << "SM" << sm_id << " warp" << warp_id << " ICACHE: read fetch_ins.bit=ins_mem[" <<
                //     std::hex << hwarp->pc.read() << "]=" << hwarp->fetch_ins.origin32bit
                //     << std::dec
                //          << ", will pass to decode_ins at the same cycle at " << sc_time_stamp() << "," <<
                //          sc_delta_count_at_current_time() << std::endl;

                hwarp->ev_decode.notify();
            }
        } else if (hwarp->endprg_flush_pipe) {
            hwarp->fetch_valid12 = false;
            hwarp->ev_decode.notify();
        }
    }
}

void BASE::cycle_IBUF_ACTION(int warp_id, I_TYPE& dispatch_ins_, I_TYPE& _readdata3) {
    auto& hwarp = m_hw_warps[warp_id];
    hwarp->ibuf_swallow = false;
    if (rst_n.read() == 0)
        hwarp->ififo.clear();
    else {
        if (hwarp->dispatch_warp_valid && (!opc_full | doemit)) {
            // std::cout << "before dispatch, ififo has " << ififo.used() << " elems at " << sc_time_stamp() <<","<<
            // sc_delta_count_at_current_time() << std::endl;
            dispatch_ins_ = hwarp->ififo.get();
            // if (sm_id == 0 && warp_id == 0)
            // {
            //     if (!hwarp->ififo.isempty())
            //         std::cout << "SM" << sm_id << " warp" << warp_id << " IBUF dispatch ins.bit=" << std::hex <<
            //         dispatch_ins_.origin32bit << ", and ibuf.top become " <<
            //         hwarp->ififo.front().origin32bit << std::dec << " at " << sc_time_stamp() << "," <<
            //         sc_delta_count_at_current_time() << std::endl;
            //     else
            //         std::cout << "SM" << sm_id << " warp" << warp_id << " IBUF dispatch ins.bit=" << std::hex <<
            //         dispatch_ins_.origin32bit << ", and ibuf become empty at " << sc_time_stamp() << "," <<
            //         sc_delta_count_at_current_time() << std::endl;
            // }
            // std::cout << "IBUF: after dispatch, ififo has " << ififo.used() << " elems at " << sc_time_stamp()
            // <<","<< sc_delta_count_at_current_time() << std::endl;
        } else {
            // std::cout << "IBUF: dispatch == false at " << sc_time_stamp() <<","<< sc_delta_count_at_current_time() <<
            // std::endl;
        }

        if (hwarp->fetch_valid2 && hwarp->jump == false
            && hwarp->simtstk_jump == false) {
            if (hwarp->ififo.isfull()) {
                // std::cout << "SM" << sm_id << " warp" << warp_id << " IFIFO is full(not error) at " <<
                // sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
            } else {
                hwarp->ififo.push(hwarp->decode_ins.read());
                hwarp->ibuf_swallow = true;

                // std::cout << "SM" << sm_id << " warp " << warp_id << " IFIFO push decode_ins=" <<
                // hwarp->decode_ins << " at " << sc_time_stamp() << "," <<
                // sc_delta_count_at_current_time() << std::endl;
            }
            // std::cout << "before put, ififo has " << ififo.used() << " elems at " << sc_time_stamp() <<","<<
            // sc_delta_count_at_current_time() << std::endl; std::cout << "after put, ififo has " << ififo.used() << "
            // elems at " << sc_time_stamp() <<","<< sc_delta_count_at_current_time() << std::endl;
        } else if (hwarp->jump || hwarp->simtstk_jump) {
            // std::cout << "ibuf detected jump at " << sc_time_stamp() <<","<< sc_delta_count_at_current_time() <<
            // std::endl;
            hwarp->ififo.clear();
        }
    }
    hwarp->ibuf_empty = hwarp->ififo.isempty();
    hwarp->ibuf_full = hwarp->ififo.isfull();
    if (hwarp->ififo.isempty()) {
        hwarp->ififo_elem_num = 0;
        hwarp->ibuftop_ins = I_TYPE(INVALID_, -1, 0, 0);
    } else {
        hwarp->ibuftop_ins.write(hwarp->ififo.front());
        hwarp->ififo_elem_num = hwarp->ififo.used();
        // std::cout << "ififo has " << ififo.used() << " elems in it at " << sc_time_stamp() <<","<<
        // sc_delta_count_at_current_time() << std::endl;
    }
    // if (sm_id == 0 && warp_id == 0)
    //     std::cout << "SM" << sm_id << " warp" << warp_id << " IBUF ififo_elem_num=" <<
    //     hwarp->ififo_elem_num << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time()
    //     << std::endl;
}

void BASE::cycle_UPDATE_SCORE(int warp_id, I_TYPE& tmpins, std::set<SCORE_TYPE>::iterator& it, REG_TYPE& regtype_,
                              bool& insertscore) {
    auto& hwarp = m_hw_warps[warp_id];
    if (wb_ena && wb_warpid == warp_id) {
        //
        // 写回阶段，删除score
        //
        tmpins = wb_ins;
        // std::cout << "scoreboard: wb_ins is " << tmpins << " at " << sc_time_stamp() <<","<<
        // sc_delta_count_at_current_time() << std::endl;
        if (tmpins.ddd.wvd) {
            if (tmpins.ddd.wxd)
                std::cout << "Scoreboard warp" << warp_id << " error: wb_ins wvd=wxd=1 at the same time at "
                          << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
            regtype_ = v;
        } else if (tmpins.ddd.wxd)
            regtype_ = s;
        else
            std::cout << "Scoreboard warp" << warp_id << " error: wb_ins wvd=wxd=0 at the same time at "
                      << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
        it = hwarp->score.find(SCORE_TYPE(regtype_, tmpins.d));
        // std::cout << "scoreboard写回: 正在寻找 SCORE " << SCORE_TYPE(regtype_, tmpins.d) << " at " << sc_time_stamp()
        // <<","<< sc_delta_count_at_current_time() << std::endl;
        if (it == hwarp->score.end()) {
            std::cout << "warp" << warp_id << "_wb_ena error: scoreboard can't find rd in score set, wb_ins=" << wb_ins
                      << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
        } else {
            hwarp->score.erase(it);
        }
        // std::cout << "warp" << warp_id << "_scoreboard: succesfully erased SCORE " << SCORE_TYPE(regtype_, tmpins.d)
        // << ", wb_ins=" << wb_ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() <<
        // std::endl;
    }

    //
    // dispatch阶段，写入score
    //
    tmpins = hwarp->ibuftop_ins; // this ibuftop_ins is the old data
    if (hwarp->branch_sig || hwarp->vbran_sig) {
        if (hwarp->wait_bran == 0)
            std::cout << "warp" << warp_id
                      << "_scoreboard error: detect (v)branch_sig=1(from salu) while wait_bran=0 at " << sc_time_stamp()
                      << "," << sc_delta_count_at_current_time() << std::endl;
        else if (hwarp->dispatch_warp_valid && (!opc_full | doemit))
            std::cout << "warp" << warp_id
                      << "_scoreboard error: detect (v)branch_sig=1(from salu) while dispatch=1 at " << sc_time_stamp()
                      << "," << sc_delta_count_at_current_time() << std::endl;
        hwarp->wait_bran = 0;
    } else if ((tmpins.ddd.branch != 0) && hwarp->dispatch_warp_valid
               && (!opc_full | doemit)) // 表示将要dispatch
    {
        // std::cout << "ibuf let wait_bran=1 at " << sc_time_stamp() <<","<< sc_delta_count_at_current_time() <<
        // std::endl;
        hwarp->wait_bran = 1;
    } else if (tmpins.op == OP_TYPE::ENDPRG_ && hwarp->dispatch_warp_valid
               && (!opc_full | doemit)) { // TODO: 权宜之计，让endprg后暂停dispatch
        // std::cout << "SM" << sm_id << " warp " << warp_id << " UPDATE_SCORE detect ENDPRG, suspend to dispatch at "
        // << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
        hwarp->wait_bran = 1;
    }

    if (hwarp->dispatch_warp_valid && (!opc_full | doemit)) { // 加入 score
        insertscore = true;
        if (tmpins.ddd.wvd) {
            if (tmpins.ddd.wxd)
                std::cout << "Scoreboard warp" << warp_id << " error: dispatch_ins wvd=wxd=1 at the same time at "
                          << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
            // if (sm_id == 0 && warp_id == 0 && tmpins.d == 0)
            //     std::cout << "SM" << sm_id << " warp" << warp_id << " UPDATE_SCORE insert ins.bit=" << std::hex <<
            //     tmpins.origin32bit << std::dec << " vector regfile 0 to scoreboard at " << sc_time_stamp() << "," <<
            //     sc_delta_count_at_current_time() << std::endl;
            regtype_ = v;
        } else if (tmpins.ddd.wxd)
            regtype_ = s;
        else
            insertscore = false;
        if (insertscore)
            hwarp->score.insert(SCORE_TYPE(regtype_, tmpins.d));
        // if (sm_id == 0)
        //     std::cout << "SM0 warp" << warp_id << "_scoreboard: insert " << SCORE_TYPE(regtype_, tmpins.d)
        //          << " because of dispatch " << tmpins << " at " << sc_time_stamp() << "," <<
        //          sc_delta_count_at_current_time() << std::endl;
    }
}

void BASE::cycle_JUDGE_DISPATCH(int warp_id, I_TYPE& _readibuf) {
    auto& hwarp = m_hw_warps[warp_id];
    if (hwarp->wait_bran | hwarp->jump) {
        hwarp->can_dispatch = false;
    } else if (!hwarp->ififo.isempty()) {
        _readibuf = hwarp->ififo.front();
        hwarp->can_dispatch = true;

        if (_readibuf.op == INVALID_)
            hwarp->can_dispatch = false;
        if (_readibuf.op == ENDPRG_ && !hwarp->score.empty())
            hwarp->can_dispatch = false;

        if (_readibuf.ddd.wxd
            && hwarp->score.find(SCORE_TYPE(s, _readibuf.d)) != hwarp->score.end())
            hwarp->can_dispatch = false;
        else if (_readibuf.ddd.wvd
                 && hwarp->score.find(SCORE_TYPE(v, _readibuf.d)) != hwarp->score.end())
            hwarp->can_dispatch = false;
        else if (_readibuf.ddd.sel_alu1 == DecodeParams::A1_RS1
                 && hwarp->score.find(SCORE_TYPE(s, _readibuf.s1)) != hwarp->score.end())
            hwarp->can_dispatch = false;
        else if (_readibuf.ddd.sel_alu1 == DecodeParams::A1_VRS1
                 && hwarp->score.find(SCORE_TYPE(v, _readibuf.s1)) != hwarp->score.end())
            hwarp->can_dispatch = false;
        else if (_readibuf.ddd.sel_alu2 == DecodeParams::sel_alu2_t::A2_RS2
                 && hwarp->score.find(SCORE_TYPE(s, _readibuf.s2)) != hwarp->score.end())
            hwarp->can_dispatch = false;
        else if (_readibuf.ddd.sel_alu2 == DecodeParams::sel_alu2_t::A2_VRS2
                 && hwarp->score.find(SCORE_TYPE(v, _readibuf.s2)) != hwarp->score.end())
            hwarp->can_dispatch = false;
        else if (_readibuf.ddd.sel_alu3 == DecodeParams::sel_alu3_t::A3_FRS3
                 && hwarp->score.find(SCORE_TYPE(s, _readibuf.s3)) != hwarp->score.end())
            hwarp->can_dispatch = false;
        else if (_readibuf.ddd.sel_alu3 == DecodeParams::sel_alu3_t::A3_VRS3
                 && hwarp->score.find(SCORE_TYPE(v, _readibuf.s3)) != hwarp->score.end())
            hwarp->can_dispatch = false;
        else if (_readibuf.ddd.sel_alu3 == DecodeParams::sel_alu3_t::A3_PC
                 && _readibuf.ddd.branch == DecodeParams::branch_t::B_R
                 && hwarp->score.find(SCORE_TYPE(s, _readibuf.s1)) != hwarp->score.end())
            hwarp->can_dispatch = false;
        else if (_readibuf.ddd.sel_alu3 == DecodeParams::sel_alu3_t::A3_SD
                 && (_readibuf.ddd.isvec & (!_readibuf.ddd.readmask))
                 && hwarp->score.find(SCORE_TYPE(s, _readibuf.s3)) != hwarp->score.end())
            hwarp->can_dispatch = false;
        else if (_readibuf.ddd.sel_alu3 == DecodeParams::sel_alu3_t::A3_SD
                 && !(_readibuf.ddd.isvec & (!_readibuf.ddd.readmask))
                 && hwarp->score.find(SCORE_TYPE(s, _readibuf.s2)) != hwarp->score.end())
            hwarp->can_dispatch = false;

        // if (sm_id == 0 && warp_id == 0)
        //     if (hwarp->can_dispatch == false)
        //         std::cout << "SM" << sm_id << " warp" << warp_id << " JUDGE_DISPATCH=false with ins.bit=" << std::hex
        //         << _readibuf.origin32bit << std::dec << " at " << sc_time_stamp() << "," <<
        //         sc_delta_count_at_current_time() << std::endl;

        // if (sm_id == 0 && warp_id == 0 && _readibuf.origin32bit == uint32_t(0x96013057) &&
        // hwarp->can_dispatch == false) if (sm_id == 0 && warp_id == 0 &&
        // hwarp->can_dispatch == false)
        //     std::cout << "SM" << sm_id << " warp" << warp_id << " JUDGE_DISPATCH meet ins.bit=" << std::hex <<
        //     _readibuf.origin32bit << std::dec << ", can't dispatch, ins.d=" << _readibuf.d << " at " <<
        //     sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
    } else if (hwarp->ififo.isempty())
        hwarp->can_dispatch = false;
}

void BASE::BEFORE_DISPATCH(int warp_id) {
    I_TYPE dispatch_ins_;
    I_TYPE _readdata3;
    I_TYPE tmpins;
    std::set<SCORE_TYPE>::iterator it;
    REG_TYPE regtype_;
    bool insertscore = false;
    I_TYPE _readibuf;

    auto& hwarp = m_hw_warps[warp_id];
    while (true) {
        wait(ev_warp_assigned);
        if (hwarp->is_warp_activated) {
            // if (sm_id == 0 && warp_id == 0)
            // std::cout << "SM" << sm_id << " warp" << warp_id << " before action, fetch_valid2=" <<
            // hwarp->fetch_valid2 << ", decode_ins=" << std::hex <<
            // hwarp->decode_ins.read().origin32bit
            //      << std::dec << ", jump=" << hwarp->jump << ", ififo.isfull=" <<
            //      hwarp->ififo.isfull() << " at " << sc_time_stamp() << "," <<
            //      sc_delta_count_at_current_time() << std::endl;

            cycle_IBUF_ACTION(warp_id, dispatch_ins_, _readdata3);
            cycle_UPDATE_SCORE(warp_id, tmpins, it, regtype_, insertscore);
            cycle_JUDGE_DISPATCH(warp_id, _readibuf);
            hwarp->ev_warp_dispatch.notify();
        } else {
            // 某个warp结束后，依然出发issue_list，否则warp_scheduler无法运行
            hwarp->ev_warp_dispatch.notify();
        }
        if (hwarp->endprg_flush_pipe) {
            hwarp->ififo.clear();
            hwarp->wait_bran = false;
        }
    }
}

void BASE::set_kernel(std::shared_ptr<kernel_info_t> kernel) {
    assert(kernel);
    m_kernel = kernel;
    std::cout << "SM " << sm_id << " bind to kernel " << m_kernel->get_kid() << " \"" << m_kernel->get_kname()
              << "\" at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
}

bool BASE::can_issue_1block(std::shared_ptr<kernel_info_t> kernel) {
    if (max_cta_num(kernel) < 1)
        return false;
    else {
        // 若找到core中空闲的一组warp，则可以分派线程块
        bool found_idle_cta_slot;
        for (int idx = 0; idx < MAX_CTA_PER_CORE; idx++) {
            found_idle_cta_slot = true;
            for (int i = 0; i < kernel->get_num_warp_per_cta(); i++) {
                uint32_t wid = idx * kernel->get_num_warp_per_cta() + i;
                if (wid >= hw_num_warp) {
                    return false;
                } else {
                    found_idle_cta_slot = !m_hw_warps[wid]->is_warp_activated;
                    if (!found_idle_cta_slot)
                        break;
                }
            }
            if (found_idle_cta_slot) {
                return true;
            }
        }
        return false;
    }
}

unsigned BASE::max_cta_num(std::shared_ptr<kernel_info_t> kernel) {
    unsigned kernel_num_thread_per_warp = kernel->get_num_thread_per_warp();
    unsigned kernel_num_warp_per_cta = kernel->get_num_warp_per_cta();
    if (kernel_num_thread_per_warp > hw_num_thread)
        return 0;

    // limited by warps
    unsigned result_warp;
    result_warp = (hw_num_warp - m_num_warp_activated) / kernel_num_warp_per_cta;

    // limited by local memory size
    unsigned kernel_ldsSize_per_cta = kernel->get_ldsSize_per_cta();
    unsigned result_localmem;
    result_localmem = hw_lds_size / kernel_ldsSize_per_cta - m_num_active_cta;
    // TODO: Bug? Where is m_num_active_cta initialized?

    return result_warp < result_localmem ? result_warp : result_localmem;
}

// SM receive new block
void BASE::receive_warp(uint32_t block_idx, uint32_t warp_idx, std::shared_ptr<kernel_info_t> kernel,
                        uint32_t block_slot, uint32_t lds_baseaddr) {
    assert(kernel);
    assert(kernel->get_num_thread_per_warp() <= hw_num_thread);

    assert(block_idx == kernel->get_next_cta_id_single());
    dim3 block_idx_3d = kernel->get_next_cta_id();

    // Find a idle hardware warp
    WARP_BONE* hwarp = nullptr; // hardware warp
    uint32_t hw_warp_idx = 0xFFFFFFFF;
    for (uint32_t idx = 0; idx < hw_num_warp; idx++) {
        if (m_hw_warps[idx]->is_warp_activated == false && m_hw_warps[idx]->will_warp_activate == false) {
            hwarp = m_hw_warps[idx];
            hw_warp_idx = idx;
            break;
        }
    }
    assert(hwarp != nullptr); // should always find a idle warp, as CTA scheduler has checked warp_slot before
    hwarp->will_warp_activate = true;

    hwarp->m_ctaid_in_core = block_slot;
    hwarp->CSR_reg[0x800] = warp_idx * kernel->get_num_thread_per_warp();
    hwarp->CSR_reg[0x801] = kernel->get_num_warp_per_cta();
    hwarp->CSR_reg[0x802] = kernel->get_num_thread_per_warp();
    hwarp->CSR_reg[0x803] = kernel->get_metadata_baseaddr();
    hwarp->CSR_reg[0x804] = block_slot;
    hwarp->CSR_reg[0x805] = warp_idx;
    hwarp->CSR_reg[0x806] = ldsBaseAddr_core + lds_baseaddr;
    hwarp->CSR_reg[0x807] = kernel->get_pdsBaseAddr()
        + (block_idx * kernel->get_num_warp_per_cta() + warp_idx) * kernel->get_num_thread_per_warp()
            * kernel->get_pdsSize_per_thread();
    hwarp->CSR_reg[0x808] = block_idx_3d.x;
    hwarp->CSR_reg[0x809] = block_idx_3d.y;
    hwarp->CSR_reg[0x80a] = block_idx_3d.z;
    hwarp->CSR_reg[0x300] = 0x00001800; // WHY? CSR[mstatus] default value

    hwarp->is_warp_activated.write(true);
    hwarp->pc.write(kernel->get_startaddr());
    hwarp->pagetable = kernel->get_pagetable();
    hwarp->num_thread = kernel->get_num_thread_per_warp();
    hwarp->blk_slot_idx = block_slot;
    hwarp->warp_idx_in_blk = warp_idx;

    sc_bv<hw_num_thread> _validmask = 0;
    for (int i = 0; i < kernel->get_num_thread_per_warp(); i++) {
        _validmask[i] = 1;
    }
    hwarp->current_mask.write(_validmask);

    m_num_warp_activated++;
    kernel->m_warp_status[block_idx][warp_idx] = kernel_info_t::WARP_STATUS_RUNNING;
    std::cout << std::dec << "SM " << sm_id << " warp " << hw_warp_idx << " is activated at " << sc_time_stamp() << ","
              << sc_delta_count_at_current_time() << " (kernel " << kernel->get_kname() << " block " << block_idx
              << " warp " << warp_idx << ")" << std::endl;
}

void BASE::issue_block2core(std::shared_ptr<kernel_info_t> kernel) {

    unsigned kernel_num_thread_per_warp = kernel->get_num_thread_per_warp();
    unsigned kernel_num_warp_per_cta = kernel->get_num_warp_per_cta();

    unsigned free_ctaid_in_core; // 为cta分配的core内ctaid. 对于给定kernel，存在ctaid和warp之间的确定映射
    unsigned hw_start_warpid = (unsigned)-1;

    assert(can_issue_1block(kernel));
    // 找到core中空闲的一组warp和相应的ctaid
    for (int idx = 0; idx < MAX_CTA_PER_CORE; idx++) {
        free_ctaid_in_core = idx;
        for (int i = 0; i < kernel->get_num_warp_per_cta(); i++) {
            uint32_t wid = idx * kernel->get_num_warp_per_cta() + i;
            if (wid >= hw_num_warp) {
                assert(0);
            } else if (m_hw_warps[wid]->is_warp_activated) {
                free_ctaid_in_core = -1;
                break;
            }
        }
        if (free_ctaid_in_core == idx) {
            hw_start_warpid = idx * kernel->get_num_warp_per_cta();
            break;
        }
    }
    assert(free_ctaid_in_core != (unsigned)-1);

    dim3 ctaid_kernel = kernel->get_next_cta_id();
    unsigned ctaid_kernel_single = kernel->get_next_cta_id_single();
    assert(kernel->m_block_status[ctaid_kernel_single] == kernel_info_t::BLOCK_STATUS_WAIT);
    kernel->m_block_status[ctaid_kernel_single] = kernel_info_t::BLOCK_STATUS_RUNNING;

    // 遍历并激活每个warp
    for (unsigned widINcta = 0; widINcta < kernel_num_warp_per_cta; widINcta++) {
        unsigned hw_wid = widINcta + hw_start_warpid;
        m_hw_warps[hw_wid]->m_ctaid_in_core = free_ctaid_in_core;
        m_hw_warps[hw_wid]->CSR_reg[0x800] = widINcta * kernel_num_thread_per_warp;
        m_hw_warps[hw_wid]->CSR_reg[0x801] = kernel_num_warp_per_cta;
        m_hw_warps[hw_wid]->CSR_reg[0x802] = kernel_num_thread_per_warp;
        m_hw_warps[hw_wid]->CSR_reg[0x803] = kernel->get_metadata_baseaddr();
        m_hw_warps[hw_wid]->CSR_reg[0x804] = free_ctaid_in_core;
        m_hw_warps[hw_wid]->CSR_reg[0x805] = widINcta;
        m_hw_warps[hw_wid]->CSR_reg[0x806] = ldsBaseAddr_core + free_ctaid_in_core * kernel->get_ldsSize_per_cta();
        m_hw_warps[hw_wid]->CSR_reg[0x807] = kernel->get_pdsBaseAddr()
            + (ctaid_kernel_single * kernel_num_warp_per_cta + widINcta) * kernel_num_thread_per_warp
                * kernel->get_pdsSize_per_thread();
        m_hw_warps[hw_wid]->CSR_reg[0x808] = ctaid_kernel.x;
        m_hw_warps[hw_wid]->CSR_reg[0x809] = ctaid_kernel.y;
        m_hw_warps[hw_wid]->CSR_reg[0x80a] = ctaid_kernel.z;

        m_hw_warps[hw_wid]->CSR_reg[0x300] = 0x00001800; // WHY? mstatus CSR default value

        m_hw_warps[hw_wid]->is_warp_activated.write(true);
        std::cout << std::dec << "SM " << sm_id << " warp " << hw_wid << " is activated at " << sc_time_stamp() << ","
                  << sc_delta_count_at_current_time() << " (kernel " << kernel->get_kname() << " CTA "
                  << ctaid_kernel_single << ")" << std::endl;
        m_hw_warps[hw_wid]->pc.write(kernel->get_startaddr());
        m_hw_warps[hw_wid]->pagetable = kernel->get_pagetable();
        m_hw_warps[hw_wid]->num_thread = kernel->get_num_thread_per_warp();
        // m_hw_warps[hw_wid]->fetch_valid = true;

        // warp finish callback to CTA Scheduler
        m_hw_warps[hw_wid]->finish_callback = [kernel, ctaid_kernel_single, widINcta](int sm_id, int hw_wid) {
            assert(kernel->m_status == kernel_info_t::KERNEL_STATUS_RUNNING);
            assert(kernel->m_block_status[ctaid_kernel_single] == kernel_info_t::BLOCK_STATUS_RUNNING);
            assert(kernel->m_warp_status[ctaid_kernel_single][widINcta] == kernel_info_t::WARP_STATUS_RUNNING);
            assert(kernel->m_block_sm_id[ctaid_kernel_single] == sm_id);
            kernel->m_warp_status[ctaid_kernel_single][widINcta] = kernel_info_t::WARP_STATUS_FINISHED;
        };

        sc_bv<hw_num_thread> _validmask = 0;
        for (int i = 0; i < kernel_num_thread_per_warp; i++) {
            _validmask[i] = 1;
        }
        m_hw_warps[hw_wid]->current_mask.write(_validmask);
        m_issue_block2warp[hw_wid] = true;
        kernel->m_warp_status[ctaid_kernel_single][hw_wid] = kernel_info_t::WARP_STATUS_RUNNING;
    }
    m_num_warp_activated += kernel_num_warp_per_cta;
    m_current_kernel_running.write(true);
    m_current_kernel_completed.write(false);
    kernel->increment_cta_id();
    std::cout << "SM " << sm_id << " issue 1 block of kernel \"" << kernel->get_kname() << "\" at " << sc_time_stamp()
              << "," << sc_delta_count_at_current_time() << std::endl;
}

void increment_x_then_y_then_z(dim3& i, const dim3& bound) {
    i.x++;
    if (i.x >= bound.x) {
        i.x = 0;
        i.y++;
        if (i.y >= bound.y) {
            i.y = 0;
            if (i.z < bound.z)
                i.z++;
        }
    }
}
