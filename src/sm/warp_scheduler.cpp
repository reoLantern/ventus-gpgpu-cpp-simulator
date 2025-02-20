#include "BASE.h"

void BASE::WARP_SCHEDULER()
{
    bool find_dispatchwarp = 0;
    last_dispatch_warpid = 0;
    I_TYPE _newissueins;
    uint32_t wait_barrier_ins_pc;
    I_TYPE new_ins; // from opc, barrier ins
    int new_ins_warpid;
    bool end_this_kernel;
    bool reset_endprg_flush_pipe[hw_num_warp];
    while (true)
    {
        wait(clk.posedge_event());

        for(int i = 0; i < hw_num_warp; i++){
            if(reset_endprg_flush_pipe[i]){
                m_hw_warps[i]->endprg_flush_pipe.write(false);
                reset_endprg_flush_pipe[i] = false;
            }
        }

        // receive new warp from CTA Scheduler
        for(int wid = 0; wid < hw_num_warp; wid++) {
            if(m_issue_block2warp[wid]) {
                // TODO
            }
        }

        // std::cout << "SM" << sm_id << " WARP SCHEDULER start at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;

        // handle warp end

        //if (m_kernel && m_kernel->no_more_ctas_to_run() && m_current_kernel_running.read())
        //{
        //    end_this_kernel = true;
        //    for (auto &warp : m_hw_warps)
        //    {
        //        if (warp->is_warp_activated.read() == true)
        //            end_this_kernel = false;
        //    }
        //    if (end_this_kernel)
        //    {
        //        m_current_kernel_running.write(false);
        //        m_current_kernel_completed.write(true);
        //        std::cout << "SM" << sm_id << " Warp Scheduler: finish current kernel at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
        //    }
        //}

        ev_warp_assigned.notify();

        // barrier from opc

        if (emito_warpscheduler)
        {
            // std::cout << "SM" << sm_id << " WARP SCHEDULER receive emit ins" << emit_ins << ", warpid=" << emitins_warpid << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
            new_ins = emit_ins;
            new_ins_warpid = emitins_warpid;
            switch (new_ins.op)
            {
            case OP_TYPE::BARRIER_:     // m_num_warp_activated情况变复杂，待修改
                if (wait_barrier.areFirstNValue(m_num_warp_activated, false)) // 未设置barrier
                {
                    wait_barrier[new_ins_warpid] = true;
                    wait_barrier_ins_pc = new_ins.currentpc;
                }
                else
                {
                    if (new_ins.currentpc == wait_barrier_ins_pc)
                    {
                        wait_barrier[new_ins_warpid] = true;
                        if (wait_barrier.areFirstNValue(m_num_warp_activated, true))
                        {
                            wait_barrier.fill(false);
                            std::cout << "SM" << sm_id << " warp scheduler: all warps reach barrier pc=0x" << std::hex << new_ins.currentpc << " " << new_ins << std::dec << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << "SM" << sm_id << " WARP_SCHEDULER ERROR: Barrier pc not match at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
                    }
                }
                break;

            case OP_TYPE::ENDPRG_:
                assert(m_hw_warps[new_ins_warpid]->is_warp_activated);
                if (m_hw_warps[new_ins_warpid]->is_warp_activated) {
                    m_hw_warps[new_ins_warpid]->is_warp_activated = false;
                    if(m_warp_finish_callback){
                        m_warp_finish_callback(sm_id, m_hw_warps[new_ins_warpid]->blk_slot_idx,
                                               m_hw_warps[new_ins_warpid]->warp_idx_in_blk);
                        m_hw_warps[new_ins_warpid]->will_warp_activate = false;
                        // m_hw_warps[new_ins_warpid]->finish_callback(sm_id, new_ins_warpid);
                    }
                }
                m_num_warp_activated--;
                m_hw_warps[new_ins_warpid]->initwarp();
                reset_endprg_flush_pipe[new_ins_warpid] = true;
                std::cout << "SM" << sm_id << " warp " << new_ins_warpid << " 0x" << std::hex << new_ins.currentpc
                     << " " << new_ins << " endprg"
                     << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
                break;

            default:
                std::cout << "warp scheduler warning, receive unrecognized instruction\n";
                break;
            }
        }

        // wait for all warps dispatch
        wait(ev_warp_dispatch_list);
        // std::cout << "SM" << sm_id << " WARP SCHEDULER receive issue_list " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;

        if (!opc_full | doemit) // 这是dispatch_ready，来自opc (ready-valid机制)
        {
            find_dispatchwarp = false; // 是否已经确定要dispatch的warp
            for (int i = last_dispatch_warpid; i < last_dispatch_warpid + hw_num_warp; i++)
            {
                if (!find_dispatchwarp && m_hw_warps[i % hw_num_warp]->can_dispatch && !wait_barrier[i % hw_num_warp] && m_hw_warps[i % hw_num_warp]->is_warp_activated)
                {
                    m_hw_warps[i % hw_num_warp]->dispatch_warp_valid = true;
                    dispatch_valid = true;
                    _newissueins = m_hw_warps[i % hw_num_warp]->ififo.front();
                    _newissueins.mask = m_hw_warps[i % hw_num_warp]->current_mask;
                    // std::cout << "let issue_ins mask=" << _newissueins.mask << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
                    issue_ins = _newissueins;
                    issueins_warpid = i % hw_num_warp;
                    find_dispatchwarp = true;
                    last_dispatch_warpid = i % hw_num_warp + 1;
                }
                else
                {
                    m_hw_warps[i % hw_num_warp]->dispatch_warp_valid = false;
                    // std::cout << "ISSUE: let warp" << i % hw_num_warp << " dispatch_warp_valid=false at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << std::endl;
                }
            }
            if (!find_dispatchwarp)
                dispatch_valid = false;
        }
    }
}
