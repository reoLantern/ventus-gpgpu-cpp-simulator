#ifndef CTA_SCHEDULER_H_
#define CTA_SCHEDULER_H_

#include "context_model.hpp"
#include "parameters.h"
#include "sm/BASE.h"
#include <memory>
#include <vector>

class BASE;

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
    struct {
        uint32_t num_warp; // number of warps running on corresponding SM, limited by number of warp-slot
        bool block_valid[MAX_CTA_PER_CORE]; // block is running on corresponding SM.block_slot
        uint32_t lds;
    } sm_usage[NUM_SM];

    // sc_event_or_list all_warp_ev_kernel_ret;

    int m_last_issued_kernel = -1; // -1 = none is selected (initial state)
    uint32_t m_last_issue_core = 0;
};

#endif
