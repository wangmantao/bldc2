# BLDC OF Nu

### 进展 
	- 目标：实现反电动势检测
		- All off 要改善，不能全部ＭＯＳ关断，会产生高电动势
		- 过０点检测时机(ADC比较)为，ＰＷＭ工作周期（ＯＮ时，或ＯＦＦ时）
		- 为了看清反电动势的情况，测试两相电流环与６端驱动波形关系
		- 查看示例，电路图中是什么检查０点并触发中断了；分不分检查未通电相，还是全部一起检查。
		- 中断处理程序是怎么实现哪一路产生的中断，再去驱动哪一路打开，还是workstep++
		　　　　ＦB口中断处理函数,处理硬件比较产生的中断信号，再相应设bHStatus相应的位置１:;
		- 新唐有没有ＡＤ触发采集功能, 保证在PWM ON/ON 间作ＡＤＣ数据采集
	
	- 目标：实现加速
		- 占空比不变，改变换相频率
			在规定的时间内实现提速，设定最低与最高的换相频率（参考正弦驱动换相时间）
			程序写法思路：全程时间／换相的频率步增＝多少次换相后改变换相频率
				设定为５ｓ加速时区
				最低换相频率（50mS), 最高换相频率（10ms)
		- 占空比变化，不变换相频率
				
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
	- 反电动势
		ＢＥＭＦ检测是在它过零时刻，而非其峰值。为了达到好的过０精度，在过０点时，ＢＥＭＦ信号必须有较大的斜率. ＢＥＭＦ的幅值(及斜率）随速度成比例增加。
		__8.2.2 转子速度测量__
		bldc BEMF信号是周期性的，其频率与电机转动频率成正比，比例系数是电机的极对数。Moto(rpm)=f(BEMF)*60/(极对数）
		转速测量是通过计算ＢＥＭＦ信号两人个连续过零点之间的时间间隔而得。"单步时间测量"计算法，TIM1 UPD ISR 中断管理　软年计数器　Ｐ３８７页
		__8.2.3 换相延时和退磁时间__
		为了电机达到最在效率
		在浮相电流到达０点后才可以对反电动势信号进行测量。		
		


		--------------
		通过ＢＥＭＦ过零检测，ＡＤＣ可以实现ＢＬＤＣ控制
		BEMF采样要求，在ＰＷＭ周期内某一刻实施ＡＤｃ采样，不仅必须在判断或开通时间内采样ＢＥＭＦ信号，而且必须在采样和切换ＰＷＭ开关时建立稳定的时间，以防止产生换相噪声。这一点可由stm8s adc 转换和ＴＩＭ１产生ＰＷＭ信号同步来完成。ＴＩＭ１通道４是用来触发ＡＤＣ转换的。
		Ｐ４０７页，图８－３６，图８－３７　ＴＩＭ１与ＡＤＣ同步
A/D 转换完成后，就会启动相关的中断服务程序，转换值和阀值进行比较，以确定过零点是否发生。
	  关断时间内的ＢＥＭＦ转换：阀值可以在程序内定义常量
	　开通时间内的ＢＥＭＦ转换：比较采样值和中性点电压。中性点电压可以由不同的ＡＤＣ转换通道所采集的总线电压来进行重建。

	 
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
