#ifndef MLSDPROCESSER_H
#define MLSDPROCESSER_H

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>



class MlsdJson
{
public:
    MlsdJson();


public:
    QJsonObject m_ConvertedJson;
    QJsonObject m_CurrAddJson;
    QJsonArray  m_OutJsonArray;


    void clearData();
    void addSingleJson(QString JsonPath);
    void addSingleJson(QString JsonPath, QString ImgDir);
    void toSaveColletedJson(QString SavePath);
    QJsonObject convertSingleToValid(const QJsonObject& singleObj);
    QJsonObject convertSingleToValid(const QJsonObject& singleObj, QString ImgDir);

};


class MlsdYaml
{
public:
    MlsdYaml();

private:
    int     m_Batchsize{1};
    int     m_EpochNum{100};
    int     m_InputSize{512};
    QString m_PretrainedPath{""};
    QString m_ImgDir{""};
    QString m_TrainJsonPath{""};
    QString m_ValidJsonPath{""};
    QString m_OutputDir{""};


public:
    void setBatchSize(int BatchSize);
    void setEpochNum(int EpochNum);
    void setInputSize(int InputSize);
    void setPretrainedPath(QString path);
    void setImgDir(QString path);
    void setTrainJsonPath(QString path);
    void setValidJsonPath(QString path);
    void setOutputDir(QString path);
    void SaveMlsdYaml(QString yamlSavePath);

};

class MlsdProcesser
{
public:
    MlsdProcesser();


//json处理对象
public:
    MlsdJson m_TrainJsonObj;
    MlsdJson m_ValidJsonObj;
    MlsdYaml m_YamlObj;

};

#endif // MLSDPROCESSER_H
