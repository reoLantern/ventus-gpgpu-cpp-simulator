#ifndef CONTEXT_MODEL_H_
#define CONTEXT_MODEL_H_

#include "parameters.h"
#include <functional>
#include <string>
#include "membox_sv39/memory.h"

struct dim3
{
    uint64_t x, y, z;
};

void increment_x_then_y_then_z(dim3 &i, const dim3 &bound);

struct meta_data_t
{ // 这个metadata是供驱动使用的，而不是给硬件的
    uint64_t startaddr;
    uint64_t kernel_id;
    uint64_t kernel_size[3];    ///> 每个kernel的workgroup三维数目
    uint64_t wf_size;           ///> 每个warp的thread数目
    uint64_t wg_size;           ///> 每个workgroup的warp数目
    uint64_t metaDataBaseAddr;  ///> CSR_KNL的值，
    uint64_t ldsSize;           ///> 每个workgroup使用的local memory的大小
    uint64_t pdsSize;           ///> 每个thread用到的private memory大小
    uint64_t sgprUsage;         ///> 每个workgroup使用的标量寄存器数目
    uint64_t vgprUsage;         ///> 每个thread使用的向量寄存器数目
    uint64_t pdsBaseAddr;       ///> private memory的基址，要转成每个workgroup的基地址， wf_size*wg_size*pdsSize
    uint64_t num_buffer;        ///> buffer的数目，包括pc
    uint64_t *buffer_base;      ///> 各buffer的基址
    uint64_t *buffer_size;      ///> 各buffer的size，以Bytes为单位。实际使用的大小，用于初始化.data
    uint64_t *buffer_allocsize; ///> 各buffer的size，以Bytes为单位。分配的大小

    int insBufferIndex; // 指令在哪一个buffer
};

class task_t;

class kernel_info_t
{
public:
    kernel_info_t(uint32_t kernel_id, const std::string &kernel_name, const std::string &metadata_file, const std::string &data_file,
                  uint64_t pagetable);

    bool no_more_ctas_to_run() const;
    uint32_t get_kid() { return m_kernel_id; }
    std::string get_kname() { return m_kernel_name; }
    dim3 get_next_cta_id() const { return m_next_cta; }
    unsigned get_next_cta_id_single() const;
    void increment_cta_id() { increment_x_then_y_then_z(m_next_cta, m_grid_dim); }

    unsigned get_startaddr() const { return m_metadata.startaddr; }
    unsigned get_num_buffer() const { return m_metadata.num_buffer; }
    unsigned get_num_warp_per_cta() const { return m_metadata.wg_size; }
    unsigned get_num_thread_per_warp() const { return m_metadata.wf_size; }
    unsigned get_ldsSize_per_cta() const { return m_metadata.ldsSize; }
    unsigned get_pdsSize_per_thread() const { return m_metadata.pdsSize; }
    uint64_t get_pdsBaseAddr() const { return m_metadata.pdsBaseAddr; }
    uint64_t get_metadata_baseaddr() const { return m_metadata.metaDataBaseAddr; }
    uint64_t get_pagetable() const { return m_pagetable; }

    int m_num_sm_running_this;

    // Load initial data and get ready to run
    void activate(Memory *mem, std::function<void()> finish_callback);
    bool is_running() const { return m_is_running; }

    // After kernel finished
    void finish();
    bool is_finished() const { return m_is_finished; }

private:
    uint64_t m_pagetable;        // pagetable root (address space ID), see membox_sv39/memory.h
    const std::string m_data_filename;
    const std::string m_kernel_name;
    meta_data_t m_metadata;
    const uint32_t m_kernel_id;
    unsigned m_running_cta;      // 当前正在运行的cta数量
    std::array<int, MAX_RUNNING_CTA_PER_KERNEL> m_cta_status_panel;
    dim3 m_next_cta = {0, 0, 0}; // start from 0 ~ (grid_dim - 1)
    dim3 m_grid_dim;

    // Helpers
    bool isHexCharacter(char c);
    int charToHex(char c);

    // Load testcase.metadata file
    void initMetaData(const std::string &filename);
    void readHexFile(const std::string &filename, int itemSize, std::vector<uint64_t> &items);
    void assignMetadata(const std::vector<uint64_t> &metadata, meta_data_t &mtd);

    // Load testcase.data file
    void readTextFile(Memory *mem);

    // After kernel finished
    bool m_is_running;
    bool m_is_finished;
    std::function<void ()> m_finish_callback;

public:
    uint64_t start_cycle;
    uint64_t end_cycle;
};

#endif
