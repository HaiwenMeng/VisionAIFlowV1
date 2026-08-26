## 代码规范

中文的写法应参考: QString(u8"中文");、QAction *pAc1=new QAction(u8"批量设置标签");而不是QString::fromUtf8("\346\211"));等格式。
不要使用中文标点、智能引号、全角符号。

添加ui时，必须要创建对应的.ui文件，控件的添加和简单布局应在.ui文件实现，如有必要，也可以在.cpp文件中进行一些布局，ui控件的文字除英文专业术语和OK，NG等常见英文外，应使用中文显示。



## 代码格式

\- C++ 类、结构体、函数、枚举和命名空间禁止压缩成单行。

\- 每个成员变量、成员函数声明必须独占一行。

\- `public:`、`protected:`、`private:` 必须独占一行。

\- 函数定义的左花括号必须换行，采用 Allman 风格。

\- 类定义的左、右花括号必须分别独占一行。

\- 参数逗号后必须有一个空格。

\- 指针和引用符号靠近类型，例如 `QWidget *widget`、`QString &name`。

\- 默认参数运算符两侧必须留空格，例如 `display = nullptr`。

\- 参数列表过长时必须按参数换行，禁止为了减少行数压缩代码。

\- 修改 `.cpp`、`.h` 后必须使用项目根目录的 `.clang-format` 格式化本次修改的文件。

\- 不得手工把已经规范排版的代码重新压缩为单行。



## 编译环境

Qt 6.7.3：F:\Qt6.7.3\6.7.3\msvc2019_64\bin\qmake.exe

MSVC 2019：19.29 x64

Kit：唯一默认 Kit  QT673_MSVC2019

VS2019 初始化脚本：

```
D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat amd64
```

初始化参数： amd64

请使用 MSVC 编译链，不要使用 MinGW。
如果是 qmake 项目，先在同一个 cmd 会话里执行 vcvarsall.bat x86_amd64，再执行 qmake 和 nmake/jom 编译。



## Core Principles (CRITICAL)

**Less is more. The simplest solution is the best solution.** The action hierarchy for every change: **Delete > Replace > Add**.

1. **Solve at the owner**: Put behavior in the code path that owns or observes it. For fixes, never guard a symptom with a staleness check, initialization flag, skip-first-call branch, or `try/except` around broken logic; relocate the trigger and delete the wrong path. For features, extend the existing owner rather than creating a parallel abstraction.
2. **Search and reuse first**: Search the whole repository before creating a feature, component, helper, workflow, or utility. Reuse or adapt what exists, consolidate in-scope duplication in the shared owner, and delete duplicate paths. Three similar lines beat a helper nobody else calls.
3. **Delete and modify existing code before creating new code**: Bugfixes are net-negative by default unless deletion and relocation are demonstrably impossible. A new file must first prove it cannot fit cleanly in an existing owner.
4. **Keep scope minimal**: Implement only the simplest complete solution. Avoid impossible-state handling, speculative flags, compatibility shims, policy scaffolding, and unrelated cleanup. Tests are out of scope by default — rely on existing coverage and focused validation; only an uncovered, high-risk regression path justifies minimal new test code.
5. **Ship zero-regression, production-ready changes**: Understand what you remove instead of retaining broken code as insurance. Remove unused imports, functions, types, files, and comments; run relevant cleanup checks; 

