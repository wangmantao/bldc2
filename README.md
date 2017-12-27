# BLDC OF Nu

### 进展 
	- 目标：产生三相PWM
		- STM8S 是怎么产生三相的
			它首先用了换相函数(解析如下）:
			 入参－－１，当前的step; 2, 要输出的PWM值
			__过程：__
			用BKR禁止ＰＷＭ输出
			根据当前的step值，关闭相应的下管。（须要分step＆关闭对应逻辑表）
			根据当前的step值，设定相应的PWM定时值，并打开相应的下管
			启用PWM输出（PWM 表，参１:step值）（定义了与step序相应的PWM使能与极性寄存器）
		- 怎样定义新的上/下管动作
			找到示例６个管子ON/OFF 执行的地方
			仿同样的管脚命, 用Define 定义A_OFF
			用set_XXX /  clr_XXX 实现
			main函数测试on/off动作
			再写换相逻辑	
		- 上管的PWM_ON / OFF 通过什么方式切换的，要改变I/O定义吗？
			在换相中会执行相应的关断，用到了PWM_EN_TABLE, 该TABLE分６种情况设置CCER1/2寄存器
			CCER为caputure / comparetor 使能寄存器, 暂分析为关闭第一通道(TIM1_CCER1寄存器的CC1E=0)
			怎么知道输出口定义的类型 --- PWMIO_Init()已经定义了６个端口为ＰＰ输出类型
			那接下来看，怎么设置新唐的PWM通道的关闭方法
	- 目标：测试PWM波形p1.2
	- 目标：输出PWM
		- TimeBaseInit()
		
	- 目标 
		- 编译一个sample sample.hex
		- hex 通过nu-link download TO MCU 1. 编译一个sample. 得一个sample.hex

### Q&A
	- 怎么开启关断新唐的某个PWM
		利用输出掩码功能PMEN0/1/2/3/4/5	
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
	- 关于解放号的应对
		主要目的，完成铺面展示，及业务介绍
		铺面参照已经有的，copy
		业务方面，突出自定义，软件开发方向是工业化应用 
