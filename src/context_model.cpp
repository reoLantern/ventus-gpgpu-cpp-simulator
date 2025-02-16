#include "context_model.hpp"
#include "parameters.h"
#include "task.hpp"
#include "utils/log.h"
#include <algorithm>

kernel_info_t::kernel_info_t(uint32_t kernel_id, const std::string& kernel_name, const std::string& metadata_file,
                             const std::string& data_file, uint64_t pagetable)
    : m_kernel_id(kernel_id)
    , m_pagetable(pagetable)
    , m_kernel_name(kernel_name)
    , m_data_filename(data_file)
    // , m_is_running(false)
    // , m_is_finished(false)
    , m_finish_callback(nullptr) {

    initMetaData(metadata_file);
    assert(get_num_warp_per_cta() <= hw_num_warp);

    m_status = kernel_info_t::KERNEL_STATUS_WAIT;
    m_num_sm_running_this = 0;
    m_block_status = std::make_unique<int[]>(get_num_block());
    m_warp_status = std::make_unique<int[][hw_num_warp]>(get_num_block());
    std::fill(m_block_status.get(), m_block_status.get() + get_num_block(), BLOCK_STATUS_WAIT);
    std::fill(*m_warp_status.get(), *m_warp_status.get() + get_num_block() * hw_num_warp, WARP_STATUS_WAIT);

    log_info("kernel %s initialized, set grid_dim = %d,%d,%d", kernel_name.c_str(), m_grid_dim.x, m_grid_dim.y,
             m_grid_dim.z);
}

void kernel_info_t::finish() {
    assert(m_status == KERNEL_STATUS_RUNNING);
    m_status = KERNEL_STATUS_FINISHED;
    // m_is_finished = true;
    // m_is_running = false;
    if (m_finish_callback) {
        m_finish_callback();
    }
    log_info("Kernel-%d %s finished", get_kid(), get_kname().c_str());
}

bool kernel_info_t::no_more_ctas_to_run() const {
    return (m_next_cta.x >= m_grid_dim.x || m_next_cta.y >= m_grid_dim.y || m_next_cta.z >= m_grid_dim.z);
}

unsigned kernel_info_t::get_next_cta_id_single() const {
    return m_next_cta.x + m_grid_dim.x * m_next_cta.y + m_grid_dim.x * m_grid_dim.y * m_next_cta.z;
}

// Helpers
bool kernel_info_t::isHexCharacter(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
int kernel_info_t::charToHex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else
        return -1; // Invalid character
}

void kernel_info_t::initMetaData(const std::string& filename) {
    std::vector<uint64_t> metadata;
    readHexFile(filename, 64, metadata);
    assignMetadata(metadata, m_metadata);
}

// convert (.metadata) hex file into raw metadata buffer
void kernel_info_t::readHexFile(const std::string& filename, int itemSize, std::vector<uint64_t>& items) {
    // itemSize为每个数据的比特数，这里为64
    ifstream file(filename);

    if (!file) {
        std::cout << "Error opening file: " << filename << std::endl;
        return;
    }

    char c;
    int bits = 0;
    uint64_t value = 0;
    bool leftside = false;

    while (file.get(c)) {
        if (c == '\n') {
            if (bits != 0)
                leftside = true;
            continue;
        }

        if (!isHexCharacter(c)) {
            log_error("Invalid character found: '%c' in %s", c, filename.c_str());
            continue;
        }

        int hexValue = charToHex(c);
        if (leftside)
            value = value | ((uint64_t)hexValue << (92 - bits));
        else
            value = (value << 4) | hexValue;
        bits += 4;

        if (bits >= itemSize) {
            items.push_back(value);
            value = 0;
            bits = 0;
            leftside = false;
        }
    }

    if (bits > 0) {
        log_error("Warning: Incomplete item found at the end of the file!");
    }

    file.close();
}

// convert raw metadata buffer into struct meta_data_t
void kernel_info_t::assignMetadata(const std::vector<uint64_t>& metadata, meta_data_t& mtd) {
    int index = 0;

    mtd.startaddr = metadata[index++];

    mtd.kernel_id = metadata[index++];

    for (int i = 0; i < 3; i++) {
        mtd.kernel_size[i] = metadata[index++];
    }
    m_grid_dim.x = mtd.kernel_size[0];
    m_grid_dim.y = mtd.kernel_size[1];
    m_grid_dim.z = mtd.kernel_size[2];

    mtd.wf_size = metadata[index++];
    mtd.wg_size = metadata[index++];
    mtd.metaDataBaseAddr = metadata[index++];
    mtd.ldsSize = metadata[index++];
    mtd.pdsSize = metadata[index++];
    mtd.sgprUsage = metadata[index++];
    mtd.vgprUsage = metadata[index++];
    mtd.pdsBaseAddr = metadata[index++];

    mtd.num_buffer = metadata[index++] + 1; // add localmem buffer

    mtd.buffer_base = new uint64_t[mtd.num_buffer];

    for (int i = 0; i < mtd.num_buffer - 1; i++) {
        mtd.buffer_base[i] = metadata[index++];
        if (mtd.buffer_base[i] == mtd.startaddr)
            mtd.insBufferIndex = i;
    }
    mtd.buffer_base[mtd.num_buffer - 1] = ldsBaseAddr_core; // localmem base addr

    mtd.buffer_size = new uint64_t[mtd.num_buffer];
    for (int i = 0; i < mtd.num_buffer - 1; i++) {
        mtd.buffer_size[i] = metadata[index++];
    }
    mtd.buffer_size[mtd.num_buffer - 1] = 0;

    mtd.buffer_allocsize = new uint64_t[mtd.num_buffer];
    for (int i = 0; i < mtd.num_buffer - 1; i++) {
        mtd.buffer_allocsize[i] = metadata[index++];
    }
    mtd.buffer_allocsize[mtd.num_buffer - 1] = mtd.ldsSize;
}

// read (testcase.data) hexfile, and setup initial memory
void kernel_info_t::readTextFile(Memory* mem) {
    meta_data_t& mtd = m_metadata;
    std::ifstream file(m_data_filename);
    if (!file.is_open()) {
        log_fatal("Failed to open file: %s", m_data_filename.c_str());
        return;
    }

    std::string line;
    int bufferIndex = 0;
    std::vector<uint8_t> buffer;
    for (int bufferIndex = 0; bufferIndex < mtd.num_buffer; bufferIndex++) {
        buffer.reserve(mtd.buffer_allocsize[bufferIndex]); // 提前分配空间
        mem->allocateMemory(get_pagetable(), mtd.buffer_base[bufferIndex], mtd.buffer_allocsize[bufferIndex]);
        int readbytes = 0;
        while (readbytes < mtd.buffer_size[bufferIndex]) {
            std::getline(file, line);
            for (int i = line.length(); i > 0; i -= 2) {
                std::string hexChars = line.substr(i - 2, 2);
                uint8_t byte = std::stoi(hexChars, nullptr, 16);
                buffer.push_back(byte);
            }
            readbytes += 4;
        }
        buffer.resize(mtd.buffer_allocsize[bufferIndex]);
        mem->writeDataVirtual(get_pagetable(), mtd.buffer_base[bufferIndex], mtd.buffer_size[bufferIndex],
                              buffer.data());
        buffer.clear();
    }
    // buffers[mtd.num_buffer-1] is localmem(LDS)
    // It contains no initial data, mtd.buffer_size[mtd.num_buffer-1] = 0

    file.close();
}

// 激活Kernel，载入初始数据，随时开始运行
void kernel_info_t::activate(Memory* mem, std::function<void()> finish_callback) {
    readTextFile(mem);
    m_finish_callback = finish_callback;
    // m_is_running = true;
    m_status = KERNEL_STATUS_RUNNING;
    log_info("Kernel%d %s load init data", m_kernel_id, m_kernel_name.c_str());
}
