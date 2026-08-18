/*
 * File: zh-CN.h
 * Version: v1.0.0
 * Owner: David William Bull
 * Created: 2026-08-18
 * Last Modified: 2026-08-18
 * Description: Simplified Chinese (zh-CN) text: the option reference and return codes, the message table, the interface fragments and label tables.
 * Dependencies: None
 * ISA: Scalar
 * Thread-safety: N/A
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */
#pragma once

// One of the two source files of PITC that are not ASCII, and so one of the two that can be mis-decoded:
// read in the system ANSI code page instead of UTF-8, every Han character and every fullwidth mark below
// arrives in these literals as the three characters of its UTF-8 encoding, and the interface ships as
// mojibake that compiles cleanly. CPU.vcxproj pins /utf-8 in both configurations; this backstops the switch
// the way each ISA unit backstops its /arch, at the file that needs it rather than at the project setting
// that grants it
static_assert(sizeof(L"中") == sizeof(cwchar[2]), "zh-CN.h must be read as UTF-8: /utf-8 is absent from this build.");

// Translator's note on the layout contracts this file keeps, all of them measured rather than described.
// Every Han character and every fullwidth punctuation mark below occupies TWO console columns, so each of
// these contracts is counted in rendered columns and not in wchar_t -- the two are the same number in every
// other language header of this project and in none of this one's lines:
//   The return-code column of the instructions begins at column 80, and the comment column that follows the
//   option descriptions at column 96, exactly as en-GB.h places them
//   wstrInterface[8] renders three cells of 8, 10 and 18 columns, because the rule under it in [9] and every
//   results row in [18]~[21] are drawn to those widths; [9] carries that rule and may not be re-spaced
//   [1] and [5] are the two halves of the banner and hold their fields at one width, as do [2] and [6]
//   [0] and [4] head the two label lists at one width, and that width may not exceed seven columns: the
//   banner pads only three of its four-column label slots, so a run selecting four -- 'Ia123', or 'B Spt'
//   on the sync line -- reaches the prefix plus sixteen, and a prefix of eight lands that on column 24,
//   where the tab after it steps to 32 while the other line is still stepping to 24. Six columns here
//   [13]~[21] are pure format: their conversions are fixed by the lanes of the unit each one reports
//   wstrUnitsCPU and wstrSyncCPU render exactly three columns per label, which one Han character cannot be
//   and two cannot fit, so each translated label is one Han character and one narrow character: a digit for
//   the cache levels, a space for the sync shapes. Those spaces are the label's third column and are
//   load-bearing (see the note above those two tables)
// Two rules on which characters may be written here, both of them invariants a later edit can break without
// any run reporting it:
//   Nothing in this file may be an East-Asian Ambiguous-width character -- the curly quotes, the ellipsis,
//   the em dash, the degree and multiplication signs. Those render one column under a Latin console and two
//   under a CJK one, so a single one of them in a width-critical string makes the layout depend on who is
//   reading it. Every character below is Wide or Fullwidth, which is two columns everywhere
//   Nothing in this file may fall outside CP936. Unless 'O8' or 'O16' asks otherwise, the results file is
//   written through WideCharToMultiByte in the system ANSI code page -- not the console's, which is forced
//   to UTF-8 -- and wstrMessage[46] reports the characters that had to be written as substitutes. On the
//   machines this language is meant for that code page is 936, so one character outside it makes the warning
//   a fixture of every ANSI results file they write. Only the tables that reach wstrOutput are converted,
//   but the rule is held over the whole file so that it needs no per-table exception

inline al64 cwchptrc wstrInstructions_Chinese =
L"\nPulsed Integrity Tests for CPUs v1.1   ---   Copyright (c) David William Bull\n"
 "\n返回值"
 "\n------"
 "\n-1  : 未找到存放正确值的文件                                                    0 : 稳定性测试顺利完成"
 "\n-2  : 文件中的输入条目数量不足"
 "\n-3  : 文件中的输出条目数量不足                                                  1 : 正确值已成功保存至文件"
 "\n-4  : 生成正确值时检测到计算错误"
 "\n-5  : 无法创建或替换存放正确值的文件                                            2 : 已在控制台显示使用说明"
 "\n-6  : 未能将全部正确的输入值写入文件"
 "\n-7  : 未能将全部正确的输出值写入文件"
 "\n-8  : 结果文件的文件名无效"
 "\n-9  : 无法创建结果文件"
 "\n-10 : 未能写入结果文件"
 "\n-11 : 处理器不支持所请求的处理单元"
 "\n-12 : 请求了多个线程同步选项"
 "\n-13 : 请求的测试时长小于或等于零"
 "\n-14 : 请求的脉冲开启时长为零"
 "\n-15 : 未请求任何处理单元"
 "\n-16 : 请求了多个非 ALU 处理单元"
 "\n-17 : 无法分配所请求的内存量"
 "\n-18 : 每线程内存不足以支持所请求的处理单元"
 "\n-19 : 无法创建计算线程"
 "\n-20 : 无法写入 \"cpu.values\" 的文件头"
 "\n-21 : \"cpu.values\" 的内容对本次编译无效"
 "\n-22 : 某个作业内核与它必须重现的内核不一致"
 "\n-23 : 无法枚举本系统的处理器拓扑"
 "\n-24 : 命令行选项的值缺失、格式错误或超出范围"
 "\n-25 : 无法识别的命令行选项"
 "\n-26 : 所选的核心映射不含任何可测试的核心"
 "\n-27 : 系统未报告所请求的缓存级别\n"
 "\n命令行选项   ---   示例：pitc.exe I3x Spt Tcd8.0t3600 Ua"
 "\n----------"
 "\n 选项按给出的顺序生效：若其中两个设置同一个属性，以最后一个为准。"
 "\n 'B' 与各预设会重置处理单元与内存配置，因此请把它们写在任何 'I' 或 'M' 选项之前。"
 "\n 扫描式脉冲的运行没有关闭时长，因此 ']' 无论写在哪里都会被忽略。"
 "\n 脉冲模式（不给 '[' 也不给 ']' 时选中）默认开启 100ms、关闭 900ms。"
 "\n 每次测试都以 \"cpu.values\" 文件为准评定，因此请在一次编译的首次测试之前用 'W' 生成它。\n"
 "\n B  : 运行基准测试。写在 'B' 之后的选项会覆盖默认值；例如 pitc.exe B Iaf mt1024"
 "\n      默认使用每个虚拟核心的 ALU 与最宽的向量单元、每线程 8MB 内存、并行的恒定计算、2000ms 启动延迟，"
 "\n      以及 60 秒的时长。"
 "\n Ix : 设置指令使用选项。指定要使用哪些单元。选项可以叠加；例如 I2av"
 "\n      缓存：1==一级、2==二级、3==三级                                                           |  取所给出的最高缓存级别"
 "\n      处理：A==ALU、F==FPU、S==SSE2、V==AVX、X==AVX512                                          |  F、S、V 与 X 互斥"
 "\n         至少需要一个处理单元；缓存级别本身不指明任何单元，可以省略"
 "\n         缓存级别会据此设定每线程的内存量：共用该级别同一个实例的所有选定线程，其内存块合起来填满该级别，"
 "\n         并一起溢出低一级的缓存。'M' 选项会覆盖据此推导的大小；本系统未报告的级别会以 -27 拒绝，"
 "\n         而不是改用其他大小来测试"
 "\n Lx : 设置界面语言。"
 "\n      语言代码会不分大小写地与本次编译所带的代码比对：en-GB、en-US、fr-FR 与 zh-CN；例如 Lzh-CN"
 "\n         无法识别的代码会给出警告，并保持语言不变"
 "\n Mx : 设置测试期间要使用的内存量。数值以 MiB 为单位；例如 Mt128"
 "\n      C==每个虚拟核心、N==每个第一类核心、S==每个第二类虚拟核心、T==在所有虚拟核心间分摊的总量"
 "\n         两个核心类别是处理器的非 SMT 核心与 SMT 核心；在混合式处理器上则是它的能效核心与性能核心"
 "\n         'N' 与 'S' 各自只覆盖一个类别：若处理器两类核心兼有，请两个都给出，否则未获内存的那一类会被拒绝"
 "\n         'M' 会覆盖 'I1'、'I2' 或 'I3' 推导出的大小；超出该级别驻留窗口的大小会给出警告"
 "\n Ox : 结果文件的输出选项。文件名可以与其余任一选项叠加；例如 O[results.txt]16"
 "\n      []=文件名、A=非 UTF 的 ASCII、8=UTF-8、16=UTF-16"
 "\n Sx : 设置核心同步选项。前三个选项（P、R、S）之一可与最后一个（T）叠加；例如 Spt"
 "\n      P==并行、R==轮转、S==交错、T==时间同步                                                    |  P、R 与 S 互斥"
 "\n         'T' 会对齐所有线程的脉冲边沿。不给出它时，并行运行会把每个线程偏移一个周期的随机比例"
 "\n Tx : 设置计时选项。前三个选项（C、F、S）之一可与其余任一个（D、T、[、]）叠加；例如 Tfd1.0t12.5[100]2400"
 "\n      C==恒定、F==定长脉冲、S==扫描长度脉冲                                                     |  取 C、F、S 中最后给出的一个"
 "\n      全局选项：Dx==设置启动延迟、Tx==设置测试时长                                              |  把 'x' 换成小数值；例如 d10.0"
 "\n      定长脉冲选项（毫秒）：[x==开启时长、]x==关闭时长                                          |  把 'x' 换成整数；例如 [250"
 "\n      扫描长度脉冲选项（毫秒）：[x==周期时长                                                    |  扫描没有关闭时长"
 "\n         每个周期都从空闲开始，占空比呈直线上升，到测试时长结束时达到 100%"
 "\n Ux : 设置核心使用选项。前两个选项（C、T）之一可与其余之一（A、E、O）叠加；例如 Uc!.!!...!a"
 "\n      C==要使用的物理核心的二进制序列映射、T==要使用的虚拟核心的二进制序列映射"
 "\n         核心停用：'.' ',' '_' '-' '0'  |  核心启用：'!' '*' '#' '+' '1' 'x' 'X'  |  其他任何字符都会结束该映射"
 "\n         该映射就是全部选择：它没有列出的核心不会被使用，空的选择会被拒绝"
 "\n         'C' 按顺序给物理核心编号，一个处理器组接着一个。'T' 给每个处理器组 64 个字符，"
 "\n         无论该组带有多少虚拟核心，因此该组最后一个核心之后的字符是填充，但仍必须写出才能到达下一组；"
 "\n         线程位图打印每个组时用的是该组自己的宽度，而不是 64"
 "\n      A==对称多线程；强制使用每个活动物理核心的所有虚拟核心"
 "\n      E==每个活动物理核心只使用它的第一个虚拟核心、O==每个活动物理核心只使用它的最后一个虚拟核心"
 "\n         两者都为每个活动物理核心保留一个虚拟核心，无论其 SMT 宽度为何；只带一个虚拟核心的核心两者都会保留"
 "\n W  : 写入新的 \"cpu.values\" 文件。"
 "\n      文件先以 \"cpu.values.tmp\" 之名构建，完成之后才移动到位，因此中断的运行会让此前的 \"cpu.values\""
 "\n      保持原样。"
 "\n      只有当结果的完整性通过 65,536 次迭代后，才会创建该文件。"
 "\n      全部 512 个条目都会被验证，而不是每线程一个，因此这项检查耗时以分钟计，而非以秒计。"
 "\n      作业内核会先被交叉核对：每个内存内核与组合内核对照它自己单元的寄存器驻留内核，每个向量内核逐通道"
 "\n      对照 FPU 内核，正是这一点让该文件能在另一种向量宽度的处理器上读取。"
 "\n -x : 配置预设。默认会使用 ALU 与最宽的向量单元，以及每核心 8MB 内存。"
 "\n      1==恒定压力；每个物理核心一个线程。时长 10 分钟"
 "\n      2==对所有虚拟核心施加恒定压力。时长 30 分钟"
 "\n      3==定长轮转脉冲压力；每个物理核心一个线程。时长 10 分钟"
 "\n      4==同步的定长脉冲压力；每个物理核心一个线程。时长 10 分钟"
 "\n      5==对所有虚拟核心施加同步的定长脉冲压力。时长 30 分钟"
 "\n      6==扫描长度脉冲压力；每个物理核心一个线程。时长 30 分钟"
 "\n      7==对所有虚拟核心施加同步的扫描长度脉冲压力。时长 30 分钟"
 "\n      8==交错的定长脉冲压力；每个物理核心一个线程。时长 1 小时"
 "\n      9==对所有虚拟核心施加同步且交错的定长脉冲压力。时长 4 小时"
 "\n      0==对所有虚拟核心施加同步的定长脉冲压力，使用 ALU 与 SSE 代码路径，每核心 2MB 内存。时长 1 小时\n\n";

inline cwchptrc wstrMessage_Chinese[47] = {
   L"\n结果已成功写入 \"%s\" 文件。\n\n",
   L"\n\n已生成新的 \"cpu.values\" 文件。\n\n",
   L"\n\n未找到 \"cpu.values\" 文件。请用 'W' 命令行选项生成。\n\n",
   L"\n\n\"cpu.values\" 文件中的输入条目数量不足。\n\n",
   L"\n\n\"cpu.values\" 文件中的输出条目数量不足。\n\n",
   L"\n\n检测到计算错误。\"cpu.values\" 未写入。\n\n",
   L"\n\n无法创建 \"%s\" 文件。\n\n",
   L"\n\n未能将全部输入条目写入 \"cpu.values\" 文件。\n\n",
   L"\n\n未能将全部输出条目写入 \"cpu.values\" 文件。\n\n",
   L"\n参数 \"%s\" 中没有有效的结果文件名；应为 'O[名称]'。\n\n",
   L"\n\n无法创建结果文件 \"%s\"。\n\n",
   L"\n\n未能将结果写入 \"%s\" 文件。\n\n",
   L"\n本系统的处理器核心不支持 SSE2 指令集。\n",
   L"\n本系统的处理器核心不支持 AVX 指令集。\n",
   L"\n本系统的处理器核心不支持 AVX512F 指令集。\n",
   L"\n'S' 选项 P、R 与 S 中只能有一个生效；它们互斥。\n",
   L"\n测试时长必须大于零。\n",
   L"\n脉冲开启时长必须大于零。\n",
   L"\n必须通过 'I' 选项至少选中一个处理单元；例如 Ia\n",
   L"\n'I' 选项 F、S、V 与 X 中只能有一个生效；它们互斥。\n",
   L"\n每线程只有 %lld 字节内存；所请求的处理单元至少需要 %lld 字节。\n",
   L"\n请求了 %lld MB 内存，但只有 %lld MB 可用。\n",
   L"\n无法分配 %lld MB 内存。\n",
   L"\n无法创建计算线程 #%d。\n\n",
   L"\n警告：无法把线程 #%d 固定到某个核心；它将在调度器安排的位置上运行。\n",
   L"\n\n未能写入 \"cpu.values\" 文件的文件头。\n\n",
   L"\n\n\"cpu.values\" 不是 PITC 的值文件。请用 'W' 命令行选项生成。\n\n",
   L"\n\n\"cpu.values\" 的格式版本为 %u；本次编译读取的是版本 %u。请用 'W' 重新生成。\n\n",
   L"\n\n\"cpu.values\" 由另一次编译或另一组作业内核生成。请用 'W' 重新生成。\n\n",
   L"\n\n\"cpu.values\" 已损坏；其内容与文件头中的哈希值不符。请用 'W' 重新生成。\n\n",
   L"\n\n%s 内核与它自己单元的寄存器驻留内核不一致。\"cpu.values\" 未写入。\n\n",
   L"\n无法枚举本系统的处理器拓扑；错误代码 %u。\n\n",
   L"\n系统未报告任何处理器核心；没有可测试的对象。\n\n",
   L"\n警告：本系统有 %d 个虚拟核心；本次编译最多测试 %d 个，因此有 %d 个不会被测试。\n",
   L"\n混合式处理器：%d 个性能核心，SMT 为 %d 路；%d 个能效核心，SMT 为 %d 路。\n"
    "  两个核心类别指的是这两者，而不是非 SMT 核心与 SMT 核心，因此 'Mn' 与第一条缓存记录\n"
    "  描述的是能效核心，'Ms' 与第二条描述的是性能核心。\n",
   L"\n'%c' 选项（在参数 \"%s\" 中）需要一个 %lld 到 %lld 之间的整数。\n\n",
   L"\n'%c' 选项（在参数 \"%s\" 中）需要一个 %.1f 到 %.1f 之间的小数值。\n\n",
   L"\n无法识别的命令行参数 \"%s\"。不带参数运行可显示选项参考。\n\n",
   L"\n无法识别的 '%c' 选项，出现在参数 \"%s\" 中。不带参数运行可显示选项参考。\n\n",
   L"\n警告：本次编译不带语言 \"%s\"；界面语言保持不变。\n",
   L"\n'U' 核心映射未选中任何核心；没有可测试的对象。\n\n",
   L"\n\n%s 内核并未逐元素地重现 JobFPU 的结果，因此在此写出的 \"cpu.values\" 无法在另一种向量宽度\n"
    "  的处理器上读取。\"cpu.values\" 未写入。\n\n",
   L"\n\n无法替换 \"%s\" 文件；此前的文件已原样保留。\n\n",
   L"\n本系统未报告 %u 级缓存，因此无法在此按该级别设定测试的规模。\n\n",
   L"\n警告：在此线程数下，%u 级工作集无法保持驻留；将以能溢出低一级缓存的最小尺寸运行。\n"
    "  最多只能让 %u 个线程在每个 %u 级缓存实例中保持驻留，因此请选中更少的核心来测试\n"
    "  该级别本身。\n",
   L"\n警告：所请求的每线程内存超出了第 %u 级缓存 %llu ~ %llu KiB 的驻留窗口（针对第 %u 类核心），\n"
    "  因此本次运行并未被限制在该缓存级别之内。\n",
   L"\n警告：\"%s\" 文件的 ANSI 编码无法表示本语言的全部字符，因此那些字符已被写成替代字符。\n"
    "  若要保留它们，请改用 'O8' 或 'O16'。\n"
};

inline cwchptrc wstrInterface_Chinese[22] = {
   L"单元：",
   L"\t已分配内存：%3lld MB\t启动延迟：%7d ms",
   L"\t脉冲开启时长：%d ms",
   L"\t周期时长：%d ms",
   L"\n同步：",
   L"\t  线程数量：%-3d   \t最大时长：%5.1f s",
   L"\t脉冲关闭时长：%d ms",
   // The characters of [7] after its last newline are measured at run time: they are the hanging indent every
   // thread-bitmap row after the first is given, and the amount trimmed after the last. That measure counts
   // wchar_t, and every Han character of this language is one wchar_t rendered in two columns, so a label
   // left on the bitmap's own line would indent the second and later processor groups by half its width.
   // The label is therefore closed with a newline of its own: the indent measures zero, every group starts
   // at column zero, and no row can be out of true. A label ending in a tab is forbidden for the same reason
   L"\n\n线程位图：\n",
   L"\n  线程  | 处理单元 | 正确值           ",
   L"| 结果\n--------+----------+--",
   L"\nPITC 基准测试得分：%lld KUPS（每秒 Kibi 单元数）",
   L"\n错误！核心 %2.1lld  期望值： ",
   L"实测值：",
   // [13]~[17]: the value line Failed() prints after [11], one per processing unit, in the order of the 'unit'
   // argument that selects them: 0==AVX-512, 1==AVX, 2==SSE, 3==FPU, 4==ALU. Each carries the expected lanes,
   // then [12], then the observed lanes, and the specifier count of each is fixed by the lanes of its unit --
   // sixteen, eight, four, two and two conversions respectively, in that order and no other. The separator
   // [12] between the two halves is the only part of these five a language writes
   L"%1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  %s %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n",
   L"%1.9f, %1.9f, %1.9f, %1.9f  %s %1.9f, %1.9f, %1.9f, %1.9f\n",
   L"%1.9f, %1.9f  %s %1.9f, %1.9f\n",
   L"%1.9f  %s %1.9f\n",
   L"%lld  %s %lld\n",
   // [18]~[21]: one results-table row per value width -- 64-bit, 128-bit, 256-bit and 512-bit. They sit here
   // because their cells have to line up under the column headers of [8] and [9], which a language owns: the
   // thread number, the ProcUnit cell and the two value cells are one layout with those headers. Every row
   // renders the ProcUnit cell 10 characters wide, and the rule below [9] is drawn to that width, so a row
   // that renders it otherwise puts the table's own separator out of true.
   // [18] serves both units of the 64-bit width and takes their name from wstrUnitsCPU; the three vector rows
   // name their unit in the literal, and this language leaves wstrUnitsCPU[0]~[4] spelled as en-GB spells them
   L"\n  #%3.1d  |  %s 64  | %16.16llX | %16.16llX | %s",
   L"\n  #%3.1d  | SSE  128 | %16.16llX%16.16llX | %16.16llX%16.16llX | %s",
   L"\n  #%3.1d  | AVX  256 | %16.16llX%16.16llX%16.16llX%16.16llX | %16.16llX%16.16llX%16.16llX%16.16llX | %s",
   // Sixteen conversions and their separators are 224 columns on one line, against the 180 GCS e2 makes a hard
   // cap; a wide literal is the one token here that cannot be broken any other way, so the row is joined by
   // concatenation exactly as the multi-line messages above are
   L"\n  #%3.1d  | AVX  512 | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX"
    " | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %s"
};

// The bit-indexed label tables. Both are indexed by the bit position of the property they name, so their order
// is fixed by GLOBAL_CFG's bit-fields and not by this file, and every entry must be present in every language.
// Their array type is what holds them to three characters: the banner prints each selected label in a slot of
// four columns and pads the unused slots with four spaces each, and the results table gives wstrUnitsCPU a
// fixed cell, so a wider label would silently shift every column to its right (see translations.h)
// Three columns is a width no Han text falls on: one character renders two of them and two characters render
// four. Every label this language translates is therefore one Han character and one narrow character. The
// five processing units keep the names their instruction sets carry in every language; the three cache
// labels spend their third column on the level number, and the sync shapes below spend theirs on a space --
// a space that is the label's third column, and that a run reports nothing about if it is deleted
inline cwchar wstrUnitsCPU_Chinese[8][4] = { L"ALU", L"FPU", L"SSE", L"AVX", L"512", L"缓1", L"缓2", L"缓3" };
// 轮转, 并行, 交错, 同步, 恒定, 定长脉冲, 扫描长度脉冲, 基准测试 -- the head character of each, and the space
// that makes its third column. Staggering is 交错 and not the equally good 错开 for one reason: this table
// prints its labels one character wide, and a lone 错 is the head of 错误 -- the word this program opens
// every failure with -- so a staggered run would announce itself in the vocabulary of a fault
inline cwchar wstrSyncCPU_Chinese[8][4]  = { L"轮 ", L"并 ", L"交 ", L"同 ", L"恒 ", L"定 ", L"扫 ", L"基 " };
// The verdict of one results row, indexed by Evaluate: 0==every lane matched, 1==at least one did not.
// Nothing follows this cell on its row, so it is the one label table a language may render at its own width
inline cwchar wstrPass_Chinese[2][8]     = { L".通过.", L"!失败!" };
