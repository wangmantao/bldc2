关于noverton的实验：
# BLDC OF Nu

### 进展 

	- 目标2：完成一路PWM受控
		1. 引脚模式设置(push pull) -- GPIO_Init()
		2. 高级定时器配置 -- Tim1_init()
			- Tim1 是怎么控制三个引脚的。怎么与PC3关联的
			- Tim1 有OC1, OC2, OC3 每个OC有对应的引脚 (定时器输出）
			- 每个OC设置了哪些功能? 1,输出模式(PWM1)，
	- 目标1: 
		- 编译一个sample sample.hex
		- hex 通过nu-link download TO MCU 1. 编译一个sample. 得一个sample.hex

### Q&A
	- 听说串口只能烧BIN文件。
		- keil 自动输出bin。 [keil_bin](https://wenku.baidu.com/view/2307570c90c69ec3d5bb75c1.html)
### 相关软件
	- Nu-Link USB Driver 保存位置 ：C:\Program Files (x86)\Nuvoton Tools\Nu-Link_Keil
	 
