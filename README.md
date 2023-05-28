# uC/OS II课程大作业 电梯控制系统

- [uC/OS II课程大作业 电梯控制系统](#ucos-ii课程大作业-电梯控制系统)
    - [文件说明](#文件说明)
    - [log](#log)
    - [任务分配](#任务分配)
    - [串口通信协议](#串口通信协议)
    - [请求判优算法](#请求判优算法)


### 文件说明

`Client`文件夹内为上位机程序

`elevator`文件夹内为单片机程序    

### log

* V1.0 使用stm32F401 ，LCD调通，正在驱动矩阵键盘
  	遇到问题：uC/OS系统下按键扫描应该放在什么地方...
* V1.1 更改方案：使用F103ZET6，移植太难了，打算制作一个软件上位机，和单片机串口通信

* 2023.5.3：~~狠狠拿下~~  拿下不了一点，单片机部分实现了，开始做上位机程序

### 任务分配

* 任务0：
  * 从上位机接收指令
  * 从指令中提取数据

* 任务1：
  * 控制电梯运动状态
  * 将当前楼层及运动状态发送至上位机

* 任务2
  * 将上位机发送的请求进行优先级排序，发送到任务1

### 串口通信协议

根据实际需求，设计两种协议，分别模拟在电梯舱内乘客以及电梯舱外乘客的操作，具体内容如下：

* 指令1：电梯舱外乘客请求指令，根据实际需求，电梯舱外乘客需要向系统发送“乘客当前所在楼层”及“上行/下行”两个数据，以“8层楼有人要上楼”为例（0x00为下行、0x01为上行），如表1所示。若发送成功，系统会通过串口返回“x楼层有人要上楼/下楼”。

  | 帧头 | 所在楼层 | 上/下行 |   帧尾    |
  | ---- | -------- | ------- | :-------: |
  | 0x01 | 0x08     | 0x01    | 0x0D+0x0A |

  示例：01 08 01 0D 0A		8层有人要上楼

* 指令2：电梯舱内乘客请求指令，根据实际需求，进入舱内的乘客需要选择自己需要前往的楼层，因此只需要向系统发送目的楼层一个数据，若目的楼层为八层，则该协议如表2所示。若发送成功，系统会返回“目的楼层 x”。

  | 帧头 | 目的楼层 | 帧尾      |
  | :--: | -------- | --------- |
  | 0x02 | 0x08     | 0x0D+0x0A |

  示例：02 08 0D 0A			电梯内用户要去8层
  
  

### 系统返回消息

1) 某楼层有乘客要乘坐电梯时（指令1），系统返回：“x 楼有人要上楼/下楼”。

2) 乘客到达电梯内，选择需要前往的楼层（指令2），系统返回：“目标楼层 x”。

3) 电梯到达需要停留的楼层时返回：“电梯已到达 x 层”。

4) 电梯经过不需要停留的楼层时返回：“电梯当前运行至第x层，方向为向上/向下”。

### 请求判优算法

​		根据电梯实际运行逻辑，需要一个算法对当前发起请求的楼层进行判优处理，结合楼层的高低、当前电梯的位置及运行方向以及楼层发起请求的先后次序等数据，计算出电梯下一步应当前往的楼层，设计方案如下：

1. 由于在判优逻辑中每个楼层的上行和下行并不是相邻的，例如在电梯上行的过程中，某一即将经过的楼层的上行请求会优先被响应，而同楼层下行请求会在电梯下行回到此楼层时才响应。因此，本设计将同一楼层的上行、下行看作两个楼层，将各个楼层按照表3方式重新进行排列（假设共有4层楼）。应注意，表中的负数楼层并不代表实际楼层，而是代表电梯以下行方向前往该楼层（下文中所提及“楼层”均包括楼层号以及特定的运动方向）。经过实际观察电梯运动方式后总结出，在每次电梯经过一个楼层时，以该楼层为起点，按照表3中从左至右的方向，依次扫描并响应有请求的楼层，即符合实际电梯的响应逻辑。

| 楼层号     | -4   | -3   | -2   | -1   |  1   | 2    | 3    | 4    |
| ---------- | ---- | ---- | ---- | ---- | :--: | ---- | ---- | ---- |
| 请求标志位 | 0    | 0    | 0    | 0    |  0   | 0    | 0    | 0    |

2. 在本系统中，使用数组实现上述算法。系统接收到上位机指令后，将指令中请求的楼层在数组中对应楼层的一位置位，例如接收到指令1的“2楼有人要下楼”，即将数组中“-2”位置位，同理若“2楼有人上楼”就将“2”位置位。若接收到指令2内容为“目标楼层3”，则根据当前电梯位置及运动方向，就近置位即可。

3. 使用for循环在每次电梯经过一个楼层时以该楼层为起点，按照上文所提方向对数组进行循环扫描，若所有楼层均没有请求，则电梯在原地等待，若扫描的有楼层请求，则系统暂时前往此楼层，在到达该楼层后，将该楼层对应位置零。

4. 由于本设计使用uC/OS系统，故单独使用一个任务执行此算法，此任务在每次执行后挂起自身，并在电梯每次经过一个楼层时由电梯控制任务解挂，由此可以保证此任务在每次电梯到达一个楼层时仅执行一次。

​		上述算法的优点在于，电梯每经过一个楼层时对系统当前要优先响应的楼层数进行调整一次，实时性较好，经过验证可以在电梯当前楼层与目的楼层间有突发楼层请求时及时响应，调整目的楼层，能够满足系统实时性要求。
