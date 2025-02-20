#ifndef BASE_H_
#define BASE_H_

#define SC_INCLUDE_DYNAMIC_PROCESSES
#include <systemc.h>
#include "../parameters.h"
//#include "../CTA_Scheduler.hpp"
#include "../context_model.hpp"

class CTA_Scheduler;

class BASE : public sc_core::sc_module
{
public:
    sc_in_clk clk{"clk"};
    sc_in<bool> rst_n{"rst_n"};

    // Memory Access: global & local(shared)
    Memory *m_mem;  // global memory handle
    std::array<uint8_t, hw_lds_size> m_local_mem;     // LDS of this SM
    bool mem_read_word(uint32_t *data, uint32_t vaddr, const I_TYPE &ins, uint64_t pagetable) const;
    bool mem_write_word(uint32_t data, uint32_t vaddr, const I_TYPE &ins, uint64_t pagetable) ;

    void debug_sti();
    void debug_display();
    void debug_display1();
    void debug_display2();
    void debug_display3();
    void INIT_INSMEM();
    uint32_t getBufferData(const std::vector<std::vector<uint8_t>> &buffers, unsigned int virtualAddress, int num_buffer, uint64_t *buffer_base, uint64_t *buffer_size, bool &addrOutofRangeException, I_TYPE ins);
    uint32_t readInsBuffer(unsigned int virtualAddress, bool &addrOutofRangeException);
    void writeBufferData(int writevalue, std::vector<std::vector<uint8_t>> &buffers, unsigned int virtualAddress, int num_buffer, uint64_t *buffer_base, uint64_t *buffer_size, I_TYPE ins);

    // fetch
    void INIT_INS();
    void INIT_DECODETABLE();
    void INIT_INSTABLE();
    void PROGRAM_COUNTER(int warp_id);
    // void FETCH_2();
    void INSTRUCTION_REG(int warp_id);
    void DECODE(int warp_id);
    // ibuffer
    void cycle_IBUF_ACTION(int warp_id, I_TYPE &dispatch_ins_, I_TYPE &_readdata3);
    void IBUF_PARAM(int warp_id);
    // scoreboard
    void cycle_UPDATE_SCORE(int warp_id, I_TYPE &tmpins, std::set<SCORE_TYPE>::iterator &it, REG_TYPE &regtype_, bool &insertscore);
    void JUDGE_DISPATCH(int warp_id);
    void cycle_JUDGE_DISPATCH(int warp_id, I_TYPE &_readibuf);
    void BEFORE_DISPATCH(int warp_id);
    // issue
    void WARP_SCHEDULER();
    // opc
    void OPC_FIFO();
    void OPC_FETCH();
    void OPC_EMIT();
    bank_t bank_decode(int warp_id, int srcaddr);
    warpaddr_t bank_undecode(int bank_id, int addr);

    // regfile
    void INIT_REG(int warp_id);
    std::pair<int, int> reg_arbiter(const std::array<std::array<bank_t, 3>, OPCFIFO_SIZE> &addr_arr, // opc_srcaddr
                                    const std::array<std::array<bool, 3>, OPCFIFO_SIZE> &valid_arr,  // opc_valid
                                    std::array<std::array<bool, 3>, OPCFIFO_SIZE> &ready_arr,        // opc_ready
                                    int bank_id,
                                    std::array<int, BANK_NUM> &REGcurrentIdx,
                                    std::array<int, BANK_NUM> &read_bank_addr);
    void READ_REG();
    void WRITE_REG(int warp_id);
    // exec
    void SALU_IN();
    void SALU_CALC();
    void SALU_CTRL();
    void VALU_IN();
    void VALU_CALC();
    void VALU_CTRL();
    void VFPU_IN();
    void VFPU_CALC();
    void VFPU_CTRL();
    void LSU_IN();
    void LSU_CALC();
    void LSU_CTRL();
    void SIMT_STACK(int warp_id);
    void CSR_IN();
    void CSR_CALC();
    void CSR_CTRL();
    void MUL_IN();
    void MUL_CALC();
    void MUL_CTRL();
    void SFU_IN();
    void SFU_CALC();
    void SFU_CTRL();
    void TC_IN();
    void TC_CALC();
    void TC_CTRL();
    // writeback
    void WRITE_BACK();

    //void set_CTA_Scheduler(CTA_Scheduler *_cta_scheduler_ptr) { m_cta_scheduler = _cta_scheduler_ptr; }

    // initialize
    void start_of_simulation()
    {
        for (auto &warp_ : m_hw_warps)
        {
            warp_->pc = -1;
            warp_->ibuftop_ins = I_TYPE(INVALID_, 0, 0, 0);
        }
        issue_ins = I_TYPE(INVALID_, 0, 0, 0);
    }

    BASE(sc_core::sc_module_name name, int _sm_id, Memory *mem);

public:
    std::map<OP_TYPE, decodedat> decode_table;
    std::vector<instable_t> instable_vec;
    /*** SIMT frontend ***/
    SafeArray<WARP_BONE *, hw_num_warp> m_hw_warps;
    // std::array<WARP_BONE *, hw_num_warp> m_hw_warps;
    // std::unordered_map<int, WARP_BONE*> m_hw_warps;


    std::array<std::array<sc_core::sc_process_handle *, hw_num_warp>, 9> warp_threads_group;
    // std::array<sc_core::sc_process_handle *, hw_num_warp> threads_PROGRAM_COUNTER;
    // std::array<sc_core::sc_process_handle *, hw_num_warp> threads_INSTRUCTION_REG;
    // std::array<sc_core::sc_process_handle *, hw_num_warp> threads_DECODE;
    // std::array<sc_core::sc_process_handle *, hw_num_warp> threads_IBUF_ACTION;
    // std::array<sc_core::sc_process_handle *, hw_num_warp> threads_JUDGE_DISPATCH;
    // std::array<sc_core::sc_process_handle *, hw_num_warp> threads_UPDATE_SCORE;
    // std::array<sc_core::sc_process_handle *, hw_num_warp> threads_INIT_REG;
    // std::array<sc_core::sc_process_handle *, hw_num_warp> threads_SIMT_STACK;
    // std::array<sc_core::sc_process_handle *, hw_num_warp> threads_WRITE_REG;

    /*** SIMD backend ***/
    // issue
    sc_event_and_list ev_warp_dispatch_list;
    sc_signal<I_TYPE> issue_ins{"issue_ins"};
    sc_signal<int> issueins_warpid{"issueins_warpid"};
    sc_signal<int> last_dispatch_warpid{"last_dispatch_warpid"}; // 需要设为sc_signal，否则dispatch判断对i的循环边界【i < last_dispatch_warpid + hw_num_warp】会变化
    sc_signal<bool> dispatch_valid{"dispatch_valid"};
    BoolArray<hw_num_warp> wait_barrier; // true为等待barrier
    sc_signal<bool> emito_warpscheduler{"emito_wrpschdler"};

    // warp scheduler
    sc_event ev_warp_assigned;
    // opc
    sc_signal<int> last_emit_entryid{"last_emit_entryid"};
    sc_event ev_opc_pop, ev_regfile_readdata,
        ev_opc_judge_emit, ev_opc_store, ev_opc_collect;
    StaticEntry<opcfifo_t, OPCFIFO_SIZE> opcfifo; // tlm::tlm_fifo<I_TYPE> opcfifo;
    std::array<std::array<bool, 3>, OPCFIFO_SIZE> opc_valid;
    std::array<std::array<bool, 3>, OPCFIFO_SIZE> opc_ready;
    std::array<std::array<bank_t, 3>, OPCFIFO_SIZE> opc_srcaddr;
    std::array<std::array<bool, 3>, OPCFIFO_SIZE> opc_banktype; // 0-s, 1-v
    std::array<int, BANK_NUM> read_bank_addr;                   // regfile arbiter给出
    std::array<int, BANK_NUM> REGcurrentIdx;                    // OPC轮询到哪了
    std::array<std::array<reg_t, hw_num_thread>, BANK_NUM> read_data;
    std::array<std::pair<int, int>, BANK_NUM> REGselectIdx; // 轮询选出哪个了（索引，有这个数据，ready其实没有用了）
    sc_signal<int> emit_idx{"emit_idx"};                    // 上一周期emit的ins在opc中的索引，最大是BANK_NUM
    sc_signal<bool> opc_full{"opc_full"};
    bool opc_empty;
    sc_signal<I_TYPE> emit_ins{"emit_ins"};
    sc_signal<int> emitins_warpid{"emitins_warpid"};
    sc_signal<int> opcfifo_elem_num{"opcfifo_elem_num"};
    bool findemit; // 轮询时，找到了全ready且执行单元也ready的entry
    sc_signal<bool> doemit{"doemit"};
    // regfile
    sc_signal<int> rdv1_addr{"rdv1_addr"};
    sc_signal<reg_t> rds1_data{"rds1_data"};
    sc_vector<sc_signal<reg_t>> rdv1_data{"rdv1_data", hw_num_thread};

    //
    // exec
    //
    // salu
    sc_signal<bool> emito_salu{"emito_salu"};
    sc_signal<reg_t> tosalu_data1{"tosalu_data1"}, tosalu_data2{"tosalu_data2"}, tosalu_data3{"tosalu_data3"}; // OPC TO SALU
    bool salu_ready;
    sc_signal<bool> salu_ready_old{"salu_ready_old"};
    sc_event_queue salu_eqa, salu_eqb; // 分别负责a time和b time，最后一个是SALU_IN的，优先级比eqb低
    sc_event salu_eva, salu_evb, salu_unready, salu_nothinghappen,
        ev_salufifo_pushed, ev_saluready_updated;
    std::queue<salu_in_t> salu_dq;
    StaticQueue<salu_out_t, 3> salufifo;
    salu_out_t salutop_dat;
    bool salufifo_empty, salufifo_push;
    int salufifo_elem_num;
    salu_in_t salutmp1;
    salu_out_t salutmp2;
    sc_signal<bool> salueqa_triggered{"salueqa_triggered"}, salueqb_triggered{"salueqb_triggered"}; // 例如eqa_triggered，仅在eqa被触发时，delta 0变为1，delta 1给SALU_IN看，同时又变回0
    sc_signal<bool> execpop_salu{"execpop_salu"};

    // valu
    sc_signal<bool> emito_valu{"emito_valu"};
    sc_vector<sc_signal<reg_t>> tovalu_data1{"tovalu_data1", hw_num_thread}, // OPC TO VALU
        tovalu_data2{"tovalu_data2", hw_num_thread}, tovalu_data3{"tovalu_data3", hw_num_thread};
    bool valu_ready;
    sc_signal<bool> valu_ready_old{"Valu_ready_old"};
    sc_event_queue valu_eqa, valu_eqb;
    sc_event valu_eva, valu_evb, valu_unready, valu_nothinghappen,
        ev_valufifo_pushed, ev_valuready_updated;
    std::queue<valu_in_t> valu_dq;
    StaticQueue<valu_out_t, 3> valufifo;
    valu_out_t valutop_dat;
    bool valufifo_empty, valufifo_push;
    int valufifo_elem_num;
    sc_signal<bool> valueqa_triggered{"valueqa_triggered"}, valueqb_triggered{"valueqb_triggered"};
    sc_signal<bool> execpop_valu{"execpop_valu"};

    // vfpu
    sc_signal<bool> emito_vfpu{"emito_vfpu"};
    sc_vector<sc_signal<int32_t>> tovfpu_data1{"tovfpu_data1", hw_num_thread}, // OPC TO VFPU
        tovfpu_data2{"tovfpu_data2", hw_num_thread}, tovfpu_data3{"tovfpu_data3", hw_num_thread};
    bool vfpu_ready;
    sc_signal<bool> vfpu_ready_old{"vfpu_ready_old"};
    sc_event_queue vfpu_eqa, vfpu_eqb;
    sc_event vfpu_eva, vfpu_evb, vfpu_unready, vfpu_nothinghappen,
        ev_vfpufifo_pushed, ev_vfpuready_updated;
    std::queue<vfpu_in_t> vfpu_dq;
    StaticQueue<vfpu_out_t, 3> vfpufifo;
    vfpu_out_t vfputop_dat;
    bool vfpufifo_empty, vfpufifo_push;
    int vfpufifo_elem_num;
    sc_signal<bool> vfpueqa_triggered{"vfpueqa_triggered"}, vfpueqb_triggered{"vfpueqb_triggered"};
    sc_signal<bool> execpop_vfpu{"execpop_vfpu"};

    // lsu
    sc_signal<bool> emito_lsu{"emito_lsu"};
    sc_vector<sc_signal<int32_t>> tolsu_data1{"emitolsu_data1", hw_num_thread}, // OPC TO LSU
        tolsu_data2{"emitolsu_data2", hw_num_thread}, tolsu_data3{"emitolsu_data3", hw_num_thread};
    bool lsu_ready;
    sc_signal<bool> lsu_ready_old{"lsu_ready_old"};
    sc_event_queue lsu_eqa, lsu_eqb;
    sc_event lsu_eva, lsu_evb, lsu_unready, lsu_nothinghappen,
        ev_lsufifo_pushed, ev_lsuready_updated;
    std::queue<lsu_in_t> lsu_dq;
    StaticQueue<lsu_out_t, 10> lsufifo;
    lsu_out_t lsutop_dat;
    bool lsufifo_empty;
    int lsufifo_elem_num;
    sc_signal<bool> lsueqa_triggered{"lsueqa_triggered"}, lsueqb_triggered{"lsueqb_triggered"};
    sc_signal<bool> execpop_lsu{"execpop_lsu"};

    // simt stack
    sc_signal<bool> emito_simtstk{"emito_simtstk"};                     // 对应join，由于wait_bran的存在，这两个不会同时为1
    sc_signal<bool, SC_MANY_WRITERS> valuto_simtstk{"valuto_simtstk"};  // 对应beq类，由于wait_bran的存在，这两个不会同时为1
    simtstack_t simtstk_newelem;                                        // from VALU to SIMT-stack
    sc_signal<int> simtstk_new_warpid{"simtstk_new_warpid"};            // newelem对应的warp
    sc_signal<sc_bv<hw_num_thread>> branch_elsemask{"branch_elsemask"}; // VALU计算出的elsemask，将发给SIMT-stack，elsemask为1表示判断跳转
    sc_signal<sc_bv<hw_num_thread>> branch_ifmask{"branch_ifmask"};     // 与elsemask相反
    sc_signal<uint32_t> branch_elsepc{"branch_elsepc"};                 // VALU处理分支跳转的else分支pc
    sc_signal<I_TYPE> vbranch_ins{"vbranch_ins"};
    sc_signal<int> vbranchins_warpid{"vbranchins_warpid"};

    // csr
    sc_signal<bool> emito_csr{"emito_csr"};
    sc_signal<int32_t> tocsr_data1{"tocsr_data1"}, tocsr_data2{"tocsr_data2"}; // OPC TO CSR
    bool csr_ready;
    sc_signal<bool> csr_ready_old{"csr_ready_old"};
    sc_event_queue csr_eqa, csr_eqb;
    sc_event csr_eva, csr_evb, csr_unready, csr_nothinghappen,
        ev_csrfifo_pushed, ev_csrready_updated;
    std::queue<csr_in_t> csr_dq;
    StaticQueue<csr_out_t, 10> csrfifo;
    csr_out_t csrtop_dat;
    bool csrfifo_empty;
    int csrfifo_elem_num;
    sc_signal<bool> csreqa_triggered{"csreqa_triggered"}, csreqb_triggered{"csreqb_triggered"};
    sc_signal<bool> execpop_csr{"execpop_csr"};

    // mul
    sc_signal<bool> emito_mul{"emito_mul"};
    sc_vector<sc_signal<reg_t>> tomul_data1{"tomul_data1", hw_num_thread},
        tomul_data2{"tomul_data2", hw_num_thread}, tomul_data3{"tomul_data3", hw_num_thread};
    bool mul_ready;
    sc_signal<bool> mul_ready_old{"mul_ready_old"};
    sc_event_queue mul_eqa, mul_eqb;
    sc_event mul_eva, mul_evb, mul_unready, mul_nothinghappen,
        ev_mulfifo_pushed, ev_mulready_updated;
    std::queue<mul_in_t> mul_dq;
    StaticQueue<mul_out_t, 3> mulfifo;
    mul_out_t multop_dat;
    bool mulfifo_empty, mulfifo_push;
    int mulfifo_elem_num;
    sc_signal<bool> muleqa_triggered{"muleqa_triggered"}, muleqb_triggered{"muleqb_triggered"};
    sc_signal<bool> execpop_mul{"execpop_mul"};

    // sfu
    sc_signal<bool> emito_sfu{"emito_sfu"};
    sc_vector<sc_signal<reg_t>> tosfu_data1{"tosfu_data1", hw_num_thread},
        tosfu_data2{"tosfu_data2", hw_num_thread};
    bool sfu_ready;
    sc_signal<bool> sfu_ready_old{"sfu_ready_old"};
    sc_event_queue sfu_eqa, sfu_eqb;
    sc_event sfu_eva, sfu_evb, sfu_unready, sfu_nothinghappen,
        ev_sfufifo_pushed, ev_sfuready_updated;
    std::queue<sfu_in_t> sfu_dq;
    StaticQueue<sfu_out_t, 3> sfufifo;
    sfu_out_t sfutop_dat;
    bool sfufifo_empty, sfufifo_push;
    int sfufifo_elem_num;
    sc_signal<bool> sfueqa_triggered{"sfueqa_triggered"}, sfueqb_triggered{"sfueqb_triggered"};
    sc_signal<bool> execpop_sfu{"execpop_sfu"};

    // tc
    sc_signal<bool> emito_tc{"emito_tc"};
    sc_vector<sc_signal<int32_t>> totc_data1{"totc_data1", hw_num_thread}, // OPC TO TC
        totc_data2{"totc_data2", hw_num_thread}, totc_data3{"totc_data3", hw_num_thread};
    bool tc_ready;
    sc_signal<bool> tc_ready_old{"tc_ready_old"};
    sc_event_queue tc_eqa, tc_eqb;
    sc_event tc_eva, tc_evb, tc_unready, tc_nothinghappen,
        ev_tcfifo_pushed, ev_tcready_updated;
    std::queue<tc_in_t> tc_dq;
    StaticQueue<tc_out_t, 3> tcfifo;
    tc_out_t tctop_dat;
    bool tcfifo_empty, tcfifo_push;
    int tcfifo_elem_num;
    sc_signal<bool> tceqa_triggered{"tceqa_triggered"}, tceqb_triggered{"tceqb_triggered"};
    sc_signal<bool> execpop_tc{"execpop_tc"};

    // writeback
    sc_signal<bool> write_s{"write_s"}, write_v{"write_v"}, write_f{"write_f"};
    sc_signal<I_TYPE> wb_ins{"wb_ins"};
    sc_signal<int> wb_warpid{"wb_warpid"};
    sc_signal<bool> wb_ena{"wb_ena"};

    // debug，没实际用处
    sc_signal<bool> dispatch_ready{"dispatch_ready"};

    // 外部存储，暂时在BASE中实现
    std::array<I_TYPE, ireg_size> ireg;
    // std::vector<std::vector<uint8_t>> *buffer_data;

    std::array<int, 32> testCSR;
    meta_data_t mtd;

    // 命令行参数
    std::string metafile;
    std::string datafile;

    int m_num_warp_activated = 0;
    int sm_id;

    // CTA Scheduling
    int m_num_active_cta;
    //CTA_Scheduler *m_cta_scheduler;
    void issue_block2core(std::shared_ptr<kernel_info_t> kernel);
    void receive_warp(uint32_t block_idx, uint32_t warp_idx, std::shared_ptr<kernel_info_t> kernel, uint32_t block_slot, uint32_t lds_baseaddr);
    std::function<void(int sm_id, int blk_slot_idx, int warp_idx_in_blk)> m_warp_finish_callback; // warp执行完毕后回调通知CTA Scheduler
    void set_kernel(std::shared_ptr<kernel_info_t> kernel);
    bool can_issue_1block(std::shared_ptr<kernel_info_t> kernel);
    std::shared_ptr<kernel_info_t> m_kernel;
    std::shared_ptr<kernel_info_t> get_current_kernel() { return m_kernel; }
    sc_signal<bool, SC_MANY_WRITERS> m_current_kernel_running{"m_current_kernel_running"};     // 是否应用signal待定
    sc_signal<bool, SC_MANY_WRITERS> m_current_kernel_completed{"m_current_kernel_completed"}; // 是否应用signal待定
    bool is_current_kernel_completed() { return m_current_kernel_completed.read(); }
    std::array<sc_signal<bool>, hw_num_warp> m_issue_block2warp;

    unsigned max_cta_num(std::shared_ptr<kernel_info_t> kernel);
    int m_cta_status[MAX_CTA_PER_CORE];
};

#endif
