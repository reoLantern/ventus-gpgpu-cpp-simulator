#include "CTA_Scheduler.hpp"
#include "context_model.hpp"
#include "parameters.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <tuple>

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

// std::shared_ptr<kernel_info_t> CTA_Scheduler::select_kernel() {
//     if (m_last_issued_kernel >= 0 && m_running_kernels[m_last_issued_kernel] != nullptr
//         && !m_running_kernels[m_last_issued_kernel]->no_more_ctas_to_run()) { // 贪婪策略
//         // if (std::find(m_executed_kernels.begin(), m_executed_kernels.end(),
//         //               m_running_kernels[m_last_issued_kernel]) == m_executed_kernels.end())
//         //{
//         //     m_executed_kernels.push_back(m_running_kernels[m_last_issued_kernel]);
//         //     m_running_kernels[m_last_issued_kernel]->start_cycle = uint64_t(sc_time_stamp().to_double() / PERIOD);
//         //     assert(0);
//         // }
//         return m_running_kernels[m_last_issued_kernel];
//     }
//
//     for (unsigned i = 0; i < m_running_kernels.size(); i++) { // 贪婪不成则轮询
//         unsigned idx = (i + m_last_issued_kernel + 1) % m_running_kernels.size();
//         if (m_running_kernels[idx] != nullptr && !m_running_kernels[idx]->no_more_ctas_to_run()) {
//             if (std::find(m_executed_kernels.begin(), m_executed_kernels.end(), m_running_kernels[idx])
//                 == m_executed_kernels.end()) {
//                 m_executed_kernels.push_back(m_running_kernels[idx]);
//                 m_running_kernels[idx]->start_cycle = uint64_t(sc_time_stamp().to_double() / PERIOD);
//             } else
//                 assert(0);
//             m_last_issued_kernel = idx;
//             return m_running_kernels[idx];
//         }
//     }
//
//     return nullptr;
// }

void CTA_Scheduler::do_reset() {
    for (int i = 0; i < NUM_SM; i++) {
        sm_group[i]->m_current_kernel_completed = false;
        sm_group[i]->m_current_kernel_running = false;
    }
    for (auto& sm : sm_usage) {
        sm.num_warp = 0;
        sm.lds.clear();
        for (auto& block_slot : sm.block_slot) {
            block_slot.valid = false;
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

        // 若GPU上无已激活的kernel，尝试激活waiting_kernel
        if (m_running_kernels.empty()) {
            if (m_waiting_kernels.empty()) {
                continue;
            } else {
                assert(0); // Todo: need to activate this waiting kernel
                m_running_kernels.push_back(m_waiting_kernels[0]);
                m_waiting_kernels.erase(m_waiting_kernels.begin());
            }
        }

        // 线程块调度策略之选择线程块：先进先出（选择最先到来的kernel的最小index的线程块）
        std::shared_ptr<kernel_info_t> kernel = nullptr;
        for (auto kernel_ : m_running_kernels) {
            assert(kernel_->m_status == kernel_info_t::KERNEL_STATUS_RUNNING);
            if (!kernel_->no_more_ctas_to_run()) {
                kernel = kernel_;
                break;
            }
        }
        if (kernel == nullptr)
            continue; // No kernel & block to run. Idle cycle.
        assert(kernel->no_more_ctas_to_run() == false);
        uint32_t block_idx = kernel->get_next_cta_id_single();

        // For each SM, check its status. Dispatch new CTA or change to new kernel if needed.
        // 线程块调度策略之选择SM：轮询优先级
        for (int i = 0; i < NUM_SM; i++) {
            const unsigned sm_idx = (i + m_last_issue_core + 1) % NUM_SM;
            BASE* const sm = sm_group[sm_idx];
            sm_resource_t* const sm_resource = &sm_usage[sm_idx];

            // TODO: 目前单个SM上只能容纳来自同一kernel的线程块
            //if (sm->m_kernel != kernel) { // 若选定的block不归属与SM当前运行的kernel，需额外判定
            //    bool sm_idle = true;
            //    for (auto& warp : sm->m_hw_warps) {
            //        if (warp->is_warp_activated) {
            //            sm_idle = false;
            //            break;
            //        }
            //    }
            //    if (sm_idle) { // 若此SM已经空闲，将其切换到新kernel，稍后派发选定线程块
            //        sm->set_kernel(kernel);
            //        kernel->m_num_sm_running_this++;
            //    } else { // 若此SM正在运行其他kernel的线程块，无法派发选定线程块，继续轮询其他SM
            //        continue;
            //    }
            //}

            // 资源判定
            bool warp_slot_ok = false, block_slot_ok = false, lds_ok = false;
            uint32_t block_slot_idx = 0, lds_baseaddr = 0;
            // warp slot
            warp_slot_ok = (sm_resource->num_warp + kernel->get_num_warp_per_cta() <= hw_num_warp);
            // block slot
            while (block_slot_idx < sm_resource->block_slot.size()) {
                if (sm_resource->block_slot[block_slot_idx].valid == false) {
                    block_slot_ok = true;
                    break;
                }
                block_slot_idx++;
            }
            // Local Data Share (local memory)
            std::tie(lds_ok, lds_baseaddr) = sm_resource->lds.find_idle(kernel->get_ldsSize_per_cta());

            // 若资源判定通过，派发线程块的各warp到SM上
            if (block_slot_ok && warp_slot_ok && lds_ok) {
                // resource alloc
                sm_resource->num_warp += kernel->get_num_warp_per_cta();
                sm_resource->block_slot[block_slot_idx].valid = true;
                sm_resource->lds.alloc(block_slot_idx, lds_baseaddr, kernel->get_ldsSize_per_cta(), block_idx);
                // block info recorded to block_slot in CTA scheduler, for resource dealloc after block finished
                sm_resource->block_slot[block_slot_idx].kernel = kernel;
                sm_resource->block_slot[block_slot_idx].block_idx = block_idx;
                sm_resource->block_slot[block_slot_idx].warp_finished.fill(false);
                // 逐个warp派发到SM上
                // TODO：时序改为每周期派发一个warp
                for (int warp_idx = 0; warp_idx < kernel->get_num_warp_per_cta(); warp_idx++) {
                    sm->receive_warp(block_idx, warp_idx, kernel, block_slot_idx, lds_baseaddr);
                }
                kernel->m_block_sm_id[block_idx] = sm->sm_id;
                kernel->m_block_status[block_idx] = kernel_info_t::BLOCK_STATUS_RUNNING;
                kernel->increment_cta_id();
                m_last_issue_core = sm_idx;
                break;
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
            // if (kernel && !kernel->no_more_ctas_to_run() && sm->m_kernel == kernel && sm->can_issue_1block(kernel)) {
            //     sm->issue_block2core(kernel);
            //     m_last_issue_core = sm_idx;
            //     break;
            // }
        }
    }
}

void CTA_Scheduler::warp_finished(int sm_id, int block_slot_idx, int warp_idx_in_block) {
    assert(sm_id < NUM_SM && sm_group[sm_id]->sm_id == sm_id); // it is assumed that sm_id == sm_index
    assert(sm_usage[sm_id].block_slot.at(block_slot_idx).valid == true);
    assert(sm_usage[sm_id].block_slot.at(block_slot_idx).warp_finished.at(warp_idx_in_block) == false);
    sm_usage[sm_id].block_slot[block_slot_idx].warp_finished[warp_idx_in_block] = true;
}

void CTA_Scheduler::collect_finished_blocks() {
    while (true) {
        wait(clk.posedge_event());
        if (!rst_n) {
            continue;
        }

        // For each SM, check its block_slots one by one, if all warps of that block finished
        for (int sm_idx = 0; sm_idx < NUM_SM; sm_idx++) {
            for (int blk_slot_idx = 0; blk_slot_idx < MAX_CTA_PER_CORE; blk_slot_idx++) {
                if (sm_usage[sm_idx].block_slot[blk_slot_idx].valid == false) {
                    continue; // only check slots with running block
                }
                std::shared_ptr<kernel_info_t> kernel = sm_usage[sm_idx].block_slot[blk_slot_idx].kernel;
                int blk_idx = sm_usage[sm_idx].block_slot[blk_slot_idx].block_idx;
                assert(kernel && kernel->m_status == kernel_info_t::KERNEL_STATUS_RUNNING);
                assert(kernel->m_block_status[blk_idx] == kernel_info_t::BLOCK_STATUS_RUNNING);
                if (std::all_of(sm_usage[sm_idx].block_slot[blk_slot_idx].warp_finished.begin(),
                                sm_usage[sm_idx].block_slot[blk_slot_idx].warp_finished.begin()
                                    + kernel->get_num_warp_per_cta(),
                                [](bool finished) { return finished; })) {
                    // block finished, dealloc resource
                    sm_usage[sm_idx].block_slot[blk_slot_idx].valid = false;
                    sm_usage[sm_idx].num_warp -= kernel->get_num_warp_per_cta();
                    sm_usage[sm_idx].lds.dealloc(blk_slot_idx, sm_usage[sm_idx].block_slot[blk_slot_idx].block_idx);
                    // mark that this block is finished (on real gpu: tell host)
                    kernel->m_block_status[blk_idx] = kernel_info_t::BLOCK_STATUS_FINISHED;

                    // if all blocks of this kernel finished, release this kernel
                    if (std::all_of(kernel->m_block_status.begin(), kernel->m_block_status.end(),
                                    [](int status) { return status == kernel_info_t::BLOCK_STATUS_FINISHED; })) {
                        kernel->finish();
                        m_finished_kernels.push_back(kernel);
                        m_running_kernels.erase(std::remove(m_running_kernels.begin(), m_running_kernels.end(), kernel),
                                                m_running_kernels.end());
                    }
                }
            }
        }

        // // 轮询所有已激活的kernel的所有block，检查各block是否所有warp都已完成，更新block/kernel运行状态
        // for (auto iter = m_running_kernels.begin(); iter != m_running_kernels.end();) {
        //     std::shared_ptr<kernel_info_t> kernel = *iter;
        //     assert(kernel->m_status == kernel_info_t::KERNEL_STATUS_RUNNING);
        //     bool kernel_finished = true;                        // default
        //     for (int i = 0; i < kernel->get_num_block(); i++) { // check each block finished or not
        //         if (kernel->m_block_status[i] == kernel_info_t::BLOCK_STATUS_WAIT) {
        //             kernel_finished = false;
        //         } else if (kernel->m_block_status[i] == kernel_info_t::BLOCK_STATUS_RUNNING) {
        //             bool block_finished = true;
        //             for (int j = 0; j < kernel->get_num_warp_per_cta(); j++) { // check each warp finished or not
        //                 if (kernel->m_warp_status[i][j] != kernel_info_t::WARP_STATUS_FINISHED) {
        //                     // After a warp finishes, SM warp will callback and change m_warp_status to FINISHED
        //                     block_finished = false;
        //                     break;
        //                 }
        //             }
        //             if (block_finished) { // newly finished block
        //                 kernel->m_block_status[i] = kernel_info_t::BLOCK_STATUS_FINISHED;
        //                 int sm_idx = 0;
        //                 while (sm_group[sm_idx]->sm_id != kernel->m_block_sm_id[i]) {
        //                     sm_idx++;
        //                 }
        //                 sm_usage[sm_idx].num_warp -= kernel->get_num_warp_per_cta();
        //                 assert(0);
        //                 sm_usage[sm_idx].block_slot[] // TODO

        //             } else {
        //                 kernel_finished = false;
        //             }
        //         }
        //     }
        //     // 发现某kernel执行结束后，移出running_kernels列表，回调&释放资源
        //     if (kernel_finished) {
        //         kernel->finish();
        //         m_finished_kernels.push_back(kernel);
        //         iter = m_running_kernels.erase(iter); // 附带循环步进作用
        //     } else {
        //         iter++; // 循环步进
        //     }
        // }
    }
}

std::tuple<bool, uint32_t> ResourceUsage::find_idle(uint32_t size) const {
    bool found = (m_cnt == 0);
    uint32_t found_size = m_total;
    uint32_t found_addr1 = 0;
    // uint32_t found_addr2 = m_total - 1;
    uint32_t idx = m_head_idx;
    uint32_t this_addr1, this_addr2_plus1, this_size;
    for (int i = 0; i < m_cnt; i++) {
        this_addr1 = (idx == m_head_idx) ? 0 : m_slot[m_slot[idx].prev].addr2 + 1;
        this_addr2_plus1 = m_slot[idx].addr1;
        this_size = (this_addr2_plus1 >= this_addr1 + 1) ? (this_addr2_plus1 - this_addr1) : 0;
        if (this_size >= size && this_size < found_size) {
            found = true;
            found_size = this_size;
            found_addr1 = this_addr1;
            // found_addr2 = this_addr2_plus1 - 1;
        }
        idx = m_slot[idx].next;
    }
    if (m_cnt != 0) {
        this_addr1 = m_slot[m_tail_idx].addr2 + 1;
        this_addr2_plus1 = m_total;
        this_size = (this_addr2_plus1 >= this_addr1 + 1) ? (this_addr2_plus1 - this_addr1) : 0;
        if (this_size >= size && this_size < found_size) {
            found = true;
            found_size = this_size;
            found_addr1 = this_addr1;
            // found_addr2 = this_addr2_plus1 - 1;
        }
    }
    return std::make_tuple(found, found_addr1);
}

void ResourceUsage::alloc(uint32_t block_slot, uint32_t addr, uint32_t size, uint32_t block_id) {
    assert(m_slot[block_slot].valid == false);
    m_slot[block_slot].valid = true;
    m_slot[block_slot].block_id = block_id;
    m_slot[block_slot].addr1 = addr;
    m_slot[block_slot].addr2 = addr + size - 1;

    // insert to linked-list

    // empty linked-list
    if (m_cnt == 0) {
        m_tail_idx = block_slot;
        m_head_idx = block_slot;
        m_cnt++;
        return;
    }

    // find the right position and insert (not empty)
    uint32_t idx = m_head_idx;
    uint32_t prev_addr = 0;
    while (true) {
        if (m_slot[idx].addr1 > addr) {
            assert(m_slot[idx].addr1 >= addr + size);
            m_slot[idx].prev = block_slot;
            m_slot[block_slot].next = idx;
            if (idx == m_head_idx) { // will be head node
                assert(prev_addr == 0);
                m_head_idx = block_slot;
            } else { // not head node
                assert(m_slot[m_slot[idx].prev].addr2 < addr);
                m_slot[block_slot].prev = m_slot[idx].prev;
                m_slot[m_slot[idx].prev].next = block_slot;
            }
            break;
        }
        if (idx == m_tail_idx) { // will be tail node
            assert(m_slot[idx].addr2 < addr);
            m_slot[m_tail_idx].next = block_slot;
            m_slot[block_slot].prev = m_tail_idx;
            m_tail_idx = block_slot;
            break;
            // must not be head node, because m_cnt > 0
        }
        prev_addr = m_slot[idx].addr2;
        idx = m_slot[idx].next;
    }
    m_cnt++;
}

void ResourceUsage::dealloc(uint32_t block_slot, uint32_t block_id) {
    assert(m_slot[block_slot].valid == true);
    assert(m_slot[block_slot].block_id == block_id);
    assert(m_cnt > 0);
    m_slot[block_slot].valid = false;
    if (block_slot == m_head_idx) {
        m_head_idx = m_slot[block_slot].next;
    } else {
        m_slot[m_slot[block_slot].prev].next = m_slot[block_slot].next;
    }
    if (block_slot == m_tail_idx) {
        m_tail_idx = m_slot[block_slot].prev;
    } else {
        m_slot[m_slot[block_slot].next].prev = m_slot[block_slot].prev;
    }
    m_cnt--;
}

// std::tuple<uint32_t, uint32_t> CTA_Scheduler::resource_find_idle(uint32_t size,
//                                                                  const std::list<std::tuple<uint32_t, uint32_t>>*
//                                                                  used, uint32_t total) const {
//     assert(total > 0);
//     assert(used->size() <= MAX_CTA_PER_CORE);
//     bool found = false;
//     uint32_t found_size = total;
//     uint32_t found_addr1 = 0;
//     uint32_t found_addr2 = total - 1;
//     auto iter = used->begin();
//     uint32_t this_addr1 = 0;
//     uint32_t this_addr2_plus1 = (used->empty()) ? total : std::get<0>(*iter);
//     uint32_t this_size = (this_addr2_plus1 >= this_addr1 + 1) ? (this_addr2_plus1 - this_addr1) : 0;
//     if (this_size >= size && this_size < found_size) {
//         found = true;
//         found_size = this_size;
//         found_addr1 = this_addr1;
//         found_addr2 = this_addr2_plus1 - 1;
//     }
//     while (iter != used->end()) {
//         this_addr1 = std::get<1>(*iter);
//         this_addr2_plus1 = (iter == std::prev(used->end())) ? total : std::get<0>(*(++iter));
//         this_size = (this_addr2_plus1 >= this_addr1 + 1) ? (this_addr2_plus1 - this_addr1) : 0;
//         if (this_size >= size && this_size < found_size) {
//             found = true;
//             found_size = this_size;
//             found_addr1 = this_addr1;
//             found_addr2 = this_addr2_plus1 - 1;
//         }
//     }
//     if (!found)
//         return std::make_tuple(0xFFFFFFFF, 0);
//     return std::make_tuple(found_addr1, found_addr2);
// }

bool CTA_Scheduler::kernel_add(std::shared_ptr<kernel_info_t> kernel) {
    // currently, all kernels are activated before added to CTA scheduler
    // so they are directly added to m_running_kernels, instead of m_waiting_kernels
    m_running_kernels.push_back(kernel);
    return true;
}
