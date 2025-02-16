#ifndef CONTEXT_MODEL_H_
#define CONTEXT_MODEL_H_

#include "membox_sv39/memory.h"
#include "parameters.h"
#include <functional>
#include <memory>
#include <string>

struct dim3 {
    uint64_t x, y, z;
};

void increment_x_then_y_then_z(dim3& i, const dim3& bound);

struct meta_data_t { // 这个metadata是供驱动使用的，而不是给硬件的
    uint64_t startaddr;
    uint64_t kernel_id;
    uint64_t kernel_size[3];   ///> 每个kernel的workgroup三维数目
    uint64_t wf_size;          ///> 每个warp的thread数目
    uint64_t wg_size;          ///> 每个workgroup的warp数目
    uint64_t metaDataBaseAddr; ///> CSR_KNL的值，
    uint64_t ldsSize;          ///> 每个workgroup使用的local memory的大小
    uint64_t pdsSize;          ///> 每个thread用到的private memory大小
    uint64_t sgprUsage;        ///> 每个workgroup使用的标量寄存器数目
    uint64_t vgprUsage;        ///> 每个thread使用的向量寄存器数目
    uint64_t pdsBaseAddr; ///> private memory的基址，要转成每个workgroup的基地址， wf_size*wg_size*pdsSize
    uint64_t num_buffer;  ///> buffer的数目，包括pc
    uint64_t* buffer_base;      ///> 各buffer的基址
    uint64_t* buffer_size;      ///> 各buffer的size，以Bytes为单位。实际使用的大小，用于初始化.data
    uint64_t* buffer_allocsize; ///> 各buffer的size，以Bytes为单位。分配的大小

    int insBufferIndex; // 指令在哪一个buffer
};

class task_t;

class kernel_info_t {
public:
    kernel_info_t(uint32_t kernel_id, const std::string& kernel_name, const std::string& metadata_file,
                  const std::string& data_file, uint64_t pagetable);

    //
    // Static: only determined by the testcase/host
    //

    uint32_t get_kid() { return m_kernel_id; }
    std::string get_kname() { return m_kernel_name; }

    unsigned get_startaddr() const { return m_metadata.startaddr; }
    uint32_t get_num_block() const { return m_grid_dim.x * m_grid_dim.y * m_grid_dim.z; }
    unsigned get_num_buffer() const { return m_metadata.num_buffer; }
    unsigned get_num_warp_per_cta() const { return m_metadata.wg_size; }
    unsigned get_num_thread_per_warp() const { return m_metadata.wf_size; }
    unsigned get_ldsSize_per_cta() const { return m_metadata.ldsSize; }
    unsigned get_pdsSize_per_thread() const { return m_metadata.pdsSize; }
    uint64_t get_pdsBaseAddr() const { return m_metadata.pdsBaseAddr; }
    uint64_t get_metadata_baseaddr() const { return m_metadata.metaDataBaseAddr; }
    uint64_t get_pagetable() const { return m_pagetable; }

    //
    // Dynamic: changes on GPU. Maybe they should be moved to CTA_Scheduler
    //

    bool no_more_ctas_to_run() const;
    dim3 get_next_cta_id() const { return m_next_cta; }
    unsigned get_next_cta_id_single() const;
    void increment_cta_id() { increment_x_then_y_then_z(m_next_cta, m_grid_dim); }

    int m_num_sm_running_this;

    // Three status of kernel, block and warp
    //    | waiting       | data loaded to mem, running |  all blocks finished  |
    enum { KERNEL_STATUS_WAIT, KERNEL_STATUS_RUNNING, KERNEL_STATUS_FINISHED } m_status;
    enum { BLOCK_STATUS_WAIT, BLOCK_STATUS_RUNNING, BLOCK_STATUS_FINISHED };
    enum { WARP_STATUS_WAIT, WARP_STATUS_RUNNING, WARP_STATUS_FINISHED };
    std::unique_ptr<int[]> m_block_status; // status of each block
    std::unique_ptr<int[][hw_num_warp]> m_warp_status; // status of each warp

    // Load initial data and get ready to run
    void activate(Memory* mem, std::function<void()> finish_callback);
    bool is_running() const { return m_status == KERNEL_STATUS_RUNNING; }

    // After kernel finished
    void finish();
    bool is_finished() const { return m_status == KERNEL_STATUS_FINISHED; }

private:
    //
    // Static: only determined by the testcase/host
    //
    const std::string m_data_filename;
    const std::string m_kernel_name;
    meta_data_t m_metadata;
    dim3 m_grid_dim;      // grid size 3D (number of blocks)
    uint64_t m_pagetable; // pagetable root (address space ID), see membox_sv39/memory.h
    const uint32_t m_kernel_id;
    std::function<void()> m_finish_callback; // callback this when kernel finished

    // Helpers
    bool isHexCharacter(char c);
    int charToHex(char c);

    // Load testcase.metadata file
    void initMetaData(const std::string& filename);
    void readHexFile(const std::string& filename, int itemSize, std::vector<uint64_t>& items);
    void assignMetadata(const std::vector<uint64_t>& metadata, meta_data_t& mtd);

    //
    // Dynamic: changes on GPU. Maybe they should be moved to CTA_Scheduler
    //

    unsigned m_running_cta; // 当前正在运行的cta数量
    // std::array<int, MAX_RUNNING_CTA_PER_KERNEL> m_cta_status_panel;
    dim3 m_next_cta = { 0, 0, 0 }; // start from 0 ~ (grid_dim - 1)

    // Load testcase.data file to GPU memory
    void readTextFile(Memory* mem);


public:
    uint64_t start_cycle;
    uint64_t end_cycle;
};

#endif
