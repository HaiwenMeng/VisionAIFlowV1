#ifndef HALXAI_H
#define HALXAI_H

#include <halconcpp/HalconCpp.h>
#include <halconcpp/HDevThread.h>

// Generated stubs for parallel procedure calls. Wrapped in name
// space to avoid name conflicts with actual procedure names
namespace HDevExportCpp
{
// Parallel execution wrapper for get_dl_dataset_from_coco_annotation_image_id(...)
static void* _hcppthread_get_dl_dataset_from_coco_annotation_image_id(void *hcthread);
// Parallel execution wrapper for create_dl_dataset_from_coco_samples(...)
static void* _hcppthread_create_dl_dataset_from_coco_samples(void *hcthread);
}

using namespace HalconCpp;
namespace HalxAI
{
// Procedure declarations
// External procedures
// Chapter: Image / Channel
void add_colormap_to_image (HObject ho_GrayValueImage, HObject ho_Image, HObject *ho_ColoredImage,
                            HTuple hv_HeatmapColorScheme);
// Chapter: Image / Channel
// Short Description: Create a lookup table and convert a gray scale image.
void apply_colorscheme_on_gray_value_image (HObject ho_InputImage, HObject *ho_ResultImage,
                                            HTuple hv_Schema);
// Chapter: Deep Learning / Object Detection and Instance Segmentation
void area_iou (HTuple hv_Sample, HTuple hv_Result, HTuple hv_InstanceType, HTuple hv_ResultSortIndices,
               HTuple *hv_SampleArea, HTuple *hv_ResultArea, HTuple *hv_IoU);
// Chapter: Deep Learning / Model
void augment_dl_sample_brightness_variation (HTuple hv_DLSample, HTuple hv_BrightnessVariation);
// Chapter: Deep Learning / Model
void augment_dl_sample_brightness_variation_spot (HTuple hv_DLSample, HTuple hv_BrightnessVariation);
// Chapter: Deep Learning / Model
void augment_dl_sample_contrast_variation (HTuple hv_DLSample, HTuple hv_ContrastVariation);
// Chapter: Deep Learning / Model
void augment_dl_sample_crop_percentage (HTuple hv_DLSample, HTuple hv_CropPercentage);
// Chapter: Deep Learning / Model
void augment_dl_sample_crop_pixel (HTuple hv_DLSample, HTuple hv_CropPixel);
// Chapter: Deep Learning / Model
void augment_dl_sample_mirror (HTuple hv_DLSample, HTuple hv_MirrorMethods, HTuple hv_ClassIDsNoOrientation,
                               HTuple hv_IgnoreDirection);
// Chapter: Deep Learning / Model
void augment_dl_sample_remove_pixel (HTuple hv_DLSample, HTuple hv_NumPixelsToRemoveX,
                                     HTuple hv_NumPixelsToRemoveY);
// Chapter: Deep Learning / Model
void augment_dl_sample_rotate (HTuple hv_DLSample, HTuple hv_RotationStep, HTuple hv_ClassIDsNoOrientation,
                               HTuple hv_IgnoreDirection);
// Chapter: Deep Learning / Model
void augment_dl_sample_rotate_range (HTuple hv_DLSample, HTuple hv_RotateRange);
// Chapter: Deep Learning / Model
void augment_dl_sample_saturation_variation (HTuple hv_DLSample, HTuple hv_SaturationVariation);
// Chapter: Deep Learning / Model
// Short Description: Perform data augmentation on the given samples.
void augment_dl_samples (HTuple hv_DLSampleBatch, HTuple hv_GenParam);
// Chapter: Deep Learning / OCR
// Short Description: Compute zoom factors to fit an image to a target size.
void calculate_dl_image_zoom_factors (HTuple hv_ImageWidth, HTuple hv_ImageHeight,
                                      HTuple hv_TargetWidth, HTuple hv_TargetHeight, HTuple hv_DLPreprocessParam, HTuple *hv_ZoomFactorWidth,
                                      HTuple *hv_ZoomFactorHeight);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Calculate the channel-wise mean and standard deviation of a DL model layer.
void calculate_dl_model_layer_mean_stddev (HTuple hv_DLModelHandle, HTuple hv_LayerName,
                                           HTuple hv_DLSamples, HTuple *hv_Mean, HTuple *hv_StdDev);
// Chapter: Deep Learning / Semantic Segmentation and Edge Extraction
// Short Description: Calculate the class weights for a semantic segmentation dataset.
void calculate_dl_segmentation_class_weights (HTuple hv_DLDataset, HTuple hv_MaxWeight,
                                              HTuple hv_IgnoreClassIDs, HTuple *hv_ClassWeights);
// Chapter: Deep Learning / Model
// Short Description: Calculate evaluation measures based on the values of RunningMeasures and the settings in EvalParams.
void calculate_evaluation_measures (HTuple hv_RunningMeasures, HTuple hv_EvalParams,
                                    HTuple *hv_EvaluationResult);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Calculate anomaly measures based on RunningMeasures.
void calculate_image_anomaly_measures (HTuple hv_RunningMeasures, HTuple hv_EvalParams,
                                       HTuple *hv_EvaluationResult);
// Chapter: Deep Learning / Classification
// Short Description: Calculate image classification measures based on RunningMeasures.
void calculate_image_classification_measures (HTuple hv_RunningMeasures, HTuple hv_EvalParams,
                                              HTuple *hv_EvaluationResult);
// Chapter: Deep Learning / Object Detection and Instance Segmentation
// Short Description: Calculate instance measures based on RunningMeasures.
void calculate_instance_measures (HTuple hv_RunningMeasures, HTuple hv_EvalParams,
                                  HTuple *hv_EvaluationResult);
// Chapter: OCR / Deep OCR
// Short Description: Computes the ocr_detection relevant evaluation measures.
void calculate_ocr_detection_measures (HTuple hv_DetectionEvaluationResult, HTuple *hv_EvaluationResult);
// Chapter: OCR / Deep OCR
// Short Description: Calculate OCR recognition measures based on RunningMeasures.
void calculate_ocr_recognition_measures (HTuple hv_RunningMeasures, HTuple hv_EvalParams,
                                         HTuple *hv_EvaluationResult);
// Chapter: Deep Learning / Semantic Segmentation and Edge Extraction
// Short Description: Calculate pixel measures based on RunningMeasures.
void calculate_pixel_measures (HTuple hv_RunningMeasures, HTuple hv_EvalParams, HTuple *hv_EvaluationResult);
// Chapter: Deep Learning / Model
// Short Description: Calculate region measures based on running measure values.
void calculate_region_measures (HTuple hv_RunningMeasures, HTuple hv_EvalParams,
                                HTuple *hv_EvaluationResult);
// Chapter: 3D Matching / 3D Gripping Point Detection
// Short Description: Calculate 3D gripping point measures based on RunningMeasures.
void calculate_running_gripping_point_measures (HTuple hv_RunningMeasures, HTuple hv_EvalParams,
                                                HTuple *hv_EvaluationResult);
// Chapter: Deep Learning / Model
// Short Description: Check and sanitize the parameters of augment_dl_samples.
void check_augment_dl_samples_gen_param (HTuple hv_GenParam);
// Chapter: 3D Matching / 3D Gripping Point Detection
// Short Description: Initialize and check parameter for the generation of 3D gripping points and poses.
void check_dl_3d_gripping_points_and_poses_params (HTuple hv_DLGrippingPointParams);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Check if scores of a Global Context Anomaly Detection model have been normalized
void check_dl_gc_anomaly_scores_normalization (HTuple hv_DLModelHandle, HTuple hv_GenParam);
// Chapter: Deep Learning / Model
// Short Description: Check the content of the parameter dictionary DLPreprocessParam.
void check_dl_preprocess_param (HTuple hv_DLPreprocessParam);
// Chapter: Deep Learning / Model
void check_train_dl_model_params (HTuple hv_DLDataset, HTuple hv_DLModelHandle, HTuple hv_NumTrainSamples,
                                  HTuple hv_StartEpoch, HTuple hv_TrainParam);
// Chapter: Deep Learning / Model
// Short Description: Collect the information required for displaying the training progress update.
void collect_train_dl_model_info (HTuple hv_DLModelHandle, HTuple hv_TrainResults,
                                  HTuple hv_EvaluationInfos, HTuple hv_EvaluationComparisonKeys, HTuple hv_EvaluationOptimizationMethod,
                                  HTuple hv_Iteration, HTuple hv_NumIterations, HTuple hv_NumIterationsPerEpoch,
                                  HTuple hv_NumSamplesMeanLoss, HTuple *hv_TrainInfo);
// Chapter: Deep Learning / Model
// Short Description: Compute 3D normals.
void compute_normals_xyz (HObject ho_x, HObject ho_y, HObject ho_z, HObject *ho_NXImage,
                          HObject *ho_NYImage, HObject *ho_NZImage, HTuple hv_Smoothing);
// Chapter: Deep Learning / Classification
// Short Description: Calculate top-K error.
void compute_top_k_error (HTuple hv_ImageLabelIDs, HTuple hv_TopKPredictions, HTuple hv_K,
                          HTuple *hv_TopKError);
// Chapter: 3D Matching / 3D Gripping Point Detection
// Short Description: Compute a pose from a 3D point and orientation.
void convert_dl_3d_gripping_point_to_pose (HTuple hv_X, HTuple hv_Y, HTuple hv_Z,
                                           HTuple hv_NX, HTuple hv_NY, HTuple hv_NZ, HTuple *hv_Pose);
// Chapter: OCR / Deep OCR
// Short Description: This procedure converts Deep OCR Detection results to an Object Detection results.
void convert_ocr_detection_result_to_object_detection (HTuple hv_OcrResults, HTuple *hv_DetectionResults);
// Chapter: Tools / Geometry
// Short Description: Convert the parameters of rectangles with format rectangle2 to the coordinates of its 4 corner-points.
void convert_rect2_5to8param (HTuple hv_Row, HTuple hv_Col, HTuple hv_Length1, HTuple hv_Length2,
                              HTuple hv_Phi, HTuple *hv_Row1, HTuple *hv_Col1, HTuple *hv_Row2, HTuple *hv_Col2,
                              HTuple *hv_Row3, HTuple *hv_Col3, HTuple *hv_Row4, HTuple *hv_Col4);
// Chapter: Tools / Geometry
// Short Description: Convert for four-sided figures the coordinates of the 4 corner-points to the parameters of format rectangle2.
void convert_rect2_8to5param (HTuple hv_Row1, HTuple hv_Col1, HTuple hv_Row2, HTuple hv_Col2,
                              HTuple hv_Row3, HTuple hv_Col3, HTuple hv_Row4, HTuple hv_Col4, HTuple hv_ForceL1LargerL2,
                              HTuple *hv_Row, HTuple *hv_Col, HTuple *hv_Length1, HTuple *hv_Length2, HTuple *hv_Phi);
// Chapter: Deep Learning / Model
// Short Description: Create a dict which maps class IDs to class indices.
void create_dl_class_id_mapping (HTuple hv_ClassIDs, HTuple *hv_ClassIDsToClassIndex);
// Chapter: Deep Learning / Model
// Short Description: Create a dictionary with preprocessing parameters.
void create_dl_preprocess_param (HTuple hv_DLModelType, HTuple hv_ImageWidth, HTuple hv_ImageHeight,
                                 HTuple hv_ImageNumChannels, HTuple hv_ImageRangeMin, HTuple hv_ImageRangeMax,
                                 HTuple hv_NormalizationType, HTuple hv_DomainHandling, HTuple hv_IgnoreClassIDs,
                                 HTuple hv_SetBackgroundID, HTuple hv_ClassIDsBackground, HTuple hv_GenParam,
                                 HTuple *hv_DLPreprocessParam);
// Chapter: Deep Learning / Model
// Short Description: Create a dictionary with the preprocessing parameters based on a given DL model.
void create_dl_preprocess_param_from_model (HTuple hv_DLModelHandle, HTuple hv_NormalizationType,
                                            HTuple hv_DomainHandling, HTuple hv_SetBackgroundID, HTuple hv_ClassIDsBackground,
                                            HTuple hv_GenParam, HTuple *hv_DLPreprocessParam);
// Chapter: Deep Learning / Model
// Short Description: Create a training parameter dictionary which is used in train_dl_model.
void create_dl_train_param (HTuple hv_DLModelHandle, HTuple hv_NumEpochs, HTuple hv_EvaluationIntervalEpochs,
                            HTuple hv_EnableDisplay, HTuple hv_RandomSeed, HTuple hv_GenParamName, HTuple hv_GenParamValue,
                            HTuple *hv_TrainParam);
// Chapter: Deep Learning / Model
// Short Description: Generate a dictionary EvalParams, which contains default values for evaluation parameters.
void create_evaluation_default_param (HTuple hv_EvaluationType, HTuple hv_ClassIDsModel,
                                      HTuple *hv_EvalParams);
// Chapter: Deep Learning / Model
// Short Description: Crops a given image object based on the given domain handling.
void crop_dl_sample_image (HObject ho_Domain, HTuple hv_DLSample, HTuple hv_Key,
                           HTuple hv_DLPreprocessParam);
// Chapter: Graphics / Window
// Short Description: Close all window handles contained in a dictionary.
void dev_close_window_dict (HTuple hv_WindowHandleDict);
// Chapter: Graphics / Output
// Short Description: Display a map of the confidences.
void dev_display_confidence_regions (HObject ho_ImageConfidence, HTuple hv_DrawTransparency,
                                     HTuple *hv_Colors);
// Chapter: OCR / Deep OCR
// Short Description: Visualize Deep OCR detection and recognition results.
void dev_display_deep_ocr_results (HObject ho_Image, HTuple hv_WindowHandle, HTuple hv_DeepOcrResult,
                                   HTuple hv_GenParamName, HTuple hv_GenParamValue);
// Chapter: Deep Learning / Model
// Short Description: Visualize different images, annotations and inference results for a sample.
void dev_display_dl_data (HTuple hv_DLSample, HTuple hv_DLResult, HTuple hv_DLDatasetInfo,
                          HTuple hv_KeysForDisplay, HTuple hv_GenParam, HTuple hv_WindowHandleDict);
// Chapter: Deep Learning / Model
// Short Description: Try to guess the maximum class id based on the given sample/result.
void dev_display_dl_data_get_max_class_id (HTuple hv_DLSample, HTuple *hv_MaxClassId,
                                           HTuple *hv_Empty);
// Chapter: Deep Learning / Model
// Short Description: Visualize for a given number of samples the raw image, ground truth annotation, and inferred results.
void dev_display_dl_data_tiled (HTuple hv_DLDataset, HTuple hv_DLModelHandle, HTuple hv_NumSamples,
                                HTuple hv_Split, HTuple hv_GenParam, HTuple hv_WindowHandle, HTuple *hv_WindowHandleOut);
// Chapter: OCR / Deep OCR
// Short Description: Displays information about invalid dataset samples of a Deep OCR dataset.
void dev_display_dl_invalid_samples (HTuple hv_DLDataset, HTuple hv_DLModelHandle,
                                     HTuple hv_InvalidSamplesIndices, HTuple hv_InvalidSamplesReasons);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Display the ground truth anomaly regions of the given DLSample.
void dev_display_ground_truth_anomaly_regions (HTuple hv_SampleKeys, HTuple hv_DLSample,
                                               HTuple hv_CurrentWindowHandle, HTuple hv_LineWidth, HTuple hv_AnomalyRegionLabelColor,
                                               HTuple hv_AnomalyColorTransparency, HTuple *hv_AnomalyRegionExists);
// Chapter: Graphics / Output
// Short Description: Display the ground truth bounding boxes of DLSample.
void dev_display_ground_truth_detection (HTuple hv_DLSample, HTuple hv_SampleKeys,
                                         HTuple hv_LineWidthBbox, HTuple hv_ClassIDs, HTuple hv_BboxColors, HTuple hv_BboxLabelColor,
                                         HTuple hv_WindowImageRatio, HTuple hv_TextColor, HTuple hv_ShowLabels, HTuple hv_ShowDirection,
                                         HTuple hv_WindowHandle, HTuple *hv_BboxIDs);
// Chapter: Deep Learning / Model
// Short Description: Initialize the visualization of the training progress. This includes setting default values for visualization parameters.
void dev_display_init_train_dl_model (HTuple hv_DLModelHandle, HTuple hv_TrainParam,
                                      HTuple *hv_DisplayData);
// Chapter: Graphics / Output
// Short Description: Display a color bar next to an image.
void dev_display_map_color_bar (HTuple hv_ImageWidth, HTuple hv_ImageHeight, HTuple hv_MapColorBarWidth,
                                HTuple hv_Colors, HTuple hv_MaxValue, HTuple hv_WindowImageRatio, HTuple hv_WindowHandle);
// Chapter: Graphics / Output
// Short Description: Display a pie chart inside a window.
void dev_display_pie_chart (HTuple hv_WindowHandle, HTuple hv_Ratios, HTuple hv_Row,
                            HTuple hv_Column, HTuple hv_Radius, HTuple hv_Colors, HTuple hv_GenParam);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Display the detected anomaly regions.
void dev_display_result_anomaly_regions (HObject ho_AnomalyRegion, HTuple hv_CurrentWindowHandle,
                                         HTuple hv_LineWidth, HTuple hv_AnomalyRegionResultColor);
// Chapter: Graphics / Output
// Short Description: Display result bounding boxes.
void dev_display_result_detection (HTuple hv_DLResult, HTuple hv_ResultKeys, HTuple hv_LineWidthBbox,
                                   HTuple hv_ClassIDs, HTuple hv_TextConf, HTuple hv_Colors, HTuple hv_BoxLabelColor,
                                   HTuple hv_WindowImageRatio, HTuple hv_TextPositionRow, HTuple hv_TextColor, HTuple hv_ShowLabels,
                                   HTuple hv_ShowDirection, HTuple hv_WindowHandle, HTuple *hv_BboxClassIndices);
// Chapter: Graphics / Output
// Short Description: Display the ground truth/result segmentation as regions.
void dev_display_segmentation_regions (HObject ho_SegmentationImage, HTuple hv_ClassIDs,
                                       HTuple hv_ColorsSegmentation, HTuple hv_ExcludeClassIDs, HTuple *hv_ImageClassIDs);
// Chapter: Deep Learning / Model
// Short Description: Display a legend according to the generic parameters.
void dev_display_tiled_legend (HTuple hv_WindowImages, HTuple hv_GenParam);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Display information about the training of an anomaly detection model.
void dev_display_train_info_anomaly_detection (HTuple hv_TrainParam, HTuple *hv_WindowHandleInfo);
// Chapter: Deep Learning / Model
// Short Description: Update the various texts and plots during training.
void dev_display_update_train_dl_model (HTuple hv_TrainParam, HTuple hv_DisplayData,
                                        HTuple hv_TrainInfo, HTuple hv_Epochs, HTuple hv_Loss, HTuple hv_LearningRate,
                                        HTuple hv_EvalEpochs, HTuple hv_EvalValues, HTuple hv_EvalValuesTrain);
// Chapter: Graphics / Output
// Short Description: Display a map of weights.
void dev_display_weight_regions (HObject ho_ImageWeight, HTuple hv_DrawTransparency,
                                 HTuple hv_SegMaxWeight, HTuple *hv_Colors);
// Chapter: Develop
// Short Description: Open a new graphics window that preserves the aspect ratio of the given image.
void dev_open_window_fit_image (HObject ho_Image, HTuple hv_Row, HTuple hv_Column,
                                HTuple hv_WidthLimit, HTuple hv_HeightLimit, HTuple *hv_WindowHandle);
// Chapter: Develop
// Short Description: Open a new graphics window that preserves the aspect ratio of the given image size.
void dev_open_window_fit_size (HTuple hv_Row, HTuple hv_Column, HTuple hv_Width,
                               HTuple hv_Height, HTuple hv_WidthLimit, HTuple hv_HeightLimit, HTuple *hv_WindowHandle);
// Chapter: Develop
// Short Description: Resize a graphics window with a given maximum extent such that it preserves the aspect ratio of a given width and height
void dev_resize_window_fit_size (HTuple hv_Row, HTuple hv_Column, HTuple hv_Width,
                                 HTuple hv_Height, HTuple hv_WidthLimit, HTuple hv_HeightLimit);
// Chapter: Develop
// Short Description: Switch dev_update_pc, dev_update_var, and dev_update_window to 'off'.
void dev_update_off ();
// Chapter: System / Operating System
// Short Description: Estimate the remaining time for a task given the current progress.
void estimate_progress (HTuple hv_SecondsStart, HTuple hv_ProgressMin, HTuple hv_ProgressCurrent,
                        HTuple hv_ProgressMax, HTuple *hv_SecondsElapsed, HTuple *hv_SecondsRemaining,
                        HTuple *hv_ProgressPercent, HTuple *hv_ProgressPerSecond);
// Chapter: Deep Learning / Model
// Short Description: Evaluate the model given by DLModelHandle on the selected samples of DLDataset.
void evaluate_dl_model (HTuple hv_DLDataset, HTuple hv_DLModelHandle, HTuple hv_SampleSelectMethod,
                        HTuple hv_SampleSelectValues, HTuple hv_GenParam, HTuple *hv_EvaluationResult,
                        HTuple *hv_EvalParams);
// Chapter: Deep Learning / Object Detection and Instance Segmentation
// Short Description: Filter the instance segmentation masks of a DL sample based on a given selection.
void filter_dl_sample_instance_segmentation_masks (HTuple hv_DLSample, HTuple hv_BBoxSelectionMask);
// Chapter: Deep Learning / Model
// Short Description: Retrieve the indices of Samples that contain KeyName matching KeyValue according to the Mode set.
void find_dl_samples (HTuple hv_Samples, HTuple hv_KeyName, HTuple hv_KeyValue, HTuple hv_Mode,
                      HTuple *hv_SampleIndices);
// Chapter: XLD / Creation
// Short Description: Create an arrow shaped XLD contour.
void gen_arrow_contour_xld (HObject *ho_Arrow, HTuple hv_Row1, HTuple hv_Column1,
                            HTuple hv_Row2, HTuple hv_Column2, HTuple hv_HeadLength, HTuple hv_HeadWidth);
// Chapter: Deep Learning / Model
// Short Description: Create blank train sample dictionaries for a given model.
void gen_blank_dl_train_samples (HTuple hv_DLModelHandle, HTuple *hv_TrainSamples);
// Chapter: Deep Learning / Classification
// Short Description: Compute a confusion matrix, which an be visualized and/or returned.
void gen_confusion_matrix (HTuple hv_GroundTruthLabels, HTuple hv_PredictedClasses,
                           HTuple hv_GenParamName, HTuple hv_GenParamValue, HTuple hv_WindowHandle, HTuple *hv_ConfusionMatrix);
// Chapter: 3D Matching / 3D Gripping Point Detection
// Short Description: Generate gripping points for connected regions of high gripping confidence.
void gen_dl_3d_gripping_point_image_coord (HObject ho_GrippingMap, HObject *ho_Regions,
                                           HTuple hv_MinAreaSize, HTuple *hv_Rows, HTuple *hv_Columns);
// Chapter: 3D Matching / 3D Gripping Point Detection
// Short Description: Extract gripping points based on a 3D gripping point detection model output.
void gen_dl_3d_gripping_points_and_poses (HTuple hv_DLSampleBatch, HTuple hv_DLGrippingPointParams,
                                          HTuple hv_DLResultBatch);
// Chapter: OCR / Deep OCR
// Short Description: Generate ground truth characters if they don't exist and words to characters mapping.
void gen_dl_ocr_detection_gt_chars (HTuple hv_DLSampleTargets, HTuple hv_DLSample,
                                    HTuple hv_ScaleWidth, HTuple hv_ScaleHeight, HTupleVector/*{eTupleVector,Dim=1}*/ *hvec_WordsCharsMapping);
// Chapter: OCR / Deep OCR
// Short Description: Generate target link score map for ocr detection training.
void gen_dl_ocr_detection_gt_link_map (HObject *ho_GtLinkMap, HTuple hv_ImageWidth,
                                       HTuple hv_ImageHeight, HTuple hv_DLSampleTargets, HTupleVector/*{eTupleVector,Dim=1}*/ hvec_WordToCharVec,
                                       HTuple hv_Alpha);
// Chapter: OCR / Deep OCR
// Short Description: Generate target orientation score maps for ocr detection training.
void gen_dl_ocr_detection_gt_orientation_map (HObject *ho_GtOrientationMaps, HTuple hv_ImageWidth,
                                              HTuple hv_ImageHeight, HTuple hv_DLSample);
// Chapter: OCR / Deep OCR
// Short Description: Generate target text score map for ocr detection training.
void gen_dl_ocr_detection_gt_score_map (HObject *ho_TargetText, HTuple hv_DLSample,
                                        HTuple hv_BoxCutoff, HTuple hv_RenderCutoff, HTuple hv_ImageWidth, HTuple hv_ImageHeight);
// Chapter: OCR / Deep OCR
// Short Description: Preprocess dl samples and generate targets and weights for ocr detection training.
void gen_dl_ocr_detection_targets (HTuple hv_DLSampleOriginal, HTuple hv_DLPreprocessParam);
// Chapter: OCR / Deep OCR
// Short Description: Generate link score map weight for ocr detection training.
void gen_dl_ocr_detection_weight_link_map (HObject ho_LinkMap, HObject ho_TargetWeight,
                                           HObject *ho_TargetWeightLink, HTuple hv_LinkZeroWeightRadius);
// Chapter: OCR / Deep OCR
// Short Description: Generate orientation score map weight for ocr detection training.
void gen_dl_ocr_detection_weight_orientation_map (HObject ho_InitialWeight, HObject *ho_OrientationTargetWeight,
                                                  HTuple hv_DLSample);
// Chapter: OCR / Deep OCR
// Short Description: Generate text score map weight for ocr detection training.
void gen_dl_ocr_detection_weight_score_map (HObject *ho_TargetWeightText, HTuple hv_ImageWidth,
                                            HTuple hv_ImageHeight, HTuple hv_DLSample, HTuple hv_BoxCutoff, HTuple hv_WSWeightRenderThreshold,
                                            HTuple hv_Confidence);
// Chapter: Deep Learning / Model
// Short Description: Return the DLSample dictionaries for given sample indices of a DLDataset.
void gen_dl_samples (HTuple hv_DLDataset, HTuple hv_SampleIndices, HTuple hv_RestrictKeysDLSample,
                     HTuple hv_GenParam, HTuple *hv_DLSampleBatch);
// Chapter: Deep Learning / Model
// Short Description: Store the given images in a tuple of dictionaries DLSamples.
void gen_dl_samples_from_images (HObject ho_Images, HTuple *hv_DLSampleBatch);
// Chapter: Deep Learning / Semantic Segmentation and Edge Extraction
// Short Description: Generate weight images for the training dataset.
void gen_dl_segmentation_weight_images (HTuple hv_DLDataset, HTuple hv_DLPreprocessParam,
                                        HTuple hv_ClassWeights, HTuple hv_GenParam);
// Chapter: OCR / Deep OCR
// Short Description: Provide a class mapping based on a DLDataset
void gen_ocr_detection_class_id_mapping (HTuple hv_DLDataset, HTuple *hv_ClassToClassIndex);
// Chapter: Deep Learning / Classification
// Short Description: Generate a tiled image for the classified DLSamples and add indications whether the predictions are true or not.
void gen_tiled_classification_image_result (HObject *ho_TiledImageRow, HTuple hv_DLSamples,
                                            HTuple hv_SpacingCol, HTuple hv_PredictionsCorrect, HTuple hv_ResClasses, HTuple *hv_TextImageRows,
                                            HTuple *hv_TextImageColumns, HTuple *hv_TextImageWidth, HTuple *hv_TextImageHeight);
// Chapter: Deep Learning / Classification
// Short Description: Generate a tiled image for the Deep OCR DLSamples and add indications whether the predictions are true or not.
void gen_tiled_ocr_recognition_image_result (HObject *ho_TiledImage, HTuple hv_DLSamples,
                                             HTuple hv_PredictionsCorrect, HTuple *hv_TextImageRows, HTuple *hv_TextImageColumns,
                                             HTuple *hv_TextImageWidth, HTuple *hv_TextImageHeight);
// Chapter: Deep Learning / Semantic Segmentation and Edge Extraction
// Short Description: Generate a tiled image for segmentation and 3D Gripping Point Detection DLSamples.
void gen_tiled_segmentation_image (HObject *ho_TiledImageRow, HTuple hv_DLSamples,
                                   HTuple hv_SpacingCol, HTuple hv_Width, HTuple hv_Height);
// Chapter: OCR / Deep OCR
// Short Description: Generate a word to characters mapping.
void gen_words_chars_mapping (HTuple hv_DLSample, HTupleVector/*{eTupleVector,Dim=1}*/ *hvec_WordsCharsMapping);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Get the ground truth anomaly label and label ID.
void get_anomaly_ground_truth_label (HTuple hv_SampleKeys, HTuple hv_DLSample, HTuple *hv_AnomalyLabelGroundTruth,
                                     HTuple *hv_AnomalyLabelIDGroundTruth);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Get the anomaly results out of DLResult and apply thresholds (if specified).
void get_anomaly_result (HObject *ho_AnomalyImage, HObject *ho_AnomalyRegion, HTuple hv_DLResult,
                         HTuple hv_AnomalyClassThreshold, HTuple hv_AnomalyRegionThreshold, HTuple hv_AnomalyResultPostfix,
                         HTuple *hv_AnomalyScore, HTuple *hv_AnomalyClassID, HTuple *hv_AnomalyClassThresholdDisplay,
                         HTuple *hv_AnomalyRegionThresholdDisplay);
// Chapter: Graphics / Window
// Short Description: Get the next child window that can be used for visualization.
void get_child_window (HTuple hv_HeightImage, HTuple hv_Font, HTuple hv_FontSize,
                       HTuple hv_Text, HTuple hv_PrevWindowCoordinates, HTuple hv_WindowHandleDict,
                       HTuple hv_WindowHandleKey, HTuple *hv_WindowImageRatio, HTuple *hv_PrevWindowCoordinatesOut);
// Chapter: Deep Learning / Classification
// Short Description: Get the ground truth classification label id.
void get_classification_ground_truth (HTuple hv_SampleKeys, HTuple hv_DLSample, HTuple *hv_ClassificationLabelIDGroundTruth);
// Chapter: Deep Learning / Classification
// Short Description: Get the predicted classification class ID.
void get_classification_result (HTuple hv_ResultKeys, HTuple hv_DLResult, HTuple *hv_ClassificationClassID);
// Chapter: Deep Learning / Semantic Segmentation and Edge Extraction
// Short Description: Get the confidences of the segmentation result.
void get_confidence_image (HObject *ho_ImageConfidence, HTuple hv_ResultKeys, HTuple hv_DLResult);
// Chapter: OCR / Deep OCR
// Short Description: Get the detection word boxes contained in the specified ResultKey.
void get_deep_ocr_detection_word_boxes (HTuple hv_DeepOcrResult, HTuple hv_ResultKey,
                                        HTuple *hv_WordBoxRow, HTuple *hv_WordBoxCol, HTuple *hv_WordBoxPhi, HTuple *hv_WordBoxLength1,
                                        HTuple *hv_WordBoxLength2);
// Chapter: Deep Learning / Model
// Short Description: Generate NumColors distinct colors
void get_distinct_colors (HTuple hv_NumColors, HTuple hv_Random, HTuple hv_StartColor,
                          HTuple hv_EndColor, HTuple *hv_Colors);
// Chapter: Deep Learning / Model
// Short Description: Generate NumColors distinct colors
void get_distinct_colors_dev_display_pie_chart (HTuple hv_NumColors, HTuple hv_Random,
                                                HTuple hv_StartColor, HTuple hv_EndColor, HTuple *hv_Colors);
// Chapter: Deep Learning / Model
// Short Description: Generate certain colors for different ClassNames
void get_dl_class_colors (HTuple hv_ClassNames, HTuple hv_AdditionalGreenClassNames,
                          HTuple *hv_Colors);
// Chapter: Deep Learning / Model
// Short Description: Return the intended optimization method based on given evaluation key(s).
void get_dl_evaluation_optimization_method (HTuple hv_EvaluationKeys, HTuple *hv_OptimizationMethod);
// Chapter: Deep Learning / Model
// Short Description: Get an image of a sample with a certain key.
void get_dl_sample_image (HObject *ho_Image, HTuple hv_SampleKeys, HTuple hv_DLSample,
                          HTuple hv_Key);
// Chapter: Deep Learning / OCR
// Short Description: Determine the ocr type of the sample based on the sample structure.
void get_dl_sample_ocr_type (HTuple hv_DLSample, HTuple *hv_OCRType);
// Chapter: Deep Learning / Model
// Short Description: Get a parameter value from GenParamValue with the name RequestedGenParamName.
void get_genparam_single_value (HTuple hv_GenParamName, HTuple hv_GenParamValue,
                                HTuple hv_RequestedGenParamName, HTuple *hv_FoundGenParamValue);
// Chapter: 3D Matching / 3D Gripping Point Detection
// Short Description: Extract gripping points from a dictionary.
void get_gripping_points_from_dict (HTuple hv_DLResult, HTuple *hv_Rows, HTuple *hv_Columns);
// Chapter: Graphics / Window
// Short Description: Get the next window that can be used for visualization.
void get_next_window (HTuple hv_Font, HTuple hv_FontSize, HTuple hv_ShowBottomDesc,
                      HTuple hv_WidthImage, HTuple hv_HeightImage, HTuple hv_MapColorBarWidth, HTuple hv_ScaleWindows,
                      HTuple hv_ThresholdWidth, HTuple hv_PrevWindowCoordinates, HTuple hv_WindowHandleDict,
                      HTuple hv_WindowHandleKey, HTuple *hv_CurrentWindowHandle, HTuple *hv_WindowImageRatioHeight,
                      HTuple *hv_PrevWindowCoordinatesOut);
// Chapter: Deep Learning / Model
// Short Description: Return all pixel measures from a specified list of measures.
void get_requested_pixel_measures (HTuple hv_Measures, HTuple hv_EvaluationType,
                                   HTuple *hv_PixelMeasures);
// Chapter: Deep Learning / Semantic Segmentation and Edge Extraction
// Short Description: Get the ground truth segmentation image.
void get_segmentation_image_ground_truth (HObject *ho_SegmentationImagGroundTruth,
                                          HTuple hv_SampleKeys, HTuple hv_DLSample);
// Chapter: Deep Learning / Semantic Segmentation and Edge Extraction
// Short Description: Get the predicted segmentation result image.
void get_segmentation_image_result (HObject *ho_SegmentationImageResult, HTuple hv_ResultKeys,
                                    HTuple hv_DLResult);
// Chapter: Deep Learning / Model
// Short Description: Returns the list of available pixel evaluation measures for the specified type.
void get_valid_pixel_measures (HTuple hv_EvaluationType, HTuple *hv_EvaluationMeasures);
// Chapter: Deep Learning / Semantic Segmentation and Edge Extraction
// Short Description: Get the weight image of a sample.
void get_weight_image (HObject *ho_ImageWeight, HTuple hv_SampleKeys, HTuple hv_DLSample);
// Chapter: Deep Learning / Model
// Short Description: Handle the 'auto' option of the 'overwrite_files' parameter in preprocess_dl_dataset.
void handle_overwrite_files_auto_in_preprocess_dl_dataset (HTuple hv_DLDataset, HTuple hv_DLPreprocessParam,
                                                           HTuple hv_DLDatasetFileName, HTuple *hv_OverwriteFiles);
// Chapter: Deep Learning / Model
// Short Description: Initialize the dictionary RunningMeasures for the evaluation.
void init_running_evaluation_measures (HTuple hv_EvalParams, HTuple *hv_RunningMeasures);
// Chapter: Deep Learning / Model
// Short Description: Initialize change strategies data.
void init_train_dl_model_change_strategies (HTuple hv_TrainParam, HTuple *hv_ChangeStrategyData);
// Chapter: Deep Learning / Model
// Short Description: Initialize the dictionary setting for serialization strategies.
void init_train_dl_model_serialization_strategies (HTuple hv_TrainParam, HTuple *hv_SerializationData);
// Chapter: Deep Learning / Model
// Short Description: Shuffle the input colors in a deterministic way
void make_neighboring_colors_distinguishable (HTuple hv_ColorsRainbow, HTuple *hv_Colors);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Normalize the output features of the Global Context Anomaly Detection model before training.
void normalize_dl_gc_anomaly_features (HTuple hv_DLDataset, HTuple hv_DLModelHandle,
                                       HTuple hv_GenParam);
// Chapter: Graphics / Window
// Short Description: Open a window next to the given WindowHandleFather.
void open_child_window (HTuple hv_WindowHandleFather, HTuple hv_Font, HTuple hv_FontSize,
                        HTuple hv_Text, HTuple hv_PrevWindowCoordinates, HTuple hv_WindowHandleDict,
                        HTuple hv_WindowHandleKey, HTuple *hv_WindowHandleChild, HTuple *hv_PrevWindowCoordinatesOut);
// Chapter: Graphics / Window
// Short Description: Open a new window, either next to the last ones, or in a new row.
void open_next_window (HTuple hv_Font, HTuple hv_FontSize, HTuple hv_ShowBottomDesc,
                       HTuple hv_WidthImage, HTuple hv_HeightImage, HTuple hv_MapColorBarWidth, HTuple hv_ScaleWindows,
                       HTuple hv_ThresholdWidth, HTuple hv_PrevWindowCoordinates, HTuple hv_WindowHandleDict,
                       HTuple hv_WindowHandleKey, HTuple *hv_WindowHandleNew, HTuple *hv_WindowImageRatioHeight,
                       HTuple *hv_PrevWindowCoordinatesOut);
// Chapter: File / Misc
// Short Description: Parse a filename into directory, base filename, and extension
void parse_filename (HTuple hv_FileName, HTuple *hv_BaseName, HTuple *hv_Extension,
                     HTuple *hv_Directory);
// Chapter: OCR / Deep OCR
// Short Description: Parse generic visualization parameters.
void parse_generic_visualization_parameters (HTuple hv_GenParamName, HTuple hv_GenParamValue,
                                             HTuple *hv_BoxColor, HTuple *hv_LineWidth, HTuple *hv_FontSize, HTuple *hv_ShowWords,
                                             HTuple *hv_ShowOrientation);
// Chapter: Graphics / Output
// Short Description: Plot tuples representing functions or curves in a coordinate system.
void plot_tuple_no_window_handling (HTuple hv_WindowHandle, HTuple hv_XValues, HTuple hv_YValues,
                                    HTuple hv_XLabel, HTuple hv_YLabel, HTuple hv_Color, HTuple hv_GenParamName,
                                    HTuple hv_GenParamValue);
// Chapter: Deep Learning / Model
// Short Description: Preprocess the entire dataset declared in DLDataset.
void preprocess_dl_dataset (HTuple hv_DLDataset, HTuple hv_DataDirectory, HTuple hv_DLPreprocessParam,
                            HTuple hv_GenParam, HTuple *hv_DLDatasetFileName);
// Chapter: Deep Learning / Model
// Short Description: Preprocess 3D data for deep-learning-based training and inference.
void preprocess_dl_model_3d_data (HTuple hv_DLSample, HTuple hv_DLPreprocessParam);
// Chapter: Deep Learning / Model
// Short Description: Preprocess anomaly images for evaluation and visualization of deep-learning-based anomaly detection or Global Context Anomaly Detection.
void preprocess_dl_model_anomaly (HObject ho_AnomalyImages, HObject *ho_AnomalyImagesPreprocessed,
                                  HTuple hv_DLPreprocessParam);
// Chapter: Deep Learning / Model
// Short Description: Preprocess the provided DLSample image for augmentation purposes.
void preprocess_dl_model_augmentation_data (HTuple hv_DLSample, HTuple hv_DLPreprocessParam);
// Chapter: Deep Learning / Object Detection and Instance Segmentation
// Short Description: Preprocess the bounding boxes of type 'rectangle1' for a given sample.
void preprocess_dl_model_bbox_rect1 (HObject ho_ImageRaw, HTuple hv_DLSample, HTuple hv_DLPreprocessParam);
// Chapter: Deep Learning / Object Detection and Instance Segmentation
// Short Description: Preprocess the bounding boxes of type 'rectangle2' for a given sample.
void preprocess_dl_model_bbox_rect2 (HObject ho_ImageRaw, HTuple hv_DLSample, HTuple hv_DLPreprocessParam);
// Chapter: Deep Learning / Model
// Short Description: Preprocess images for deep-learning-based training and inference.
void preprocess_dl_model_images (HObject ho_Images, HObject *ho_ImagesPreprocessed,
                                 HTuple hv_DLPreprocessParam);
// Chapter: OCR / Deep OCR
// Short Description: Preprocess images for deep-learning-based training and inference of Deep OCR detection models.
void preprocess_dl_model_images_ocr_detection (HObject ho_Images, HObject *ho_ImagesPreprocessed,
                                               HTuple hv_DLPreprocessParam);
// Chapter: OCR / Deep OCR
// Short Description: Preprocess images for deep-learning-based training and inference of Deep OCR recognition models.
void preprocess_dl_model_images_ocr_recognition (HObject ho_Images, HObject *ho_ImagesPreprocessed,
                                                 HTuple hv_DLPreprocessParam);
// Chapter: Deep Learning / Object Detection and Instance Segmentation
// Short Description: Preprocess the instance segmentation masks for a sample given by the dictionary DLSample.
void preprocess_dl_model_instance_masks (HObject ho_ImageRaw, HTuple hv_DLSample,
                                         HTuple hv_DLPreprocessParam);
// Chapter: Deep Learning / Semantic Segmentation and Edge Extraction
// Short Description: Preprocess segmentation and weight images for deep-learning-based segmentation training and inference.
void preprocess_dl_model_segmentations (HObject ho_ImagesRaw, HObject ho_Segmentations,
                                        HObject *ho_SegmentationsPreprocessed, HTuple hv_DLPreprocessParam);
// Chapter: Deep Learning / Model
// Short Description: Preprocess given DLSamples according to the preprocessing parameters given in DLPreprocessParam.
void preprocess_dl_samples (HTuple hv_DLSampleBatch, HTuple hv_DLPreprocessParam);
// Chapter: Tuple / Conversion
// Short Description: Print a tuple of values to a string.
void pretty_print_tuple (HTuple hv_Tuple, HTuple *hv_TupleStr);
// Chapter: OCR / Deep OCR
// Short Description: Read a dictionary file and return a dl dataset.
void read_dl_dataset_ocr_detection (HTuple hv_DatasetFilename, HTuple hv_ImageDir,
                                    HTuple hv_GenParam, HTuple *hv_DLDataset, HTuple *hv_InvalidSamplesIndices, HTuple *hv_InvalidSamplesReasons);
// Chapter: Deep Learning / Model
// Short Description: Read the dictionaries DLSamples from files.
void read_dl_samples (HTuple hv_DLDataset, HTuple hv_SampleIndices, HTuple *hv_DLSampleBatch);
// Chapter: Image / Manipulation
// Short Description: Change value of ValuesToChange in Image to NewValue.
void reassign_pixel_values (HObject ho_Image, HObject *ho_ImageOut, HTuple hv_ValuesToChange,
                            HTuple hv_NewValue);
// Chapter: Deep Learning / Model
// Short Description: Reduce the evaluation result to a single value.
void reduce_dl_evaluation_result (HTuple hv_EvaluationResult, HTuple hv_EvaluationComparisonKeys,
                                  HTuple *hv_Value, HTuple *hv_ValidEvaluationKeys);
// Chapter: File / Misc
// Short Description: Remove a directory recursively.
void remove_dir_recursively (HTuple hv_DirName);
// Chapter: Deep Learning / Model
// Short Description: Remove invalid 3D pixels from a given domain.
void remove_invalid_3d_pixels (HObject ho_ImageX, HObject ho_ImageY, HObject ho_ImageZ,
                               HObject ho_Domain, HObject *ho_DomainOut, HTuple hv_InvalidPixelValue);
// Chapter: Deep Learning / Model
// Short Description: Replace legacy preprocessing parameters or values.
void replace_legacy_preprocessing_parameters (HTuple hv_DLPreprocessParam);
// Chapter: Deep Learning / Model
// Short Description: Restore serialized DL train information to resume the training.
void restore_dl_train_info_for_resuming (HTuple hv_StartEpoch, HTuple hv_SerializationData,
                                         HTuple hv_TrainParam, HTuple hv_DisplayData, HTuple *hv_EvaluationInfos, HTuple *hv_TrainInfos,
                                         HTuple *hv_DisplayEvaluationEpochs, HTuple *hv_DisplayValidationEvaluationValues,
                                         HTuple *hv_DisplayTrainEvaluationValues, HTuple *hv_DisplayLossEpochs, HTuple *hv_DisplayLoss,
                                         HTuple *hv_DisplayLearningRates, HTuple *hv_TrainResultsRestored, HTuple *hv_StartEpochNumber);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Scale and shift a DL model layer.
void scale_and_shift_dl_model_layer (HTuple hv_DLModelHandle, HTuple hv_LayerName,
                                     HTuple hv_Scale, HTuple hv_Shift);
// Chapter: Filters / Arithmetic
// Short Description: Scale the gray values of an image from the interval [Min,Max] to [0,255]
void scale_image_range (HObject ho_Image, HObject *ho_ImageScaled, HTuple hv_Min,
                        HTuple hv_Max);
// Chapter: Deep Learning / Model
// Short Description: Serialize a DLModelHandle with current meta information.
void serialize_train_dl_model_intermediate (HTuple hv_DLModelHandle, HTuple hv_Epoch,
                                            HTuple hv_EvaluationValueReduced, HTuple hv_Strategy, HTuple hv_TrainInfos, HTuple hv_EvaluationInfos,
                                            HTuple *hv_FilenameModel, HTuple *hv_FilenameMetaData);
// Chapter: Graphics / Text
// Short Description: Set font independent of OS
void set_display_font (HTuple hv_WindowHandle, HTuple hv_Size, HTuple hv_Font, HTuple hv_Bold,
                       HTuple hv_Slant);
// Chapter: Deep Learning / Model
// Short Description: Set the maximum batch size for a given DLModelHandle and GPU.
void set_dl_model_param_max_gpu_batch_size (HTuple hv_DLModelHandle, HTuple hv_BatchSizeUpperBound);
// Chapter: Deep Learning / Model
// Short Description: Split the samples into training, validation, and test subsets.
void split_dl_dataset (HTuple hv_DLDataset, HTuple hv_TrainingPercent, HTuple hv_ValidationPercent,
                       HTuple hv_GenParam);
// Chapter: OCR / Deep OCR
// Short Description: Split rectangle2 into a number of rectangles.
void split_rectangle2 (HTuple hv_Row, HTuple hv_Column, HTuple hv_Phi, HTuple hv_Length1,
                       HTuple hv_Length2, HTuple hv_NumSplits, HTuple *hv_SplitRow, HTuple *hv_SplitColumn,
                       HTuple *hv_SplitPhi, HTuple *hv_SplitLength1Out, HTuple *hv_SplitLength2Out);
// Chapter: System / Operating System
// Short Description: Create a formatted string of a time span.
void timespan_string (HTuple hv_TotalSeconds, HTuple hv_Format, HTuple *hv_TimeString);
// Chapter: Deep Learning / Model
// Short Description: Train a deep-learning-based model on a dataset.
void train_dl_model (HTuple hv_DLDataset, HTuple hv_DLModelHandle, HTuple hv_TrainParam,
                     HTuple hv_StartEpoch, HTuple *hv_TrainResults, HTuple *hv_TrainInfos, HTuple *hv_EvaluationInfos);
// Chapter: Tuple / Element Order
// Short Description: Sort the elements of a tuple randomly.
void tuple_shuffle (HTuple hv_Tuple, HTuple *hv_Shuffled);
// Chapter: Tuple / Arithmetic
// Short Description: Calculate the cross product of two vectors of length 3.
void tuple_vector_cross_product (HTuple hv_V1, HTuple hv_V2, HTuple *hv_VC);
// Chapter: Deep Learning / Model
// Short Description: Update RunningMeasures by evaluating Samples and corresponding Results.
void update_running_evaluation_measures (HTuple hv_Samples, HTuple hv_Results, HTuple hv_EvalParams,
                                         HTuple hv_RunningMeasures);
// Chapter: 3D Matching / 3D Gripping Point Detection
// Short Description: Update running measures for 3D gripping points.
void update_running_gripping_point_measures (HTuple hv_Samples, HTuple hv_Results,
                                             HTuple hv_EvalParams, HTuple hv_RunningMeasures);
// Chapter: Deep Learning / Anomaly Detection and Global Context Anomaly Detection
// Short Description: Update running measures for an anomaly detection or Global Context Anomaly Detection evaluation.
void update_running_image_anomaly_measures (HTuple hv_Samples, HTuple hv_Results,
                                            HTuple hv_EvalParams, HTuple hv_RunningMeasures);
// Chapter: Deep Learning / Classification
// Short Description: Update running measures for an image classification evaluation.
void update_running_image_classification_measures (HTuple hv_Samples, HTuple hv_Results,
                                                   HTuple hv_EvalParams, HTuple hv_RunningMeasures);
// Chapter: Deep Learning / Object Detection and Instance Segmentation
// Short Description: Update running measures for an instance-based evaluation.
void update_running_instance_measures (HTuple hv_Samples, HTuple hv_Results, HTuple hv_EvalParams,
                                       HTuple hv_RunningMeasures);
// Chapter: OCR / Deep OCR
// Short Description: Update running measures for an OCR recognition evaluation.
void update_running_ocr_recognition_measures (HTuple hv_Samples, HTuple hv_Results,
                                              HTuple hv_EvalParams, HTuple hv_RunningMeasures);
// Chapter: Deep Learning / Semantic Segmentation and Edge Extraction
// Short Description: Update running measures for a pixel-based evaluation.
void update_running_pixel_measures (HTuple hv_Samples, HTuple hv_Results, HTuple hv_EvalParams,
                                    HTuple hv_RunningMeasures);
// Chapter: Deep Learning / Model
// Short Description: Update running measures for a region-based evaluation.
void update_running_region_measures (HTuple hv_Samples, HTuple hv_Results, HTuple hv_EvalParams,
                                     HTuple hv_RunningMeasures);
// Chapter: Deep Learning / Model
// Short Description: Update model parameters according to the change strategies.
void update_train_dl_model_change_strategies (HTuple hv_DLModelHandle, HTuple hv_ChangeStrategyData,
                                              HTuple hv_Epoch);
// Chapter: Deep Learning / Model
// Short Description: Serialize the model if a strategy applies to the current training status.
void update_train_dl_model_serialization (HTuple hv_TrainParam, HTuple hv_SerializationData,
                                          HTuple hv_Iteration, HTuple hv_NumIterations, HTuple hv_Epoch, HTuple hv_EvaluationResult,
                                          HTuple hv_EvaluationOptimizationMethod, HTuple hv_DLModelHandle, HTuple hv_TrainInfos,
                                          HTuple hv_EvaluationInfos);
// Chapter: Graphics / Window
// Short Description: Set and return meta information to display images correctly.
void update_window_meta_information (HTuple hv_WindowHandle, HTuple hv_WidthImage,
                                     HTuple hv_HeightImage, HTuple hv_WindowRow1, HTuple hv_WindowColumn1, HTuple hv_MapColorBarWidth,
                                     HTuple hv_MarginBottom, HTuple *hv_WindowImageRatioHeight, HTuple *hv_WindowImageRatioWidth,
                                     HTuple *hv_SetPartRow2, HTuple *hv_SetPartColumn2, HTuple *hv_PrevWindowCoordinatesOut);
// Chapter: Deep Learning / Model
// Short Description: Check that all given entries in EvalParams are valid.
void validate_evaluation_param (HTuple hv_EvalParams, HTuple *hv_Valid, HTuple *hv_Exception);
// Chapter: Deep Learning / Model
// Short Description: Write the dictionaries of the samples in DLSampleBatch to hdict files and store the paths in DLDataset.
void write_dl_samples (HTuple hv_DLDataset, HTuple hv_SampleIndices, HTuple hv_DLSampleBatch,
                       HTuple hv_GenParamName, HTuple hv_GenParamValue);
// Local procedures
// Short Description: Local example procedure for cleaning up files written by example script.
void clean_up_output (HTuple hv_OutputDir, HTuple hv_RemoveResults);
// Short Description: This procedure displays the accuracy comparison of 2 models.
void dev_display_evaluation_comparison (HTuple hv_ModelNames, HTuple hv_EvaluationResults,
                                        HTuple hv_WindowColumn, HTuple *hv_WindowHandle);
// Short Description: Evaluates a Deep OCR detection model on a given dataset split.
void evaluate_ocr_detection (HTuple hv_DLModelHandle, HTuple hv_DLDevice, HTuple hv_ImageWidth,
                             HTuple hv_ImageHeight, HTuple hv_BatchSize, HTuple hv_DLDataset, HTuple hv_EvaluationSplit,
                             HTuple hv_MaxNumSamples, HTuple *hv_EvaluationResult);
// Short Description: This procedure prepares the comparison visualization.
void prepare_detailed_comparison_visualization (HTuple hv_ImageWidth, HTuple hv_ImageHeight,
                                                HTuple *hv_VisualizationDict);

//添加ocr——rec训练接口
void convert_dl_dataset_ocr_detection_to_recognition (HTuple hv_DLDatasetOCRSource,
    HTuple *hv_DLDatasetOCRRecognition);

void read_dl_dataset_ocr_recognition (HTuple hv_FileName, HTuple hv_ImageDir, HTuple hv_GenParam,
    HTuple *hv_DLDataset);

void gen_dl_dataset_ocr_recognition_statistics (HTuple hv_DLDataset, HTuple *hv_CharStats);

void find_invalid_samples_dl_ocr_recognition (HTuple hv_Samples, HTuple hv_DLModelHandle,
    HTuple hv_GenericParam, HTuple *hv_Indices, HTuple *hv_Reasons);

//det模型  api
void read_dl_dataset_from_coco (HTuple hv_CocoFileName, HTuple hv_ImageDir, HTuple hv_GenParam,
    HTuple *hv_DLDataset);

void get_dl_dataset_from_coco_annotation_image_id (HTuple hv_AnnotationList, HTuple hv_AnnotationKeys,
    HTuple hv_NumKeysPerThread, HTuple hv_UniqueIndex, HTuple *hv_AnnotImageIDsOutput,
    HTuple *hv_Exception);

void create_dl_dataset_from_coco_samples (HTuple hv_ImageList, HTuple hv_ImageKeys,
    HTuple hv_ImageDir, HTuple hv_AnnotationList, HTuple hv_AnnotationKeys, HTuple hv_AnnotImageIDs,
    HTuple hv_Purpose, HTuple hv_ReadOnlyNonCrowd, HTuple hv_ReadRawAnnotations,
    HTuple hv_ReadMasks, HTuple hv_NumSamplesPerThread, HTuple hv_UniqueIndex, HTuple *hv_DLSamplesOutput,
    HTuple *hv_Exception);

void convert_coco_segmentation_to_region (HObject *ho_Region, HTuple hv_Segmentation);

void decompress_coco_rle (HTuple hv_CompressedRLE, HTuple hv_Width, HTuple hv_Height,
    HTuple *hv_RunLengthEncoding);

void convert_coco_rle_to_region (HObject *ho_Region, HTuple hv_RunLengthEncoding,
    HTuple hv_Width, HTuple hv_Height);
}



#endif
