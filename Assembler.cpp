#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <stack>
#include <vector>
#include <regex> //正则表达式，用于匹配
#include <cmath>
#include <ctime>
using namespace std;

//包含了mips32汇编、链接功能的综合汇编器
//作者:纪新龙
//时间:2021年12月30号
//使用方法:在windows或其他操作系统下编译出可执行文件，例如Assembler.exe
//cmd执行：          ./Assembler.exe test.asm
//会自动在同目录下寻找test.asm文件。如果有boot.asm需要加在test.asm的末尾，也请放在同目录下。
//输出:五个dmem32文件（总的+四个分割后的）、一个prgmip32文件，一个可供串口下载的out.txt文件

//设置一些隐性参数

long long break_code = 0;

long long syscall_code = 1;
int minint = -pow(2, sizeof(int) * 8 - 1);
int maxint = pow(2, sizeof(int) * 8 - 1) - 1;
// bool ifUseLinker = true;
//中断程序入口地址 0x 00008004 硬件跳转
//软件跳转

//64KB的ROM 4B*16384（16K=16384）行
//输入： .asm
//输出：dmem32.coe prgmip32.coe out.txt
int lineCounter = 0, lineCounterKeep, locationKeeper; //全局的汇编程序行数计数器，跟踪处理到的行数，报错时指出错误行
streampos rd2tag;                                     //记录.TEXT区的位置
map<string, int> syTable;                             //全局的符号表

string IntToString(int n, int sizeofbytes)
{
    //int型输出为16进制
    string ret;
    ret.resize(sizeofbytes * 8, '0');
    for (int i = ret.size() - 1; i >= 0 && n != 0; --i, n /= 16)
    {
        ret[i] = n % 16 + '0';
    }
    return ret;
}

long long StringToInt(string ns, int bitlength = 32)
{
    //5 、8 、16 、26 、32
    //可以判断输入的数字是否超出范围
    //string格式的十进制、十六进制理解成限制长度的二进制原码，然后取源码的正实值
    //例如： 限长4 0XFFFF->15 0XFF->3 5->0X101->5 -1->0XFFFF->15
    long long number = 0;
    char c;
    istringstream sin(ns);
    if (ns.empty())
    { //如果输入是空串，不允许，所以报错
        cerr << "line " << lineCounter << ":empty input as a number \'" << ns << "\'" << endl;
        exit(1);
    }
    //十六进制（长度小于32）直接用long long取出
    if (ns.length() >= 3 && ns[0] == '0' && ns[1] == 'X')
    {

        sin >> hex >> number;
        if (number > pow(2, bitlength) - 1)
        {
            cerr << "line " << lineCounter << ": number \'" << ns << " \' too large or too small " << endl;
            exit(1);
        }
    }
    else
    {
        sin >> dec >> number;
        if (number < -pow(2, bitlength - 1) || number > pow(2, bitlength - 1) - 1)
        {
            cerr << "line " << lineCounter << ": number \'" << ns << " \' too large or too small " << endl;
            exit(1);
        }

        if (bitlength < 32 && number < 0) //读入为负
        {
            number = pow(2, bitlength) + number;
        }
    }

    //从输入的ns前部提取不到合法的数字
    //或者：如果op的左半部分是可以吸收的数字，但是剩余一些字符不能被吸收
    if (sin >> c)
    {
        cerr << "line " << lineCounter << ":invalid number format \'" << ns << "\'" << endl;
        exit(1);
    }

    return number;
}

//initial processing the asm line
string StringToUpper(const string &str)
{
    string ret;
    for (auto i = 0; i != str.size() && str[i] != '#'; ++i)
    {
        //调节格式
        if (str[i] == ',')
        {
            ret += ' ';
            continue;
        }
        else if (str[i] == ':')
        {
            ret += ": ";
            continue;
        }
        else if (str[i] == '(' || str[i] == ')')
        {
            ret += " ";
            ret += str[i];
            ret += " ";
            continue;
        }

        ret += toupper(str[i]);
    }
    return ret;
}
//第一次扫描所有的变量标号名
int judgeAndProcValueOrLabel(string op, int location)
{
    //若有变量、标号名
    if (op.length() > 1 && op[op.length() - 1] == ':')
        if ((op[0] >= 'A' && op[0] <= 'Z') || op[0] == '_') //若变量、标号名合法
        {
            //将此变量、标号名及其在dmem、procmem中的位置记录到符号表中
            syTable[op.substr(0, op.length() - 1)] = location;
            cout << op << location << endl;
            return 0;
        }
        else //是变量、标号名但不合法
        {
            cerr << "line " << lineCounter << ":invalid value name" << endl;
            exit(1);
        }
    else
    {
        return 1; //不是变量、标号名
    }
}
//第二次扫描所有的寄存器
long long procRegister(string op)
{
    // op = "$12";
    // if (regex_match(op, regex("(\\$)([0-9]|[1-2][0-9]|3[01])")))
    //     cout << op.substr(1, op.length() - 1) << endl;
    //regex的速度有点慢
    if (op == "$ZERO")
        return 0;
    if (op.length() <= 1 || op.length() > 3)
    {
        cerr << "line " << lineCounter << ":invalid register name" << endl;
        exit(1);
    }
    //op的长度为2或者3
    if (op[0] != '$')
    {
        cerr << "line " << lineCounter << ":invalid register name" << endl;
        exit(1);
    }
    //op第一个必为$了
    if (op.length() == 2 && op[1] >= '0' && op[1] <= '9')
        return op[1] - '0';
    //op长度必为3了
    if (op.length() != 3)
    {
        cerr << "line " << lineCounter << ":invalid register name" << endl;
        exit(1);
    }
    if (op.length() == 3 && op[1] >= '0' && op[1] <= '9' && op[2] >= '0' && op[2] <= '9')
    {
        int t = stoi(op.substr(1, 2));
        if (t >= 32)
        {
            cerr << "line " << lineCounter << ":invalid register name" << t << endl;
            exit(1);
        }
        return t;
    }
    if (op == "$AT")
        return 1;
    if (op == "$GP")
        return 28;
    if (op == "$SP")
        return 29;
    if (op == "$S8" || op == "$FP")
        return 30;
    if (op == "$RA")
        return 31;

    switch (op[1])
    {
    case 'V':
        if (op[2] == '0' || op[2] == '1')
            return op[2] - '0' + 2;
    case 'A':
        if (op[2] >= '0' && op[2] <= '3')
            return op[2] - '0' + 4;
    case 'T':
        if (op[2] >= '0' && op[2] <= '7')
            return op[2] - '0' + 8;
        if (op[2] >= '8' && op[2] <= '9')
            return op[2] - '8' + 24;
    case 'S':
        if (op[2] >= '0' && op[2] <= '7')
            return op[2] - '0' + 16;
    case 'K':
        if (op[2] == '0' || op[2] == '1')
            return op[2] - '0' + 26;
    default:
        break;
    }
    //正确形式都捞完了，剩下全是错误的
    cerr << "line " << lineCounter << ":invalid register name" << endl;
    exit(1);
}
//第二次扫描.TEXT区，当一行指令的参数不够的时候报错
void throwParaLessError()
{
    cerr << "line " << lineCounter << ":parameter too less" << endl;
    exit(1);
}

int main(int argc, char **argv)
{
    int startt=clock();
    //默认输出
    // string sss;
    // int ss;
    // for (int i = 0; i < 10; ++i)
    // {
    //     cin >> sss;
    //     cin >> ss;
    //     cout << hex << (unsigned int)StringToInt(sss, ss) << endl;
    // }
    // return 0;
    string asmFilePath;
    string bootPath;
    string boot_link_code_path;

    cout << "assmebling...\n";
    if (argc == 1)
    {
        cout << "请在同目录下放置汇编源码code.asm以及boot源码boot.asm\n";
    }
    //缺省路径
    asmFilePath = "code.asm";
    bootPath = "boot.asm";
    boot_link_code_path = "boot_link_code.asm";

    if (argc >= 4)
    {
        cerr << "cerr: argument format error\n Try : ./Assembler.exe asmPath [boot.asm]\n";
        exit(1);
    }
    else if (argc == 3)
    {
        asmFilePath = argv[1];
        bootPath = argv[2];
    }
    else if (argc == 2)
    {
        asmFilePath = argv[1];
    }

    cout << "mood: link needed\n";
    ifstream linker_boot;
    ifstream linker_asmSrc;
    ofstream boot_link_code;
    linker_boot.open(bootPath, ios::in);
    linker_asmSrc.open(asmFilePath, ios::in); //Seek to end before each write.
    boot_link_code.open(boot_link_code_path,ios::out);
    if (!linker_boot.is_open() ||(!boot_link_code.is_open()))
    {
        cerr << "Warning : can not open boot.asm OR input asmFile OR boot_link_code.asm, which are needed by linker \n";
    }
    else
    {
        string lk;
        
        while (getline(linker_asmSrc, lk))
            boot_link_code<< lk << endl;
        while (getline(linker_boot, lk))
            boot_link_code<< lk << endl;

        linker_boot.close();
        linker_asmSrc.close();
        boot_link_code.close();
    }

    ifstream asmFile(boot_link_code_path);
    fstream dmemFile;
    //asmFilePath:asmPath "dmem32.coe":dmemPath "prgmip32.coe":prgmipPath "out.txt":outPath
    dmemFile.open("dmem32.coe", ios::out);
    ofstream prgFile("prgmip32.coe", ios::out);
    ofstream outFile("out.txt", ios::out);

    //4 bc d0
    //3 ba ce
    //2 b8 cc 差值14
    //- ac bf     13
    //果然，跟注释有关，由于中文注释的存在，无法用定位的方法二次扫描
    if (!(asmFile.is_open() && dmemFile.is_open() && prgFile.is_open() && outFile.is_open()))
    {
        cerr << "cerr : open assmbly File failed OR create output file failed \n";
        exit(1);
    }
    string temp;
    string op;
    string ns;
    int location; //跟踪变量在数据区填补到哪一行，指令在代码区填补到哪一行
    bool indata = 0, incode = 0;

    //一行一行读取、翻译
    while (getline(asmFile, temp))
    {
        lineCounter++;
        //operate the .DATA
        // cout<<StringToUpper(temp)<<endl;
        // continue;

        istringstream line(StringToUpper(temp));
        if (!indata && incode)
        { //暂存已经经过初始处理的汇编指令到out.txt中
            outFile << line.str() << endl;
        }

        // cout<<line.eof()<<endl<<line.tellg()<<endl;
        // if(line>>op)
        // cout<<op<<endl;
        // cout<<line.eof()<<endl<<line.tellg()<<endl;
        // if(line>>op)
        // cout<<op<<endl;
        // return 0;
        //测试结果：1、eof()具有预测性，能够预测当前文件有没有读到EOF。如果eof()结果为1了，下一次将读不到内容，同时tellg()返回-1.
        //         2、要注意的是，刚打开的文件流，即使文件没有内容，eof()=tellg()=0。必须读取一次，才能使其分别变成1和-1。
        line >> op;
        if (line.eof() && op != "NOP" && op != ".DATA" && op != ".TEXT") //如果这一行是空行，啥也没有，直接跳下一行
            continue;
        //此行非空，则分为下面几种情况
        else if (op == ".DATA") //情况1，非空.DATA伪指令行
        {
            dmemFile << "memory_initialization_radix = 16" << endl
                     << "memory_initialization_vector =" << endl;
            indata = 1;
            incode = 0;
            location = 0;
            if (line >> ns && StringToInt(ns) != 0)
            { //如果指定了数据区的起始位置,且不等于0
                location = StringToInt(ns);
                cout << "data_location指定值不为0，为" << hex << location << endl;
                location %= 16384;
                for (auto i = 0; i < location / 4; ++i)
                {
                    dmemFile << "00000000," << endl;
                }
            }
        }
        else if (op == ".TEXT") //情况2，非空.TEXT伪指令行
        {
            //.TEXT的出现意味着.DATA的结束，把dmem32.coe里的全零行补齐
            for (auto i = location / 4; i < 16384; ++i)
            {
                dmemFile << "00000000," << endl;
            }

            //修改相关记号量
            indata = 0;
            incode = 1;
            location = 0;
            lineCounterKeep = lineCounter; //记录行数
            //预填prgmip32.coe文件
            prgFile << "memory_initialization_radix = 16" << endl
                    << "memory_initialization_vector =" << endl;
            if (line >> ns && StringToInt(ns) != 0)
            { //如果指定了数据区的起始位置,且不等于0
                location = StringToInt(ns);
                locationKeeper = location; //保存代码区开始的位置
                cout << "program_location指定值不为0，为" << hex << location << endl;
                location %= 16384;
                for (auto i = 0; i < location / 4; ++i)
                {
                    prgFile << "00000000," << endl;
                }
            }

            continue;
            /* code */
        }
        else if (indata && !incode) //情况3，非空数据区
        {
            //[变量名:]  类型  初始值[,初始值[,初始值......]]
            if (judgeAndProcValueOrLabel(op, location) == 0) //若先前的op已经确认是合法的变量名，则需要再吸收一个新的op
                line >> op;

            //op进行变量类项匹配
            if (op == ".WORD")
            {
                if (location % 4 != 0)
                {
                    cerr << "line " << lineCounter << ":数据边界不对齐" << endl;
                    exit(1);
                }
                while (line >> ns)
                {
                    dmemFile << setfill('0') << setw(8) << hex << (unsigned int)StringToInt(ns, 32) << ',' << endl;
                    location += 4;
                }
            }
            else if (op == ".HALF")
            {
                if (location % 2 != 0)
                {
                    cerr << "line " << lineCounter << ":数据边界不对齐" << endl;
                    exit(1);
                }
                int c = 0;
                vector<int> vc;
                while (line >> ns)
                {
                    vc.insert(vc.begin(), StringToInt(ns, 16));
                    location += 2;

                    if (location % 4 == 0 && vc.size() == 2)
                    {
                        dmemFile << setfill('0') << hex << setw(4) << short(vc[0]) << setw(4) << short(vc[1]) << ',' << endl;
                        vc.clear();
                    }
                    else if (location % 4 == 0 && vc.size() == 1)
                    {
                        dmemFile << setfill('0') << hex << setw(4) << short(vc[0]) << ',' << endl;
                        vc.clear();
                    }
                }
            }
            else if (op == ".BYTE")
            {
                stack<int> st;
                while (line >> ns)
                {
                    st.push(StringToInt(ns, 8));
                    location++;
                    if (location % 4 == 0)
                    {
                        //输出
                        while (!st.empty())
                        {
                            dmemFile << setfill('0') << setw(2) << (signed int)(unsigned char)st.top();
                            st.pop();
                        }
                        dmemFile << ',' << endl;
                    }
                }
            }
            else
            {
                cerr << "line " << lineCounter << ":there should be proper value type, which can be one of \'.word\'\'.half\'\'.byte\'" << endl;
                exit(1);
            }
        }
        else if (!indata && incode)
        {
            //[标号:] 指令助记符 第 1 操作数 [, 第 2 操作数 [, 第 3 操作数]]  [# 注释]
            //[标号:]
            if (judgeAndProcValueOrLabel(op, location) == 0) //若先前的op已经确认是合法的标号名，则需要再吸收一个新的op
                line >> op;
            //计算行数
            //为了硬件上的流水线不受冲突，到现在为止发现的：jr、jalr前面放三条nop；mfhi，mflo，mfc0前面放两条nop；
            if (op == "JR" || op == "JALR")
                location += 16;
            else if (op == "MFHI" || op == "MFLO" || op == "MFC0")
                location += 12;
            else if (op == "PUSH" || op == "POP" || op == "JG" || op == "JL" || op == "JGE" || op == "JLE") //宏指令
                location += 8;
            else
            {
                location += 4;
            }

            continue;
        }

        //end
        //检测一行汇编语句有没有多余的内容，有的话报错
        if (line >> op)
        {
            cerr << "line " << lineCounter << ": \"" << op << "\" shouldn't  be here " << endl;
            exit(1);
        }

        // cout << op << endl;
    }

    asmFile.close();
    outFile.close();

    asmFile.open("out.txt", ios::in); //out.txt文件成为新的asm句柄
    //第二遍扫码.TEXT区域，处理指令本身

    //预填各个指令的op或func码
    map<string, int> insDif;
    //R型
    insDif["ADD"] = 32;
    insDif["ADDU"] = 33;
    insDif["SUB"] = 34;
    insDif["SUBU"] = 35;
    insDif["AND"] = 36;

    insDif["MULT"] = 24;
    insDif["MULTU"] = 25;
    insDif["DIV"] = 26;
    insDif["DIVU"] = 27;

    insDif["MFHI"] = 16;
    insDif["MFLO"] = 18;
    insDif["MTHI"] = 17;
    insDif["MTLO"] = 19;

    insDif["MFC0"] = 16 * 32;
    insDif["MTC0"] = 16 * 32 + 4;

    insDif["OR"] = 37;
    insDif["XOR"] = 38;
    insDif["NOR"] = 39;
    insDif["SLT"] = 42;
    insDif["SLTU"] = 43;

    insDif["SLL"] = 0;
    insDif["SRL"] = 2;
    insDif["SRA"] = 3;
    insDif["SLLV"] = 4;
    insDif["SRLV"] = 6;
    insDif["SRAV"] = 7;
    insDif["JR"] = 8;
    insDif["JALR"] = 9;
    insDif["BREAK"] = 13 + break_code * pow(2, 6);
    insDif["SYSCALL"] = 12 + syscall_code * pow(2, 6);
    insDif["ERET"] = 24 + 33 * pow(2, 25);
    //I型
    insDif["ADDI"] = 8;
    insDif["ADDIU"] = 9;
    insDif["ANDI"] = 12;
    insDif["ORI"] = 13;
    insDif["XORI"] = 14;
    insDif["LUI"] = 15;
    insDif["LB"] = 32;
    insDif["LBU"] = 36;
    insDif["LH"] = 33;
    insDif["LHU"] = 37;
    insDif["SB"] = 40;
    insDif["SH"] = 41;
    insDif["LW"] = 35;
    insDif["SW"] = 43;
    insDif["BEQ"] = 4;
    insDif["BNE"] = 5;

    insDif["BGEZ"] = 1 + 1024;
    insDif["BGTZ"] = 7 * 1024;
    insDif["BLEZ"] = 6 * 1024;
    insDif["BLTZ"] = 1024;
    insDif["BGEZAL"] = 17 + 1024;
    insDif["BITZAL"] = 16 + 1024;

    insDif["SLTI"] = 10;
    insDif["SLTIU"] = 11;
    //J-TYPE
    insDif["J"] = 2;
    insDif["JAL"] = 3;
    //宏指令

    //预填各个指令的op或func码
    map<string, int> ins;
    //R型
    ins["ADD"] = 0;
    ins["ADDU"] = 1;
    ins["SUB"] = 2;
    ins["SUBU"] = 3;
    ins["AND"] = 4;

    ins["MULT"] = 5;
    ins["MULTU"] = 6;
    ins["DIV"] = 7;
    ins["DIVU"] = 8;

    ins["MFHI"] = 9;
    ins["MFLO"] = 10;
    ins["MTHI"] = 11;
    ins["MTLO"] = 12;

    ins["MFC0"] = 13;
    ins["MTC0"] = 14;

    ins["OR"] = 15;
    ins["XOR"] = 16;
    ins["NOR"] = 17;
    ins["SLT"] = 18;
    ins["SLTU"] = 19;

    ins["SLL"] = 20;
    ins["SRL"] = 21;
    ins["SRA"] = 22;

    ins["SLLV"] = 23;
    ins["SRLV"] = 24;
    ins["SRAV"] = 25;

    ins["JR"] = 26;
    ins["JALR"] = 27;
    ins["BREAK"] = 28;
    ins["SYSCALL"] = 29;
    ins["ERET"] = 30;
    //I型
    ins["ADDI"] = 31;
    ins["ADDIU"] = 32;
    ins["ANDI"] = 33;
    ins["ORI"] = 34;
    ins["XORI"] = 35;
    ins["LUI"] = 36;

    ins["LB"] = 37;
    ins["LBU"] = 38;
    ins["LH"] = 39;
    ins["LHU"] = 40;
    ins["SB"] = 41;
    ins["SH"] = 42;
    ins["LW"] = 43;
    ins["SW"] = 44;

    ins["BEQ"] = 45;
    ins["BNE"] = 46;

    ins["BGEZ"] = 47;
    ins["BGTZ"] = 48;
    ins["BLEZ"] = 49;
    ins["BLTZ"] = 50;
    ins["BGEZAL"] = 51;
    ins["BITZAL"] = 52;

    ins["SLTI"] = 53;
    ins["SLTIU"] = 54;
    //J-TYPE
    ins["J"] = 55;
    ins["JAL"] = 56;
    lineCounter = lineCounterKeep; //使用之前保存的lineCounter，将lineCounter同步回归到.TEXT开头部位
    location = locationKeeper;
    while (getline(asmFile, temp))
    {
        lineCounter++;

        //operate the .DATA
        istringstream line(temp);

        line >> op;
        //防止NOP被错误地跳过
        if (line.eof() && op != "NOP") //如果这一行是空行，啥也没有，直接跳下一行
            continue;
        //此行非空，则分为下面几种情况
        else if (!indata && incode)
        {
            //[标号:] 指令助记符 第 1 操作数 [, 第 2 操作数 [, 第 3 操作数]]  [# 注释]
            //[标号:]
            if (op.length() > 1 && op[op.length() - 1] == ':')
            { //之前一遍扫描已经检查并处理过所有标号名了，这里仅需要略过标号名
                // cout << dec<<lineCounter << " " << op << endl;
                line >> op;
            }

            //用location表示当前指令的末尾（包括宏指令的情况）
            if (op == "JR" || op == "JALR")
                location += 16;
            else if (op == "MFHI" || op == "MFLO" || op == "MFC0")
                location += 12;
            else if (op == "PUSH" || op == "POP" || op == "JG" || op == "JL" || op == "JGE" || op == "JLE") //宏指令
                location += 8;
            else
            {
                location += 4;
            }

            //{T_RCOM}{T_RD}{T_COMMA}{T_RS}{T_COMMA}{T_RT}
            string rs, rt, rd, sel, shamt, imme, offset, addr;
            //先展开宏指令
            if (op == "NOP")
            {
                //相当于 sll  r0,r0,0
                cout << "NOP 00000000" << endl;
                prgFile << "00000000," << endl;
            }
            else if (op == "PUSH")
                if (line >> rt)
                {
                    cout << "ADDI 23bdfffc" << endl;
                    long long t = insDif["SW"] * pow(2, 26) + procRegister("$SP") * pow(2, 21) + procRegister(rt) * pow(2, 16);
                    cout << "SW"
                         << " " << insDif["SW"] << " " << setfill('0') << setw(8) << hex << t << endl;

                    prgFile << "23bdfffc\n"
                            << setfill('0') << setw(8) << hex << t << ',' << endl;
                }
                else
                    throwParaLessError();
            else if (op == "POP")
                if (line >> rt)
                {

                    long long t = insDif["LW"] * pow(2, 26) + procRegister("$SP") * pow(2, 21) + procRegister(rt) * pow(2, 16);
                    cout << "LW"
                         << " " << insDif["LW"] << " " << setfill('0') << setw(8) << hex << t << endl;
                    cout << "ADDI 23bd0004" << endl;
                    prgFile << setfill('0') << setw(8) << hex << t << ',' << endl
                            << "23bd0004\n";
                }
                else
                    throwParaLessError();
            else if ((op == "JG" || op == "JL"))
                if (line >> rt && line >> rs && line >> addr)
                {
                    if (op == "JL")
                        rt.swap(rs);
                    long long t = procRegister(rs) * pow(2, 21) + procRegister(rt) * pow(2, 16) + procRegister("$1") * pow(2, 11) + insDif["SLT"];
                    cout << "SLT"
                         << " " << insDif["SLT"] << " " << setfill('0') << setw(8) << hex << t << endl;
                    prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    long long tn;
                    if (syTable.count(addr))
                        tn = syTable[addr] / 4 - location / 4;
                    else
                        tn = StringToInt(addr, 20) / 4 - location / 4; //2^(16+2)
                    t = insDif["BNE"] * pow(2, 26) + procRegister("$1") * pow(2, 21) + tn;
                    cout << "BNE"
                         << " " << insDif["BNE"] << " " << setfill('0') << setw(8) << hex << t << endl;
                    prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                }
                else
                    throwParaLessError();
            else if ((op == "JLE" || op == "JGE"))
                if (line >> rt && line >> rs && line >> addr)
                {
                    if (op == "JGE")
                        rt.swap(rs);
                    long long t = procRegister(rs) * pow(2, 21) + procRegister(rt) * pow(2, 16) + procRegister("$1") * pow(2, 11) + insDif["SLT"];
                    cout << "SLT"
                         << " " << insDif["SLT"] << " " << setfill('0') << setw(8) << hex << t << endl;
                    prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;

                    long long tn;
                    if (syTable.count(addr))
                        tn = syTable[addr] / 4 - location / 4;
                    else
                        tn = StringToInt(addr, 20) / 4 - location / 4; //2^(16+2)
                    t = insDif["BEQ"] * pow(2, 26) + procRegister("$1") * pow(2, 21) + tn;
                    cout << "BEQ"
                         << " " << insDif["BEQ"] << " " << setfill('0') << setw(8) << hex << t << endl;
                    prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                }
                else
                    throwParaLessError();
            else if (ins.count(op))
                switch (ins[op])
                {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 23:
                case 24:
                case 25:
                    if (line >> rd && line >> rs && line >> rt)
                    {
                        long long t = procRegister(rs) * pow(2, 21) + procRegister(rt) * pow(2, 16) + procRegister(rd) * pow(2, 11) + insDif[op];
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;

                case 5:
                case 6:
                case 7:
                case 8:
                    if (line >> rs && line >> rt)
                    {
                        //mult $2,$3
                        rs = "$2";
                        rt = "$3";
                        long long t = procRegister(rs) * pow(2, 21) + procRegister(rt) * pow(2, 16) + insDif[op];
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;
                case 9:
                case 10:
                    if (line >> rd)
                    {
                        if (op == "MFHI" || op == "MFLO")
                        { //2 nop
                            cout << "NOP 00000000\nNOP 00000000" << endl;
                            prgFile << "00000000,\n00000000," << endl;
                        }
                        //mfhi $1
                        long long t = procRegister(rd) * pow(2, 11) + insDif[op];
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;
                case 11:
                case 12:
                case 26:
                    if (line >> rs)
                    {
                        if (op == "JR")
                        { //3 nop
                            cout << "NOP 00000000\nNOP 00000000\nNOP 00000000" << endl;
                            prgFile << "00000000,\n00000000,\n00000000," << endl;
                        }
                        long long t = procRegister(rs) * pow(2, 21) + insDif[op];
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;
                case 13:
                case 14:
                    if (line >> rt && line >> rd && line >> sel)
                    {
                        if (op == "MFC0")
                        { //2 nop
                            cout << "NOP 00000000\nNOP 00000000" << endl;
                            prgFile << "00000000,\n00000000," << endl;
                        }
                        int tn = StringToInt(sel);
                        long long t = insDif[op] * pow(2, 21) + procRegister(rt) * pow(2, 16) + procRegister(rd) * pow(2, 11) + tn;
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;
                case 20:
                case 21:
                case 22:
                    if (line >> rd && line >> rt && line >> shamt)
                    {
                        int tn = StringToInt(shamt);
                        long long t = insDif[op] + procRegister(rt) * pow(2, 16) + procRegister(rd) * pow(2, 11) + tn * pow(2, 6);
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;

                case 27:
                    if (line >> rd && line >> rs)
                    {
                        if (op == "JALR")
                        { //3 nop
                            cout << "NOP 00000000\nNOP 00000000\nNOP 00000000" << endl;
                            prgFile << "00000000,\n00000000,\n00000000," << endl;
                        }
                        long long t = procRegister(rs) * pow(2, 21) + procRegister(rd) * pow(2, 11) + insDif[op];
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;

                case 28:
                case 29:
                case 30:
                    cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << insDif[op] << endl;
                    prgFile << setfill('0') << setw(8) << hex << insDif[op] << ',' << endl;

                    break;
                case 31:
                case 32:
                case 33:
                case 34:
                case 35:
                case 53:
                case 54:
                    if (line >> rt && line >> rs && line >> imme)
                    {
                        long long tn = StringToInt(imme, 16);
                        int lk1 = procRegister(rs);
                        int lk2 = procRegister(rt);
                        long long t = insDif[op] * pow(2, 26) + procRegister(rs) * pow(2, 21) + procRegister(rt) * pow(2, 16) + tn;
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;
                case 36:
                    if (line >> rt && line >> imme)
                    {
                        long long tn = StringToInt(imme, 16);
                        long long t = insDif[op] * pow(2, 26) + procRegister(rt) * pow(2, 16) + tn;
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;
                case 37: //lb
                case 38:
                case 39:
                case 40:
                case 41:
                case 42:
                case 43:
                case 44:
                    char lcomma, rcomma;
                    if (line >> rt && line >> offset && line >> lcomma && line >> rs && line >> rcomma)
                    {
                        if (lcomma != '(' || rcomma != ')')
                        {
                            cerr << "line " << lineCounter << ": comma not match " << endl;
                            exit(1);
                        }
                        long long tn;
                        if (syTable.count(offset))
                            tn = syTable[offset];
                        else
                            tn = StringToInt(offset, 16); //offset可正可负，但是访问的是数据区，所以可以不为4的倍数
                        if (tn < 0)
                            tn = pow(2, 16) + tn;
                        long long t = insDif[op] * pow(2, 26) + procRegister(rs) * pow(2, 21) + procRegister(rt) * pow(2, 16) + tn;
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;
                case 45: //beq 寻址 相对转移 offset=imme/4 可正可负
                case 46:
                    if (line >> rs && line >> rt && line >> offset)
                    {
                        long long tn;
                        if (syTable.count(offset))
                            tn = syTable[offset] / 4 - location / 4;
                        else
                            tn = StringToInt(offset, 20) / 4 - location / 4; //2^(16+2)
                        int LK1 = procRegister(rs);
                        int LK2 = procRegister(rt);
                        if (tn < 0)
                            tn = pow(2, 16) + tn;
                        long long t = insDif[op] * pow(2, 26) + procRegister(rs) * pow(2, 21) + procRegister(rt) * pow(2, 16) + tn;
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;
                case 47: //bgez 相对转移 offset=imme/4
                case 48:
                case 49:
                case 50:
                case 51:
                case 52:
                    if (line >> rs && line >> offset)
                    {
                        long long tn;
                        if (syTable.count(offset))
                            tn = syTable[offset] / 4 - location / 4;
                        else
                            tn = StringToInt(offset, 20) / 4 - location / 4; //2^(16+2)
                        if (tn < 0)
                            tn = pow(2, 16) + tn;
                        long long t = insDif[op] * pow(2, 16) + procRegister(rs) * pow(2, 21) + tn;
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;

                case 55: //j 直接跳转 零扩展
                case 56:
                    if (line >> addr)
                    {
                        long long tn;
                        if (syTable.count(addr))
                            tn = syTable[addr] / 4;
                        else
                            tn = StringToInt(addr, 28) / 4;
                        long long t = insDif[op] * pow(2, 26) + tn;
                        cout << op << " " << insDif[op] << " " << setfill('0') << setw(8) << hex << t << endl;
                        prgFile << setfill('0') << setw(8) << hex << t << ',' << endl;
                    }
                    else
                        throwParaLessError();
                    break;
                default: //这种情况不会出现,因为进入此SWITCH的时候已经确认op是ins[]里的一个键了
                    break;
                }
            else //op不合法
            {
                cerr << "line " << lineCounter << ": \"" << op << "\" can not be recognized! " << endl;
                exit(1);
            }
        }
        //end
        //检测一行汇编语句有没有多余的内容，有的话报错
        if (line >> op)
        {
            cerr << "line " << lineCounter << ": \"" << op << "\" shouldn't  be here " << endl;
            exit(1);
        }
    }
    //.TEXT二次扫描结束，将prgmip32剩下部分填充
    for (auto i = location / 4; i < 16384; ++i)
    {
        prgFile << "00000000," << endl;
    }
    //切割dmem32.coe为四份
    dmemFile.close();
    //把分割成四个dmem32i.coe
    dmemFile.open("dmem32.coe", ios::in);
    ofstream dmem32_0File("dmem32_0.coe", ios::out);
    ofstream dmem32_1File("dmem32_1.coe", ios::out);
    ofstream dmem32_2File("dmem32_2.coe", ios::out);
    ofstream dmem32_3File("dmem32_3.coe", ios::out);
    //在vscode里调试的时候用相对路径不行，但是用命令执行是可以用相对路径的！
    dmem32_0File << "memory_initialization_radix = 16" << endl
                 << "memory_initialization_vector =" << endl;
    dmem32_1File << "memory_initialization_radix = 16" << endl
                 << "memory_initialization_vector =" << endl;
    dmem32_2File << "memory_initialization_radix = 16" << endl
                 << "memory_initialization_vector =" << endl;
    dmem32_3File << "memory_initialization_radix = 16" << endl
                 << "memory_initialization_vector =" << endl;
    string str;
    getline(dmemFile, str);
    getline(dmemFile, str);
    for (auto i = 0; i < 16384; ++i)
    {
        getline(dmemFile, str);
        dmem32_3File << str.substr(0, 2) << ",\n";
        dmem32_2File << str.substr(2, 2) << ",\n";
        dmem32_1File << str.substr(4, 2) << ",\n";
        dmem32_0File << str.substr(6, 2) << ",\n";
    }
    dmem32_0File.close();
    dmem32_1File.close();
    dmem32_2File.close();
    dmem32_3File.close();
    dmemFile.close();
    //生成out.txt
    asmFile.close();
    prgFile.close();
    ifstream inFile;
    string line;
    inFile.open("dmem32.coe", ios::in);
    outFile.open("out.txt", ios::out);
    getline(inFile, line);
    getline(inFile, line);
    for (auto i = 0; i < 16384; ++i)
    {
        getline(inFile, line);
        outFile << line.substr(0, 8);
    }
    inFile.close();
    inFile.open("prgmip32.coe", ios::in);
    getline(inFile, line);
    getline(inFile, line);
    for (auto i = 0; i < 16384; ++i)
    {
        getline(inFile, line);
        outFile << line.substr(0, 8);
    }
    inFile.close();
    outFile.close();
    cout << "asmembling ok" << endl;
    int endt=clock();
    cout<<oct<<endt-startt<<"毫秒"<<endl;
    return 0;
}
