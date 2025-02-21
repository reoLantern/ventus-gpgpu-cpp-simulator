#include "BASE.h"

void BASE::MUL_IN()
{
    mul_in_t new_data;
    int a_delay, b_delay;
    while (true)
    {
        wait();
        if (emito_mul)
        {
            if (mul_ready_old == false)
                std::cout << "mul error: not ready at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
            mul_unready.notify();
            switch (emit_ins.read().op)
            {

            default:
                new_data.ins = emit_ins;
                new_data.warp_id = emitins_warpid;
                for (int i = 0; i < m_hw_warps[new_data.warp_id]->CSR_reg[0x802]; i++)
                {
                    new_data.rsv1_data[i] = tomul_data1[i];
                    new_data.rsv2_data[i] = tomul_data2[i];
                    new_data.rsv3_data[i] = tomul_data3[i];
                }
                mul_dq.push(new_data);
                a_delay = 3;
                b_delay = 1;
                // std::cout << "mul: receive VADD_VV_, will notify eq, at " << sc_time_stamp() <<","<< sc_delta_count_at_current_time() << "\n";
                if (a_delay == 0)
                    mul_eva.notify();
                else if (muleqa_triggered)
                    mul_eqa.notify(sc_time((a_delay)*PERIOD, SC_NS));
                else
                {
                    mul_eqa.notify(sc_time((a_delay)*PERIOD, SC_NS));
                    ev_mulfifo_pushed.notify();
                }
                if (b_delay == 0)
                    mul_evb.notify();
                else
                {
                    mul_eqb.notify(sc_time((b_delay)*PERIOD, SC_NS));
                    ev_mulready_updated.notify();
                }
                break;

                // default:
                //     std::cout << "mul error: receive wrong ins " << emit_ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                // break;
            }
        }
        else
        {
            if (!muleqa_triggered)
            {
                ev_mulfifo_pushed.notify();
            }
            if (!muleqb_triggered)
                ev_mulready_updated.notify();
        }
    }
}

void BASE::MUL_CALC()
{
    mulfifo_elem_num = 0;
    mulfifo_empty = 1;
    muleqa_triggered = false;
    mul_in_t multmp1;
    mul_out_t multmp2;
    bool succeed;
    while (true)
    {
        wait(mul_eva | mul_eqa.default_event());
        if (mul_eqa.default_event().triggered())
        {
            muleqa_triggered = true;
            wait(SC_ZERO_TIME);
            muleqa_triggered = false;
        }
        // std::cout << "mul_eqa.default_event triggered at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
        multmp1 = mul_dq.front();
        mul_dq.pop();
        auto& hwarp = m_hw_warps[multmp1.warp_id];
        if (multmp1.ins.ddd.wxd | multmp1.ins.ddd.wvd)
        {
            multmp2.ins = multmp1.ins;
            multmp2.warp_id = multmp1.warp_id;
            switch (multmp1.ins.ddd.alu_fn)
            {

            case DecodeParams::alu_fn_t::FN_MUL:
                // VMUL.VV, VMUL.VX
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                {
                    if (multmp2.ins.mask[i] == 1)
                        multmp2.rdv1_data[i] = multmp1.rsv1_data[i] * multmp1.rsv2_data[i];
                }
                break;

            case DecodeParams::alu_fn_t::FN_MADD:
                // VMADD
                // std::cout << "EXEC_MUL: FN_MADD,{thread,s1,s2,s3}: " << std::hex;
                for (int i = 0; i < hwarp->CSR_reg[0x802]; i++)
                {
                    if (multmp2.ins.mask[i] == 1)
                    {
                        // std::cout << "{" << i << "," << multmp1.rsv1_data[i] << "," << multmp1.rsv2_data[i] << "," << multmp1.rsv3_data[i] << "};";
                        multmp2.rdv1_data[i] = multmp1.rsv1_data[i] * multmp1.rsv3_data[i] + multmp1.rsv2_data[i];
                    }
                }
                // std::cout << std::dec << "\n";
                break;

            default:
                std::cout << "MUL_CALC warning: switch to unrecognized ins" << multmp1.ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                break;
            }
            mulfifo.push(multmp2);
        }
        else
        {
            switch (multmp1.ins.op)
            {

            default:
                std::cout << "MUL_CALC warning: switch to unrecognized ins" << multmp1.ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                break;
            }
        }

        ev_mulfifo_pushed.notify();
    }
}

void BASE::MUL_CTRL()
{
    mul_ready = true;
    mul_ready_old = true;
    muleqb_triggered = false;
    while (true)
    {
        wait(mul_eqb.default_event() | mul_unready | mul_evb);
        if (mul_eqb.default_event().triggered())
        {
            mul_ready = true;
            mul_ready_old = mul_ready;
            muleqb_triggered = true;
            wait(SC_ZERO_TIME);
            muleqb_triggered = false;
            ev_mulready_updated.notify();
        }
        else if (mul_evb.triggered())
        {
            mul_ready = true;
            mul_ready_old = mul_ready;
            ev_mulready_updated.notify();
        }
        else if (mul_unready.triggered())
        {
            mul_ready = false;
            mul_ready_old = mul_ready;
            ev_mulready_updated.notify();
        }
    }
}
