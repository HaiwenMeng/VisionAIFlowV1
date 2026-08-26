#ifndef YTFRAMELESSWIDGETLIB_H
#define YTFRAMELESSWIDGETLIB_H
#include <QtCore/qglobal.h>
//窗口使用特性
//@字段用于判断是否正在运行
//*字段用于判断显示的窗体
//# 字段用于显示用户权限标签
class YtFrameLessDlg;
#include <QString>
#define YtStGetBuildTime QString("%1-%2-%3(%4)").arg(QString(__DATE__).mid(7,4))\
    .arg(QString("Jan#Feb#Mar#Apr#May#Jun#Jul#Aug#Sep#Oct#Nov#Dec").split("#").indexOf(QString(__DATE__).mid(0,3))+1)\
    .arg(QString(__DATE__).mid(4,2))\
    .arg(__TIME__)
/*
 * 使用YtFrameLessWidgetLib类提供的全局函数显示QWidget和QDialog窗口，内部已经实现无边框和样式表，所以外部的窗体不需要设置无边框，
 * 直接设置好布局即可。
 *
 * 如果窗口中不能直接关闭，则需要在运行的时候调用 "setWhatsThis("Running");" 这句话将窗口设置为运行状态，这样外部的显示窗口在关闭
 * 的时候就会判断这个状态。在停止的时候将Running设置为其他字符串即可。
 *
 * 显示窗口的注意事项：
 * 1、如果没有设置布局的话那么需要设置一下整个窗口的最小大小，否则只会显示标题栏，最大化后才会显示需要显示的窗口
 * 2、如果设置完布局，则需要将最外层的布局的LayoutSizeConstraint的值改为SetFixedSize
 * 3、设置窗口的WhatsThis值为Running，这样窗口就无法关闭，一般用于主程序退出时的判断
 * 4、无边框窗口的标题和icon都是由外部窗口的windowtitle和windowicon提供，需要在外部设计好
 *
*/

#include <QWidget>
#include <QMutex>
#include <QMap>

typedef enum {
    kOperator = 1,      // 操作员
    kEngineer = 2,      // 工程师
    kAdministrator = 3, // 管理员
    kSuperUser = 4      // 超级管理员
} TYP_USER_AUTHORITY;

#define FLAG_SET_SOFTWARE_STATUS        QString("@")
#define FLAG_SET_USER_INFO              QString("!")
#define FLAG_SET_SHOW_SCREEN            QString("#")

static QMap<QString, QWidget*> m_mapWidget;
static QMutex m_mtxWidgetLock;

class Q_DECL_EXPORT YtFrameLessWidgetLib
{
public:
    /**
     * @brief YtSetShowForm 全局函数，输入Widget/Dialog的指针，直接调用无边框窗口显示这个Widget/Dialog
     * @param form          Widget或Dialog的指针
     * @param aboutstr      右上角关于按钮显示的信息，如果为空则不显示这个按钮
     * @param showpos       控制窗口的显示位置，如果是(0,0)则剧中显示
     * @param isModality    是否为模态显示，默认false表示弹窗置于最顶层，父窗口不可点击，true表示父窗口可以点击。
     * @param isShowLogo    设置是否显示左上角的logo，这个logo直接由外部的窗口设置好自己的windowicon
     * @param isShowTitle   设置是否显示左上角的标题，这个标题也是由窗口自己管理自己的windowtitle
     * @param isShowMin     设置是否显示最小化按钮
     * @param isShowMax     设置是否显示最大化按钮，如果不显示则双击标题栏最大化也会禁用
     * @param isShowClose   设置是否显示关闭按钮
     * @param isFullScreen  是否全屏显示
     * @param nShowScreenIndex  要显示的屏幕编号
     * @return              根据自己的需要返回accept\reject\done中返回的值，默认关闭返回0
     */
    static int YtSetShowForm(QWidget *form, QString aboutstr = "", QPoint showpos = QPoint(0, 0), \
                             bool isModality = false, bool isShowLogo = true, bool isShowTitle = true, \
                             bool isShowMin = true, bool isShowMax = true, bool isShowClose = true,\
                             bool isFullScreen = false, int nShowScreenIndex = -1);

    /**
     * @brief YtAppendGrabForm  将需要截图的窗口设置到全局中
     * @param WidgetKey 窗口的唯一表示key，后续通过这个key值取对应的窗口指针
     * @param ShowForm  直接调用显示窗口的"this->nativeParentWidget();"接口即可
     */
    static void YtAppendGrabForm(QString WidgetKey, QWidget *ShowForm);
    static void YtDeleteGrabForm(QString WidgetKey);
    static QPixmap YtGrabShowForm(QString WidgetKey, QRect GrabRect = QRect(0, 0, 0, 0));
};

class Q_DECL_EXPORT YtMessageBox
{
public:
    // 所有的弹窗返回值只有两个，1表示确认，0表示取消
    static int YtShowSuccess(const QString &title = "", const QString &message = "");
    static int YtShowError(const QString &title = "", const QString &message = "");
    static int YtShowQuestion(const QString &title = "", const QString &message = "");
    static int YtShowWarning(const QString &title = "", const QString &message = "");
};

/// 密码登录窗口
class Q_DECL_EXPORT YtPasswardDlg
{
public:
    YtPasswardDlg();

    /**
     * @brief YtShowPassword        密码登录（只用密码修改功能）
     * @param parent
     * @param strTargetPassword     目标密码
     * @param bVirtualKeyBoard      是否使用虚拟键盘
     * @param bDarkMode             是否启用黑色主题
     * @return                      用户权限（小于等于0为无权限）
     */
    static int YtShowPassword(QWidget *parent, QString strTargetPassword = "", bool bVirtualKeyBoard = false, bool bDarkMode = false);

    /**
     * @brief YtShowPasswordEx      密码登录（带用户管理功能）
     * @param parent
     * @param bVirtualKeyBoard      是否使用虚拟键盘
     * @param bDarkMode             是否启用黑色主题
     * @param bUserName             是否使需要用户名登录（不使用用户名的模式下只能添加三个权限的用户）
     * @return                      用户权限（小于等于0为无权限）
     */
    static int YtShowPasswordEx(QWidget *parent, bool bVirtualKeyBoard = false, bool bDarkMode = false, bool bUserName = false);
};

/// 软键盘输入
// 兼容旧的接口
bool Q_DECL_EXPORT toGetNewChineseQString(QString Oldstr, QString &Newstr, QString title = "");
bool Q_DECL_EXPORT toGetNewQStringNumberAll(QString TitleStr, QString Oldstr, QString &Newstr);
bool Q_DECL_EXPORT toGetNewQStringAll(QString TitleStr, QString Oldstr, QString &Newstr);

/**
* @brief YtShowInputDlg     显示输入窗口
* @param InitStr            初始化显示的字符串，即需要编辑的控件本身的信息
* @param RefStr             输入窗口返回的字符串
* @param bIsNumber          是否只显示数字输入
* @return
*/
bool Q_DECL_EXPORT YtShowInputDlg(QString InitStr, QString &RefStr, bool bIsNumber = false);

/// 产品管理窗口
class Q_DECL_EXPORT YtProductDlg
{
public:
    //获取内部配方名称
    static QVector<QString> YtGetInnerProInfo();
    // 产品管理设置界面
    static void YtShowProductDialog(int UserAuthority=4);
    // 查找产品信息
    static bool YtQueryProduct(const int product_recipe, QString &product_name);
    // 获取当前产品信息
    static void YtUpdateCurrProduct(int &product_recipe, QString &product_name);
    static void YtSetCurentProduct(int product_recipe);
};
class Q_DECL_EXPORT YtWidgetShowLib
{
public:
    YtWidgetShowLib();
    ~YtWidgetShowLib();
private:
    YtFrameLessDlg *m_Prowidget=nullptr;
public:
   int toSetShowWidget(QWidget *form);
   void toShow(bool istopMode=false);
};


//增加翻译接口因为大部分仓库都包含这个头文件
//翻译类型，其实就是本地的一个csv文件，对应的列号
//0 中文
//1 英文
void Q_DECL_EXPORT toInitTranlateData();//
void Q_DECL_EXPORT toTranslateControls(QWidget *parent);//
void Q_DECL_EXPORT toTranslateControlsTips(QWidget *parent);//
QString Q_DECL_EXPORT toGetTranTip(QString SetKey);//
//增加接口把内部的变量全部搞出来
void Q_DECL_EXPORT toSaveDataCapter();//


#endif // YTFRAMELESSWIDGETLIB_H
