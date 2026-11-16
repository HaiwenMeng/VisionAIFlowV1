import json
from pathlib import Path
from types import SimpleNamespace

import cv2
import numpy as np
import torch
import torch.nn.functional as F
from models.pidinet import pidinet

SIZE = (320, 240)
MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32)
STD = np.array([0.229, 0.224, 0.225], dtype=np.float32)


def read_image(path):
    image = cv2.imdecode(np.fromfile(str(path), dtype=np.uint8), cv2.IMREAD_COLOR)
    if image is None:
        raise ValueError(f'Cannot read image: {path}')
    return image


def read_sample(sample):
    image_path = Path(sample['image'])
    annotation_path = Path(sample['annotation'])
    image = read_image(image_path)
    document = json.loads(annotation_path.read_text(encoding='utf-8'))
    height, width = image.shape[:2]
    if document['imageWidth'] != width or document['imageHeight'] != height:
        raise ValueError(f'Annotation image size mismatch: {annotation_path}')
    mask = np.zeros((SIZE[1], SIZE[0]), dtype=np.uint8)
    shapes = document['shapes']
    for shape in shapes:
        points = np.asarray(shape['points'], dtype=np.float64)
        if shape['shape_type'] != 'line' or points.shape != (2, 2) or not np.isfinite(points).all():
            raise ValueError(f'Invalid two-point line: {annotation_path}')
        if (points < 0).any() or (points[:, 0] >= width).any() or (points[:, 1] >= height).any():
            raise ValueError(f'Line outside image: {annotation_path}')
        points = np.rint(points * np.array([SIZE[0] / width, SIZE[1] / height])).astype(np.int32)
        if np.array_equal(points[0], points[1]):
            raise ValueError(f'Line collapses at training resolution: {annotation_path}')
        cv2.line(mask, tuple(points[0]), tuple(points[1]), 255, 1, cv2.LINE_8)
    if not mask.any():
        raise ValueError(f'No line annotations: {annotation_path}')
    return cv2.resize(image, SIZE, interpolation=cv2.INTER_AREA), (mask > 0).astype(np.float32)


def tensor_image(image):
    rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
    rgb = (rgb - MEAN) / STD
    return torch.from_numpy(rgb.transpose(2, 0, 1).copy()).unsqueeze(0).cuda()


def make_model(weights):
    if not torch.cuda.is_available():
        raise RuntimeError('CUDA unavailable')
    model = pidinet(SimpleNamespace(config='carv4', dil=True, sa=True)).cuda()
    checkpoint = torch.load(weights, map_location='cpu', weights_only=True)
    model.load_state_dict({key.removeprefix('module.'): value for key, value in checkpoint['state_dict'].items()})
    return model


def balanced_loss(prediction, target):
    positives = target.sum()
    negatives = target.numel() - positives
    if positives.item() == 0 or negatives.item() == 0:
        raise ValueError('Training label has no positive or negative pixels')
    weights = torch.where(target > 0, negatives / target.numel(), positives / target.numel())
    return F.binary_cross_entropy(prediction, target, weight=weights, reduction='mean')


def score(probability, label):
    prediction = (probability >= 0.5).astype(np.uint8)
    truth = (label > 0).astype(np.uint8)
    kernel = np.ones((5, 5), np.uint8)
    p = int(prediction.sum())
    t = int(truth.sum())
    precision = float((prediction * cv2.dilate(truth, kernel)).sum() / p) if p else 0.0
    recall = float((truth * cv2.dilate(prediction, kernel)).sum() / t) if t else 0.0
    f1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
    return precision, recall, f1
