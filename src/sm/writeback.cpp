#include "BASE.h"

void BASE::WRITE_BACK()
{
    // FloatAndInt newFI;

    while (true)
    {
        wait(ev_salufifo_pushed & ev_valufifo_pushed & ev_vfpufifo_pushed &
             ev_lsufifo_pushed & ev_csrfifo_pushed & ev_mulfifo_pushed & ev_sfufifo_pushed &
             ev_tcfifo_pushed);
        // if (sm_id == 1)
        //     std::cout << "SM" << sm_id << " WRITEBACK: start at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
        if (execpop_salu)
        {
            salufifo.pop();
            // if (sm_id == 0)
            //     std::cout << "SM" << sm_id << " WB pop salufifo at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
        }
        if (execpop_valu)
            valufifo.pop();
        if (execpop_vfpu)
            vfpufifo.pop();
        if (execpop_lsu)
            lsufifo.pop();
        if (execpop_csr)
            csrfifo.pop();
        if (execpop_mul)
            mulfifo.pop();
        if (execpop_sfu)
            sfufifo.pop();
        if (execpop_tc)
            tcfifo.pop();

        salufifo_empty = salufifo.isempty();
        if (!salufifo_empty)
            salutop_dat = salufifo.front();
        salufifo_elem_num = salufifo.used();
        valufifo_empty = valufifo.isempty();
        if (!valufifo_empty)
            valutop_dat = valufifo.front();
        valufifo_elem_num = valufifo.used();
        vfpufifo_empty = vfpufifo.isempty();
        if (!vfpufifo_empty)
            vfputop_dat = vfpufifo.front();
        vfpufifo_elem_num = vfpufifo.used();
        lsufifo_empty = lsufifo.isempty();
        if (!lsufifo_empty)
            lsutop_dat = lsufifo.front();
        lsufifo_elem_num = lsufifo.used();
        csrfifo_empty = csrfifo.isempty();
        if (!csrfifo_empty)
            csrtop_dat = csrfifo.front();
        csrfifo_elem_num = csrfifo.used();
        mulfifo_empty = mulfifo.isempty();
        if (!mulfifo_empty)
            multop_dat = mulfifo.front();
        mulfifo_elem_num = mulfifo.used();
        sfufifo_empty = sfufifo.isempty();
        if (!sfufifo_empty)
            sfutop_dat = sfufifo.front();
        sfufifo_elem_num = sfufifo.used();
        tcfifo_empty = tcfifo.isempty();
        if (!tcfifo_empty)
            tctop_dat = tcfifo.front();
        tcfifo_elem_num = tcfifo.used();

        execpop_valu = false;
        execpop_salu = false;
        execpop_vfpu = false;
        execpop_lsu = false;
        execpop_csr = false;
        execpop_mul = false;
        execpop_sfu = false;
        execpop_tc = false;

        if (salufifo_empty == false)
        {
            // if (sm_id == 0)
            //     std::cout << "SM" << sm_id << " WB judge popsalu, write_s=true, salutop.ins=" << salutop_dat.ins << ",pc=" << std::hex << salutop_dat.ins.currentpc << std::dec << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
            write_s = true;
            write_v = false;
            wb_ena = true;
            execpop_salu = true;
            wb_ins = salutop_dat.ins;
            rdv1_addr = salutop_dat.ins.d;
            rdv1_data[0] = salutop_dat.data;
            wb_warpid = salutop_dat.warp_id;
        }
        else if (valufifo_empty == false)
        {
            // if (sm_id == 0)
            //     std::cout << "SM" << sm_id << " WB judge popvalu, write_v=true at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
            write_s = false;
            write_v = true;
            wb_ena = true;
            execpop_valu = true;
            wb_ins = valutop_dat.ins;
            rdv1_addr = valutop_dat.ins.d;
            for (int i = 0; i < hw_num_thread; i++)
                rdv1_data[i] = valutop_dat.rdv1_data[i];
            wb_warpid = valutop_dat.warp_id;
        }
        else if (vfpufifo_empty == false)
        {
            wb_ena = true;
            execpop_vfpu = true;
            wb_ins = vfputop_dat.ins;
            if (vfputop_dat.ins.ddd.wxd) // FEQ_S_等指令
            {
                // if (sm_id == 0)
                //     std::cout << "SM" << sm_id << " WB judge popvfpu, write_s=true at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                write_s = true;
                write_v = false;
                rdv1_addr = vfputop_dat.ins.d;
                rdv1_data[0] = vfputop_dat.rds1_data;
            }
            else
            {
                // if (sm_id == 0)
                //     std::cout << "SM" << sm_id << " WB judge popvfpu, write_v=true at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                write_s = false;
                write_v = true;
                rdv1_addr = vfputop_dat.ins.d;
                // std::cout << "WB: let wb_ins=" << vfputop_dat.ins << "warp" << vfputop_dat.warp_id << ", rdf1_data={";
                for (int i = 0; i < hw_num_thread; i++)
                    rdv1_data[i].write(vfputop_dat.rdf1_data[i]);
            }
            wb_warpid = vfputop_dat.warp_id;
            // std::cout << "} at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
        }
        else if (lsufifo_empty == false)
        {
            if (sm_id == 0)
                std::cout << "SM" << sm_id << " WB judge poplsu, at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
            execpop_lsu = true;
            if (lsutop_dat.ins.ddd.wxd)
            {
                write_s = true;
                write_v = false;
            }
            else if (lsutop_dat.ins.ddd.wvd)
            {
                write_s = false;
                write_v = true;
            }

            wb_ena = true;
            wb_ins = lsutop_dat.ins;

            rdv1_addr = lsutop_dat.ins.d;
            for (int i = 0; i < hw_num_thread; i++)
                rdv1_data[i] = lsutop_dat.rdv1_data[i];

            wb_warpid = lsutop_dat.warp_id;
        }
        else if (csrfifo_empty == false)
        {
            // if (sm_id == 0)
            //     std::cout << "SM" << sm_id << " WB judge popcsr, write_s=true, csrtop.ins=" << csrtop_dat.ins << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
            write_s = true;
            write_v = false;
            wb_ena = true;
            execpop_csr = true;
            wb_ins = csrtop_dat.ins;
            rdv1_addr = csrtop_dat.ins.d;
            rdv1_data[0] = csrtop_dat.data;
            wb_warpid = csrtop_dat.warp_id;
        }
        else if (mulfifo_empty == false)
        {
            // if (sm_id == 0)
            //     std::cout << "SM" << sm_id << " WB judge popmul, at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
            wb_ena = true;
            execpop_mul = true;
            wb_ins = multop_dat.ins;
            wb_warpid = multop_dat.warp_id;
            if (multop_dat.ins.ddd.wxd)
            {
                write_s = true;
                write_v = false;
                rdv1_addr = multop_dat.ins.d;
                rdv1_data[0] = multop_dat.rdv1_data[0];
            }
            else if (multop_dat.ins.ddd.wvd)
            {
                write_s = false;
                write_v = true;
                rdv1_addr = multop_dat.ins.d;
                for (int i = 0; i < hw_num_thread; i++)
                    rdv1_data[i] = multop_dat.rdv1_data[i];
            }
        }
        else if (sfufifo_empty == false)
        {
            // if (sm_id == 0)
            //     std::cout << "SM" << sm_id << " WB judge popsfu, at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
            wb_ena = true;
            execpop_sfu = true;
            wb_ins = sfutop_dat.ins;
            wb_warpid = sfutop_dat.warp_id;
            if (sfutop_dat.ins.ddd.wxd)
            {
                write_s = true;
                write_v = false;
                rdv1_addr = sfutop_dat.ins.d;
                rdv1_data[0] = sfutop_dat.rdv1_data[0];
            }
            else if (sfutop_dat.ins.ddd.wvd)
            {
                write_s = false;
                write_v = true;
                rdv1_addr = sfutop_dat.ins.d;
                for (int i = 0; i < hw_num_thread; i++)
                    rdv1_data[i] = sfutop_dat.rdv1_data[i];
            }
        }
        else if (tcfifo_empty == false)
        {
            write_s = false;
            write_v = true;
            wb_ena = true;
            execpop_tc = true;
            wb_ins = tctop_dat.ins;
            rdv1_addr = tctop_dat.ins.d;
            for (int i = 0; i < hw_num_thread; i++)
                rdv1_data[i] = tctop_dat.rdv1_data[i];
            wb_warpid = tctop_dat.warp_id;
        }
        else
        {
            // if (sm_id == 0)
            //     std::cout << "SM" << sm_id << " WB judge not writeback, at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
            write_s = false;
            write_v = false;
            wb_ena = false;
        }
    }
}
