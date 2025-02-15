#ifndef CTA_SCHEDULER_H_
#define CTA_SCHEDULER_H_

#include <memory>
#include <vector>
#include "sm/BASE.h"
#include "context_model.hpp"
#include "utils.hpp"

class BASE;

class CTA_Scheduler : public sc_core::sc_module
{
public:
    sc_in_clk clk{"clk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_vector<sc_port<IO_out_if<int>>> outs;

public:
    CTA_Scheduler(sc_core::sc_module_name name) : sc_module(name)
    {
        SC_HAS_PROCESS(CTA_Scheduler);
        SC_THREAD(schedule_kernel2core);
    }

public:
    //sc_event ev_activate_warp;

    void schedule_kernel2core();
    //void set_running_kernels(std::vector<std::shared_ptr<kernel_info_t>> &_kernels) { m_running_kernels = _kernels; }
    std::shared_ptr<kernel_info_t> select_kernel();

    BASE **sm_group;

    // Interface between this and driver
    bool kernel_add(std::shared_ptr<kernel_info_t> kernel);

private:
    // Helpers
    bool isHexCharacter(char c);
    int charToHex(char c);

    //sc_event_or_list all_warp_ev_kernel_ret;

    std::vector<std::shared_ptr<kernel_info_t>> m_running_kernels;
    std::vector<std::shared_ptr<kernel_info_t>> m_executed_kernels; // kernel第一次被issue，加入executed列表
    //std::vector<std::shared_ptr<kernel_info_t>> m_finished_kernels; // kernel执行完毕，加入finished列表
    int m_last_issued_kernel = -1;      // -1 = none is selected (initial state)
    uint32_t m_last_issue_core = 0;
};

#endif
