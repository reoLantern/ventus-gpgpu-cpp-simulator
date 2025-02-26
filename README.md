Requirements:

- GCC version >= 11  
- enable C++20 support
- to use the latest release of SystemC is highly recommended

---

## Quick Start

配置SystemC可以参考我的[博文](https://zhuanlan.zhihu.com/p/638360098)（也参考了很多别人的经验，但这篇比较适合本工程）。

运行`make -j $(nproc)`编译程序。运行`make run`进行仿真测试。如果运行报错，可以先尝试`make clean`.

### Understanding Program Output in Our Project

This section is dedicated to explaining the output generated by our program, which is crucial for developers who wish to understand the inner workings or debug the software. The output is structured to provide detailed insights into the program's execution, including instruction addresses, operations on warp units, and register manipulations.

#### Warp Execution Output

The program output is like:

```plain
SM0 warp 3 0x80000194               JAL_0x28c000ef x 001 80000198 at 11135 ns,1
```

- **`SM0`** identifies the SM in action, **`warp 3`** identifies the warp unit in action, which start from 0.
- **`0x80000194`** specifies the virtual address of the instruction being executed.
- **`JAL_0x28c000ef`** represents the instruction itself.
- **`x 001 80000198`** signifies an operation where the program writes the value `80000198` to scalar register 001 of warp 3.

For a more complex example:

```plain
SM0 warp 1 0x80000114           VLW12_V_0x0000a0fb v 001 40e00000 40c00000 40a00000 40800000; mask=00001111, s1=1,s2=0,s3=1 at 12865 ns,1
```

- **`v 001 40e00000 40c00000 40a00000 40800000; mask=00001111`** indicates an operation on vector register 001, where `00001111` is a mask specifying that only the last 4 threads is active (due to branch divergence or kernel info).
- The data **`40e00000 40c00000 40a00000 40800000`** is written to the last 4 elements of the vector register due to the mask setting.

#### Jump Instructions

The output related to jump instructions follows this format:

```plain
SM1 warp 1 0x80000034               BEQ_0x00c50863 jump=true, jumpTO 0x80000044 at 925 ns,1
```

- **`jump=true, jumpTO 0x80000044`** indicates a conditional jump to the address `0x80000044` depending on the evaluation of the preceding condition.

### Debug

出现Segmentation fault的调试方法：  
参考[Linux下Segmentation Fault的定位方法](https://blog.csdn.net/whahu1989/article/details/110881842)、[linux下不产生core文件的原因](https://blog.csdn.net/qq_35621436/article/details/120870746)。  
调试步骤（调试完记得把core文件删除，文件太大上传到github会报错）：

```bash
make gdb
```

根据行号设置断点：

```text
(gdb) b 5
```

运行和继续

```text
运行 r
继续单步调试 n
继续执行到下一个断点 c
```

---

To configure SystemC, you can refer to my [blog post](https://zhuanlan.zhihu.com/p/638360098).

Run `make -j $(nproc)` to compile the program. Run `make run` to start simulation. If you encounters an error, you can try `make clean` command first.

---

## Acknowledgement

[ventus-gpgpu](https://github.com/THU-DSP-LAB/ventus-gpgpu), [GPGPU-Sim](https://github.com/accel-sim/gpgpu-sim_distribution)
