关于noverton的实验：
# BLDC OF Nu

### 进展 
	- 目标：测试PWM波形p1.2
	- 目标：输出PWM
		- TimeBaseInit()
		
	- 目标 
		- 编译一个sample sample.hex
		- hex 通过nu-link download TO MCU 1. 编译一个sample. 得一个sample.hex

### Q&A
	- 听说串口只能烧BIN文件。
		- keil 自动输出bin。 [keil_bin](https://wenku.baidu.com/view/2307570c90c69ec3d5bb75c1.html)
	- 快速分析代码（跳转: 定义／引用）
		- 用到了scope :cs f 表示find；s表示符号，g表示查找定义，c查找函数调用者，更多请 :help cscope.
			另外，改 :cs 为 :scs 则切分窗口显示将跳转到的查找
		- 自定义了快速查找脚本
			s.sh "*.c" "keyWord"  在当前目录中，相关类型的文件，查找关键词。
			比cscope更通用于各种要求，不局限于Ｃ的符号，定义
			与sumlime 全局查找，关注性更集中相关的类型文件；差在，不能快速打开并定位到文件关键行。

### 相关软件
	- Nu-Link USB Driver 保存位置 ：C:\Program Files (x86)\Nuvoton Tools\Nu-Link_Keil
	 
