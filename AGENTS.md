<1>codex输出

Plan模式下生成的计划应该用中文写，不要用英文。

<2> qt/cpp代码风格：

本项目源码保持 GBK/System 编码。

中文的写法应参考: QString(u8"中文");、QAction *pAc1=new QAction(u8"批量设置标签");而不是QString::fromUtf8("\346\211"));等格式。
不要把文件转换成 UTF-8。代码文件要按 GBK/System 保存。即系统 ANSI/GBK 编码 。
不要使用中文标点、智能引号、全角符号。

添加ui时，必须要创建对应的.ui文件，控件的添加和简单布局应在.ui文件实现，如有必要，也可以在.cpp文件中进行一些布局，ui控件的文字除英文专业术语和OK，NG等常见英文外，应使用中文显示。

<3> 编译环境：

Qt 6.7.3：F:\Qt6.7.3\6.7.3\msvc2019_64

MSVC 2019：19.29 x64

Kit：唯一默认 Kit  QT673_MSVC2019

请使用 MSVC 编译链，不要使用 MinGW。

<4> 编码范围：

- `.cpp`、`.h`：GBK/System 编码。
- `.ui`：保持 Qt Designer 生成的 XML 编码，不手工转换。
- Python、JSON、JSONL、JSON Schema、CMake、Markdown：UTF-8 编码。


