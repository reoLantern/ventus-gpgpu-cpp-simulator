#ifndef CTA_SCHEDULER_H_
#define CTA_SCHEDULER_H_

#include "parameters.h"
#include <cstdint>
#include <list>
#include <memory>
#include <tuple>
#include <vector>

class BASE;
class kernel_info_t;

// Helper class for SM LDS,sGPR,vGPR resource management
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
        for (int i = 0; i < MAX_CTA_PER_CORE; i++) {
            m_slot[i].valid = false;
        }
    }
    void clear() { m_cnt = 0; }

    std::tuple<bool, uint32_t> find_idle(uint32_t size) const;
    void alloc(uint32_t block_slot, uint32_t addr, uint32_t size, uint32_t block_id = 0xFFFFFFFF);
    void dealloc(uint32_t block_slot, uint32_t block_id = 0xFFFFFFFF);

private:
    resource_usage_t m_slot[MAX_CTA_PER_CORE]; // resource alloc record for each block running on corresponding SM
    uint32_t m_head_idx, m_tail_idx;           // index of head and tail node of linked-list
    uint32_t m_cnt;                            // how many valid nodes in linked-list
};

// Helper class for SM management
class CTA_Scheduler_SM_management {
public:
    typedef struct sm_block_slot_t {
        bool valid;
        std::shared_ptr<kernel_info_t> kernel;
        uint32_t block_idx;
        std::array<bool, MAX_WARP_PER_BLOCK> warp_finished;
    } sm_block_slot_t;
    typedef struct sm_resource_t {
        uint32_t num_warp; // number of warps running on corresponding SM, limited by number of warp-slot
        std::array<sm_block_slot_t, MAX_CTA_PER_CORE> blk_slots; // block is running on corresponding SM.block_slot
        ResourceUsage lds { hw_lds_size };                       // local data share (local memory)
    } sm_resource_t;

    CTA_Scheduler_SM_management(BASE* sm_ptr = nullptr)
        : m_sm_ptr(sm_ptr) {
        construct_init();
    }
    BASE* operator->() const { return m_sm_ptr; }
    void set_sm_ptr(BASE* sm_ptr) { m_sm_ptr = sm_ptr; }

    sm_resource_t rsrc; // SM resource usage

private:
    BASE* m_sm_ptr = nullptr;
    void construct_init();
};

class CTA_Scheduler : public sc_core::sc_module {
public:
    sc_in_clk clk { "clk" };
    sc_in<bool> rst_n { "rst_n" };

public:
    CTA_Scheduler(sc_core::sc_module_name name, BASE** sm_group_)
        : sc_module(name) {
        for (int sm_idx = 0; sm_idx < NUM_SM; sm_idx++) {
            m_sm[sm_idx].set_sm_ptr(sm_group_[sm_idx]);
        }
        do_reset();
        SC_HAS_PROCESS(CTA_Scheduler);
        SC_THREAD(schedule_kernel2core);
        SC_THREAD(collect_finished_blocks);
    }

public:
    // Interface: callback function for SM when a warp finished
    void warp_finished(int sm_id, int block_slot_idx, int warp_idx_in_block);

    // Interface between this and driver
    bool kernel_add(std::shared_ptr<kernel_info_t> kernel);

private:
    // Helpers
    bool isHexCharacter(char c);
    int charToHex(char c);
    void do_reset();

    // SC threads
    void schedule_kernel2core();    // block dispatch to SM
    void collect_finished_blocks(); // finished warps/blocks return from SM

    // Kernel management (split kernel into blocks)
    std::vector<std::shared_ptr<kernel_info_t>> m_waiting_kernels;  // Data not yet loaded to memory
    std::vector<std::shared_ptr<kernel_info_t>> m_running_kernels;  // Data loaded to memory, some blocks may be running
    std::vector<std::shared_ptr<kernel_info_t>> m_finished_kernels; // All blocks finished, memory released

    // all SMs, and its resource management data
    std::array<CTA_Scheduler_SM_management, NUM_SM> m_sm;

    // which SM does a block issued to last time (for SM select strategy)
    uint32_t m_last_issue_core = 0;
};

#endif
