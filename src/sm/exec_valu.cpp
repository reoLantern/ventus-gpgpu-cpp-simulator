#include "BASE.h"

void BASE::VALU_IN()
{
    valu_in_t new_data;
    int a_delay, b_delay;
    sc_bv<hw_num_thread> _velsemask;
    sc_bv<hw_num_thread> _vifmask;
    while (true)
    {
        wait();
        if (emito_valu)
        {
            if (valu_ready_old == false)
                std::cout << "valu error: not ready at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
            valu_unready.notify();

            if (emit_ins.read().ddd.branch != DecodeParams::branch_t::B_N)
            {
                new_data.ins = emit_ins;
                new_data.warp_id = emitins_warpid;
                for (int i = 0; i < m_hw_warps[new_data.warp_id]->CSR_reg[0x802]; i++)
                {
                    new_data.rsv1_data[i] = tovalu_data1[i];
                    new_data.rsv2_data[i] = tovalu_data2[i];
                    new_data.rsv3_data[i] = tovalu_data3[i];
                }
                valu_dq.push(new_data);
                a_delay = 3;
                b_delay = 1;
                if (a_delay == 0)
                    valu_eva.notify();
                else if (valueqa_triggered)
                    valu_eqa.notify(sc_time((a_delay)*PERIOD, SC_NS));
                else
                {
                    valu_eqa.notify(sc_time((a_delay)*PERIOD, SC_NS));
                    ev_valufifo_pushed.notify();
                    valuto_simtstk = false;
                }
                if (b_delay == 0)
                    valu_evb.notify();
                else
                {
                    valu_eqb.notify(sc_time((b_delay)*PERIOD, SC_NS));
                    ev_valuready_updated.notify();
                }
            }
            else
            {
                new_data.ins = emit_ins;
                new_data.warp_id = emitins_warpid;
                for (int i = 0; i < m_hw_warps[new_data.warp_id]->CSR_reg[0x802]; i++)
                {
                    new_data.rsv1_data[i] = tovalu_data1[i];
                    new_data.rsv2_data[i] = tovalu_data2[i];
                }

                // if (new_data.ins.origin32bit == (uint32_t)0x42026057)
                //     std::cout << "valu vmv.s.x, rsv1_data=" << new_data.rsv1_data[0] << std::endl;

                valu_dq.push(new_data);
                a_delay = 3;
                b_delay = 1;
                // std::cout << "valu: receive VADD_VV_, will notify eq, at " << sc_time_stamp() <<","<< sc_delta_count_at_current_time() << std::endl;
                if (a_delay == 0)
                    valu_eva.notify();
                else if (valueqa_triggered)
                    valu_eqa.notify(sc_time((a_delay)*PERIOD, SC_NS));
                else
                {
                    valu_eqa.notify(sc_time((a_delay)*PERIOD, SC_NS));
                    ev_valufifo_pushed.notify();
                    valuto_simtstk = false;
                }
                if (b_delay == 0)
                    valu_evb.notify();
                else
                {
                    valu_eqb.notify(sc_time((b_delay)*PERIOD, SC_NS));
                    ev_valuready_updated.notify();
                }
            }
        }
        else
        {
            if (!valueqa_triggered)
            {
                ev_valufifo_pushed.notify();
                valuto_simtstk = false;
            }
            if (!valueqb_triggered)
                ev_valuready_updated.notify();
        }
    }
}

void BASE::VALU_CALC()
{
    valufifo_elem_num = 0;
    valufifo_empty = 1;
    valueqa_triggered = false;
    valu_in_t valutmp1;
    valu_out_t valutmp2;
    bool succeed;
    sc_bv<hw_num_thread> _velsemask, _vifmask;
    while (true)
    {
        wait(valu_eva | valu_eqa.default_event());
        if (valu_eqa.default_event().triggered())
        {
            valueqa_triggered = true;
            wait(SC_ZERO_TIME);
            valueqa_triggered = false;
        }
        valuto_simtstk = false;
        // std::cout << "valu_eqa.default_event triggered at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
        valutmp1 = valu_dq.front();
        valu_dq.pop();
        auto& hwarp = m_hw_warps[valutmp1.warp_id];
        if (valutmp1.ins.ddd.wxd | valutmp1.ins.ddd.wvd)
        {
            valutmp2.ins = valutmp1.ins;
            valutmp2.warp_id = valutmp1.warp_id;
            switch (valutmp1.ins.ddd.alu_fn)
            {

            case DecodeParams::alu_fn_t::FN_ADD:
                // VADD12.VI, VADD.VI, VADD.VV, VADD.VX
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                {
                    if (valutmp2.ins.mask[i] == 1)
                        valutmp2.rdv1_data[i] = valutmp1.rsv1_data[i] + valutmp1.rsv2_data[i];
                }
                break;

            case DecodeParams::alu_fn_t::FN_AND:
                // VAND.VI, VAND.VV, VAND.VX
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                {
                    if (valutmp2.ins.mask[i] == 1)
                        valutmp2.rdv1_data[i] = valutmp1.rsv1_data[i] & valutmp1.rsv2_data[i];
                }
                break;

            case DecodeParams::alu_fn_t::FN_SL:
                // VSLL.VI, VSLL.VV, VSLL.VX
                if (!valutmp1.ins.ddd.reverse)
                    for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                    {
                        if (valutmp2.ins.mask[i] == 1)
                            valutmp2.rdv1_data[i] = valutmp1.rsv1_data[i] << valutmp1.rsv2_data[i];
                    }
                else
                    for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                    {
                        if (valutmp2.ins.mask[i] == 1)
                            valutmp2.rdv1_data[i] = valutmp1.rsv2_data[i] << valutmp1.rsv1_data[i];
                    }
                break;

            case DecodeParams::alu_fn_t::FN_SUB:
                // VSUB12.VI, VSUB.VV, VSUB.VX
                if (!valutmp1.ins.ddd.reverse)
                    for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                    {
                        if (valutmp2.ins.mask[i] == 1)
                            valutmp2.rdv1_data[i] = valutmp1.rsv1_data[i] - valutmp1.rsv2_data[i];
                    }
                else
                    for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                    {
                        if (valutmp2.ins.mask[i] == 1)
                            valutmp2.rdv1_data[i] = valutmp1.rsv2_data[i] - valutmp1.rsv1_data[i];
                    }
                break;
            case DecodeParams::alu_fn_t::FN_VID:
                // VID.V
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                    valutmp2.rdv1_data[i] = i;
                break;

            case DecodeParams::alu_fn_t::FN_A2ZERO:
                // VMV.S.X
                // 由于指令编码错误 现在当成vmv.v.x
                // std::cout << "VALU_CALC switch to FN_A2ZERO, RSDATA=" << valutmp1.rsv1_data[0]
                //      << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                    valutmp2.rdv1_data[i] = valutmp1.rsv1_data[0];
                break;

            default:
                std::cout << "VALU_CALC warning: switch to unrecognized ins" << valutmp1.ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
                break;
            }
            valufifo.push(valutmp2);
        }
        else
        { // for branch instructions
            _velsemask = 0;
            _vifmask = 0;
            switch (valutmp1.ins.ddd.alu_fn)
            { // mask低位对应有效线程数；else分支为跳转
            case DecodeParams::alu_fn_t::FN_SEQ:
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                {
                    if (valutmp1.ins.mask[i] == 0)
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 0;
                    }
                    else if (valutmp1.rsv1_data[i] == valutmp1.rsv2_data[i])
                    {
                        _velsemask[i] = 1;
                        _vifmask[i] = 0;
                    }
                    else
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 1;
                    }
                }
                branch_elsemask = _velsemask;
                branch_ifmask = _vifmask;
                branch_elsepc = valutmp1.rsv3_data[0];
                valuto_simtstk = true;
                vbranch_ins = valutmp1.ins;
                vbranchins_warpid = valutmp1.warp_id;
                break;

            case DecodeParams::alu_fn_t::FN_SNE:
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                {
                    if (valutmp1.ins.mask[i] == 0)
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 0;
                    }
                    else if (valutmp1.rsv1_data[i] != valutmp1.rsv2_data[i])
                    {
                        _velsemask[i] = 1;
                        _vifmask[i] = 0;
                    }
                    else
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 1;
                    }
                }
                branch_elsemask = _velsemask;
                branch_ifmask = _vifmask;
                branch_elsepc = valutmp1.rsv3_data[0];
                valuto_simtstk = true;
                vbranch_ins = valutmp1.ins;
                vbranchins_warpid = valutmp1.warp_id;
                break;

            case DecodeParams::alu_fn_t::FN_SGE:
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                {
                    if (valutmp1.ins.mask[i] == 0)
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 0;
                    }
                    else if (valutmp1.rsv2_data[i] >= valutmp1.rsv1_data[i])
                    {
                        _velsemask[i] = 1;
                        _vifmask[i] = 0;
                    }
                    else
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 1;
                    }
                }
                branch_elsemask = _velsemask;
                branch_ifmask = _vifmask;
                branch_elsepc = valutmp1.rsv3_data[0];
                valuto_simtstk = true;
                vbranch_ins = valutmp1.ins;
                vbranchins_warpid = valutmp1.warp_id;
                break;

            case DecodeParams::alu_fn_t::FN_SLT:
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                {
                    if (valutmp1.ins.mask[i] == 0)
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 0;
                    }
                    else if (valutmp1.rsv2_data[i] < valutmp1.rsv1_data[i])
                    {
                        _velsemask[i] = 1;
                        _vifmask[i] = 0;
                    }
                    else
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 1;
                    }
                }
                branch_elsemask = _velsemask;
                branch_ifmask = _vifmask;
                branch_elsepc = valutmp1.rsv3_data[0];
                valuto_simtstk = true;
                vbranch_ins = valutmp1.ins;
                vbranchins_warpid = valutmp1.warp_id;
                break;

            case DecodeParams::alu_fn_t::FN_SGEU:
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                {
                    if (valutmp1.ins.mask[i] == 0)
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 0;
                    }
                    else if (static_cast<unsigned>(valutmp1.rsv2_data[i]) >= static_cast<unsigned>(valutmp1.rsv1_data[i]))
                    {
                        _velsemask[i] = 1;
                        _vifmask[i] = 0;
                    }
                    else
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 1;
                    }
                }
                branch_elsemask = _velsemask;
                branch_ifmask = _vifmask;
                branch_elsepc = valutmp1.rsv3_data[0];
                valuto_simtstk = true;
                vbranch_ins = valutmp1.ins;
                vbranchins_warpid = valutmp1.warp_id;
                break;

            case DecodeParams::alu_fn_t::FN_SLTU:
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                {
                    if (valutmp1.ins.mask[i] == 0)
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 0;
                    }
                    else if (static_cast<unsigned>(valutmp1.rsv2_data[i]) < static_cast<unsigned>(valutmp1.rsv1_data[i]))
                    {
                        _velsemask[i] = 1;
                        _vifmask[i] = 0;
                    }
                    else
                    {
                        _velsemask[i] = 0;
                        _vifmask[i] = 1;
                    }
                }
                branch_elsemask = _velsemask;
                branch_ifmask = _vifmask;
                branch_elsepc = valutmp1.rsv3_data[0];
                valuto_simtstk = true;
                vbranch_ins = valutmp1.ins;
                vbranchins_warpid = valutmp1.warp_id;
                break;

            default:
                std::cout << "VALU_CALC warning: switch to unrecognized ins" << valutmp1.ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
                break;
            }
        }

        ev_valufifo_pushed.notify();
    }
}

void BASE::VALU_CTRL()
{
    valu_ready = true;
    valu_ready_old = true;
    valueqb_triggered = false;
    while (true)
    {
        wait(valu_eqb.default_event() | valu_unready | valu_evb);
        if (valu_eqb.default_event().triggered())
        {
            valu_ready = true;
            valu_ready_old = valu_ready;
            valueqb_triggered = true;
            wait(SC_ZERO_TIME);
            valueqb_triggered = false;
            ev_valuready_updated.notify();
        }
        else if (valu_evb.triggered())
        {
            valu_ready = true;
            valu_ready_old = valu_ready;
            ev_valuready_updated.notify();
        }
        else if (valu_unready.triggered())
        {
            valu_ready = false;
            valu_ready_old = valu_ready;
            ev_valuready_updated.notify();
        }
    }
}
