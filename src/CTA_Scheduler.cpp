#include "CTA_Scheduler.hpp"
#include "context_model.hpp"
#include <memory>

bool CTA_Scheduler::isHexCharacter(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int CTA_Scheduler::charToHex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else
        return -1; // Invalid character
}

//std::shared_ptr<kernel_info_t> CTA_Scheduler::select_kernel() {
//    if (m_last_issued_kernel >= 0 && m_running_kernels[m_last_issued_kernel] != nullptr
//        && !m_running_kernels[m_last_issued_kernel]->no_more_ctas_to_run()) { // 贪婪策略
//        // if (std::find(m_executed_kernels.begin(), m_executed_kernels.end(),
//        //               m_running_kernels[m_last_issued_kernel]) == m_executed_kernels.end())
//        //{
//        //     m_executed_kernels.push_back(m_running_kernels[m_last_issued_kernel]);
//        //     m_running_kernels[m_last_issued_kernel]->start_cycle = uint64_t(sc_time_stamp().to_double() / PERIOD);
//        //     assert(0);
//        // }
//        return m_running_kernels[m_last_issued_kernel];
//    }
//
//    for (unsigned i = 0; i < m_running_kernels.size(); i++) { // 贪婪不成则轮询
//        unsigned idx = (i + m_last_issued_kernel + 1) % m_running_kernels.size();
//        if (m_running_kernels[idx] != nullptr && !m_running_kernels[idx]->no_more_ctas_to_run()) {
//            if (std::find(m_executed_kernels.begin(), m_executed_kernels.end(), m_running_kernels[idx])
//                == m_executed_kernels.end()) {
//                m_executed_kernels.push_back(m_running_kernels[idx]);
//                m_running_kernels[idx]->start_cycle = uint64_t(sc_time_stamp().to_double() / PERIOD);
//            } else
//                assert(0);
//            m_last_issued_kernel = idx;
//            return m_running_kernels[idx];
//        }
//    }
//
//    return nullptr;
//}

void CTA_Scheduler::do_reset() {
    for (int i = 0; i < NUM_SM; i++) {
        sm_group[i]->m_current_kernel_completed = false;
        sm_group[i]->m_current_kernel_running = false;
    }
    for (auto& sm : sm_usage) {
        sm.num_warp = 0;
        sm.lds = 0;
        for (auto& block_slot : sm.block_valid) {
            block_slot = false;
        }
    }
}

void CTA_Scheduler::schedule_kernel2core() {
    while (true) {
        wait(clk.posedge_event());

        if (!rst_n) {
            do_reset();
            continue;
        }

        if(m_running_kernels.empty()) {
            if(m_waiting_kernels.empty()) {
                continue;
            } else {
                assert(0); // need to activate this waiting kernel
                m_running_kernels.push_back(m_waiting_kernels[0]);
                m_waiting_kernels.erase(m_waiting_kernels.begin());
            }
        }

        // For each SM, check its status. Dispatch new CTA or change to new kernel if needed.
        for (int i = 0; i < NUM_SM; i++) {
            const unsigned sm_idx = (i + m_last_issue_core + 1) % NUM_SM;
            BASE* const sm = sm_group[sm_idx];

        //    // Check if this SM needs changing to a new kernel, and change it if needed
            std::shared_ptr<kernel_info_t> kernel = nullptr;
            for(auto kernel_ : m_running_kernels) {
                assert(kernel_->m_status == kernel_info_t::KERNEL_STATUS_RUNNING);
                if(!kernel_->no_more_ctas_to_run()) {
                    kernel = kernel_;
                    break;
                }
            }
            if(kernel && sm->m_kernel != kernel) {
                bool sm_idle = true;
                for(auto& warp : sm->m_hw_warps) {
                    if(warp->is_warp_activated) {
                        sm_idle = false;
                        break;
                    }
                }
                if(sm_idle) {
                    sm->set_kernel(kernel);
                    kernel->m_num_sm_running_this++;
                }
            }
        //    if (kernel == nullptr || !sm->m_current_kernel_running) {   // no kernel running on this SM
        //        kernel = select_kernel();   // get new kernel
        //        if (kernel != nullptr) {
        //            sm->set_kernel(kernel);
        //            kernel->m_num_sm_running_this++;
        //        }
        //    } else if (kernel->no_more_ctas_to_run() && sm->m_current_kernel_running) { 
        //        // All blocks of this kernel have been issued, check if these warps are still running
        //        bool warp_all_finished = true; // default
        //        for (auto& warp : sm->m_hw_warps) {
        //            if (warp->is_warp_activated) {
        //                warp_all_finished = false;
        //                break;
        //            }
        //        }
        //        if (warp_all_finished) { // change to a new kernel
        //            sm->m_current_kernel_completed = true;
        //            sm->m_current_kernel_running = false; // The new kernel will start to run later
        //            log_debug("SM%d finish kernel%d %s", sm_idx, kernel->get_kid(), kernel->get_kname().c_str());
        //            kernel->m_num_sm_running_this--;
        //            if (kernel->m_num_sm_running_this == 0) {
        //                kernel->finish();
        //                // m_finished_kernels.push_back(kernel);
        //            }
        //            kernel = select_kernel(); // get new kernel
        //            if (kernel != nullptr) {
        //                sm->set_kernel(kernel);
        //                kernel->m_num_sm_running_this++;
        //            }
        //        }
        //    }

        //    for (int w = 0; w < hw_num_warp; w++) {
        //        sm->m_issue_block2warp[w] = false; // default: not issued
        //    }

            // try to issue 1 block for each SM
            if (kernel && !kernel->no_more_ctas_to_run() && sm->m_kernel == kernel && sm->can_issue_1block(kernel)) {
                sm->issue_block2core(kernel);
                m_last_issue_core = sm_idx;
                break;
            }
        }
    }
}

void CTA_Scheduler::collect_finished_blocks() {
    while (true) {
        wait(clk.posedge_event());
        if (!rst_n) {
            continue;
        }
        
        for(auto iter = m_running_kernels.begin(); iter != m_running_kernels.end(); ) {
            std::shared_ptr<kernel_info_t> kernel = *iter;
            assert(kernel->m_status == kernel_info_t::KERNEL_STATUS_RUNNING);
            bool kernel_finished = true; // default
            for(int i = 0; i < kernel->get_num_block(); i++) { // check each block finished or not
                if(kernel->m_block_status[i] != kernel_info_t::BLOCK_STATUS_FINISHED) {
                    bool block_finished = true;
                    for(int j = 0; j < kernel->get_num_warp_per_cta(); j++) { // check each warp finished or not
                        if(kernel->m_warp_status[i][j] != kernel_info_t::WARP_STATUS_FINISHED) {
                            // After a warp finishes, SM warp will callback and change m_warp_status to FINISHED
                            block_finished = false;
                            break;
                        }
                    }
                    if(block_finished) {
                        kernel->m_block_status[i] = kernel_info_t::BLOCK_STATUS_FINISHED;
                    } else {
                        kernel_finished = false;
                    }
                }
            }
            if(kernel_finished) {
                kernel->finish();
                m_finished_kernels.push_back(kernel);
                iter = m_running_kernels.erase(iter);
            } else {
                iter++;
            }
        }
    }
}

bool CTA_Scheduler::kernel_add(std::shared_ptr<kernel_info_t> kernel) {
    // currently, all kernels are activated before added to CTA scheduler
    // so they are directly added to m_running_kernels, instead of m_waiting_kernels
    m_running_kernels.push_back(kernel);
    return true;
}
