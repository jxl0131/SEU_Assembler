# Assembler

东南大学计算机科学与技术专业每届学生在毕业设计之前要做一个较为完整的计算机系统，从硬件到软件。
我负责汇编器部分，用C++实现了一个我所理解的汇编器。

## Notice

- 未采用编译器的词法语法分析模式。我认为汇编器比较简单，可以用全if结构来跳转实现将汇编语言翻译成二进制机器指令。
- 汇编器处于上层软件和下层硬件之间，编写汇编器的人需要向上和做编译器的同学沟通，知晓应该支持哪些编译器翻译出的汇编语言，一些优化交给编译器来做还是给汇编器来做。向下和底层硬件设计的人沟通，知晓寄存器等信息。

- .vscode是在ubuntu上的配置，需要有g++
- .vscode_winddows是在windows上的配置，我当时安装是mingw64，然后基于C:\\mingw64\\bin\\g++.exe编译的。如果你有VS，你也可以在VS自带的编译器上进行编译。使用时将"_windows"去除。
- 该汇编器在编写时充分考虑了耗时，采用了我认为最快速的一些函数、方法。

## Usage
- 在windows或其他操作系统下编译出可执行文件，例如Assembler.exe
- cmd执行：          ./Assembler.exe [YOUR_SRC_CODE_ASM_FILE.asm [YOUR_BOOT_ASM.asm] ]

Assembler.exe会自动在同目录下寻找源代码的汇编文件YOUR_SRC_CODE_ASM_FILE.asm，默认命名为"code.asm"。boot的汇编文文件（默认为boot.asm）需要加在YOUR_SRC_CODE_ASM_FILE.asm的末尾。都请放在同目录下。
-  输出:
    - 五个dmem32文件（总的+四个分割后的）、一个prgmip32文件，一个可供串口下载的out.txt文件
    - boot_link_code.asm是在汇编源代码尾部加boot链接后的文件。

