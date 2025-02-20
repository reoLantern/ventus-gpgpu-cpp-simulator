#ifndef CTA_SCHEDULER_H_
#define CTA_SCHEDULER_H_

#include "context_model.hpp"
#include "parameters.h"
#include "sm/BASE.h"
#include <cstdint>
#include <list>
#include <memory>
#include <tuple>
#include <vector>

class BASE;

class ResourceUsage {
public:
    typedef struct {
        uint32_t addr1, addr2; // address range that block is using
        uint32_t prev, next;   // linked-list pointers
        bool valid;            // For debug assert
        uint32_t block_id;     // For debug assert
    } resource_usage_t;

    const uint32_t m_total; // total hardware resource size
    ResourceUsage(uint32_t total_)
        : m_total(total_) {
        assert(total_ > 0);
        m_cnt = 0;
        for(int i = 0; i < MAX_CTA_PER_CORE; i++) {
            m_slot[i].valid = false;
        }
    }
    void clear() { m_cnt = 0; }

    // const resource_usage_t& operator[](uint32_t idx) const { return m_slot[idx]; }
    // const resource_usage_t& get_head() const { return m_slot[m_head_idx]; }
    // const resource_usage_t& get_tail() const { return m_slot[m_tail_idx]; }

    std::tuple<bool, uint32_t> find_idle(uint32_t size) const;
    void alloc(uint32_t block_slot, uint32_t addr, uint32_t size, uint32_t block_id = 0xFFFFFFFF);
    void dealloc(uint32_t block_slot, uint32_t block_id = 0xFFFFFFFF);

private:
    resource_usage_t m_slot[MAX_CTA_PER_CORE]; // resource usage for each block running on corresponding SM
    uint32_t m_head_idx, m_tail_idx;
    uint32_t m_cnt;
};

class CTA_Scheduler : public sc_core::sc_module {
public:
    sc_in_clk clk { "clk" };
    sc_in<bool> rst_n { "rst_n" };
    // sc_vector<sc_port<IO_out_if<int>>> outs;

public:
    CTA_Scheduler(sc_core::sc_module_name name, BASE** sm_group_)
        : sc_module(name)
        , sm_group(sm_group_) {
        do_reset();
        SC_HAS_PROCESS(CTA_Scheduler);
        SC_THREAD(schedule_kernel2core);
        SC_THREAD(collect_finished_blocks);
    }

public:
    // sc_event ev_activate_warp;

    void schedule_kernel2core();
    void warp_finished(int sm_id, int block_slot_idx, int warp_idx_in_block);
    void collect_finished_blocks();
    // void set_running_kernels(std::vector<std::shared_ptr<kernel_info_t>> &_kernels) { m_running_kernels = _kernels; }
    std::shared_ptr<kernel_info_t> select_kernel();

    BASE** sm_group;

    // Interface between this and driver
    bool kernel_add(std::shared_ptr<kernel_info_t> kernel);

private:
    // Helpers
    bool isHexCharacter(char c);
    int charToHex(char c);
    void do_reset();

    // Kernel management (split kernel into blocks)
    std::vector<std::shared_ptr<kernel_info_t>> m_waiting_kernels;  // Data not yet loaded to memory
    std::vector<std::shared_ptr<kernel_info_t>> m_running_kernels;  // Data loaded to memory, some blocks may be running
    std::vector<std::shared_ptr<kernel_info_t>> m_finished_kernels; // All blocks finished, memory released

    // SM resource management
    typedef struct sm_block_slot_t {
        bool valid;
        std::shared_ptr<kernel_info_t> kernel;
        uint32_t block_idx;
        std::array<bool, MAX_WARP_PER_BLOCK> warp_finished;
    } sm_block_slot_t;

    typedef struct sm_resource_t {
        uint32_t num_warp; // number of warps running on corresponding SM, limited by number of warp-slot
        std::array<sm_block_slot_t, MAX_CTA_PER_CORE> block_slot; // block is running on corresponding SM.block_slot
        ResourceUsage lds { hw_lds_size };
    } sm_resource_t;
    std::array<sm_resource_t, NUM_SM> sm_usage;

    std::tuple<uint32_t, uint32_t>
    resource_find_idle(uint32_t size, const std::list<std::tuple<uint32_t, uint32_t>>* used, uint32_t total) const;

    // sc_event_or_list all_warp_ev_kernel_ret;

    int m_last_issued_kernel = -1; // -1 = none is selected (initial state)
    uint32_t m_last_issue_core = 0;
};

#endif
