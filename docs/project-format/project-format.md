# 项目文件格式

项目根目录包含稳定元数据 `project.json`、`labels.json`、`project.lock` 和 `data/index.json`。图片、标注、运行与模型产物均在独立目录保存，项目内只保存相对 `/` 路径。

项目创建在同级随机临时目录完成。仅当目录、关键 JSON 的 `QSaveFile` 原子提交和重新打开校验全部成功时，临时目录才会发布为项目根目录；失败时临时目录不是有效项目，原目标目录保持不存在。

`projectType` 与 `classificationMode` 是不可变元数据。分类项目必须指定 `single_label` 或 `multi_label`；其他八种项目类型的分类模式固定为空。
