# IMU姿态解算与坐标系规定

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MjA4ZmNhNDM4MGNkMTBlNjY3NjY0ZGJlYTFhMjdmYTRfZDFjYzIwNzg2MjE1MzA1NWVhOTNjZjAxMjU3NDg4ZWFfSUQ6NzY0NTE1MDA5NzE5ODUxNzQ1M18xNzgwNjQ4Nzc3OjE3ODA3MzUxNzdfVjM)

需要一定的前置数学知识，视觉组知识库中有很好的资料，可以参考文档[姿态表示与旋转](https://robotpilots.feishu.cn/wiki/Pm0LwwQF0iVhDBkUvxKcAti2nFd?from=from_parent_docx)

赞美陈泓宇

## 认识加速度计

首先，我们需要知道加速度计在测什么

惯性系下，陀螺仪静止、无旋转地放在滑台上，其所受到的加速度理论为0；但其加速度传感器测量结果的加速度理论上满足${acc_x}^2 + {acc_y}^2+{acc_z}^2 = 1$，这是因为加速度传感器所测量的并非是惯性系下的加速度，而是其所受非引力作用力带来的加速度。

也就是说，我现在坐在椅子上，从地球这个惯性系来看，我的加速度是0；但如果我身上有一个BMI055,它会测量其所受到的“非引力作用力”带来的加速度，也就是其测量得到的加速度理应为g。这个g是我的椅子给我的支持力，也即“非引力作用力”带来的；而我所谓感受到的“重力”，也就是椅子给我的支持力，即“非引力作用力”

> ![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MDM3NjUzMWFkMmE4ZDZjMGJkYzdkMjFkZDY4ZWQzNWRfN2Q4YWEyNjU5NmNkNjY0ZjU2OGZlZjVlYTQyNzliMDRfSUQ6NzY0NDg2NDEwNTg4MzEyNjczM18xNzgwNjQ4Nzc3OjE3ODA3MzUxNzdfVjM)
> 
> ![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=ODk0ZWNmYzkxODRmYTY5NmFkMTg0NjdkNzkzMzkxYWFfYTY0ZDczM2Q0MTNiY2U2MWNiMjg1ZjE2YWNmODRlYzlfSUQ6NzY0NDg2NDMyNjYzMzcwNDYzOF8xNzgwNjQ4Nzc3OjE3ODA3MzUxNzdfVjM)
> 
> [BMI055手册阅读记录\.pdf](https://robotpilots.feishu.cn/file/MUAoboHTvoniuLxMedZcm5UKnxd)
> 
> 手册中对静止状态下加速度传感器测量结果的相关阐述
> 
> 

基于此，制导镖出射在空中飞行，由于其几乎不受重力以外的任何力（失重），加速度传感器各轴输出 应当均为0，此时加速度传感器对制导镖姿态解算起不到任何作用。

> 手册对失重状态下加速度传感器测量结果的相关阐述如下；这里手册介绍的是低g中断，其核心理念是，加速度传感器各轴输出均接近 0 可以判断失重
> 
> ![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=YjUxYmQwYmY2NTBmOTg5MjA4ZDgwZTdmN2E2ZjhlY2RfOGZkZGMzMGY1NzVjOGZlYjc4ODVmZDVkZDc4YWI4NTVfSUQ6NzY0NDg2OTcwMDUwMTUzOTc5OF8xNzgwNjQ4Nzc3OjE3ODA3MzUxNzdfVjM)
> 
> 



那 acc\_x acc\_y acc\_z 得到的测量结果到底是什么呢？

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MWI4NTJiZjM3N2Q5MDc2NThmMDA2MDI5MzQ4YWMyZGRfM2QxODJhYmFiZDQ5MTg5NzRjYzA3MjcwNWFiY2ExMDRfSUQ6NzY0NDg2NzUxNDQ1MDY5MzA1OF8xNzgwNjQ4Nzc3OjE3ODA3MzUxNzdfVjM)

从这张图中，我们可以看出来，这里acc\_x / acc\_y / acc\_z 是BMI055静止状态下测量得到的结果，其绝对值是加速度g在x / y / z 轴方向上投影；并且若加速度g与x/y/z轴夹角为钝角，则其为正，反之为负。这个正负性乍一看有点抽象，其实如果从“非引力作用力”的角度则很好理解：因为非引力作用力的合力的作用效果与重力理应相反。

## 加速度传感器测得初始姿态

需要有一定的关于欧拉角的知识储备,可以参考文档：[姿态表示与旋转](https://robotpilots.feishu.cn/wiki/Pm0LwwQF0iVhDBkUvxKcAti2nFd)

很符合直觉地，因为重力只提供一个竖直方向，无法告诉我们飞镖朝北、朝东还是朝其他方向；故而我们只能求出飞镖相对于水平面的初始pitch 和roll，而不能确定yaw；

我们选择将滑台上飞镖的yaw初始化为0

### 坐标系定义

- 导航系：NED，North\-East\-Down

- 机体系：FRD，Forward\-Right\-Down

- 欧拉角：yaw\-pitch\-roll，即 \($\psi$,$\theta$,$\phi$\)

- 旋转顺序：3\-2\-1，即 yaw $\rightarrow$pitch $\rightarrow$ roll

- 姿态矩阵：$C_b^n$，表示把机体系向量变到导航系

其中：

$\psi = \text{yaw}$

$\theta = \text{pitch}$

$\phi = \text{roll}$

---

#### 导航系

导航系 \(n\)定义为：

$n = \{N, E, D\}
$

即：

- $x_n$: 北

- $y_n$: 东

- $z_n$: 下

所以重力加速度在 NED 中为：

$g^n = \begin{bmatrix} 0 \\ 0 \\ g \end{bmatrix}$



其中 $g \approx 9.81 \, \text{m/s}^2$。

---

#### 机体系

飞镖机体系 \(b\) 定义为：

$b = {F,R,D}$

即：

- \($x_b$\)：Forward，飞镖头部方向

- \($y_b$\)：Right，飞镖右侧

- \($z_b$\)：Down，飞镖下方

---

### 加速度计静止时到底测的是什么？

据前文所述，加速度计测量的不是普通意义上的重力加速度，而是**比力 specific force**。

比力定义近似为：

$f=a-g$

其中：

- a：载体的惯性加速度

- g：重力加速度

当飞镖静止在滑台上时：

$a^n = 0$

故而

$f^n = -g^n$

在 NED 系中：

$g^n =
\begin{bmatrix}
0\\
0\\
g
\end{bmatrix}
$

所以静止时加速度计对应的比力为：

$f^n =\begin{bmatrix}
0\\
0\\
-g
\end
{bmatrix}$

这说明：静止时，加速度计测到的是“向上”的支撑力方向，而不是向下的重力方向。

在 NED/FRD 规范下，如果飞镖完全水平，且机体系 \($z_b$\) 轴向下，则静止加速度计理想输出是：

$f^b =
\begin{bmatrix}
0\\
0\\
-g
\end{bmatrix}$

也就是说，水平静止时，机体 down 轴上的加速度计读数是 \(\-g\)。

---

### 姿态矩阵的数学形式

采用 3\-2\-1 欧拉角，即 yaw\-pitch\-roll。

从机体系到导航系的方向余弦矩阵为：

$C_b^n = R_z(\psi) R_y(\theta) R_x(\phi)$

展开后为：

$C_b^n =
\begin{bmatrix}
\cos\theta \cos\psi
&
\sin\phi \sin\theta \cos\psi - \cos\phi \sin\psi
&
\cos\phi \sin\theta \cos\psi + \sin\phi \sin\psi
\\
\cos\theta \sin\psi
&
\sin\phi \sin\theta \sin\psi + \cos\phi \cos\psi
&
\cos\phi \sin\theta \sin\psi - \sin\phi \cos\psi
\\
-\sin\theta
&
\sin\phi \cos\theta
&
\cos\phi \cos\theta
\end{bmatrix}$

其中：

$\phi = \text{roll}$

$\theta = \text{pitch}$

$\psi = \text{yaw}$

---

### 静止加速度计输出与 roll/pitch 的关系



静止时，比力在导航系中为：

$f^n =
\begin{bmatrix}
0\\
0\\
-g
\end{bmatrix}$

加速度计实际装在飞镖上，所以测得的是机体系下的比力：

$f^b =
C_n^b f^n$



而：

$C_n^b = \left(C_b^n\right)^T$

所以：

$f^b =
\left(C_b^n\right)^T
\begin{bmatrix}
0\\
0\\
-g
\end{bmatrix}$



由于只乘以导航系的第三个分量，因此最后得到：

$f^b =
-g
\begin{bmatrix}
C_b^n(3,1)\\
C_b^n(3,2)\\
C_b^n(3,3)
\end{bmatrix}$

由上面的矩阵可知第三行是：

$\begin{bmatrix}-\sin\theta & \sin\phi \cos\theta & \cos\phi \cos\theta\end{bmatrix}$

因此：

$f^b =
-g
\begin{bmatrix}
-\sin\theta\\
\sin\phi \cos\theta\\
\cos\phi \cos\theta
\end{bmatrix}$

即：

$f_x = g \sin\theta$

$f_y = -g \sin\phi \cos\theta$

$f_z = -g \cos\phi \cos\theta$

---

### 由加速度计反解 pitch

$f_x = g \sin\theta$



可得：

$\sin\theta = \frac{f_x}{g}$ 

因此：

$\theta = \arcsin\left(\frac{f_x}{g}\right)$

实际工程中，建议使用归一化加速度,因为我们不知道当地的g是多少，所以可以直接用加速度传感器读出来：

$\bar f =
\frac{f^b}{|f^b|}
\begin{bmatrix}
\bar f_x\\
\bar f_y\\
\bar f_z
\end{bmatrix}$



静止时：

$|f^b| \approx g$

因此：

$\theta = \arcsin\left(\bar f_x\right)$



设 $\mathbf{f}=(\bar f_x,\bar f_y,\bar f_z)$ 为单位向量，则 $\bar f_x^2+\bar f_y^2+\bar f_z^2=1$。

令 $\theta=\arcsin(f_x)$，则 $\sin\theta=f_x$，且 $\cos\theta=\sqrt{1-f_x^2}=\sqrt{f_y^2+f_z^2}$。

因此：

$\theta = \arctan\left(\frac{\sin\theta}{\cos\theta}\right) = \text{atan2}\left(f_x,\sqrt{f_y^2+f_z^2}\right)$



两式在 $\theta\in\left[-\frac{\pi}{2},\frac{\pi}{2}\right]$ 内等价。但后者会更加稳定：当 $f_x\to\pm1$ 时，$\arcsin(f_x)$ 的导数趋于无穷，浮点误差会被急剧放大；而 $atan2$直接接收双参数，避免了近零除法，全程数值稳定，更适合工程实现。

---

### 由加速度计反解 roll

由前面关系：

$f_y = -g \sin\phi \cos\theta$

$f_z = -g \cos\phi \cos\theta$

则有:

$-f_y = g \sin\phi \cos\theta$

$-f_z = g \cos\phi \cos\theta$

所以：

$\phi =
\operatorname{atan2}
\left(
-f_y,
-f_z
\right)$

但如果：$\theta = \pm \frac{\pi}{2}$，那$f_y=0,f_z=0$，岂不是算不出$\phi$了？

这就是所谓万向节死锁，不过我们RM场景下的飞镖的pitch不会接近$\pm \frac{\pi}{2}$

---

### pitch 和 roll 物理意义

#### pitch

在 FRD/NED 规范下：

$\theta > 0$

表示飞镖头部上仰，也就是 \($x_b$\) 轴相对于当地水平面向上。

由于 NED 的 \($z_n$\) 轴向下，飞镖头部上仰意味着飞镖 \($x_b$\) 轴在导航系中的 Down 分量为负。

从姿态矩阵中可以看到，机体系 \($x_b$\) 轴在导航系中的 Down 分量为：

$C_b^n(3,1) = -\sin\theta$

所以：

- 当 $\theta = 0^\circ$ 时，飞镖头部水平；

- 当 $\theta > 0^\circ$ 时，飞镖头部上仰；

- 当 $\theta < 0^\circ$ 时，飞镖头部下俯。

#### roll

roll \($\phi$\) 是飞镖绕自身纵轴 $x_b$ 的旋转角。

在 FRD 机体系下 , $\phi > 0$ 表示按照右手定则绕 $x_b$ 轴正向旋转。

也就是说，如果你站在飞镖尾部看向飞镖头部，正滚转方向一般表现为机体右侧向下、左侧向上。

静止时，roll 决定了重力方向在 $y_b-z_b$平面内的投影方向。

由：

$f_y = -g \sin\phi \cos\theta$

$f_z = -g \cos\phi \cos\theta$

可知：

- 当 $\phi = 0^\circ$ 时：

$f_y = 0$

$f_z = -g \cos\theta$



说明加速度计主要在 \(z\_b\) 轴上测到 \(\-g\) 分量。

- 当 $\phi > 0$ 时：

$f_y < 0$

说明比力在机体右轴方向出现负分量。

因此 roll 的本质是：

重力反方向在飞镖横向\-垂向平面，即 \(y\_b\-z\_b\) 平面内的角度。



## 陀螺仪传感器测得飞行姿态

### 卡尔曼滤波对陀螺仪度数进行滤波

我们无法同时得到BMI055内置滤波器在同一时刻下滤波前和滤波后的数据，故而我们无法评估内置滤波器的效果；而且只有在不开启任何滤波的情况下；陀螺仪读取的角速度才能够以2000Hz的频率进行更新，否则更新频率会大打折扣；~~再者在视觉组学到的卡尔曼滤波的知识不能白学~~，故而我们需要从BMI055取出未滤波的数据，然后再树莓派中进行卡尔曼滤波。

这一部分在此不再详细介绍，参考文档：

- [Task9：构建ROS项目并实现一个"简单"的卡尔曼滤波器](https://robotpilots.feishu.cn/wiki/RQPCwSaWziySRrkTzswc07Dhnfc?fromScene=spaceOverview)：视觉组2026赛季培训阶段最后一个Task，利用卡尔曼滤波构建实现对装甲板在相机坐标系下位置的最优估计；该Task文档末尾的备注部分则提供了一些卡尔曼滤波的学习途径

- 我个人培训阶段跟着视频https://www\.bilibili\.com/video/BV1Rh41117MT/?spm\_id\_from=333\.337\.search\-card\.all\.click\&vd\_source=8324c00fa138697569450dfd9429c175做的学习笔记

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MzUwOWYzNTE4MDIyZTljNmZjOTI2YTE2MTYyNjI1MWRfNzU4NTEzYmIxZjM4NTE3NDYzMDRlYzBhMmM4OTdhZjJfSUQ6NzY0NTIxODE4NzU0MTYyOTkxMl8xNzgwNjQ4Nzc3OjE3ODA3MzUxNzdfVjM)

- 我个人培训阶段写的关于卡尔曼滤波实现的文档 , 也应当是我培训阶段写的最有价值的文档；而事实上，我们现在做的卡尔曼滤波工作，也不过只是把Task中的滤波对象从装甲板角点的三维坐标变成陀螺仪测得的角速度罢了，非常类似

\[10\.TASK 9 最后的守门员： 卡尔曼滤波的实现\(1\)\.pdf\]

### 纯四元数陀螺仪积分

#### 用四元数表示旋转

BMI055 陀螺仪每一时刻给出三个角速度：

$\omega^b=
\begin{bmatrix}
p\\
q\\
r
\end{bmatrix}$

假设采样周期为：$\Delta t$,那么这一小段时间内，飞镖绕三个轴分别转了：



$\Delta \theta =
\begin{bmatrix}
p\Delta t\\
q\Delta t\\
r\Delta t
\end{bmatrix}$

这个小旋转的总角度大小为：

$\alpha =
\sqrt{
\Delta \theta_x^2+
\Delta \theta_y^2+
\Delta \theta_z^2
}=
\sqrt{
(p\Delta t)^2+
(q\Delta t)^2+
(r\Delta t)^2}$

旋转轴方向为：

$u=\frac{\Delta \theta}{\alpha}$

其中：

$u_x=\frac{\Delta \theta_x}{\alpha}$

$u_y=\frac{\Delta \theta_y}{\alpha}$

$u_z=\frac{\Delta \theta_z}{\alpha}$

所以：

$u=
\begin{bmatrix}
u_x\\
u_y\\
u_z
\end{bmatrix}$

它表示：这一小段时间里，飞镖等效为绕 u 这个方向转了 $\alpha$ 角度。

而一个“绕单位轴 $u$ 转 $\alpha$的旋转，可以写成四元数：

$\Delta q =
\begin{bmatrix}
\cos\frac{\alpha}{2}\\
u_x\sin\frac{\alpha}{2}\\
u_y\sin\frac{\alpha}{2}\\
u_z\sin\frac{\alpha}{2}
\end{bmatrix}$

这是增量四元数，意为：$\Delta q$这一个采样周期内发生的小旋转

---

#### 姿态如何更新

设当前姿态四元数为：

$q_k =
\begin{bmatrix}
q_0\\
q_1\\
q_2\\
q_3
\end{bmatrix}$

下一时刻姿态为：

$q_{k+1}=q_k\otimes \Delta q$

这里 "$\otimes$ "是四元数乘法。

其物理意义：当前姿态，再叠加一个小旋转，得到下一时刻姿态。其运算规则如下：

设：

$q=
\begin{bmatrix}
q_0\\
q_1\\
q_2\\
q_3
\end{bmatrix}$,$\Delta q=
\begin{bmatrix}
d_0\\
d_1\\
d_2\\
d_3
\end{bmatrix}$

则：

$q_{new}=q\otimes \Delta q$

其中：

$q_{new,0}=q_0d_0-q_1d_1-q_2d_2-q_3d_3$

$q_{new,1}=q_0d_1+q_1d_0+q_2d_3-q_3d_2$

$q_{new,2}=q_0d_2-q_1d_3+q_2d_0+q_3d_1$

$q_{new,3}=q_0d_3+q_1d_2-q_2d_1+q_3d_0$

---

#### 更新后归一化

由于计算误差，四元数长度会慢慢偏离 1。

所以每次更新后做：

$|q|=
\sqrt{
q_0^2+q_1^2+q_2^2+q_3^2
}$

然后：

$q_0=\frac{q_0}{|q|}$

$q_1=\frac{q_1}{|q|}$

$q_2=\frac{q_2}{|q|}$

$q_3=\frac{q_3}{|q|}$

保证：

$q_0^2+q_1^2+q_2^2+q_3^2=1$

---

#### 小角度近似版本（本项目暂不采用）

如果采样频率很高，$\alpha$ 很小，则：

$\cos\frac{\alpha}{2}\approx 1$



$\sin\frac{\alpha}{2}\approx \frac{\alpha}{2}$

因为：



$u_x\alpha=\Delta \theta_x$



所以：

$u_x\sin\frac{\alpha}{2}
\approx
u_x\frac{\alpha}{2}
\frac{\Delta \theta_x}{2}$

因此增量四元数可近似为：

$
\Delta q \approx
\begin{bmatrix}
1\\
\frac{1}{2}p\Delta t\\
\frac{1}{2}q\Delta t\\
\frac{1}{2}r\Delta t
\end{bmatrix}
$



这就是工程里常用的快速积分形式。

---

## 完整流程

每个采样周期执行：

$\omega^b=
\begin{bmatrix}
p\\
q\\
r\\
\end{bmatrix}$



去掉陀螺零偏：

$
\omega^b_{corr}=
\omega^b-b_g
$



计算：

$\Delta \theta =
\omega^b_{corr}\Delta t$



构造：

$\Delta q$

更新：

$q_{k+1}=q_k\otimes\Delta q$



归一化：

$q_{k+1}=\frac{q_{k+1}}{|q_{k+1}|}$





---

## 最核心理解

纯四元数陀螺积分就是：陀螺仪告诉你每一瞬间转多快，乘以时间得到这一瞬间转了多少，把这一小段旋转不断叠加到当前姿态上\}

所以它本质上就是：姿态 = 初始姿态 \+ 一路累计的角速度旋转

只是三维旋转不能直接用 roll、pitch、yaw 相加，所以用四元数来累计。

