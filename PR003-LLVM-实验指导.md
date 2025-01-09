# 编译原理研讨课LLVM实验PR003实验指导

## 

### 第一步：编译安装第三次实验所需的LLVM：

```shell
git clone http://124.16.71.62:8000/teacher/llvm_pr003.git
```

在llvm_pr003的同级目录下，创建release目录并编译：

```shell
 mkdir release && cd release
 cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release  ../llvm_pr003 && make
```



### **第二步：编译安装tensorc库：**

首先配置环境变量，使用第一步编译后的clang来编译tensorc库：

```shell
export PATH=[release文件夹路径]/bin:$PATH
source ~/.bashrc
```

从课程网站上拷贝tensorc.zip到服务器自己的工作目录下并解压，而后进行编译：

```shell
 	cd [tensorc文件夹路径]
    mkdir build && cd build
    mkdir basic && cd basic
    cmake -G "Unix Makefiles" ../../src/basic && make
    cd ..  (进入build目录)
    mkdir operation && cd operation
    cmake -G "Unix Makefiles" ../../src/operation && make
    cd ../..  (进入tensorc目录)
    ./make.sh
```



### ***第三步:***  跑通初始测试用例：

从课程网站上拷贝pr003_test.zip到本地并解压。检查是否具有以下的内容：

![image-20240613143826287](C:\Users\19794\AppData\Roaming\Typora\typora-user-images\image-20240613143826287.png)



编译:

```shell
clang -I ./include/ main.c -std=c99-tensorc -fno-discard-value-names  -ltensorc  -o main
```

执行编译后的main程序，会得到如下输出

![image-20240613144324209](C:\Users\19794\AppData\Roaming\Typora\typora-user-images\image-20240613144324209.png)



## 实验指导：

此次实验新增了如下的函数声明:

```c++
RValue EmitTensorcMemberFunctionExpr(const CallExpr *E, ReturnValueSlot ReturnValue);

RValue EmitTensorcBinaryOperator(const BinaryOperator *E, ReturnValueSlot ReturnValue);

RValue EmitTensorcUnaryOperator(const UnaryOperator *E, ReturnValueSlot ReturnValue);

RValue EmitTensorcBinAssign(const BinaryOperator *E, ReturnValueSlot ReturnValue);

RValue EmitTensorcTensorSliceExpr(const TensorSliceExpr *E, ReturnValueSlot ReturnValue);

```

已经提供了

```c++
RValue EmitTensorcMemberFunctionExpr(const CallExpr *E, ReturnValueSlot ReturnValue);
```

该函数是处理Tensor结构体的成员函数调用时，进行codegen, 如示例中的: double a = A.at(1,0)

```c++
RValue EmitTensorcUnaryOperator(const UnaryOperator *E, ReturnValueSlot ReturnValue);
```

提供一个示例：

```c++
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "tensor.h"

int main(int argc, char *argv[]) {
  Tensor A = zeros(DOUBLE, 1, 3);
  Tensor B = ones(DOUBLE, 1, 3);
  Tensor C = -B;
  double a = A.at(1,0);
  double c = C.at(1,1);
  printf("TensorA[0] = %d\n", (int)a);
  printf("TensorC[1] = %d\n", (int)c);

}
```

这段代码中，Tensor C = -B; 在codegen的过程，就是由EmitTensorcUnaryOperator函数进行处理，将 ***-B***  生成对Negative函数的调用。

后续对剩下的三个函数进行实现，推荐按照 *** EmitTensorcBinAssign***  => *** EmitTensorcBinaryOperator***  =>  ***EmitTensorcTensorSliceExpr***的顺序进行实现。

下面给出一些重要的结构体以及函数介绍：

#### ***QualType*** :

QualType代表一个“有修饰符的类型”(Qualified Type)。 它是一个类型加上一组类型修饰符（如const、volatile、restrict等）的组合。QualType用于表示类型系统中的类型，同时保留了类型修饰符的信息。在C和C++中，类型修饰符可以影响类型的含义，因此QualType提供了一种方式来精确地表示。



#### CallArgList:

表示函数调用时的参数列表,  记录了每个参数的值（RValue）和类型（QualType）。这个数据结构在Clang的代码生成阶段使用。



#### getFunctionType(QualType ResultTy, ArrayRef <QualType>  Args,   const FunctionProtoType::ExtProtoInfo &EPI)：

函数原型为：

```c++
QualType getFunctionType(QualType ResultTy, ArrayRef<QualType> Args,                   const FunctionProtoType::ExtProtoInfo &EPI) const;
```

ResultTy：函数的返回类型，可以是任何有效的类型，包括void（表示无返回值）。

Args：参数列表，可以为空（表示没有参数），也可以包含多个参数类型。

EPI：扩展协议信息，它提供了函数的额外属性，例如：

​	 hasNoReturn：指示函数是否声明为noreturn。

 	isVariadic：指示函数是否接受可变数量的参数。

​	 hasTrailingReturn：在C++11及以后的版本中，指示函数是否具有尾返回 类型（trailing return type）。



#### CodeGenTypes::GetFunctionType:

函数原型为：

```c++
llvm::FunctionType *
CodeGenTypes::GetFunctionType(const CGFunctionInfo &FI)
```

此函数的目的是将Clang的类型信息和调用约定转换为LLVM IR的函数类型，这是生成LLVM IR表示形式的关键步骤。通过这种方式，Clang能够生成与目标平台的ABI兼容的代码。 此函数会在IR上生成一条函数声明, 函数声明的IR如下：

```c++
declare i32 @add(i32 %a, i32 %b)
```



### Hints: 

已经给出的代码中， ***RValue EmitTensorcUnaryOperator(const UnaryOperator *E, ReturnValueSlot ReturnValue);***  是一个完整的，比较简单的从Expr=>CodeGen，的过程，整个过程是：***函数类型创建*** => ***函数信息排列*** => ***函数常量创建*** => ***直接调用表示*** => ***生成IR*** ， 结合上述对一些重要结构体的介绍以及函数介绍，完成剩余三个函数的实现



