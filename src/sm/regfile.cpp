#include "BASE.h"

std::pair<int, int> BASE::reg_arbiter(
    const std::array<std::array<bank_t, 3>, OPCFIFO_SIZE> &addr_arr, // opc_srcaddr
    const std::array<std::array<bool, 3>, OPCFIFO_SIZE> &valid_arr,  // opc_valid
    std::array<std::array<bool, 3>, OPCFIFO_SIZE> &ready_arr,        // opc_ready
    int bank_id,
    std::array<int, BANK_NUM> &REGcurrentIdx,
    std::array<int, BANK_NUM> &read_bank_addr)
{
    const int rows = OPCFIFO_SIZE; // = addr_arr.size()
    const int cols = 3;            // = addr_arr[0].size(), 每个opc_fifo_t四个待取元素
    const int size = rows * cols;
    std::pair<int, int> result(-1, -1); // 默认值表示没有找到有效数据
    int index, i, j;
    for (int idx = REGcurrentIdx[bank_id] % size;
         idx < size + REGcurrentIdx[bank_id] % size; idx++)
    {
        index = idx % size;
        i = index / cols;
        j = index % cols;
        if (valid_arr[i][j] == true)
        {
            if (addr_arr[i][j].bank_id == bank_id)
            {
                read_bank_addr[bank_id] = addr_arr[i][j].addr; // 下周期读数据
                // std::cout << "let read_bank_addr(bank" << bank_id << ")="
                //      << addr_arr[i][j].addr << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                result.first = i;
                result.second = j;
                ready_arr[i][j] = true;
                REGcurrentIdx[bank_id] = index + 1;
                break;
            }
        }
    }
    return result;
}

void BASE::READ_REG()
{
    std::pair<int, int> temp_pair(-1, -1);
    int row, col;
    REGselectIdx.fill({-1, -1});
    warpaddr_t tmp;
    while (true)
    {
        wait(clk.posedge_event());

        // 先根据上一cycle regfile arbiter的结果读数据
        for (int i = 0; i < BANK_NUM; i++)
        {
            row = REGselectIdx[i].first;
            col = REGselectIdx[i].second;
            if (REGselectIdx[i] != temp_pair)
            {
                tmp = bank_undecode(i, read_bank_addr[i]);
                // std::cout << opcfifo[row].ins << " warp " << opcfifo[row].warp_id;
                // std::cout << " decode(bank" << i << ",addr" << read_bank_addr[i]
                //      << ") undecode --> warp_id=" << tmp.warp_id << ", addr=" << tmp.addr << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
                // std::cout << "从regfile读出: REGselectIdx[" << i << "] to opc(" << REGselectIdx[i].first << "," << REGselectIdx[i].second << ") at " << sc_time_stamp() <<","<< sc_delta_count_at_current_time() << "\n";
                if (opc_banktype[row][col] == 0)
                {
                    read_data[i].fill(m_hw_warps[tmp.warp_id]->s_regfile[tmp.addr]);
                }
                else
                {
                    read_data[i] = m_hw_warps[tmp.warp_id]->v_regfile[tmp.addr];
                }
            }
        }
        ev_regfile_readdata.notify();

        // 再根据当前cycle的opc进行regfile arbiter
        wait(ev_opc_collect);
        for (auto &elem : opc_ready)
            elem.fill(0);
        for (int i = 0; i < BANK_NUM; i++)
        {
            REGselectIdx[i] = reg_arbiter(opc_srcaddr, opc_valid, opc_ready, i, REGcurrentIdx, read_bank_addr);
        }
    }
}

void BASE::WRITE_REG(int warp_id)
{
    float f1;
    float *pa1;
    auto& hwarp = m_hw_warps[warp_id];
    while (true)
    {
        wait(clk.posedge_event());
        if (wb_warpid == warp_id)
        {
            // 后续regfile要一次只能写一个，否则报错
            if (write_s)
            {
#ifdef SPIKE_OUTPUT
                std::cout << "SM" << sm_id << " warp " << warp_id << " 0x" << std::hex << wb_ins.read().currentpc << std::dec
                          << " " << wb_ins
                          << " x " << std::setfill('0') << std::setw(3) << rdv1_addr.read() << " "
                          << std::hex << std::setw(8)
                          << rdv1_data[0]
                          << std::dec << std::setfill(' ') << std::setw(0)
                          << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
#endif
                if (rdv1_addr != 0)
                    hwarp->s_regfile[rdv1_addr.read()] = rdv1_data[0];
                //if (sm_id == 0 && warp_id == 2 && rdv1_addr == 2)
                //    std::cout << "Warning! " << "SM" << sm_id << " warp " << warp_id << " 0x" << std::hex << wb_ins.read().currentpc << std::dec
                //          << " " << wb_ins << " x " << std::setfill('0') << std::setw(3) << rdv1_addr.read() << " written as 0x" << std::hex << rdv1_data[0] << "\n";
            }
            if (write_v)
            {
#ifdef SPIKE_OUTPUT
                std::cout << "SM" << sm_id << " warp " << warp_id << " 0x" << std::hex << wb_ins.read().currentpc << std::dec
                          << " " << wb_ins
                          << " v " << std::setfill('0') << std::setw(3) << rdv1_addr.read() << " "
                          << std::hex << std::setw(8);
                for (int i = m_hw_warps[wb_warpid]->CSR_reg[0x802] - 1; i > 0; i--)
                    std::cout << rdv1_data[i] << " ";
                std::cout << rdv1_data[0];
                std::cout << std::dec << std::setfill(' ') << std::setw(0)
                          << "; mask=" << wb_ins.read().mask << ", s1=" << wb_ins.read().s1 << ",s2=" << wb_ins.read().s2 << ",s3=" << wb_ins.read().s3
                          << " at " << sc_time_stamp() << "," << sc_delta_count_at_current_time() << "\n";
#endif
                for (int i = 0; i < m_hw_warps[wb_warpid]->CSR_reg[0x802]; i++)
                    if (wb_ins.read().mask[i] == 1)
                        hwarp->v_regfile[rdv1_addr.read()][i] = rdv1_data[i];
            }
        }
    }
}
