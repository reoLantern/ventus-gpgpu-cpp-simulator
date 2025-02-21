#include "CTA_Scheduler.hpp"
#include "context_model.hpp"
#include "sm/BASE.h"
#include "parameters.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <tuple>

void CTA_Scheduler_SM_management::construct_init() {
    rsrc.num_warp = 0;
    for (auto& block_slot : rsrc.blk_slots) {
        block_slot.valid = false;
    }
}

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

void CTA_Scheduler::do_reset() {
    for (int i = 0; i < NUM_SM; i++) {
        auto& sm = m_sm[i];
        sm->m_current_kernel_completed = false;
        sm->m_current_kernel_running = false;
        sm.rsrc.num_warp = 0;
        sm.rsrc.lds.clear();
        for (auto& block_slot : sm.rsrc.blk_slots) {
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
            auto& sm = m_sm[sm_idx];

            // 资源判定
            bool warp_slot_ok = false, block_slot_ok = false, lds_ok = false;
            uint32_t block_slot_idx = 0, lds_baseaddr = 0;
            // warp slot
            warp_slot_ok = (sm.rsrc.num_warp + kernel->get_num_warp_per_cta() <= hw_num_warp);
            // block slot
            while (block_slot_idx < sm.rsrc.blk_slots.size()) {
                if (sm.rsrc.blk_slots[block_slot_idx].valid == false) {
                    block_slot_ok = true;
                    break;
                }
                block_slot_idx++;
            }
            // Local Data Share (local memory)
            std::tie(lds_ok, lds_baseaddr) = sm.rsrc.lds.find_idle(kernel->get_ldsSize_per_cta());

            // 若资源判定通过，派发线程块的各warp到SM上
            if (block_slot_ok && warp_slot_ok && lds_ok) {
                // resource alloc
                sm.rsrc.num_warp += kernel->get_num_warp_per_cta();
                sm.rsrc.blk_slots[block_slot_idx].valid = true;
                sm.rsrc.lds.alloc(block_slot_idx, lds_baseaddr, kernel->get_ldsSize_per_cta(), block_idx);
                // block info recorded to block_slot in CTA scheduler, for resource dealloc after block finished
                sm.rsrc.blk_slots[block_slot_idx].kernel = kernel;
                sm.rsrc.blk_slots[block_slot_idx].block_idx = block_idx;
                sm.rsrc.blk_slots[block_slot_idx].warp_finished.fill(false);
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
        }
    }
}

void CTA_Scheduler::warp_finished(int sm_idx, int blk_slot_idx, int warp_idx_in_blk) {
    auto& sm = m_sm.at(sm_idx);
    assert(sm.rsrc.blk_slots.at(blk_slot_idx).valid == true);
    assert(sm.rsrc.blk_slots.at(blk_slot_idx).warp_finished.at(warp_idx_in_blk) == false);
    sm.rsrc.blk_slots[blk_slot_idx].warp_finished[warp_idx_in_blk] = true;
}

void CTA_Scheduler::collect_finished_blocks() {
    while (true) {
        wait(clk.posedge_event());
        if (!rst_n) {
            continue;
        }

        // For each SM, check its block_slots one by one, if all warps of that block finished
        for (int sm_idx = 0; sm_idx < NUM_SM; sm_idx++) {
            auto& sm = m_sm[sm_idx];
            for (int blk_slot_idx = 0; blk_slot_idx < MAX_CTA_PER_CORE; blk_slot_idx++) {
                if (sm.rsrc.blk_slots[blk_slot_idx].valid == false) {
                    continue; // only check slots with running block
                }
                std::shared_ptr<kernel_info_t> kernel = sm.rsrc.blk_slots[blk_slot_idx].kernel;
                int blk_idx = sm.rsrc.blk_slots[blk_slot_idx].block_idx;
                assert(kernel && kernel->m_status == kernel_info_t::KERNEL_STATUS_RUNNING);
                assert(kernel->m_block_status[blk_idx] == kernel_info_t::BLOCK_STATUS_RUNNING);
                if (std::all_of(sm.rsrc.blk_slots[blk_slot_idx].warp_finished.begin(),
                                sm.rsrc.blk_slots[blk_slot_idx].warp_finished.begin() + kernel->get_num_warp_per_cta(),
                                [](bool finished) { return finished; })) {
                    // block finished, dealloc resource
                    sm.rsrc.blk_slots[blk_slot_idx].valid = false;
                    sm.rsrc.num_warp -= kernel->get_num_warp_per_cta();
                    sm.rsrc.lds.dealloc(blk_slot_idx, sm.rsrc.blk_slots[blk_slot_idx].block_idx);
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
    }
}

// LDS,sGPR,vGPR resource find idle fragment (best-fit strategy)
std::tuple<bool, uint32_t> ResourceUsage::find_idle(uint32_t size) const {
    if (m_cnt == 0) { // if linked-list is empty, alloc from addr 0
        return std::make_tuple(true, 0);
    }
    bool found = false;
    uint32_t found_size = m_total;
    uint32_t found_addr = 0;
    uint32_t idx = m_head_idx;
    uint32_t this_addr1, this_addr2_plus1, this_size;
    for (int i = 0; i < m_cnt; i++) {
        idx = (i == 0) ? m_head_idx : m_slot[idx].next;
        // for each alloc record in linked-list, check the idle fragment (may not exist) *before* it
        // addr1: idle fragment start address, addr2: idle fragment end address
        this_addr1 = (idx == m_head_idx) ? 0 : m_slot[m_slot[idx].prev].addr2 + 1;
        this_addr2_plus1 = m_slot[idx].addr1;
        assert(this_addr2_plus1 >= this_addr1);
        this_size = this_addr2_plus1 - this_addr1;
        // this_size = (this_addr2_plus1 >= this_addr1 + 1) ? (this_addr2_plus1 - this_addr1) : 0;
        if (this_size >= size && this_size < found_size) {
            // best-fit strategy: find the smallest among all large enough fragments
            found = true;
            found_size = this_size;
            found_addr = this_addr1;
        }
    }
    assert(idx == m_tail_idx);
    // check the idle fragment after the last alloc record
    this_addr1 = m_slot[m_tail_idx].addr2 + 1;
    this_addr2_plus1 = m_total;
    assert(this_addr2_plus1 >= this_addr1);
    this_size = this_addr2_plus1 - this_addr1;
    // this_size = (this_addr2_plus1 >= this_addr1 + 1) ? (this_addr2_plus1 - this_addr1) : 0;
    if (this_size >= size && this_size < found_size) {
        found = true;
        found_size = this_size;
        found_addr = this_addr1;
    }
    return std::make_tuple(found, found_addr);
}

// LDS,sGPR,vGPR resource alloc
void ResourceUsage::alloc(uint32_t block_slot, uint32_t addr, uint32_t size, uint32_t block_id) {
    assert(m_slot[block_slot].valid == false);
    assert(size > 0); // if size=0, no need to alloc, do not call this function
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

    // find the right position and insert (linked-list not empty)
    uint32_t idx = m_head_idx;
    uint32_t prev_addr = 0;
    for (int i = 0; i < m_cnt; i++) {
        // this is true: prev.addr2 < this.addr < this.addr + size - 1 < next.addr1
        if (m_slot[idx].addr1 > addr) { // find the first node that addr1 > addr: is next node
            assert(m_slot[idx].addr1 >= addr + size);
            m_slot[idx].prev = block_slot;
            m_slot[block_slot].next = idx;
            if (idx == m_head_idx) { // this will be head node
                assert(prev_addr == 0);
                m_head_idx = block_slot;
            } else { // not head node. Since next node exists, not tail node either.
                assert(m_slot[m_slot[idx].prev].addr2 < addr);
                m_slot[block_slot].prev = m_slot[idx].prev;
                m_slot[m_slot[idx].prev].next = block_slot;
            }
            break;
        }
        if (idx == m_tail_idx) { // next node not found: this will be tail node
            assert(m_slot[idx].addr2 < addr);
            m_slot[m_tail_idx].next = block_slot;
            m_slot[block_slot].prev = m_tail_idx;
            m_tail_idx = block_slot;
            break;
            // must not be head node, because m_cnt > 0
        }
        prev_addr = m_slot[idx].addr2;  // loop step
        idx = m_slot[idx].next;
    }
    m_cnt++;
}

// LDS,sGPR,vGPR resource dealloc(release)
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

bool CTA_Scheduler::kernel_add(std::shared_ptr<kernel_info_t> kernel) {
    // currently, all kernels are activated before added to CTA scheduler
    // so they are directly added to m_running_kernels, instead of m_waiting_kernels
    // TODO
    m_running_kernels.push_back(kernel);
    return true;
}
