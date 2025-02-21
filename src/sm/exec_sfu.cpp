#include "BASE.h"

void BASE::SFU_IN()
{
    sfu_in_t new_data;
    int a_delay, b_delay;
    while (true)
    {
        wait();
        if (emito_sfu)
        {
            if (sfu_ready_old == false)
                std::cout << "sfu error: not ready at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
            sfu_unready.notify();
            switch (emit_ins.read().op)
            {

            default:
                new_data.ins = emit_ins;
                new_data.warp_id = emitins_warpid;
                for (int i = 0; i < m_hw_warps[new_data.warp_id]->CSR_reg[0x802]; i++)
                {
                    new_data.rsv1_data[i] = tosfu_data1[i];
                    new_data.rsv2_data[i] = tosfu_data2[i];
                }
                sfu_dq.push(new_data);
                a_delay = 3;
                b_delay = 1;
                // std::cout << "sfu: receive VADD_VV_, will notify eq, at " << sc_time_stamp() <<","<< sc_delta_count_at_current_time() << "\n";
                if (a_delay == 0)
                    sfu_eva.notify();
                else if (sfueqa_triggered)
                    sfu_eqa.notify(sc_time((a_delay)*PERIOD, SC_NS));
                else
                {
                    sfu_eqa.notify(sc_time((a_delay)*PERIOD, SC_NS));
                    ev_sfufifo_pushed.notify();
                }
                if (b_delay == 0)
                    sfu_evb.notify();
                else
                {
                    sfu_eqb.notify(sc_time((b_delay)*PERIOD, SC_NS));
                    ev_sfuready_updated.notify();
                }
                break;

                // default:
                //     std::cout << "sfu error: receive wrong ins " << emit_ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                // break;
            }
        }
        else
        {
            if (!sfueqa_triggered)
            {
                ev_sfufifo_pushed.notify();
            }
            if (!sfueqb_triggered)
                ev_sfuready_updated.notify();
        }
    }
}

void BASE::SFU_CALC()
{
    sfufifo_elem_num = 0;
    sfufifo_empty = 1;
    sfueqa_triggered = false;
    sfu_in_t sfutmp1;
    sfu_out_t sfutmp2;
    bool succeed;
    while (true)
    {
        wait(sfu_eva | sfu_eqa.default_event());
        if (sfu_eqa.default_event().triggered())
        {
            sfueqa_triggered = true;
            wait(SC_ZERO_TIME);
            sfueqa_triggered = false;
        }
        // std::cout << "sfu_eqa.default_event triggered at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
        sfutmp1 = sfu_dq.front();
        sfu_dq.pop();
        auto& hwarp = m_hw_warps[sfutmp1.warp_id];
        if (sfutmp1.ins.ddd.wxd | sfutmp1.ins.ddd.wvd)
        {
            sfutmp2.ins = sfutmp1.ins;
            sfutmp2.warp_id = sfutmp1.warp_id;
            switch (sfutmp1.ins.ddd.alu_fn)
            {

            case DecodeParams::alu_fn_t::FN_REMU:
                // VREMU.VV, VREMU.VX
                if (sfutmp1.ins.ddd.isvec)
                {
                    if (sfutmp1.ins.ddd.reverse)
                    {
                        if (sfutmp1.ins.ddd.sel_alu1 == DecodeParams::A1_RS1)
                        { // VREMU.VX
                            for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                            {
                                if (sfutmp2.ins.mask[i] == 1)
                                    sfutmp2.rdv1_data[i] = (unsigned)sfutmp1.rsv2_data[i] % sfutmp1.rsv1_data[0];
                            }
                        }
                        else
                        {
                            for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                            {
                                if (sfutmp2.ins.mask[i] == 1)
                                {
                                    if ((unsigned)sfutmp1.rsv1_data[i] != 0)
                                        sfutmp2.rdv1_data[i] = (unsigned)sfutmp1.rsv2_data[i] % sfutmp1.rsv1_data[i];
                                    else
                                        std::cout << "SFU_CALC error: warp " << sfutmp1.warp_id << " ins pc=0x" << std::hex << sfutmp1.ins.currentpc << sfutmp1.ins
                                             << ", ins.s1=" << sfutmp1.ins.s1 << ", rsv1_data[" << i << "]=0 at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                                }
                            }
                        }
                    }
                    else
                    {
                        for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                        {
                            if (sfutmp2.ins.mask[i] == 1)
                                sfutmp2.rdv1_data[i] = (unsigned)sfutmp1.rsv1_data[i] % sfutmp1.rsv2_data[i];
                        }
                    }
                }
                else
                    sfutmp2.rdv1_data[0] = (unsigned)sfutmp1.rsv1_data[0] % sfutmp1.rsv2_data[0];
                break;

            case DecodeParams::alu_fn_t::FN_DIVU:
                // VDIVU.VV, VDIVU.VX
                if (sfutmp1.ins.ddd.isvec)
                {
                    if (sfutmp1.ins.ddd.reverse)
                        for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                        {
                            if (sfutmp2.ins.mask[i] == 1)
                            {
                                if ((unsigned)sfutmp1.rsv1_data[i] != 0)
                                    sfutmp2.rdv1_data[i] = (unsigned)sfutmp1.rsv2_data[i] / (unsigned)sfutmp1.rsv1_data[i];
                                else
                                    std::cout << "SFU_CALC error: warp " << sfutmp1.warp_id << " ins pc=0x" << std::hex << sfutmp1.ins.currentpc << sfutmp1.ins
                                         << ", ins.s1=" << sfutmp1.ins.s1 << ", rsv1_data[" << i << "]=0 at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                            }
                        }
                    else
                        for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                        {
                            if (sfutmp2.ins.mask[i] == 1)
                                sfutmp2.rdv1_data[i] = (unsigned)sfutmp1.rsv1_data[i] / (unsigned)sfutmp1.rsv2_data[i];
                        }
                }
                else
                    sfutmp2.rdv1_data[0] = (unsigned)sfutmp1.rsv1_data[0] / (unsigned)sfutmp1.rsv2_data[0];
                break;

            default:
                std::cout << "SFU_CALC warning: switch to unrecognized ins" << sfutmp1.ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                break;
            }
            sfufifo.push(sfutmp2);
        }
        else
        {
            switch (sfutmp1.ins.op)
            {

            default:
                std::cout << "SFU_CALC warning: switch to unrecognized ins" << sfutmp1.ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                break;
            }
        }

        ev_sfufifo_pushed.notify();
    }
}

void BASE::SFU_CTRL()
{
    sfu_ready = true;
    sfu_ready_old = true;
    sfueqb_triggered = false;
    while (true)
    {
        wait(sfu_eqb.default_event() | sfu_unready | sfu_evb);
        if (sfu_eqb.default_event().triggered())
        {
            sfu_ready = true;
            sfu_ready_old = sfu_ready;
            sfueqb_triggered = true;
            wait(SC_ZERO_TIME);
            sfueqb_triggered = false;
            ev_sfuready_updated.notify();
        }
        else if (sfu_evb.triggered())
        {
            sfu_ready = true;
            sfu_ready_old = sfu_ready;
            ev_sfuready_updated.notify();
        }
        else if (sfu_unready.triggered())
        {
            sfu_ready = false;
            sfu_ready_old = sfu_ready;
            ev_sfuready_updated.notify();
        }
    }
}
