#ifndef YTVISIONBASEFUN_H
#define YTVISIONBASEFUN_H
#include "ytvisiondefine.h"
#include <QFileInfoList>
#include <QtCore/qglobal.h>

/**
 * @brief YtCopyDirectoryFiles      拷贝文件夹
 * @param src_dir                   源目录
 * @param tgt_dir                   目标目录
 * @param flag_coverfile_ifexist    是否覆盖已有文件
 * @return
 */
bool Q_DECL_EXPORT YtCopyDirectoryFiles(const QString &src_dir, const QString &tgt_dir, bool flag_coverfile_ifexist = false);

/**
 * @brief YtDeleteDir   递归删除文件
 * @param path          需要删除的目录
 * @return              是否删除成功
 */
bool Q_DECL_EXPORT YtDeleteDir(const QString &path);

/**
 * @brief YtGetFileList 提取当前目录下固定格式的文件，只做一种格式使用“#”隔开 比如"ytimage#jpg"
 * @param Path          提取的目录
 * @param Filter        提取的格式
 * @return              返回所有满足的字符串列表
 */
QStringList Q_DECL_EXPORT YtGetFileList(QString Path, QString Filter);

/**
 * @brief YtCompressDir 压缩文件夹，建议大文件或者在不知道文件夹大小的情况下使用std::thread放到线程中，防止卡住主线程
 * @param src_path      压缩目录(压缩的是当前文件夹下面的所有文件压缩成ZIP文件，而不是这个文件夹)
 * @param dst_name      压缩后的名称
 * @return              返回压缩是否成功
 */
bool Q_DECL_EXPORT YtCompressDir(const QString &src_path, const QString &dst_name, const QString &pass_word = "");

/**
 * @brief YtExtractDir      解压文件
 * @param compressed_file   压缩文件路径
 * @param dst_path          解压后的文件路径
 * @return                  返回是否解压成功
 */
bool Q_DECL_EXPORT YtExtractDir(const QString &compressed_file, const QString &dst_path, const QString &pass_word = "");

#endif // YTVISIONBASEFUN_H
