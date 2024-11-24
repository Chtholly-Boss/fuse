#import "./utils.typ":*
= 遇到的问题与解决方法
#hint[列出实验过程中遇到的主要问题，包括技术难题、设计挑战等。对应每个问题，提供采取的*解决方案*，以及解决问题后的效果评估。]

== 数据结构设计 
本实验主要涉及的数据结构为 `super_block`, `inode` 和 `dentry` 在内存和磁盘中的表示。

=== 内存中的数据结构

由于进程的堆栈空间有限，因此内存中的数据结构应当在支撑功能的前提下尽量简洁。具体来说，`super_block` 和 `inode` 的内存表示应当尽量简洁，以减少内存占用。`dentry` 的内存表示则相对复杂，因为需要维护目录树的结构。

经过慎重考虑，本系统的数据结构在参考 Simplefs 的基础上，移除了文件数据在内存中的表示，转而在需要时通过 `read` 和 `write` 操作从磁盘读取和写入文件数据，该设计基于以下考虑：
- 优化内存使用：文件数据在内存中的表示会占用大量内存，而实际上大部分时间文件数据并不需要被加载到内存中。
- 延迟分配：文件数据在需要时才被加载到内存中，可以避免在文件创建时立即分配大量内存。
- 责任转移：将文件读写的优化交由相应的系统调用实现，如使用 `mmap` 将文件数据映射到内存中。

=== 磁盘中的数据结构

对于磁盘的数据结构，本系统主要考虑以下因素：
- 空间效率：数据结构应当尽量紧凑，以减少磁盘空间的占用。
- 时间效率：数据结构应当尽可能减少磁盘 I/O 的开销。

经过慎重考虑，本系统通过维护磁盘的 `Inode Bitmap` 和 `Data Bitmap` 来管理磁盘空间，通过位图上的标记以及 `inode` 和 `dentry` 来限制对于磁盘空间的访问，从而实现空间和时间的优化。

== 接口设计
为提高代码的可读性和可复用性，需要对接口进行合理设计。具体来说，需要考虑以下因素：
- 接口的粒度：接口应当足够细，以便于复用和扩展。
- 接口的命名：接口的命名应当清晰、简洁，以便于理解和使用。
- 接口的参数：接口的参数应当尽量简洁，以便于调用。

经过精心设计，本系统的接口命名采取 `object_operation` 的形式，其中 `object` 表示操作的对象，`operation` 表示操作的具体行为。

#design_choice[
该设计本质上是在 C 语言中模拟方法调用，通过将第一个参数设置为对象，将后续参数设置为方法参数，从而实现方法的调用，即将 `object.method(param1, ...)` 转化为 `object_operation(object, method, param1, ...)` 的形式。
]

对于重复出现的逻辑，本系统尽可能将其提取为函数，以减少代码重复。如 `Bitmap::alloc` 函数封装了 `Bitmap::find_first_zero` 和 `Bitmap::set` 的逻辑，从而简化了磁盘空间分配的代码。

#clues.example[
  在 Simplefs 的 `sfs_utils.c` 中，可以看到 `sfs_alloc_inode` 与 `sfs_drop_inode` 中均出现了以下形式的代码：
  ```c
    for (byte_cursor = 0; byte_cursor < SFS_BLKS_SZ(sfs_super.map_inode_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((sfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                sfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }
  ```
  从实际出发，该函数不应关注 Bitmap 的分配逻辑。举例来说，Bitmap 即可通过找到第一个 0，再将其置 1 来实现分配，也可以通过在其内部维护一个指针，每次分配时将指针后移一位来实现。

  更进一步说，在本次实验中含有 `Inode Bitmap` 和 `Data Bitmap`，如果每次分配都需要 hand code 一次，那么代码的维护成本将会非常高。因此，本系统通过封装 `Bitmap::alloc` 函数，将 Bitmap 的分配逻辑抽象为函数，从而简化了代码的编写和维护。
]

== 效果评估
经过以上设计，本系统在扩展功能时，可以更加方便地添加新的接口，同时也可以更好地维护代码的可读性和可复用性。

仍以 Bitmap 为例，通过封装 `Bitmap::alloc` 函数，本系统在扩展功能过程中：
- 若需要申请新的数据块，只需调用 `Bitmap::alloc` 函数即可，无需关心 Bitmap 的分配逻辑
- 若需要修改 Bitmap 的磁盘空间占用，无需在功能代码中修改，只需修改 `Bitmap::alloc` 函数即可
