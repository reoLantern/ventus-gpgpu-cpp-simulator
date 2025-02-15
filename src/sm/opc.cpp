#include "BASE.h"

bank_t BASE::bank_decode(int warp_id, int srcaddr)
{
    bank_t tmp;
    tmp.bank_id = srcaddr % BANK_NUM;
    tmp.addr = warp_id * num_register_per_warp / BANK_NUM + srcaddr / BANK_NUM;
    return tmp;
}

warpaddr_t BASE::bank_undecode(int bank_id, int bankaddr)
{
    warpaddr_t tmp;
    tmp.warp_id = bankaddr / (num_register_per_warp / BANK_NUM);
    tmp.addr = (bankaddr % (num_register_per_warp / BANK_NUM)) * BANK_NUM + bank_id;
    return tmp;
}

void BASE::OPC_FIFO()
{
    vector_t printdata_;
    I_TYPE _readdata4;
    int _readwarpid;
    std::array<bool, 3> in_ready;
    std::array<bool, 3> in_valid;
    std::array<bank_t, 3> in_srcaddr;
    std::array<bool, 3> in_banktype;
    opcfifo_t newopcdat;
    while (true)
    {
        // std::cout << "SM" << sm_id << " OPC_FIFO: finish at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
        wait();
        // std::cout << "SM" << sm_id << " OPC_FIFO: start at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
        if (doemit)
        {
            // std::cout << "opcfifo is popping index " << emit_idx << " at " << sc_time_stamp() <<","<< sc_delta_count_at_current_time() << "\n";
            auto popdat = opcfifo[emit_idx];
            opcfifo.pop(emit_idx); // last cycle emit
            // std::cout << "OPC_FIFO: poped ins " << popdat.ins << "warp" << popdat.warp_id << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
        }
        ev_opc_pop.notify();
        // TODO: 按目前的事件顺序，若发生某ins进入OPC而立刻ready，则会有问题，后续要修改
        if (dispatch_valid)
        {
            if (opc_full && doemit == false) // 相当于上一cycle dispatch_ready
            {
                // if not ready, just wait, no need throw ERROR
                // std::cout << "OPC ERROR: is full but receive ins from issue at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
            }
            else
            {
                _readdata4 = issue_ins.read();
                _readwarpid = issueins_warpid;
                // std::cout << "SM" << sm_id << " opc begin to put, warpid=" << issueins_warpid << ", at " << sc_time_stamp() << ", " << sc_delta_count_at_current_time() << "\n";

                in_ready = {1, 1, 1};
                in_valid = {0, 0, 0}; // 要取操作数，则ready=0，valid=1

                if (_readdata4.ddd.sel_alu1 == DecodeParams::A1_RS1)
                {
                    in_ready[0] = 0;
                    in_valid[0] = 1;
                    in_srcaddr[0] = bank_decode(_readwarpid, _readdata4.s1);
                    in_banktype[0] = 0; // 0为scalar，1为vector
                }
                else if (_readdata4.ddd.sel_alu1 == DecodeParams::A1_VRS1)
                {
                    in_ready[0] = 0;
                    in_valid[0] = 1;
                    in_srcaddr[0] = bank_decode(_readwarpid, _readdata4.s1);
                    in_banktype[0] = 1;
                }
                else if (_readdata4.ddd.sel_alu1 == DecodeParams::A1_IMM)
                {
                    newopcdat.data[0].fill(_readdata4.imm);
                }
                else if (_readdata4.ddd.sel_alu1 == DecodeParams::A1_PC)
                {
                    newopcdat.data[0].fill(_readdata4.currentpc);
                }

                if (_readdata4.ddd.sel_alu2 == DecodeParams::sel_alu2_t::A2_RS2)
                {
                    in_ready[1] = 0;
                    in_valid[1] = 1;
                    in_srcaddr[1] = bank_decode(_readwarpid, _readdata4.s2);
                    in_banktype[1] = 0;
                }
                else if (_readdata4.ddd.sel_alu2 == DecodeParams::sel_alu2_t::A2_VRS2)
                {
                    in_ready[1] = 0;
                    in_valid[1] = 1;
                    in_srcaddr[1] = bank_decode(_readwarpid, _readdata4.s2);
                    in_banktype[1] = 1;
                }
                else if (_readdata4.ddd.sel_alu2 == DecodeParams::sel_alu2_t::A2_IMM)
                {
                    newopcdat.data[1].fill(_readdata4.imm);
                }
                else if (_readdata4.ddd.sel_alu2 == DecodeParams::sel_alu2_t::A2_SIZE)
                {
                    newopcdat.data[1].fill(4);
                }

                if (_readdata4.ddd.sel_alu3 == DecodeParams::sel_alu3_t::A3_FRS3)
                {
                    in_ready[2] = 0;
                    in_valid[2] = 1;
                    in_srcaddr[2] = bank_decode(_readwarpid, _readdata4.s3);
                    in_banktype[2] = 0;
                }
                else if (_readdata4.ddd.sel_alu3 == DecodeParams::sel_alu3_t::A3_VRS3)
                {
                    in_ready[2] = 0;
                    in_valid[2] = 1;
                    in_srcaddr[2] = bank_decode(_readwarpid, _readdata4.s3);
                    in_banktype[2] = 1;
                }
                else if (_readdata4.ddd.sel_alu3 == DecodeParams::sel_alu3_t::A3_PC && _readdata4.ddd.branch != DecodeParams::branch_t::B_R)
                { // BEQ
                    newopcdat.data[2].fill(_readdata4.imm + _readdata4.currentpc);
                }
                else if (_readdata4.ddd.sel_alu3 == DecodeParams::sel_alu3_t::A3_PC)
                { // JALR
                    in_ready[2] = 0;
                    in_valid[2] = 1;
                    in_srcaddr[2] = bank_decode(_readwarpid, _readdata4.s1);
                    in_banktype[2] = 0;
                }
                else if (_readdata4.ddd.sel_alu3 == DecodeParams::sel_alu3_t::A3_SD)
                {
                    in_ready[2] = 0;
                    in_valid[2] = 1;
                    in_srcaddr[2] = bank_decode(_readwarpid, (_readdata4.ddd.isvec & (!_readdata4.ddd.readmask)) ? _readdata4.s3 : _readdata4.s2);
                    in_banktype[2] = _readdata4.ddd.isvec ? 1 : 0;
                }

                // std::cout << "opcfifo push issue_ins " << _readdata4 << "warp" << _readwarpid
                //      << ", srcaddr=(" << in_srcaddr[0] << ";" << in_srcaddr[1] << ")"
                //      << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";

                newopcdat.ins = _readdata4;
                newopcdat.warp_id = _readwarpid;
                newopcdat.ready = in_ready;
                newopcdat.valid = in_valid;
                newopcdat.srcaddr = in_srcaddr;
                newopcdat.banktype = in_banktype;

                opcfifo.push(newopcdat);
            }
        }
        opcfifo_elem_num = opcfifo.get_size();
        opc_full = opcfifo.get_size() == OPCFIFO_SIZE;
        opc_empty = opcfifo.get_size() == 0;
        // std::cout << "OPC_FIFO waiting ev_opc_judge_emit at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
        wait(ev_opc_judge_emit & ev_regfile_readdata);
        // std::cout << "OPC_FIFO get ev_opc_judge_emit at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";

        //  由ready写入entry，不能影响当前cycle判断emit
        for (int i = 0; i < OPCFIFO_SIZE; i++)
        {
            for (int j = 0; j < 3; j++)
                if (opc_ready[i][j] == true)
                {
                    if (opcfifo[i].valid[j] == false)
                        std::cout << "opc collect error[" << i << "," << j << "]: ins " << magic_enum::enum_name((OP_TYPE)opcfifo[i].ins.op) << " ready=1 but valid=0 at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                    opcfifo[i].ready[j] = true;
                    opcfifo[i].valid[j] = false;
                    opcfifo[i].data[j] = read_data[opcfifo[i].srcaddr[j].bank_id];
                    printdata_ = read_data[opcfifo[i].srcaddr[j].bank_id];
                    // std::cout << "OPC_FIFO: store_in[" << i << "," << j << "], ins=" << magic_enum::enum_name((OP_TYPE)opcfifo[i].ins.op) << "warp" << opcfifo[i].warp_id
                    //      << ", data[" << j << "]=" << printdata_ << ", srcaddr=" << opcfifo[i].srcaddr[j] << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                }
        }
        ev_opc_store.notify();
        // std::cout << "OPC: entry[0]: ins=" << magic_enum::enum_name((OP_TYPE)opcfifo[0].ins.op) << ", tag_valid="<<opcfifo.tag_valid(0)<<", valid={" << opcfifo[0].valid[0] << "," << opcfifo[0].valid[1] << "," << opcfifo[0].valid[2] << "}, ready={" << opcfifo[0].ready[0] << "," << opcfifo[0].ready[1] << "," << opcfifo[0].ready[2] << "}\n";
        // std::cout << "     entry[1]: ins=" << magic_enum::enum_name((OP_TYPE)opcfifo[1].ins.op) << ", tag_valid="<<opcfifo.tag_valid(1)<<", valid={" << opcfifo[1].valid[0] << "," << opcfifo[1].valid[1] << "," << opcfifo[1].valid[2] << "}, ready={" << opcfifo[1].ready[0] << "," << opcfifo[1].ready[1] << "," << opcfifo[1].ready[2] << "}\n";
        // std::cout << "     entry[2]: ins=" << magic_enum::enum_name((OP_TYPE)opcfifo[2].ins.op) << ", tag_valid="<<opcfifo.tag_valid(2)<<", valid={" << opcfifo[2].valid[0] << "," << opcfifo[2].valid[1] << "," << opcfifo[2].valid[2] << "}, ready={" << opcfifo[2].ready[0] << "," << opcfifo[2].ready[1] << "," << opcfifo[2].ready[2] << "}\n";
        // std::cout << "     entry[3]: ins=" << magic_enum::enum_name((OP_TYPE)opcfifo[3].ins.op) << ", tag_valid="<<opcfifo.tag_valid(3)<<", valid={" << opcfifo[3].valid[0] << "," << opcfifo[3].valid[1] << "," << opcfifo[3].valid[2] << "}, ready={" << opcfifo[3].ready[0] << "," << opcfifo[3].ready[1] << "," << opcfifo[3].ready[2] << "} at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
    }
}

void BASE::OPC_EMIT()
{
    reg_t pa1;
    reg_t *pa2; // 用于int转float
    float *pf1, *pf2;
    // FloatAndInt pr1, pr2;
    int p20;
    last_emit_entryid = 0;
    while (true)
    {
        wait(ev_opc_pop & // 等opc当前cycle pop之后再判断下一cycle的pop
             ev_saluready_updated & ev_valuready_updated &
             ev_vfpuready_updated & ev_lsuready_updated &
             ev_csrready_updated & ev_mulready_updated &
             ev_sfuready_updated & ev_tcready_updated);
        // std::cout << "OPC_EMIT start at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
        doemit = false;
        findemit = 0;
        emito_salu = false;
        emito_valu = false;
        emito_vfpu = false;
        emito_lsu = false;
        emito_simtstk = false;
        emito_csr = false;
        emito_mul = false;
        emito_sfu = false;
        emito_tc = false;
        emito_warpscheduler = false;
        for (int i = last_emit_entryid; i < last_emit_entryid + OPCFIFO_SIZE; i++)
        {
            int entryidx = i % OPCFIFO_SIZE;
            if (findemit)
                break;
            if (opcfifo.tag_valid(entryidx) && opcfifo[entryidx].all_ready())
            {
                emit_ins = opcfifo[entryidx].ins;
                emitins_warpid = opcfifo[entryidx].warp_id;

                // std::cout << "opcfifo[" << entryidx << "]-" << opcfifo[entryidx].ins << " is all ready, at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                switch (opcfifo[entryidx].ins.ddd.sel_execunit)
                {
                case DecodeParams::SALU:
                    if (salu_ready)
                    {
                        emit_idx = entryidx;
                        last_emit_entryid = entryidx + 1;
                        findemit = 1;
                        doemit = true;
                        // std::cout << "OPC: salu is ready at " << sc_time_stamp() <<","<< sc_delta_count_at_current_time() << "\n";
                        emito_salu = true;
                        tosalu_data1 = opcfifo[entryidx].data[0][0];
                        tosalu_data2 = opcfifo[entryidx].data[1][0];
                        tosalu_data3 = opcfifo[entryidx].data[2][0];
                    }
                    else
                    {
                        // std::cout << "OPC_EMIT: find all_ready ins " << opcfifo[entryidx].ins << "warp" << opcfifo[entryidx].warp_id << " but salu not ready at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                    }
                    break;

                case DecodeParams::VALU:
                    if (valu_ready)
                    {
                        emit_idx = entryidx;
                        last_emit_entryid = entryidx + 1;
                        findemit = 1;
                        doemit = true;
                        emito_valu = true;

                        for (int j = 0; j < hw_num_thread; j++)
                        {
                            tovalu_data1[j] = opcfifo[entryidx].data[0][j];
                            tovalu_data2[j] = opcfifo[entryidx].data[1][j];
                            tovalu_data3[j] = opcfifo[entryidx].data[2][j];
                        }
                    }
                    break;

                case DecodeParams::SIMTSTK:
                    emit_idx = entryidx;
                    last_emit_entryid = entryidx + 1;
                    findemit = 1;
                    doemit = true;
                    emito_simtstk = true;
                    break;

                case DecodeParams::VFPU:
                    if (vfpu_ready)
                    {
                        emit_idx = entryidx;
                        last_emit_entryid = entryidx + 1;
                        findemit = 1;
                        doemit = true;
                        emito_vfpu = true;

                        for (int j = 0; j < hw_num_thread; j++)
                        {
                            tovfpu_data1[j] = opcfifo[entryidx].data[0][j];
                            tovfpu_data2[j] = opcfifo[entryidx].data[1][j];
                            tovfpu_data3[j] = opcfifo[entryidx].data[2][j];
                        }
                    }
                    break;

                case DecodeParams::LSU:
                    if (lsu_ready)
                    {
                        emit_idx = entryidx;
                        last_emit_entryid = entryidx + 1;
                        findemit = 1;
                        doemit = true;
                        emito_lsu = true;

                        for (int j = 0; j < hw_num_thread; j++)
                        {
                            tolsu_data1[j] = opcfifo[entryidx].data[0][j];
                            tolsu_data2[j] = opcfifo[entryidx].data[1][j];
                            tolsu_data3[j] = opcfifo[entryidx].data[2][j];
                        }
                    }
                    break;

                case DecodeParams::CSR:
                    if (csr_ready)
                    {
                        emit_idx = entryidx;
                        last_emit_entryid = entryidx + 1;
                        findemit = 1;
                        doemit = true;
                        emito_csr = true;
                        tocsr_data1 = opcfifo[entryidx].data[0][0];
                        // std::cout << "opc: emito csr, tocsr_data1=opcfifo[entryidx].data[0][0]=0x" << std::hex << opcfifo[entryidx].data[0][0] << std::dec << "\n";
                        tocsr_data2 = opcfifo[entryidx].data[1][0];
                    }
                    break;
                case DecodeParams::MUL:
                    if (mul_ready)
                    {
                        emit_idx = entryidx;
                        last_emit_entryid = entryidx + 1;
                        findemit = 1;
                        doemit = true;
                        emito_mul = true;

                        for (int j = 0; j < hw_num_thread; j++)
                        {
                            tomul_data1[j] = opcfifo[entryidx].data[0][j];
                            tomul_data2[j] = opcfifo[entryidx].data[1][j];
                            tomul_data3[j] = opcfifo[entryidx].data[2][j];
                        }
                    }
                    break;
                case DecodeParams::SFU:
                    if (sfu_ready)
                    {
                        emit_idx = entryidx;
                        last_emit_entryid = entryidx + 1;
                        findemit = 1;
                        doemit = true;
                        emito_sfu = true;

                        for (int j = 0; j < hw_num_thread; j++)
                        {
                            tosfu_data1[j] = opcfifo[entryidx].data[0][j];
                            tosfu_data2[j] = opcfifo[entryidx].data[1][j];
                        }
                    }
                    break;

                case DecodeParams::TC:
                    if (tc_ready)
                    {
                        emit_idx = entryidx;
                        last_emit_entryid = entryidx + 1;
                        findemit = 1;
                        doemit = true;
                        emito_tc = true;
                        for (int j = 0; j < hw_num_thread; j++)
                        {
                            totc_data1[j] = opcfifo[entryidx].data[0][j];
                            totc_data2[j] = opcfifo[entryidx].data[1][j];
                            totc_data3[j] = opcfifo[entryidx].data[2][j];
                        }
                    }
                    break;

                case DecodeParams::WPSCHEDLER:
                    emit_idx = entryidx;
                    last_emit_entryid = entryidx + 1;
                    findemit = 1;
                    doemit = true;
                    emito_warpscheduler = true;
                    break;

                case DecodeParams::INVALID_EXECUNIT:
                    std::cout << "SM" << sm_id << " OPC_EMIT error: ins=" << opcfifo[entryidx].ins << "," << std::hex << opcfifo[entryidx].ins.origin32bit << std::dec
                         << " but INVALID EXECUNIT at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                    break;
                default:
                    std::cout << "SM" << sm_id << " OPC_EMIT warning: ins=" << opcfifo[entryidx].ins << "," << std::hex << opcfifo[entryidx].ins.origin32bit << std::dec
                         << " but undefined EXECUNIT at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                    break;
                }
            }
        }
        // std::cout << "emit_idx is set to " << emit_idx << "\n";
        ev_opc_judge_emit.notify();
    }
}

void BASE::OPC_FETCH()
{
    while (true)
    {
        wait(ev_opc_store);
        for (int i = 0; i < OPCFIFO_SIZE; i++)
        {
            if (opcfifo.tag_valid(i) == false)
            {
                opc_valid[i].fill(false);
            }
            else
            {
                opc_valid[i] = opcfifo[i].valid;
                opc_srcaddr[i] = opcfifo[i].srcaddr;
                opc_banktype[i] = opcfifo[i].banktype;
            }
        }
        ev_opc_collect.notify();
    }
}
