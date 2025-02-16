#include "context_model.hpp"
#include "parameters.h"
#include "sm/BASE.h"
#include "sm/BASE_sti.h"
#include "CTA_Scheduler.hpp"
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include "membox_sv39/memory.h"
#include "task.hpp"
#include "utils/log.h"
#include "host.hpp"

int cmdarg_help();
int cmdarg_error(int argc, char *argv[]);
int cmdarg_task(Host *host, Memory *mem, char *arg);
int cmdarg_kernel(Host *host, Memory *mem, char *arg);

int sc_main(int argc, char *argv[])
{
    std::ios::sync_with_stdio(true);
    // 虚拟内存与页表
    Memory mem(1ull << 32ull);

    std::cout << "----------Initializing SM data-structures----------\n";
    BASE **BASE_impl;
    BASE_impl = new BASE *[NUM_SM];
    for (int i = 0; i < NUM_SM; i++)
    {
        BASE_impl[i] = new BASE(("SM" + std::to_string(i)).c_str(), i, &mem);
    }
    BASE_sti BASE_sti_impl("BASE_STI");

    std::cout << "----------Initializing CTAs----------\n";
    CTA_Scheduler cta_impl("CTA_Scheduler", BASE_impl);
    // cta_impl.CTA_INIT();
    
    Host host_impl("Host_GPGPU_Driver", &mem, &cta_impl);

    // 处理命令行参数
    std::string metafile, datafile, numcycle, kernelName;
    int numkernel = 0;
    std::vector<std::shared_ptr<kernel_info_t>> m_running_kernels;

    for (int argid = 1; argid < argc; argid++) {
        if (strcmp(argv[argid], "--task") == 0) {
            if (cmdarg_task(&host_impl, &mem, argv[++argid])) {
                cmdarg_error(2, argv + argid - 1);
            }
        } else if (strcmp(argv[argid], "--kernel") == 0) {
            if (cmdarg_kernel(&host_impl, &mem, argv[++argid])) {
                cmdarg_error(2, argv + argid - 1);
            }
        } else if (strcmp(argv[argid], "--numcycle") == 0) {
            numcycle = argv[++argid];
            std::cout << "--numcycle argument: " << numcycle << std::endl;
        } else if (strcmp(argv[argid], "--help") == 0) {
            cmdarg_help();
        } else {
            cmdarg_error(1, argv + argid);
        }
    }
    if(numcycle.empty()) {
        std::cout << "cmd arg error: --numcycle not provided\n";
        cmdarg_help();
        return 1;
    }
    log_debug("Finish reading runtime args");


//    for (int i = 0; i < NUM_SM; i++)
//    {
//        BASE_impl[i]->set_CTA_Scheduler(&cta_impl);
//    }

    for (int i = 0; i < NUM_SM; i++)
    {
        for (auto &warp_ : BASE_impl[i]->m_hw_warps)
        {
            if (warp_ != nullptr)
            {
                BASE_impl[i]->ev_warp_dispatch_list &= warp_->ev_warp_dispatch;
            }
        }
    }

    sc_clock clk("clk", PERIOD, SC_NS, 0.5, 0, SC_NS, false);
    sc_signal<bool> rst_n("rst_n");

    for (int i = 0; i < NUM_SM; i++)
    {
        (*BASE_impl[i]).clk(clk);
        (*BASE_impl[i]).rst_n(rst_n);
    }
    BASE_sti_impl.rst_n(rst_n);
    cta_impl.clk(clk);
    cta_impl.rst_n(rst_n);
    host_impl.clk(clk);
    host_impl.rst_n(rst_n);

    sc_trace_file *tf[hw_num_warp];
    BASE *recordwave_SM = BASE_impl[1];
    for (int i = 0; i < hw_num_warp; i++)
    {
        if (recordwave_SM->m_hw_warps[i] != nullptr)
        {

            tf[i] = sc_create_vcd_trace_file(("output/wave_warp" + std::to_string(i)).c_str());
            tf[i]->set_time_unit(1, SC_NS);
            for (int j = 0; j < 32; j++)
            {
                sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->CSR_reg[j], "CSR.data(" + std::to_string(j) + ")");
            }
            sc_trace(tf[i], clk, "Clk");
            sc_trace(tf[i], rst_n, "Rst_n");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->jump, "jump");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->jump_addr, "jump_addr");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->branch_sig, "branch_sig");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->fetch_valid, "fetch_valid");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->fetch_valid2, "fetch_valid2");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->pc, "pc");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->fetch_ins, "fetch_ins");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->decode_ins, "decode_ins");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->dispatch_warp_valid, "dispatch_warp_valid");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->ibuf_empty, "ibuf_empty");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->ibuf_swallow, "ibuf_swallow");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->ibuftop_ins, "ibuftop_ins");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->ififo_elem_num, "ififo_elem_num");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->wait_bran, "wait_bran");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->can_dispatch, "can_dispatch");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->is_warp_activated, "is_warp_activated");
            sc_trace(tf[i], recordwave_SM->opc_full, "opc_full");
            sc_trace(tf[i], recordwave_SM->last_dispatch_warpid, "last_dispatch_warpid");
            sc_trace(tf[i], recordwave_SM->issue_ins, "issue_ins");
            sc_trace(tf[i], recordwave_SM->issueins_warpid, "issueins_warpid");
            sc_trace(tf[i], recordwave_SM->dispatch_valid, "dispatch_valid");
            sc_trace(tf[i], recordwave_SM->dispatch_ready, "dispatch_ready");
            sc_trace(tf[i], recordwave_SM->opcfifo_elem_num, "opcfifo_elem_num");
            sc_trace(tf[i], recordwave_SM->emit_ins, "emit_ins");
            sc_trace(tf[i], recordwave_SM->emitins_warpid, "emitins_warpid");
            sc_trace(tf[i], recordwave_SM->doemit, "doemit");
            sc_trace(tf[i], recordwave_SM->findemit, "findemit");
            sc_trace(tf[i], recordwave_SM->emit_idx, "emit_idx");
            sc_trace(tf[i], recordwave_SM->emito_salu, "emito_salu");
            sc_trace(tf[i], recordwave_SM->emito_valu, "emito_valu");
            sc_trace(tf[i], recordwave_SM->emito_vfpu, "emito_vfpu");
            sc_trace(tf[i], recordwave_SM->emito_lsu, "emito_lsu");
            // salu
            sc_trace(tf[i], recordwave_SM->tosalu_data1, "tosalu_data.data1");
            sc_trace(tf[i], recordwave_SM->tosalu_data2, "tosalu_data.data2");
            sc_trace(tf[i], recordwave_SM->tosalu_data3, "tosalu_data.data3");
            sc_trace(tf[i], recordwave_SM->salu_ready, "salu_ready");
            sc_trace(tf[i], recordwave_SM->salufifo_empty, "salufifo_empty");
            sc_trace(tf[i], recordwave_SM->salutmp2, "salutmp2");
            sc_trace(tf[i], recordwave_SM->salutop_dat, "salutop_dat");
            sc_trace(tf[i], recordwave_SM->salufifo_elem_num, "salufifo_elem_num");
            // valu
            sc_trace(tf[i], recordwave_SM->valu_ready, "valu_ready");
            sc_trace(tf[i], recordwave_SM->valuto_simtstk, "valuto_simtstk");
            sc_trace(tf[i], recordwave_SM->branch_elsemask, "branch_elsemask");
            sc_trace(tf[i], recordwave_SM->branch_elsepc, "branch_elsepc");
            sc_trace(tf[i], recordwave_SM->vbranch_ins, "vbranch_ins");
            sc_trace(tf[i], recordwave_SM->vbranchins_warpid, "vbranchins_warpid");

            sc_trace(tf[i], recordwave_SM->valufifo_empty, "valufifo_empty");
            sc_trace(tf[i], recordwave_SM->valutop_dat, "valutop_dat");
            sc_trace(tf[i], recordwave_SM->valufifo_elem_num, "valufifo_elem_num");

            // simt-stack
            sc_trace(tf[i], recordwave_SM->emito_simtstk, "emito_simtstk");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->simtstk_jump, "simtstk_jump");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->simtstk_jumpaddr, "simtstk_jumpaddr");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->current_mask, "current_mask");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->vbran_sig, "vbran_sig");

            sc_trace(tf[i], recordwave_SM->vfpu_ready, "vfpu_ready");
            sc_trace(tf[i], recordwave_SM->vfpufifo_empty, "vfpufifo_empty");
            sc_trace(tf[i], recordwave_SM->vfputop_dat, "vfputop_dat");
            sc_trace(tf[i], recordwave_SM->vfpufifo_elem_num, "vfpufifo_elem_num");
            sc_trace(tf[i], recordwave_SM->lsu_ready, "lsu_ready");
            sc_trace(tf[i], recordwave_SM->lsufifo_empty, "lsufifo_empty");
            sc_trace(tf[i], recordwave_SM->lsutop_dat, "lsutop_dat");
            sc_trace(tf[i], recordwave_SM->lsufifo_elem_num, "lsufifo_elem_num");
            sc_trace(tf[i], recordwave_SM->write_s, "write_s");
            sc_trace(tf[i], recordwave_SM->write_v, "write_v");
            sc_trace(tf[i], recordwave_SM->write_f, "write_f");
            sc_trace(tf[i], recordwave_SM->execpop_salu, "execpop_salu");
            sc_trace(tf[i], recordwave_SM->execpop_valu, "execpop_valu");
            sc_trace(tf[i], recordwave_SM->execpop_vfpu, "execpop_vfpu");
            sc_trace(tf[i], recordwave_SM->execpop_lsu, "execpop_lsu");
            sc_trace(tf[i], recordwave_SM->wb_ena, "wb_ena");
            sc_trace(tf[i], recordwave_SM->wb_ins, "wb_ins");
            sc_trace(tf[i], recordwave_SM->wb_warpid, "wb_warpid");
            for (int j = 0; j < 32; j++)
            {
                sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->s_regfile[j], "s_regfile.data(" + std::to_string(j) + ")");
            }

            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->v_regfile[0][0], "v_regfile(0)(0)");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->v_regfile[1][0], "v_regfile(1)(0)");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->v_regfile[2][0], "v_regfile(2)(0)");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->v_regfile[3][0], "v_regfile(3)(0)");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->v_regfile[4][0], "v_regfile(4)(0)");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->v_regfile[5][0], "v_regfile(5)(0)");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->v_regfile[6][0], "v_regfile(6)(0)");
            sc_trace(tf[i], recordwave_SM->m_hw_warps[i]->v_regfile[7][0], "v_regfile(7)(0)");
            // sc_trace(tf[i], BASE_impl., "");
        }
    }

    std::cout << "----------Simulation start----------\n";
    auto start = std::chrono::high_resolution_clock::now();
    sc_core::sc_start(std::stoi(numcycle), SC_NS);

    std::cout << "----------Simulation end------------\n";

    for (auto tf_ : tf)
        sc_close_vcd_trace_file(tf_);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Time taken: " << std::dec << duration.count() / 1000 << " milliseconds" << std::endl;

    delete[] BASE_impl;
    return 0;
}

int cmdarg_help() {
    std::cout
        << "ventus-sim [--arg subarg1=val1,subarg2=val2,...] --numcycle $YOUR_SIM_TIME\n"
        << "\n"
        << "--task     create a new GPGPU task, supported subargs:\n"
        << "           name     string    // 任取\n"
        << "\n"
        << "--kernel   create a new GPGPU kernel, supported subargs:\n"
        << "           name     string    // 任取\n"
        << "           metafile string    // kernel的.metadata文件路径\n"
        << "           datafile string    // kernel的.data文件路径\n"
        << "           taskid   uint      // 可选，若无则为不归属任何task的独立kernel。必须指向之前已经申明的task\n"
        << "\n"
        << "-numcycle           uint      // 仿真时长，单位：纳秒"
        << std::endl;
    return 0;
}

int cmdarg_error(int argc, char* argv[]) {
    std::cout << "Incorrect argument: \n";
    for(int i = 0; i < argc; i++) {
        std::cout << "  " << argv[i] << "\n";
    }
    cmdarg_help();
    return 0;
}

int cmdarg_task(Host* host, Memory *mem, char* argraw) {
    assert(argraw);
    int len = strlen(argraw);
    char* arg = new char[len + 1];
    strcpy(arg, argraw);

    char *name = nullptr;

    //std::cout << "--task argument: \n";
    char* ptr1 = NULL;
    char* subarg = strtok_r(arg, ",", &ptr1);
    while (subarg) {
        if (strlen(subarg) > 0) {
            char* ptr2 = NULL;
            char* var = strtok_r(subarg, "=", &ptr2);
            char* val = strtok_r(NULL, "=", &ptr2);
            assert(var && val);

            if(strcmp(var, "name") == 0) {
                name = val;
            } else {
                goto RET_ERR;
            }
        }
        subarg = strtok_r(NULL, ",", &ptr1);
    }

    if(!(name)) {
        goto RET_ERR;
    } else {
        std::shared_ptr<task_t> task = std::make_shared<task_t>(host->get_num_task(), name, mem->createRootPageTable());
        host->add_task(task);
        //std::cout << "Task created: ID = " << task->m_id << ", name = " << task->m_name << std::endl;
    }


    return 0;

RET_ERR:
    delete[] arg;
    return -1;
}

int cmdarg_kernel(Host* host, Memory *mem, char* argraw) {
    assert(argraw);
    int len = strlen(argraw);
    char* arg = new char[len + 1];
    strcpy(arg, argraw);

    int taskid = -1;
    char* name = nullptr;
    char* metafile = nullptr;
    char* datafile = nullptr;

    //std::cout << "--kernel argument: \n";
    char* ptr1 = NULL;
    char* subarg = strtok_r(arg, ",", &ptr1);
    while (subarg) {
        if (strlen(subarg) > 0) {
            char* ptr2 = NULL;
            char* var = strtok_r(subarg, "=", &ptr2);
            char* val = strtok_r(NULL, "=", &ptr2);
            assert(var && val);

            if (strcmp(var, "taskid") == 0) {
                int num = std::stoi(val);
                assert(num < host->get_num_task());
                assert(num >= 0);
                taskid = num;
            } else if (strcmp(var, "name") == 0) {
                name = val;
            } else if (strcmp(var, "metafile") == 0) {
                metafile = val;
            } else if (strcmp(var, "datafile") == 0) {
                datafile = val;
            } else {
                goto RET_ERR;
            }
        }
        subarg = strtok_r(NULL, ",", &ptr1);
    }

    if(!(name && metafile && datafile)) {
        goto RET_ERR;
    }

    if(taskid != -1) {
        std::shared_ptr<kernel_info_t> kernel = std::make_shared<kernel_info_t>(
            host->get_num_kernel_total(), name, metafile, datafile, host->get_task(taskid)->m_pagetable);
        host->task_add_kernel(taskid, kernel);
        //std::cout << "Kernel created: ID = " << kernel->get_kid() << ", name = " << kernel->get_kname() 
        //    << "\n  belone to task ID = " << taskid
        //    << "\n  metafile = " << metafile 
        //    << "\n  datafile = " << datafile
        //    << std::endl;
    } else {
        std::shared_ptr<kernel_info_t> kernel = std::make_shared<kernel_info_t>(
            host->get_num_kernel_total(), name, metafile, datafile, mem->createRootPageTable());
        host->add_kernel(kernel);
        //std::cout << "Kernel created: ID = " << kernel->get_kid() << ", name = " << kernel->get_kname() 
        //    << "\n  metafile = " << metafile 
        //    << "\n  datafile = " << datafile
        //    << std::endl;
    }

    return 0;
RET_ERR:
    delete[] arg;
    return -1;
}

