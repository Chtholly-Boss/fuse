#import "../template.typ":*
= 实验收获和建议
#quote[实验中的收获、感受、问题、建议等。]

== 收获与感受

本次实验属于一个设计性的实验，通过本次实验，我收获了很多宝贵的经验：
- 模块接口设计：在本次实验中，我慎重考虑了每个模块的接口设计，以确保模块之间的交互尽可能简单和清晰，尽量将模块之间的依赖关系最小化，以减少模块之间的耦合度。
- 信息存储设计：在本次实验中，我深入思考了如何什么信息是需要存储的，以及如何存储这些信息。具体来说，我学会了如何将信息进行结构化存储，以便于后续的查询和修改。
- 复杂度管理：在本次实验中，我学会了如何管理复杂度。随着实验的进行，工程中的代码结构越来越复杂，我学会了如何将复杂度分解为更小的部分，以便于管理和维护。

== 意见与建议

本实验改编自往年学长学姐们参加比赛的作品，从中可以看到很大的设计空间，就取材而言是一个很好的*系统设计实验*。

可惜的是，本次实验的参考代码和指导书均未很好地突出这一点。

就 Simplefs 的代码组织来讲
- 其将所有函数都放在 `sfs_utils` 文件中，没有进行任何组织，这导致代码十分难以理解。
- 其大量地使用的宏定义，这导致代码难以阅读和调试。
- 其函数的命名较为混乱，对于实体的创建，既有如 `new_dentry` 的命名，又有 `alloc_inode` 的命名。有趣的是，`alloc_dentry` 的功能并非创建一个目录项，而是将目录项注册到父目录中。

就指导书来讲
- 任务一作用有限。任务一在任务二中没有起到任何实质性的作用。其目的完全可以通过在 `fs_init` 打印 "hello fs" ，在 `fs_destroy` 打印 "goodbye fs" 来达到。既能让同学们看到 hook 函数的作用，又和任务二紧密相联。Anyway，当前指导书的任务一体现不出什么价值。
- 内容重复。#link("https://os-labs.pages.dev/lab5/part3/#321", "实验实现")与#link("https://os-labs.pages.dev/lab5/part6/", "选择阅读（补充）")  以及 Simplefs 的源码注释均为相同的内容。选择阅读（补充）的本意应该是将源码进行串接，但在指导书上更像是一个网页版的注释。
- 避重就轻：本次实验的磁盘结构最大的特点就是数据块不需要连续分配，可以按需索引，但实验的实现和测评均未对该部分进行侧重。具体来讲，在实现过程中不需要读写文件，那么就无需分配任何内存给文件，经实际验证也可通过测试，Ridiculous！

总的来说，我认为本次实验还是很有价值的，但指导书和参考代码还需要改进。
